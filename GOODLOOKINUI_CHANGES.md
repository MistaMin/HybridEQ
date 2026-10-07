# GoodLookinUI integration: what changed in HybridEQ

**Status: working-tree changes for testing. Not committed on purpose.** Nothing here is a permanent
decision. This note is for the HybridEQ team so they can review, adopt or discard the changes.

The reusable UI code now lives in its own repository and is **not** part of HybridEQ:

- Toolkit: https://github.com/MistaMin/GoodLookinUI (released as **v0.2.1**)
- HybridEQ only consumes it (headers + the `goodlookinui_juce` CMake target).

---

## 1. How to pick up the toolkit (if the team adopts it)

`third_party/GoodLookinUI` in the working tree is currently a git checkout of the toolkit repo.
**Do not commit its contents into HybridEQ.** Instead register it as a submodule:

```sh
git rm -r --cached third_party/GoodLookinUI          # stop tracking the old copied snapshot
git submodule add https://github.com/MistaMin/GoodLookinUI.git third_party/GoodLookinUI
git -C third_party/GoodLookinUI checkout v0.2.1      # pin the released version
git add .gitmodules third_party/GoodLookinUI
```

A fresh clone then needs `git submodule update --init`. `CMakeLists.txt` already points
`GOODLOOKINUI_ROOT` at `third_party/GoodLookinUI`, so no CMake change is needed for this.

## 2. What changed in the plugin

### New files (plugin-specific, stay in HybridEQ)

| File | Purpose |
|---|---|
| `Source/Toolkit.h` | One place that includes the toolkit and gives its types short local names |
| `Source/UI/LookTriggers.h` | HybridEQ's built-in section looks: per parameter and choice, the knob style, colour and plate (neutral names only: N-Type, Brit, A-Type, FSF, Baxandall) |
| `Source/UI/MeterPanel.h` | LEVEL panel (VU / PPM / ladders / retro faces, 7-segment peak readout, latching CLIP lamp); reads the processor's level taps |
| `Source/UI/SpectrumOverlay.h` | Full-window spectrum / spectrogram, opened by the new SPECTRUM button |
| `Designs/LookTriggers.csv`, `Designs/EmbeddedLooks.h.in` | Baked section looks (CMake embeds them like `HybridEQ.csv`) |
| `tests/` | `look_triggers.cpp`, `look_baked.cpp`, `processor_stress.cpp`, `au_host_test.swift` (see section 5) |

### Modified files

| File | Change |
|---|---|
| `Source/PluginProcessor.h/.cpp` | Level taps (`getInputLevel/OutputLevel`) and raw sample rings for the analyser (`getInputRing/OutputRing`); latency fix (section 3) |
| `Source/PluginEditor.h/.cpp` | Section looks and colour trigger, KNOBS menu, LEVEL panel, spectrum analyser + full-window view, developer panels, per-knob names and tooltips |
| `Source/UI/EQDisplay.h` | Draws the spectrum analyser behind the EQ curve on the same log axis |
| `Source/UI/Theme.h` | Colours that were `const` are now mutable so a faceplate change can repaint everything |
| `Source/UI/SectionPanel.h`, `CycleButton.h`, `RotaryKnob.h`, `BypassButton.h`, `HarmonicsPanel.h` | Per-section plates and text colours, runtime re-tint, value range labels on every knob style, lit-key bypass buttons, slot faders; knobs pass their plate colour (`SectionPanel::getBackdrop`) to the toolkit so printed marks are dark on light plates and light on dark ones (toolkit 0.2.2) |
| `CMakeLists.txt` | Embeds `Designs/LookTriggers.csv`; adds the optional `HybridEQStress` test target (`HYBRIDEQ_BUILD_TESTS`, ON by default) |

### What users see

- 46 knob styles (switch all at once with the **KNOBS** button in GLOBAL; default keeps each knob's saved style).
- 40 plate looks; the colour trigger is **off by default** (see section 4).
- **LEVEL** panel to the right of the response display; band bypass buttons are lit keys.
- Spectrum analyser behind the EQ curve (on by default, Standard look) and a **SPECTRUM** button in the header for a full-window spectrum / spectrogram.
- Developer-only (Debug builds, compiled out of Release): **Design**, **KNOBS** menu extras and **LOOK**, with three panels (knob inspector with colour wheel, section looks, spectrum settings). All apply immediately.

## 3. Bug fixes found while testing (worth keeping even if the UI work is dropped)

1. **Release build did not compile.** `PluginEditor.cpp` used developer-only members (`highlightKnob`, `ring`) outside the `GOODLOOKINUI_ENABLE_EDITOR` guard. Now guarded.
2. **Wrong latency until the first audio block.** `prepareToPlay` never applied the oversampling factor, so the plugin reported the wrong latency to a host that reads it once after preparing. Fix: `updateParameters(false)` at the end of `prepareToPlay`.
3. **No host-visible parameter writes during initialisation.** `updateParameters` gained a `writeParameters` argument. When false it computes the same effective oversampling floor (Circuit forces at least 4x) without writing the Oversample parameter. This is what makes Apple's `auval` pass; writing the parameter during init made it fail.

## 4. Saved-state keys and compatibility

Everything new is stored as properties on `apvts.state`. Old sessions and presets load unchanged (missing keys fall back to defaults; spectrum settings saved by an earlier build still load).

| Key | Meaning |
|---|---|
| `lookTrigger` | `off` (default) / `control` (per section) / `faceplate` (whole UI, follows Preamp type) |
| `lookFaceplate`, `lookPlate` | Chosen whole-UI faceplate / plate for all sections |
| `lookTable` | Edited section looks (CSV text), per parameter and choice |
| `knobStyle` | Global knob override; empty = each knob's saved style |
| `meterFace`, `faderStyle`, `glow` | LEVEL face, harmonics slider style, neon vs flat drawing |
| `spectrum` | Analyser settings (size, look, smoothing, colours per range...) |

No audio parameter IDs were added, removed or renamed.

## 5. Tests and how to run them

```sh
# toolkit (no plugin needed, no JUCE needed)
cmake -S third_party/GoodLookinUI -B /tmp/gl -DGOODLOOKINUI_BUILD_TESTS=ON && cmake --build /tmp/gl && ctest --test-dir /tmp/gl

# plugin-side checks
clang++ -std=c++20 -I third_party/GoodLookinUI/include tests/look_triggers.cpp -o /tmp/t && /tmp/t
clang++ -std=c++20 -I third_party/GoodLookinUI/include tests/look_baked.cpp   -o /tmp/t && /tmp/t   # run from the repo root

# real processor stress test (Release recommended); --quick for sanitizer builds
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release && cmake --build build-rel --target HybridEQStress
build-rel/HybridEQStress_artefacts/Release/HybridEQStress

# AU host test (needs the AU installed) and Apple's validator
swiftc -O tests/au_host_test.swift -o /tmp/au_host_test && /tmp/au_host_test
auval -v aufx HyEq Hybr
```

Results at the time of writing: stress test 2059 checks / 0 failed (Release) and 619 / 0 under AddressSanitizer +
UBSan (`--quick`); `auval` succeeds; the AU host test passes 22 checks at 44.1 / 48 / 96 kHz with automation.

## 6. Open points for the team

- **Latency changes mid-stream (existing design, not changed):** with Oversample, High Cut, the High shelf or Circuit
  automated, the plugin changes its own oversampling factor and calls `setLatencySamples` from the audio thread
  (about 22 changes per 400 blocks in the stress test, range 0 to 64 samples). Some hosts handle that badly.
- `auval` warning: the High Freq default (8000 Hz) does not round-trip exactly through float normalisation.
- **Not tested:** REAPER or any other DAW (only Apple's audio engine host and `auval`), the VST3 beyond building it and
  checking the bundle, ThreadSanitizer, Windows and Linux.
- CPU: one instance at 48 kHz, 512-sample blocks, 8x oversampling used about 24% of one core.
- The `Designs/LookTriggers.csv` and `Designs/HybridEQ.csv` files are meant to be regenerated from developer mode
  (Save CSV buttons) and rebuilt; the knob shapes in the toolkit are approximations drawn in code.

## 7. Discarding the changes

These are uncommitted, so the plugin can be returned to its last commit with `git stash -u` (keeps them) or by
restoring the tracked files and deleting the new ones listed in section 2. Take care not to delete the
`third_party/GoodLookinUI` checkout if you still want the toolkit.

## 8. Developer mode autosave (toolkit 0.2.3)

In Debug builds every change made in the Design, Section look or Spectrum panels (and the LOOK / meter / fader / glow
menus) is written automatically into `Designs/`. There is no Save button; commit the files with git.

| Change | File |
|---|---|
| Knob style, colour, label, font, position, size, marks | `Designs/HybridEQ.csv` |
| Section looks (per parameter and choice) | `Designs/LookTriggers.csv` |
| Trigger mode, plate / faceplate, KNOBS override, meter face, fader style, glow, spectrum settings | `Designs/Settings.csv` (new; baked into the build like the other two) |

- Writes are debounced (about 0.35 s after the last edit), atomic, skipped when nothing changed, and keep the file's
  existing line endings (the committed `LookTriggers.csv` uses CRLF), so diffs show only real edits.
- A developer build starts from these files rather than the baked copy, so a relaunch shows the saved design with no
  rebuild. Release builds always use the baked text (the project's saved state still wins over `Settings.csv`).
- The inspector's status line shows `Saved <file> -> <path> (time)` after each write.
- Knobs are saved with their base style and colour. With "Per section" on, style and colour edits go to
  `LookTriggers.csv` (for the section's current choice) instead of `HybridEQ.csv`.
- The `marks` column is only added to `HybridEQ.csv` once a knob uses it, so unrelated files stay byte-identical.
- Needs `HYBRIDEQ_DESIGNS_DIR` (set by CMake for Debug builds with the editor on).
