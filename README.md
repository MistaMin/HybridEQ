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

Requires CMake 3.22+ and a C++20 compiler. A GoodLookinUI source snapshot is
included in `third_party/GoodLookinUI`; set `GOODLOOKINUI_ROOT` to use a separate
checkout instead. AAX is added only when Avid's SDK exists at
`../aax-sdk-2-8-1`, or at the path specified by `HYBRIDEQ_AAX_SDK`. JUCE itself is fetched automatically via CMake's
`FetchContent` — no manual setup needed for VST3/AU/Standalone.

```bash
cmake -B build -G Xcode
cmake --build build --config Release --target HybridEQ_VST3
```

Swap `HybridEQ_VST3` for `HybridEQ_AU`, `HybridEQ_AAX`, or
`HybridEQ_Standalone` for the other formats.

## License

Official compiled HybridEQ plug-ins, standalone applications, and installers
are **proprietary freeware**, free to use for personal and commercial audio
production. They are distributed under the [binary freeware license](BINARY_LICENSE.txt),
not an open-source license. The publisher builds them under a JUCE commercial
license; end users do not need a JUCE developer license to use these downloads.

HybridEQ's own source code is separately [MIT licensed](LICENSE). The bundled
GoodLookinUI toolkit, including its core and JUCE adapter, has its own
[MIT license](third_party/GoodLookinUI/LICENSE). Those component permissions
remain intact when included in the freeware binaries, and their copyright and
permission notices accompany binary distributions.

JUCE and optional Avid SDK code retain their own licenses. Anyone building or
distributing their own version must comply with the applicable dependency
licenses; the publisher's JUCE commercial license is not transferred with the
source. See [component notices](THIRD_PARTY_NOTICES.txt) and the
[JUCE 8 terms](https://juce.com/legal/juce-8-licence/). Choosing JUCE's
open-source licensing route requires satisfying its applicable terms.
AAX additionally requires Avid's SDK/developer permissions and applicable
signing requirements; it is excluded from public packaging by default.

Binary bundles include license notices. The macOS installer also displays the
binary freeware terms and installs the notices in
`/Library/Application Support/HybridAudio/HybridEQ/Licenses`.

## GoodLookinUI development editor

Debug builds include a **Design** inspector for generated knobs. Edit style,
label, hex colour, font size and panel-relative geometry; Apply, Undo, Save CSV
and Load CSV are available. Save the accepted design to `Designs/HybridEQ.csv`
and rebuild to embed it. Release builds exclude the editor entirely. The
finished panel scales proportionally with the window.

## Console appearance

The console edition uses GoodLookinUI's generated fluted black grips and coloured
knob caps, fixed calibration marks, shaded mechanical keys, recessed numeric
readouts and a charcoal rack faceplate. Choice keys open dropdown menus using
the existing DSP choice values. Six strips share fixed control slots and switch
positions. Resizing preserves the panel proportions. The existing DSP and
parameter IDs are retained.

## Version 1.6.0

Introduces the console interface: generated hardware knobs, aligned channel
strips, mechanical keys with dropdown choices, proportional resizing, and a
development-only visual inspector. Accepted designs are embedded from CSV.
The UI toolkit is included as source so fresh checkouts are self-contained.

The bundled [GoodLookinUI toolkit](third_party/GoodLookinUI) is independently [MIT licensed](third_party/GoodLookinUI/LICENSE). Its license file travels with the toolkit when reused in other projects.
