#!/bin/bash
# Signs the Release bundles, builds the macOS bundle archive for the OpenGrid Plugin Manager,
# and notarises it. No DMG or PKG is made any more. The Standalone app is not shipped.
# Run it in your own Terminal (codesign / notarytool need your interactive keychain access).
# Prerequisite: a Release build in build3/ (cmake --build build3 --config Release for VST3, AU, CLAP, LV2).
# Usage: tools/release_package.sh [VERSION]      (default: the VERSION file)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:-$(cat VERSION)}"
APP_ID="Developer ID Application: Marcos Deida (ZQYU67JUBH)"
ART="build3/HybridEQ_artefacts/Release"
OUT="dist/HybridEQ-$VERSION"
ZIP="$OUT/HybridEQ-$VERSION-macos-bundles.zip"
AAX_WCGUID="${AAX_WCGUID:-AF15E1B0-CF31-11F1-89F3-005056920FF7}"
PACE_ACCOUNT="${PACE_ACCOUNT:-mistamin}"
SKILL_SIGN_AAX="${SKILL_SIGN_AAX:-/Users/marcosdeida/.gemini/config/skills/aax-signing-release/scripts/sign_aax.sh}"

echo "== Signing identities"
security find-identity -v -p codesigning | grep -E "Developer ID" || { echo "No Developer ID identities found"; exit 1; }

echo "== 1/4 Signing bundles (Developer ID, timestamp, hardened runtime)"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/VST3/HybridEQ.vst3"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/AU/HybridEQ.component"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/CLAP/HybridEQ.clap"
# LV2 is a plain folder (not a bundle): sign the binary inside it
codesign --force --timestamp --options runtime --sign "$APP_ID" "$ART/LV2/HybridEQ.lv2/libHybridEQ.so"

if [ -d "$ART/AAX/HybridEQ.aaxplugin" ]; then
    echo "== Signing AAX bundle with PACE Eden & Developer ID"
    if [ -z "$AAX_WCGUID" ] || [ -z "$PACE_ACCOUNT" ]; then
        echo "Error: AAX_WCGUID and PACE_ACCOUNT must be set in the environment to sign AAX." >&2
        exit 1
    fi
    if [ -x "$SKILL_SIGN_AAX" ]; then
        "$SKILL_SIGN_AAX" "$ART/AAX/HybridEQ.aaxplugin" "$AAX_WCGUID" "$PACE_ACCOUNT" "$APP_ID"
    else
        codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/AAX/HybridEQ.aaxplugin"
        /Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool sign --verbose \
            --account "$PACE_ACCOUNT" \
            --wcguid "$AAX_WCGUID" \
            --signid "$APP_ID" \
            --in "$ART/AAX/HybridEQ.aaxplugin" \
            --out "$ART/AAX/HybridEQ.aaxplugin" \
            --autoinstall on
        /Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool verify --verbose --in "$ART/AAX/HybridEQ.aaxplugin"
    fi
fi

for b in "$ART/VST3/HybridEQ.vst3" "$ART/AU/HybridEQ.component" "$ART/CLAP/HybridEQ.clap" "$ART/LV2/HybridEQ.lv2/libHybridEQ.so"; do
    codesign --verify --deep --strict "$b"
    codesign -dv "$b" 2>&1 | grep -E "Authority=Developer ID Application|Timestamp" | head -2
done
if [ -d "$ART/AAX/HybridEQ.aaxplugin" ]; then
    codesign --verify --deep --strict "$ART/AAX/HybridEQ.aaxplugin"
    codesign -dv "$ART/AAX/HybridEQ.aaxplugin" 2>&1 | grep -E "Authority=Developer ID Application|Timestamp" | head -2
fi

echo "== 2/4 Building the bundle archive (install-tree layout)"
rm -rf "$OUT"; mkdir -p "$OUT/resources/Licenses"
{ cat BINARY_LICENSE.txt; echo; echo; cat THIRD_PARTY_NOTICES.txt; echo; echo; cat LICENSE; echo; echo; cat third_party/GoodLookinUI/LICENSE; } > "$OUT/resources/Licenses/LICENSE.txt"
cp -R Licenses/third-party "$OUT/resources/Licenses/third-party"
MIN_OS=11.0 RESOURCES_DIR="$OUT/resources" tools/make_bundle_zip.sh "HybridEQ" "$VERSION" "$ART" "$OUT"

echo "== 3/4 Notarizing the archive"
xcrun notarytool submit "$ZIP" --keychain-profile AC_NOTARY --wait

echo "== 4/4 Checksum"
(cd "$OUT" && shasum -a 256 "$(basename "$ZIP")" > "HybridEQ-$VERSION-macos.sha256")
cat "$OUT/HybridEQ-$VERSION-macos.sha256"
echo "DONE: $ZIP"
echo "Next: write opengrid-manifest.json with tools/write_opengrid_manifest.py once the Windows archive exists."
