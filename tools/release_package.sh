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

echo "== Signing identities"
security find-identity -v -p codesigning | grep -E "Developer ID" || { echo "No Developer ID identities found"; exit 1; }

echo "== 1/5 Signing bundles (Developer ID, timestamp, hardened runtime)"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/VST3/HybridEQ.vst3"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/AU/HybridEQ.component"
codesign --force --deep --timestamp --options runtime --entitlements installer/standalone.entitlements --sign "$APP_ID" "$ART/Standalone/HybridEQ.app"
for b in "$ART/VST3/HybridEQ.vst3" "$ART/AU/HybridEQ.component" "$ART/Standalone/HybridEQ.app"; do
    codesign --verify --deep --strict "$b"
    codesign -dv "$b" 2>&1 | grep -E "Authority=Developer ID Application|Timestamp" | head -2
done

echo "== 2/5 Building and signing the installer pkg"
rm -rf "$OUT"; mkdir -p "$OUT"
INCLUDE_AAX=0 ./installer/build_installer.sh "$VERSION"
productsign --sign "$INST_ID" build3/HybridEQ-Installer.pkg "$PKG"
pkgutil --check-signature "$PKG" | head -4

echo "== 3/5 Notarizing the pkg"
xcrun notarytool submit "$PKG" --keychain-profile AC_NOTARY --wait
xcrun stapler staple "$PKG"

echo "== 4/5 Building the DMG"
cp BINARY_LICENSE.txt "$OUT/LICENSE.txt"; cat THIRD_PARTY_NOTICES.txt >> "$OUT/LICENSE.txt"
cat > "$OUT/README.txt" <<README
HybridEQ $VERSION - OpenGrid
Run HybridEQ-Installer.pkg. It installs the VST3, Audio Unit and standalone app (Apple silicon).
Restart your DAW and rescan plug-ins afterwards. License and trademark notices: LICENSE.txt.
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
