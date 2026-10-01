# HybridEQ

HybridEQ is a channel-strip plug-in combining a multi-flavor analog preamp
emulation with a 6-band EQ, harmonic saturation, and mid/side processing.
Each stage can be switched between circuit models inspired by Neve, SSL,
API, Baxandall, and Focusrite consoles. Includes adaptive oversampling
(1x/4x/8x) that automatically raises the internal processing rate for
headroom near Nyquist or when the Circuit model is engaged. Built with
[JUCE](https://juce.com); ships as VST3, AU, AAX, and Standalone for macOS.

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

The compiled HybridEQ binaries distributed via GitHub Releases (the
installer, the DMG, and the plug-ins/standalone app inside it) are
**freeware** — free to use, for any purpose, at no cost.

Separately, HybridEQ's own source code in this repository is released under
the [MIT License](LICENSE).

This project is built on [JUCE](https://juce.com), which is dual-licensed and
not covered by HybridEQ's MIT license. Whoever compiles this code into a
binary is bound by their own JUCE license: a paid commercial JUCE license
(Starter, Indie, Pro, or Educational) permits closed-source distribution
under that tier's terms, while without one, JUCE's free tier requires the
resulting binary to be licensed under AGPLv3 instead. See the notes at the
bottom of [`LICENSE`](LICENSE) for details, and JUCE's own terms at
https://juce.com/juce-8-licence.

**AAX (Pro Tools) target:** building AAX additionally requires Avid's
proprietary AAX SDK (not included in this repo, and not redistributable).
Avid's SDK/developer terms are incompatible with AGPL-style source
disclosure, and legally distributing a working (non-developer-build) AAX
plugin requires being an approved, PACE-code-signed Avid developer. See the
AAX note in [`LICENSE`](LICENSE) — this repository's source can still be
used to build AAX locally for personal use if you obtain the SDK yourself,
but pre-built AAX binaries should not be redistributed from here without
your own valid Avid developer agreement.
