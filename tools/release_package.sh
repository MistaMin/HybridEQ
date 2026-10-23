#!/bin/bash
# Signs the Release bundles, builds and signs the installer pkg, builds the DMG, and notarizes + staples both.
# Run it in your own Terminal (codesign / notarytool need your interactive keychain access).
# Prerequisite: a Release build in build3/ (cmake --build build3 --config Release for VST3, AU, Standalone).
# Usage: tools/release_package.sh [VERSION]      (default: the VERSION file)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:-$(cat VERSION)}"
APP_ID="Developer ID Application: Marcos Deida (ZQYU67JUBH)"
INST_ID="Developer ID Installer: Marcos Deida (ZQYU67JUBH)"
ART="build3/HybridEQ_artefacts/Release"
OUT="dist/HybridEQ-$VERSION"
PKG="$OUT/HybridEQ-Installer.pkg"
DMG="dist/HybridEQ-$VERSION.dmg"
AAX_WCGUID="${AAX_WCGUID:-}"
PACE_ACCOUNT="${PACE_ACCOUNT:-}"
SKILL_SIGN_AAX="${SKILL_SIGN_AAX:-/Users/marcosdeida/.gemini/config/skills/aax-signing-release/scripts/sign_aax.sh}"

echo "== Signing identities"
security find-identity -v -p codesigning | grep -E "Developer ID" || { echo "No Developer ID identities found"; exit 1; }

echo "== 1/5 Signing bundles (Developer ID, timestamp, hardened runtime)"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/VST3/HybridEQ.vst3"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/AU/HybridEQ.component"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/CLAP/HybridEQ.clap"
# LV2 is a plain folder (not a bundle): sign the binary inside it
codesign --force --timestamp --options runtime --sign "$APP_ID" "$ART/LV2/HybridEQ.lv2/libHybridEQ.so"
codesign --force --deep --timestamp --options runtime --entitlements installer/standalone.entitlements --sign "$APP_ID" "$ART/Standalone/HybridEQ.app"

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

for b in "$ART/VST3/HybridEQ.vst3" "$ART/AU/HybridEQ.component" "$ART/CLAP/HybridEQ.clap" "$ART/LV2/HybridEQ.lv2/libHybridEQ.so" "$ART/Standalone/HybridEQ.app"; do
    codesign --verify --deep --strict "$b"
    codesign -dv "$b" 2>&1 | grep -E "Authority=Developer ID Application|Timestamp" | head -2
done

if [ -d "$ART/AAX/HybridEQ.aaxplugin" ]; then
    codesign --verify --deep --strict "$ART/AAX/HybridEQ.aaxplugin"
    codesign -dv "$ART/AAX/HybridEQ.aaxplugin" 2>&1 | grep -E "Authority=Developer ID Application|Timestamp" | head -2
fi

echo "== 2/5 Building and signing the installer pkg"
rm -rf "$OUT"; mkdir -p "$OUT"
INCLUDE_AAX=1 ./installer/build_installer.sh "$VERSION"
productsign --sign "$INST_ID" build3/HybridEQ-Installer.pkg "$PKG"
pkgutil --check-signature "$PKG" | head -4

echo "== 3/5 Notarizing the pkg"
xcrun notarytool submit "$PKG" --keychain-profile AC_NOTARY --wait
xcrun stapler staple "$PKG"

echo "== 4/5 Building the DMG"
{ cat BINARY_LICENSE.txt; echo; echo; cat THIRD_PARTY_NOTICES.txt; echo; echo; cat LICENSE; echo; echo; cat third_party/GoodLookinUI/LICENSE; } > "$OUT/LICENSE.txt"
cp -R Licenses/third-party "$OUT/Licenses"
cat > "$OUT/README.txt" <<README
HybridEQ $VERSION - OpenGrid
Run HybridEQ-Installer.pkg. It installs the VST3, Audio Unit, AAX, CLAP, LV2 and standalone app (universal: Apple silicon and Intel, macOS 11 or later).
Restart your DAW and rescan plug-ins afterwards. License and trademark notices: LICENSE.txt, and the Licenses folder for third-party components.
README
rm -f "$DMG"
hdiutil create -volname "HybridEQ $VERSION" -srcfolder "$OUT" -ov -format UDZO "$DMG"
codesign --force --timestamp --sign "$APP_ID" "$DMG"

echo "== 5/5 Notarizing the DMG"
xcrun notarytool submit "$DMG" --keychain-profile AC_NOTARY --wait
xcrun stapler staple "$DMG"

echo "== Verification"
spctl -a -vv -t install "$PKG" || true
spctl -a -vv -t install "$DMG" || true
echo "DONE: $PKG"
echo "DONE: $DMG"
