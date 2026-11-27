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

PENDING: filled in from the release build and tests.

## Known limits

- The Windows and Linux builds pass the validators but have not been tried in a DAW by hand. The ARM
  builds are validated without pluginval, which ships for x64 only.
- AAX is macOS only. There is no standalone application.
