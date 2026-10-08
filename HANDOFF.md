# HybridEQ — Engineering Handoff

**Version:** 1.6.8
**Repo:** `/Volumes/Developer/Developer/HybridEQ`
**Date of handoff:** 2026-10-07
**Purpose of this document:** transfer the current state, the in-flight work, and the known
problems to the next engineer. It is written to be read cold — no prior context assumed.

---

## 1. What this project is

HybridEQ is a multi-band parametric EQ plugin (inspired by Pro-Q 4 / Kirchhoff-EQ) with an
unusual extra: a **real-time SPICE-class circuit simulator** used for its preamp section. The
preamp is not a waveshaper or a convolution — it solves an actual transistor-level netlist with
nodal analysis and Newton iteration, per sample.

**Formats:** VST3, AU (macOS), LV2, CLAP, AAX (optional, if SDK present), Standalone.
**Framework:** JUCE 9.0.3 (fetched by CMake), `clap-juce-extensions` for CLAP.
**UI:** custom dark flat LookAndFeel + GoodLookinUI v0.2.2 (git submodule, `third_party/GoodLookinUI`).

### The four preamp flavours

| Enum | Name | Netlist | Notes |
|---|---|---|---|
| `Brit = 0` | Brit | `Source/DSP/BritCircuit.h` | |
| `N` | N-Type | `Source/DSP/NTypeCircuit.h` | The most complete model. Used for all CPU work below. |
| `FSF` | FSF | `Source/DSP/FsfCircuit.h` | Op-amp is a `[PLACEHOLDER]` behavioural model |
| `Off` | Off | — | |
| `AType` | A-Type | `Source/DSP/ATypeCircuit.h` | Op-amp is a `[PLACEHOLDER]` behavioural model |

**Important:** the enum order is load-bearing. Saved sessions store the *choice index*, so new
flavours must be appended at the end, never inserted.

### Provenance tagging convention

Every component value in the netlist headers carries a tag: `[NETLIST]`, `[DATASHEET]`,
`[ESTIMATE]`, or `[PLACEHOLDER]`. This is deliberate and valuable — it records what is known
versus guessed. **Preserve this convention when editing netlists.** The `[ESTIMATE]` values are
the tuning surface; the `[PLACEHOLDER]` ones are the known-weak spots.

---

## 2. Build and test

```bash
cd /Volumes/Developer/Developer/HybridEQ

# Configure (Xcode generator is what the existing build dirs use)
cmake -S . -B build3

# Build the plugin (Release)
cmake --build build3 --config Release --target HybridEQ_VST3

# Build and run the stress test
cmake --build build3 --config Release --target HybridEQStress
./build3/HybridEQStress_artefacts/Release/HybridEQStress --quick

# Build and run the CPU table
cmake --build build3 --config Release --target HybridEQCPU
./build3/HybridEQCPU_artefacts/Release/HybridEQCPU
```

**Build directories present:** `build`, `build-dev`, `build-test`, `build2`, `build3`.
`build3` is the current/active one. The others are stale — do not assume they reflect HEAD.

**Gotcha:** `cmake --build` without `--config Release` builds **Debug**, which lands in a
different artefact path (`.../Debug/...`) and is ~3x slower. This has already caused one
confusing "CPU regression" that was not a regression. Always pass `--config Release` for
performance measurements.

**Gotcha:** after adding a new CMake target you must re-run the `cmake -S . -B build3` configure
step, or `xcodebuild` will fail with "does not contain a target named ...".

**Test suite:** `tests/processor_stress.cpp` — 620 checks covering rate/block/oversampling
combinations, extreme parameter sets, state round-trip, lifecycle, thread safety, and CPU.
Run it with `--quick` for a fast pass. It is the primary regression gate.

---

## 3. Work completed in this session (uncommitted)

All changes below are **in the working tree, not committed**. `git status` shows 7 modified
files and 1 new file.

### 3.1 Fix: audio-thread latency mutation (DONE, VERIFIED)

**Problem.** `updateParameters()` was called at the top of `processBlock()` and did two things
that are illegal on the audio thread:

1. Called `setLatencySamples(...)`, changing the reported latency mid-stream.
2. Called `setValueNotifyingHost()` on the `oversampleMode` **parameter**, writing a
   host-visible parameter from the audio thread.

The project's own stress test caught this: *"reported latency changed 22 times in 400 blocks
(range 0..64 samples) from the audio thread."* Hosts and AAX validation require latency to be
settled in `prepareToPlay` and never to move while running.

**Fix.** Latency is now set **only** in `prepareToPlay()`. The Circuit headroom floor (which
raises the effective oversampling to 4x) is applied to the *engine* only; the user's
`oversampleMode` choice is never rewritten. The effective factor is published in a new
`std::atomic<int> effectiveFactorAtomic` and surfaced to the editor, which displays it on the
OVERSAMPLE key (with a `*` suffix when the floor raised it above the user's choice).

**Files changed:**
- `Source/PluginProcessor.h` — `updateParameters(bool)` → `updateParameters()`; added
  `effectiveFactorAtomic` and public `getEffectiveOversampleFactor()`.
- `Source/PluginProcessor.cpp` — removed the parameter write and the `setLatencySamples` call.
- `Source/UI/CycleButton.h` — added `setDisplayTextOverride()`.
- `Source/PluginEditor.cpp` / `.h` — `oversampleDisplayText()`, refreshed in `timerCallback()`.
- `tests/processor_stress.cpp` — the latency check is now a hard assertion, not an INFO print.

**Verification (Release, `HybridEQStress --quick`):**
```
1b) with oversampling / High Cut / High shelf / Circuit automated,
    reported latency changed 0 times in 400 blocks (range 0..0 samples)
6) CPU: 26.7% of one core per instance at 48 kHz / 512 / 8x oversampling (3.8x real time)
620 checks, 0 failed
```
Before: 22 latency changes, 29.1% CPU. After: 0 changes, 26.7% CPU.

**Behaviour change to be aware of:** previously, enabling CIRCUIT visibly flipped the OVERSAMPLE
control to 4x and it never went back down. Now the user's choice is preserved and the key shows
the effective factor instead. This is the intended trade.

### 3.2 New: per-configuration CPU table (DONE)

`tests/cpu_table.cpp` + a `HybridEQCPU` CMake target. Measures `% of one core` across
1x/4x/8x × 44.1/48/96/192 kHz × 64/128/512/1024 block size × circuit on/off, stereo, N-Type.
96 configurations. This is the baseline for any solver optimisation work.

---

## 4. THE CRITICAL FINDING — read this before doing anything

The CPU table revealed that **the oversampling control has no effect on the preamp circuit.**

`Source/DSP/Preamp.h:346`:

```cpp
nDecim = std::max(1, static_cast<int>(std::lround(sr / kCircuitRate)));
```

with `kCircuitRate = 96000.0` (`Preamp.h:524`). The circuit is fed the *oversampled* rate, then
decimated straight back down to ~96 kHz. At 48 kHz with 8x oversampling the circuit sees
384 kHz, so `nDecim = 4` → back to 96 kHz. At 192 kHz with 8x it sees 1536 kHz, `nDecim = 16` →
again 96 kHz.

**Result: the circuit always runs at exactly 96 kHz regardless of the oversampling setting.**

Evidence from the table (`% of one core`, stereo, N-Type, Release):

| rate | block | os | circuit OFF | circuit ON |
|---|---|---|---|---|
| 44100 | 512 | 1x | 34.6% | **123.0%** |
| 44100 | 512 | 4x | 67.8% | **120.6%** |
| 44100 | 512 | 8x | 136.3% | **123.1%** |
| 48000 | 512 | 1x | 37.5% | **130.5%** |
| 48000 | 512 | 4x | 74.5% | **132.0%** |
| 48000 | 512 | 8x | 148.6% | **133.6%** |
| 96000 | 512 | 1x | 37.7% | **130.5%** |
| 96000 | 512 | 4x | 149.2% | **126.6%** |
| 96000 | 512 | 8x | 295.9% | **133.8%** |
| 192000 | 512 | 1x | 77.4% | **127.6%** |
| 192000 | 512 | 4x | 297.0% | **128.5%** |
| 192000 | 512 | 8x | 590.4% | **140.6%** |

The "circuit ON" column is **flat at ~120–135%** across every rate and oversampling setting. The
"circuit OFF" column scales steeply (37% → 148% → 296% → 590%) because it is only paying for the
resampler. The circuit cost is invariant.

**Consequences:**

1. **A single preamp instance with the circuit on costs more than one full CPU core.** This is
   not shippable as a standalone preamp plugin in its current form.
2. The circuit is doing ~96 kHz × 2 ch × ~8 Newton iterations ≈ 1.5M full linear solves per
   second. That is essentially the entire cost of the plugin.
3. Any optimisation work must target the solver. Nothing else is large enough to matter.

---

## 5. The next task (agreed with the owner, NOT STARTED)

Build a **standalone preamp plugin** as a separate target alongside HybridEQ, reusing
`CircuitSolver` and the netlist headers.

### 5.1 Agreed oversampling behaviour

The owner's specification, verbatim intent:

- The circuit runs at **88.2 / 96 kHz** by default.
- If the DAW session rate is **higher** than that, the circuit **follows the session rate**
  (no decimation).
- Therefore: session at 48 kHz → circuit runs at **2x** (96 kHz). Session at 96 kHz → circuit
  runs at **native** (96 kHz). Session at 192 kHz → circuit runs at **192 kHz** (native).
- The plugin must expose **4x and 8x** oversampling modes.
- **When the circuit is activated it automatically jumps to 4x** — e.g. session at 48 kHz →
  circuit operates at 192 kHz.

In other words: the target internal rate is `max(88200 or 96000, sessionRate)`, and the user's
oversampling selection multiplies on top of that, with a 4x floor applied when the circuit is on.

> Update (1.6.10): the circuit floor is now **2x**; 4x/8x are still user choices. The static (Circuit off)
> preamp models are anti-aliased with ADAA (`Source/DSP/Adaa.h`).

**This is the inverse of the current behaviour** and is the right fix — it makes the oversampling
control actually do something for the preamp, and it makes the 48 kHz default roughly 2x cheaper
than today.

### 5.2 Work items

1. **New CMake target** for the preamp plugin (VST3/AU/LV2/CLAP/Standalone, mirroring the
   `HybridEQ` target's format options in `CMakeLists.txt:33-76`).
2. **Remove the decimation stage** from the preamp path — `nDecim`, `nAaDown`, `nAaUp`, and the
   `Lp8` anti-alias filters in `Preamp.h` become unnecessary if the circuit runs at the
   oversampled host rate. Confirm whether they are still needed for the "follow the session rate"
   case at 192 kHz.
3. **Implement the rate policy** in 5.1.
4. **Fix the latent bug in 5.3 below** — it becomes live once the rate changes at runtime.
5. **Re-run the CPU table** to prove the improvement, and extend it to cover the new plugin.

### 5.3 Latent bug that must be fixed first

`Source/DSP/CircuitSolver.h:170-177`:

```cpp
void restoreDc(double sampleRate) noexcept
{
    sr = sampleRate;
    h = 0.5 / sampleRate;
    for (auto& c : caps) c.geq = 2.0 * c.c * sampleRate;
    buildLinear();
    restoreDc();
}
```

This calls `buildLinear()` but **not** `buildSymbolic()`. `buildSymbolic()` is what freezes the
pivot order and computes the fill-in lists (`sNzIdx`, `sElimRows`) used by the fast path
`solveStatic()`. After a rate change, `staticOk` remains true but the frozen pattern was computed
for the *old* rate — so `solveStatic()` runs against a stale fill pattern.

The current fixed-96 kHz design mostly hides this because the rate rarely changes. **The new
preamp plugin changes rate on every oversampling switch, so this becomes a live correctness bug.**
Fix: call `buildSymbolic()` from `restoreDc(sampleRate)`, or invalidate `staticOk`.

### 5.4 Then: solver optimisation (the real 2–4x)

The solver is already partly sparse-aware, but the fallback path is not:

- `buildSymbolic()` (`CircuitSolver.h:668`) freezes the pivot order and derives elimination lists
  including fill-in. `solveStatic()` (`CircuitSolver.h:615`) uses them. **This path is good.**
- `solveDynamic()` (`CircuitSolver.h:566`) — the fallback — still scans the **full dense**
  `n × n` Jacobian for pivoting on every call, and `jac` is a dense `std::vector<double>` of
  `n*n` entries.

**Measured structure** (from `NTypeCircuit`, the heaviest netlist):

- 34 node variables + 4 inductor branch currents + 2 ideal-transformer currents = **n = 40**
- Dense Jacobian: 40 × 40 = **1600 doubles = 12.5 KB**
- Structural non-zeros (nodes only): **120 of 1156 = 10.4% dense**
- Per Newton iteration, `solveDynamic` scans all 1600 entries for pivoting, then up to 1600 more
  in the elimination/back-substitution loops
- × ~8 Newton iterations × 48 kHz × 2 channels ≈ **1.2 billion Jacobian-entry touches per second**

The optimisation is to make the *fallback* path use the frozen symbolic structure too, so the
cost follows the ~10% sparsity instead of the full dense grid. Expected 2–4x on the solver, which
is essentially the whole plugin.

**Do not start this until the CPU table has been re-run after items 5.1–5.3.** Optimising without
a fresh baseline is guessing.

---

## 6. Known issues and risks (not yet addressed)

| # | Issue | Severity | Location |
|---|---|---|---|
| 1 | Circuit locked at 96 kHz; oversampling control is inert for the preamp | **Critical** | `Preamp.h:346` |
| 2 | `restoreDc(sampleRate)` leaves the symbolic factorisation stale | **High** (becomes critical once rate changes at runtime) | `CircuitSolver.h:170` |
| 3 | `solveDynamic()` fallback is dense, not sparse | **High** | `CircuitSolver.h:566` |
| 4 | A-Type and FSF op-amps are `[PLACEHOLDER]` behavioural models | Medium | `ATypeCircuit.h`, `FsfCircuit.h` |
| 5 | Reported latency no longer tracks the effective oversampling factor after a runtime change. This is the correct host behaviour (latency must be settled in `prepareToPlay`), but means the oversampling filter's own delay is only compensated for the factor chosen at prepare time. | Medium | `PluginProcessor.cpp` |
| 6 | Netlist `[ESTIMATE]` values are unfitted — notably `kInL0` (input transformer inductance), flagged in-source as "THE KEY NUMBER TO FIT" | Medium | `NTypeCircuit.h:29` |

---

## 7. Environment notes

- **The shell environment on this machine is unreliable.** During this session it repeatedly
  hung for minutes with no output, and twice wedged completely mid-command (once during a
  compile). Recovery required waiting or restarting. If a command hangs, do not assume it is the
  command — check whether the shell itself is wedged with a trivial `echo`.
- Long builds and the CPU table take several minutes. The CPU table runs 96 configurations at
  4 seconds each; budget ~7 minutes and expect the tool call to time out and continue in a
  background log file.
- Platform: macOS (darwin 24.6.0), arm64. Xcode generator.

---

## 8. Suggested order of work

1. **Commit the current work.** The latency fix is verified and self-contained; it should not sit
   uncommitted while the next task begins. Suggested message:
   `Fix audio-thread latency mutation; add per-configuration CPU table`
2. **Fix `restoreDc(sampleRate)`** (§5.3) — small, contained, and a prerequisite for the new
   plugin.
3. **Build the preamp plugin target** with the rate policy in §5.1.
4. **Re-run the CPU table** and confirm the expected ~2x improvement at 48 kHz / 1x.
5. **Then** attack the solver sparsity (§5.4), with before/after numbers.
6. **Then** discuss new preamp models or improving the `[PLACEHOLDER]` op-amps.

---

## 9. Quick reference — key files

| File | What it is |
|---|---|
| `Source/DSP/CircuitSolver.h` | Nodal analysis + Newton solver. The performance-critical file. |
| `Source/DSP/Preamp.h` | Preamp engine: type dispatch, decimation, per-flavour processing. |
| `Source/DSP/NTypeCircuit.h` | The most complete netlist (34 nodes). |
| `Source/DSP/BritCircuit.h`, `FsfCircuit.h`, `ATypeCircuit.h` | The other three netlists. |
| `Source/DSP/Resampler.h` | `MultirateEngine` — 1x/2x/4x/8x oversampling, latency reporting. |
| `Source/PluginProcessor.cpp` | Parameter layout, `prepareToPlay`, `updateParameters`, `processBlock`. |
| `Source/PluginEditor.cpp` | UI. `oversampleDisplayText()` shows the effective factor. |
| `Source/UI/CycleButton.h` | The cycling parameter key used throughout the UI. |
| `tests/processor_stress.cpp` | 620-check regression gate. |
| `tests/cpu_table.cpp` | Per-configuration CPU measurement. |
| `Reference/*.cir`, `Reference/*.json` | Source netlists and calibration data. |
