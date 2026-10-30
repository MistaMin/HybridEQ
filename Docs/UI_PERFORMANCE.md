# Rendering rollback — 2026-10-29

The bitmap artwork cache caused unacceptable softness in user testing. The earlier
software-only equality checks did not establish appearance on the native window.
Do not treat the archived benchmark results below as approval of that rendering path.

Production UI drawing and scheduling files are restored byte-for-byte from v1.6.13:
PluginEditor.cpp/.h, Toolkit.h, EQDisplay.h, RotaryKnob.h, SectionPanel.h, and
SpectrumOverlay.h. This includes the original spectrum renderer and knob spring.
The cache has been removed. The spectrum experiment is retained only under
`tests/experimental/`; production does not include it. Version and installer/release
work remain intact. Published 1.6.14 assets have not been replaced by this local build.

## Next investigation, appearance first

1. Establish native-window screenshots at matching size, theme, parameter state,
   renderer and display scale against 1.6.13; require user visual acceptance.
2. Profile message-thread paint time, callback count, repaint regions and drag latency
   on Windows with the analyser on and off, including integrated graphics hardware.
3. Reduce unnecessary work first: idle timers, duplicate analysers behind overlays,
   hidden-editor updates and parameter-notification queueing. Measure each separately.
4. Keep labels, tick marks, pointers and panel artwork on the original drawing path.
   Investigate reusable vector geometry, gradient definitions and clip-aware drawing
   only after profiling identifies the expensive functions. Do not introduce raster
   layers as an assumed shortcut.
5. Rework knob scheduling separately from artwork: avoid fixed-step animation lag
   and idle callbacks, with explicit visual/interaction acceptance before adopting it.
6. Any new change must pass native-window appearance checks at supported scales and
   actual end-to-end frame/drag measurements; software pixel tests alone are insufficient.

No layout, fonts, colours, audio DSP or parameters are redesigned by this rollback.

## Step 3 change log

- Version 1.6.16 (`VERSION`, the single source for CMake). README, RELEASE_NOTES and
  UserManual still say 1.6.14 and need updating when this is released.
- Suspended the small EQ-scope analyser and its repaints while the full-window analyser
  is open (`EQDisplay::setCoveredByOverlay`, wired from `SpectrumOverlay::onVisibilityChanged`).
  Kept because it is correct, but it did not measurably reduce CPU (see below).

### Profile, arm64 Mac, standalone Release 1.6.16

Each figure is the mean of three `top` samples taken 6 s after launch, with the first
sample discarded. Each variant was run twice and the second round is quoted. Runs differ by
about ±5 points, so differences smaller than that are not meaningful.

| Variant | CPU |
|---|---|
| Overlay closed, default settings (spectrum on) | ~13% |
| Small scope spectrum off | ~12% |
| Small scope timer disabled entirely | ~7% |
| Full-window analyser open | ~53% |
| Full-window analyser open, small scope not covered (no change) | ~53% |
| Full-window analyser open, timer at 15 Hz instead of 30 Hz | ~42% |

Findings:

- The full-window analyser is the dominant cost: about 40 points of CPU when open.
  `sample` attributes most of it to `SpectrumRenderer::paint` (column `fillRect`s and
  the spectrogram `drawImage`), plus `drawText`, from the toolkit's renderer.
- Halving the overlay's timer rate saves about 11 points. It is a visible trade-off,
  so it is not adopted yet.
- The small-scope cover change is within noise when the overlay is open. Its only
  measured cost is the small scope's own timer, roughly 6 points at idle.
- Idle with the overlay closed is about 13%. The small scope's timer accounts for about
  half of that.

### Renderer investigation

The toolkit's `SpectrumRenderer` (third_party, pinned) was read. Its costs by style, from
`HybridEQSpectrumTest` (software raster, 1100×650, reference vs `tests/experimental`):

| Style | Reference | Experimental | Speedup | Pixels |
|---|---|---|---|---|
| Standard 4096 / 16384 | 0.54 / 0.51 ms | 0.52 / 0.50 ms | ~1.0× | identical |
| Multicolour 4096 / 16384 | 1.34 / 0.86 ms | 1.33 / 0.84 ms | ~1.0× | identical |
| Bars 4096 / 16384 | 0.36 / 0.28 ms | 0.34 / 0.26 ms | ~1.1× | identical |
| Spectrogram 4096 / 16384 | 2.56 / 2.65 ms | 0.36 / 0.50 ms | 5–7× | identical |

The spectrogram gain is real in the bench, and the overlay opens in spectrogram mode by
default. In the standalone it did not show: overlay-open CPU stayed at about 50% with
and without the experimental adapter. The toolkit's Multicolour path (one gradient-filled
rectangle per column) is the costly one for the small scope, but it is not what the overlay
uses by default.

Experiments run and reverted, with no adopted change:

- Repainting only below the static header: no measurable change.
- Experimental adapter in the overlay: bench gain, no measured standalone gain.

Controlled rerun (standalone Release 1.6.16, app focused via System Events, median of five
`top` samples at 2 s, three interleaved rounds). Machine load stayed at 10–21 from other
apps, which were not stopped. Round-to-round spread was within about ±3 points:

| Variant | CPU (median) |
|---|---|
| Overlay closed | ~19% |
| Overlay open | ~61% |
| Overlay open, overlay's own repaint suppressed (renderer still updates) | ~34% |
| Overlay open, overlay EQ timer suppressed | ~47% |

Reading: the overlay's own full-window repaint costs about 27 points. Its EQ-curve
timer (20 Hz, `EQDisplay` in overlay mode) costs about 14 points, and that repaint is
mostly redundant: the curve changes only with parameters, which already repaint.
The renderer's update and the rest of the overlay account for the remaining ~15 points.
Standalone CPU values from the earlier, uncontrolled runs should be disregarded.

Next: measure on a controlled machine (window focused, spectrum settings fixed, other load
stopped). Then decide whether the overlay's timer rate change (30 → 15 Hz, about 11 points
in one run) and the experimental spectrogram adapter are worth their appearance-check cost.
Both need native-window appearance checks before adoption.

The profiling hooks used for these figures were temporary and have been removed.

---

## Archived investigation (superseded)

# HybridEQ UI performance investigation — 2026-10-28

Scope: 6 items; 4 verified in local engineering checks, 2 unverified for live host/event behaviour and Windows hardware acceptance. Implementation complete; local checks pass. Affected Windows machine unavailable. Source base a327f6a54b6fbf24a0256579643ed62b8549ddb0 plus uncommitted changes. Logs and screenshots are beside this report.

The user's follow-up reports that changing FFT settings has little effect, and all knob interaction is sluggish. This points beyond the analyser. Code inspection confirms that RotaryKnob used a 60 Hz spring with a fixed simulated 1/60-second step. When the message thread runs at 10 Hz, that spring takes six times as long in wall time. This is a confirmed lag amplifier; it does not establish the original cause of slow Windows frame delivery.

Changes:

- Display knob values immediately, eliminating the fixed-step spring and every per-knob periodic timer.
- Cache the existing knob, section-panel and editor-chassis artwork. Invalidate on appearance/value/size/physical-scale changes. Reuse storage while dragging. Preserve fonts, hardware styles, colours, value labels, bindings and host parameter definitions.
- Coalesce EQ notifications into an atomic dirty flag, eliminating queued heap-allocated callAsync work for every frequency/gain change. Observe the remaining EQ controls too. Reduce idle repaint cadence with the analyser off, retain periodic engine-rate refresh, skip hidden editor work, and suspend the small analyser behind the overlay.
- Use a project-owned spectrum adapter based on the pinned MIT toolkit renderer. Cache display columns and frequency colours; retain FFT sizes, calibration, smoothing, peaks, history, settings serialization and both palettes. Colour only new spectrogram rows, then display the circular image in two slices. The toolkit submodule is unchanged.

Validation distinguishes controlled software rasterization from real DAW/GPU interaction. FFT and audio effects processing are separate: no processBlock or DSP implementation changed. Pixel/FFT comparisons cover the display path, while the existing processor stress test covers audio regressions; this is not an old/new audio-null measurement.

Renderer comparison passed: identical channel pixels across all four styles and both palettes, FFT display/hold arrays identical, ring wrap, multiple updates between paints, settings changes and resize. On this arm64 Mac, final matched 1100×650 software rendering at 4096/16384 FFT measured spectrogram 5.48× / 4.73× faster (2.702 → 0.493 ms / 2.722 → 0.575 ms), with ordinary styles approximately unchanged (0.96–1.05×). These figures cannot be presented as Windows FPS.

The static GUI binding check remains 39/45, matching the six documented regex misses: harmonic2nd..5th are attached through ids[] in HarmonicsPanel.h; preampCircuit/preampHQ are handled by PreampModeKey.h. No parameters or controls were added or removed.

Rendered editor screenshot reviewed: knobs/readouts, six EQ strips, preamp, harmonics, oversampling, output and meter remain visible and legible at 2×. This is an offscreen render, not an interaction test in a host.

Windows acceptance still required: Release x86_64 build; measure editor frame delivery and drag-to-display latency with spectrum on/off, editor scales 100/150/200%, and JUCE's Direct2D/software engines. Capture DAW, plugin version, display scaling and active renderer. No Windows device is connected; the user does not know the reporter's DAW/version/scaling. No release or deployment performed.

## Final validation

| Check | Result | Evidence / limit |
|---|---|---|
| Universal macOS Release builds | PASS | VST3, AU, AAX, CLAP, LV2, Standalone; build-verified.log. AAX was built without PACE release wrapping. |
| Processor stress test | PASS | 812 checks, 0 failed; stress-results.txt. Includes state, lifecycle, automation and hostile blocks. This is a harness, not a DAW. |
| Rendering regression | PASS | ui-results-final.txt: all pixel comparisons exact; FFT/hold arrays exact; no failures. |
| Knob response and cache | PASS | Immediate value-to-paint response without timer dispatch; direct/cached pixels identical at 1× and 2×; bypass, flat style, size and fractional-scale invalidation checks. |
| Cached knob microbenchmark | MEASURED | 18 repeated paints of the same unchanged, inactive console knob: 3.534 → 0.119 ms at 1× (29.57×); 7.107 → 0.391 ms at 2× (18.18×). This excludes moving-knob cache misses and is not whole-editor FPS. |
| Screenshot review | PASS (rendered) | editor-final.png at 2×; all four populated spectrum styles and knob.png inspected. No layout changes observed. |
| Static binding scan | KNOWN LIMIT | 39/45, six previously documented regex misses; source bindings unchanged. |
| EQ event coalescing / hidden work | IMPLEMENTED, runtime unverified | Code review confirms atomic dirty flag, bounded refresh, visibility guard and overlay suspension. Live host scheduling/interaction remains unmeasured. |
| DAW and format validation | SKIPPED | No newly installed candidate or validator/DAW run; local builds and offscreen harnesses only. |
| Windows x86_64 / Iris Xe acceptance | UNVERIFIED | No target device; cannot establish the reporter's original rendering bottleneck or end-to-end FPS. |

Existing compiler warnings remain. New exact floating-point comparisons are deliberate cache-key invalidation (value, bounds and physical scale), not numerical convergence checks.

## Reproduce

Configure with HYBRIDEQ_BUILD_TESTS=ON, then build HybridEQSpectrumTest and HybridEQStress in Release. Run HybridEQSpectrumTest with an absolute output-directory argument, followed by HybridEQStress --quick. On this machine the binaries are under build3/<target>_artefacts/Release/. Stop compilation before timing. The benchmark alternates reference/candidate order and excludes history warm-up.

Raw evidence: /private/tmp/hybrideq-spectrum-2026-10-28/. Source and binary SHA-256 identities are in source-identity.json. No commits, pushes, releases, signing workflow, or installed-plugin changes were performed.

## 1.6.13 against 1.6.16, standalone (same machine, interleaved)

Both apps were built Release, universal, from `v1.6.13` and the current tree. The
overlay was opened by clicking SPECTRUM through System Events. Three interleaved rounds
per configuration, median of five 2 s `top` samples:

| Configuration | 1.6.13 | 1.6.16 |
|---|---|---|
| Overlay closed | 14.5 / 17.1 / 14.8% | 18.3 / 17.2 / 13.8% |
| Overlay open | 60.7 / 60.5 / 58.6% | 58.9 / 57.6 / 59.6% |

Result: no measurable CPU difference. The 1.6.16 change (pausing the small scope while
the overlay is open) is within noise. The machine was under heavy load from other
processes during these runs (load averages 500–1000, idle 0–16%), so absolute numbers are
not reliable. The relative comparison was run under the same conditions.

## Thread isolation and the opaque overlay (1.6.16)

Sampled with `sample` (5 s, overlay open). Message thread: about 52% busy. Its time was
dominated by `paintEntireComponent` (about 1,500 samples), of which `SpectrumOverlay::paint`
was about 490. The rest was repainting of the whole window beneath the overlay:
`RotaryKnob::paint`, `EQDisplay::paint` and the knob drawing. The cause is that
`SpectrumOverlay` did not declare itself opaque, so JUCE repainted everything under it.
Its `paint()` fills the whole area, so it can safely be opaque.

Change: `setOpaque(true)` in the `SpectrumOverlay` constructor. Nothing else changes.

Controlled, interleaved, three rounds, overlay open (median of five 2 s samples):

| Build | CPU |
|---|---|
| 1.6.16 before | 62.9 / 61.9 / 59.7% |
| 1.6.16 with setOpaque | 37.8 / 29.9 / 36.4% |

After the change the message thread was about 40% busy in one 5 s sample (was 52%).

Not yet checked: the window could not be brought onto a display for a screenshot, so the
rendering after closing the overlay needs a visual check on a native window.

Thread breakdown with the overlay open, from that sample: message thread ~52%, CoreGraphics
queue ~11%, audio I/O thread ~7%, everything else small.
