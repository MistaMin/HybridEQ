# GoodLookinUI

An independent C++20 toolkit for generated analogue hardware interfaces, starting with HybridEQ. This is an initial implementation, not a finished framework.

## Implemented

- JUCE-free design model, strict versioned CSV reader/writer and critically damped display motion.
- JUCE adapter that generates metal, bakelite, ivory and console knobs. The console style has fluted black grips, coloured moulded caps, recessed pointers, contact shadows and fixed calibration marks. Shared drawing helpers generate mechanical keys, panel finishes and screws.
- Development-only inspector: style, label, hex colour, font size, position, dimensions, save/load and undo.
- HybridEQ integration retaining existing parameter IDs and attachments, with proportional scaling and a compiled design file.

The design inspector currently supports the 16 existing HybridEQ knobs. Positions are relative to their containing panel. It does not yet add controls, edit menus, drag controls or move them between panels.

## Build with JUCE

Supply JUCE targets before adding this directory with `add_subdirectory`. Link `goodlookinui_juce` for the adapter or `goodlookinui` for the dependency-free core. Include `GoodLookinUI.h` or `goodlookinui/Design.h` respectively. JUCE is not vendored or forked.

HybridEQ includes this toolkit in `third_party/GoodLookinUI`. Set `GOODLOOKINUI_ROOT` to use an independent toolkit checkout instead.

```sh
cmake -S ../HybridEQ -B ../HybridEQ/build -DCMAKE_BUILD_TYPE=Debug
cmake --build ../HybridEQ/build --target HybridEQ_Standalone
```

HybridEQ includes AU on macOS and adds AAX only when the optional SDK is present. Windows/Linux and LV2 validation are later milestones. In a Debug build, click **Design** to open the inspector. Select a knob, edit its properties, and click **Apply**. Save the design over `HybridEQ/Designs/HybridEQ.csv` and rebuild to embed it in the release. Release builds compile out the inspector regardless of the editor option; Debug builds can disable it with `HYBRIDEQ_DESIGN_EDITOR=OFF`.

## Verify the core without JUCE or CMake

```sh
clang++ -std=c++20 -Wall -Wextra -pedantic -I include tests/core.cpp -o /tmp/goodlookinui-tests
/tmp/goodlookinui-tests
```

Core checks cover CSV roundtrips with commas and quotes, rejected malformed geometry, duplicate IDs and stable motion. The integration has passed Clang syntax checks with JUCE 8.0.4 in both editor modes. Debug and Release standalone, VST3 and AU builds pass on Apple Silicon/macOS. The standalone applications have been launched; generated controls and the development inspector have been visually inspected. Live label/colour edits, undo and CSV save/load were exercised through the native UI. The Release application has no Design button. DAW audio and automation testing remain outstanding.

## Next milestones

See [the implementation plan](docs/IMPLEMENTATION.md). The library name is provisional.

## HybridEQ console edition

The first complete appearance built with this toolkit is an SSL-inspired
console panel for HybridEQ: brown/blue/green/red EQ caps, cream filter knobs,
aligned strips, physical choice keys with dropdowns, generated fader caps and
an inset response display. GoodLookinUI remains independent of the DSP.

Existing CSV designs with the original HardwareUI header can still be loaded. Newly saved designs use the GoodLookinUI header.
