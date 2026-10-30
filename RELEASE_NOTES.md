# HybridEQ 1.6.16

This update returns the editor's drawing to the 1.6.13 path, which 1.6.14 changed, and
reduces CPU use while the full-window analyser is open. It keeps the existing sound,
controls, parameter IDs and saved settings.

- The cached artwork introduced in 1.6.14 is removed, because it made the controls look softer. Knobs use the 1.6.13 spring animation again.
- The full-window analyser declares itself opaque, so the controls beneath it are no longer redrawn on every frame. On the development Mac, CPU with the analyser open fell from about 61% to about 35%. CPU with it closed has not been measured against 1.6.13 for this build.
- The small analyser behind the EQ curve pauses while the full-window view covers it.
- Current user manual and plugin-manager bundle archives.

## Verification

The Release build passes and the local spectrum and processor tests pass. The
1.6.13 and 1.6.16 standalones were compared on the same Mac with interleaved runs
(see Docs/UI_PERFORMANCE.md). Local measurements are not whole-editor frame rates.
The original Windows 11 / Iris Xe report still needs confirmation on the affected hardware.

## Downloads and installation

macOS universal (11+): VST3, AU, PACE-signed AAX, CLAP and LV2.
Windows x64 and Linux x86_64: VST3, CLAP and LV2; AAX requires the separately licensed SDK and signing setup and is not supplied for those platforms.
Standalone is built and checked locally but is not shipped in these bundle archives.

Use OpenGrid Plugin Manager for macOS/Windows bundle ZIPs, or install the modules manually as described in the manual. Close the DAW before updating. The macOS bundle archive is Developer ID signed and notarized. SHA-256 checksum files accompany downloads.
