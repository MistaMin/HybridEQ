# HybridEQ 1.6.14

This update improves editor responsiveness while retaining the existing sound,
controls, parameter IDs and saved settings.

- Knob indicators now follow their values immediately, removing the spring animation that became sluggish when the UI fell behind.
- Cached knob, panel and chassis artwork reduces repeated drawing; caches follow editor size, physical display scale, colour and style changes.
- EQ parameter changes are coalesced, reducing UI message traffic. Idle and hidden display work is reduced, and the small analyser pauses behind the full-window view.
- Spectrograms colour only newly arrived history rows. FFT sizes, smoothing, peak hold, palettes and display calibration are retained.
- Current user manual, screenshots, component notices and plugin-manager bundle archives.

## Verification

The optimisation baseline passed 812 processor checks. Exact spectrum pixel,
FFT/peak-hold, immediate knob response, 1x/2x artwork and cache invalidation checks
also passed. Release-version checks are recorded in the release verification report.
Local software-renderer microbenchmarks are not whole-editor FPS measurements.
The original Windows 11 / Iris Xe report still needs confirmation on the affected hardware.

## Downloads and installation

macOS universal (11+): VST3, AU, PACE-signed AAX, CLAP and LV2.
Windows x64 and Linux x86_64: VST3, CLAP and LV2; AAX requires the separately licensed SDK and signing setup and is not supplied for those platforms.
Standalone is built and checked locally but is not shipped in these bundle archives.

Use OpenGrid Plugin Manager for macOS/Windows bundle ZIPs, or install the modules manually as described in the manual. Close the DAW before updating. The macOS bundle archive is Developer ID signed and notarized. SHA-256 checksum files accompany downloads.
