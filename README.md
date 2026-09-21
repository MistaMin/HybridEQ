# HybridEQ

A preamp + 6-band equalizer audio plug-in (VST3 / AU / AAX / Standalone) built
with [JUCE](https://juce.com), modeled on the character of several classic
analog consoles (Neve, SSL, API, Baxandall, Focusrite).

Current version: see [`VERSION`](VERSION).

## Building

Requires CMake 3.22+, a C++20 compiler, and (for the AAX target only) Avid's
AAX SDK checked out as a sibling directory named `../aax-sdk-2-8-1` relative
to this repo. JUCE itself is fetched automatically via CMake's
`FetchContent` — no manual setup needed for VST3/AU/Standalone.

```bash
cmake -B build -G Xcode
cmake --build build --config Release --target HybridEQ_VST3
```

Swap `HybridEQ_VST3` for `HybridEQ_AU`, `HybridEQ_AAX`, or
`HybridEQ_Standalone` for the other formats.

## License

HybridEQ's own source code (everything in `Source/`, `CMakeLists.txt`,
`installer/`) is released under the [MIT License](LICENSE).

This project depends on JUCE, which is dual-licensed (AGPLv3 for open-source
use, or a paid commercial license for closed/proprietary distribution) —
see the note at the bottom of [`LICENSE`](LICENSE) and JUCE's own terms at
https://juce.com/juce-8-licence. The AAX build target additionally requires
Avid's proprietary AAX SDK, which is not included in this repository.
