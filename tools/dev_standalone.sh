#!/bin/bash
# Developer standalone: builds HybridEQ as a Debug standalone app with the
# GoodLookinUI Design editor compiled in (knob inspector, section looks,
# spectrum settings), then launches it. Release builds never contain the editor.
#
# The build is Debug-flagged but optimised (-O2): the circuit simulations are
# far too slow unoptimised to run in real time.
#
# Usage:  tools/dev_standalone.sh            build (incremental) and launch
#         tools/dev_standalone.sh --no-run   build only
#         tools/dev_standalone.sh --vst3     build the developer VST3 and install it into
#                                            ~/Library/Audio/Plug-Ins/VST3 (for the DAW)
#         tools/dev_standalone.sh --uninstall  remove the developer VST3 from there again
#         tools/dev_standalone.sh --clean    wipe build-dev first
#
# After editing in the app, use Save CSV (Designs/HybridEQ.csv, LookTriggers.csv)
# and re-run this script: the saved designs are embedded at configure time.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-dev"
APP="$BUILD_DIR/HybridEQ_artefacts/Debug/Standalone/HybridEQ.app"

DEST="$HOME/Library/Audio/Plug-Ins/VST3/HybridEQ.vst3"
if [ "${1:-}" = "--uninstall" ]; then
    rm -rf "$DEST" && echo "Removed $DEST"
    exit 0
fi

[ "${1:-}" = "--clean" ] && rm -rf "$BUILD_DIR"

JUCE_ARGS=()
if [ -d "$PROJECT_DIR/build3/_deps/juce-src" ]; then
    # reuse the JUCE already fetched by the main build instead of downloading it again
    JUCE_ARGS=(-DFETCHCONTENT_SOURCE_DIR_JUCE="$PROJECT_DIR/build3/_deps/juce-src")
fi

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS_DEBUG="-O2 -g" -DCMAKE_C_FLAGS_DEBUG="-O2 -g" \
    -DHYBRIDEQ_DESIGN_EDITOR=ON -DHYBRIDEQ_BUILD_TESTS=OFF \
    "${JUCE_ARGS[@]}"
if [ "${1:-}" = "--vst3" ]; then
    VST3="$BUILD_DIR/HybridEQ_artefacts/Debug/VST3/HybridEQ.vst3"
    cmake --build "$BUILD_DIR" --target HybridEQ_VST3
    codesign --force --deep --sign - "$VST3"
    mkdir -p "$(dirname "$DEST")"
    rm -rf "$DEST"
    cp -R "$VST3" "$DEST"
    echo "Developer VST3 installed: $DEST   (remove with: tools/dev_standalone.sh --uninstall)"
    exit 0
fi

cmake --build "$BUILD_DIR" --target HybridEQ_Standalone

echo "Developer standalone: $APP"
[ "${1:-}" = "--no-run" ] || open "$APP"
