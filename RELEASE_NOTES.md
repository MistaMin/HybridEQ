# HybridEQ 2.0.1

![HybridEQ 2.0.1](https://raw.githubusercontent.com/MistaMin/HybridEQ/v2.0.1/Docs/HybridEQ-2.0.1.png)

A maintenance release built on the current OpenPluginSDK. The sound, the plug-in IDs, the parameter
IDs and saved projects are unchanged from 2.0.0: renders of the two versions are bit-identical.

## What is fixed

- **VST3:** when a host starts processing from its main thread while audio is already running
  (REAPER does), HybridEQ no longer lets one block through unprocessed and resets in the middle of
  the stream. That could be heard now and then as a short knock that faded over about 40 ms.
- **Linux VST3:** in a host that embeds the editor in its own area (Ardour), the editor takes that
  area's width and keeps its proportions, and it follows a host window that is replaced while open.
- **Audio Unit:** the component version is 2.0.1, so hosts take the build as an update.

## Downloads

| File | System | Formats |
|---|---|---|
| HybridEQ-2.0.1-macos.pkg | macOS 11+, Apple silicon and Intel | VST3, AU, AAX (PACE-signed), CLAP, LV2 |
| HybridEQ-2.0.1-windows-x64-setup.exe | Windows 10+, x64 | VST3, CLAP, LV2 |
| HybridEQ-2.0.1-windows-arm64-setup.exe | Windows 10+, ARM64 | VST3, CLAP, LV2 |
| HybridEQ-2.0.1-linux-x86_64.tar.gz | Linux x86_64, glibc 2.35+ | VST3, CLAP, LV2 |
| HybridEQ-2.0.1-linux-aarch64.tar.gz | Linux aarch64, glibc 2.35+ | VST3, CLAP, LV2 |
| HybridEQ-2.0.1-User-Manual.pdf | | the user manual |
| opengrid-manifest.json | | sizes and SHA-256 of the installers, for the Plugin Manager |

Every download has a `.sha256` file.

**Install.** macOS: run the installer (Developer ID signed and notarized); it replaces 2.0.0 in place.
Windows: run the installer for your processor and choose the formats; it is not code-signed, so
SmartScreen may ask first ("More info", "Run anyway"). Linux: copy `usr/lib/vst3`, `usr/lib/clap` and
`usr/lib/lv2` from the archive to `~/.vst3`, `~/.clap` and `~/.lv2`. Close the DAW first, remove
older copies, rescan.

## Checks

All five downloads were built from HybridEQ `02dddcd` with OpenPluginSDK `19fac89`: macOS on a Mac,
Windows and Linux each on its own GitHub runner. Renders of 2.0.1 and 2.0.0 are bit-identical in 17
settings that cover every preamp circuit, the HQ and reduced models, 2x to 8x oversampling with both
filters, mid/side, harmonics, AUTO GAIN, DELTA and mono.

| Check | macOS | Windows x64 | Windows ARM64 | Linux x86_64 | Linux aarch64 |
|---|---|---|---|---|---|
| Engine and plug-in tests (including the null tests against 1.6.16 renders) | pass | pass | pass | pass | pass |
| Conformance soak, 200 seeds | pass | | | | |
| VST3 SDK validator | 47 / 47 | 47 / 47 | 47 / 47 | 47 / 47 | 47 / 47 |
| clap-validator 0.4.1 | 34 pass, 0 fail | 33 pass, 0 fail | 33 pass, 0 fail | 34 pass, 0 fail | 34 pass, 0 fail |
| pluginval, strictness 10 (VST3) | pass (no editor tests) | pass, with the editor | not available | pass, with the editor | not available |
| auval | pass (2.0.1) | | | | |
| AAX Validator (signed AAX) | every test that loads the plug-in passes | | | | |
| REAPER: VST3, AU and CLAP | pass | | | | |
| Signing | Developer ID, notarized, PACE | none | none | | |

In REAPER, each format loads, renders the same audio twice, has every parameter act on the sound and
appear in the editor, keeps its settings through saving and reopening the project, and reopens its
editor five times. Three AAX Validator tests (product IDs, general support, data model) stop inside the
validator before the plug-in loads, on this Mac and for 2.0.0 alike.

## Known limits

- The Windows and Linux builds pass the validators but have not been tried in a DAW by hand. The ARM
  builds are validated without pluginval, which ships for x64 only.
- AAX is macOS only. There is no standalone application.
- In one of several REAPER test runs, on a heavily loaded machine, two VST3 renders of the same input
  differed for about 0.4 s. It did not happen again in more than 40 later renders. The likely cause is the host
  restarting processing between two blocks of a render; 2.0.0 behaves the same there.
