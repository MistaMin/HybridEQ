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
