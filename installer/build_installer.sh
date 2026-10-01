#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ARTEFACTS="$PROJECT_DIR/build3/HybridEQ_artefacts/Release"
STAGING="$SCRIPT_DIR/staging"
PKG_ROOT="$STAGING/root"
SCRIPTS_DIR="$STAGING/scripts"
OUTPUT="$PROJECT_DIR/build3/HybridEQ-Installer.pkg"
IDENTIFIER="com.hybridaudio.hybrideq"

# ---- Version scheme ----
# One source of truth: $PROJECT_DIR/VERSION  (MAJOR.MINOR.PATCH, e.g. "1.2.0").
# No separate build numbers: the same value is stamped into every bundle
# (CFBundleShortVersionString + CFBundleVersion) and into the installer.
# When packaging you can pass the version as argv[1] or $VERSION; otherwise
# the script prompts, defaulting to whatever is in the VERSION file.
DEFAULT_VERSION="$(cat "$PROJECT_DIR/VERSION" 2>/dev/null || echo "1.0.0")"
if [ $# -ge 1 ]; then
    VERSION="$1"
elif [ -n "${VERSION:-}" ]; then
    :
else
    read -r -p "Version for this package (default: $DEFAULT_VERSION): " VERSION
    VERSION="${VERSION:-$DEFAULT_VERSION}"
fi
echo "Packaging HybridEQ version: $VERSION"

# AAX is excluded from public distribution: it requires Avid's proprietary
# AAX SDK and a PACE-code-signed Avid developer agreement to redistribute
# legally, neither of which this project holds. Set INCLUDE_AAX=1 to build
# a personal/local package that does include it.
INCLUDE_AAX="${INCLUDE_AAX:-0}"

rm -rf "$STAGING"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PKG_ROOT/Library/Audio/Plug-Ins/Components"
mkdir -p "$PKG_ROOT/Applications"
mkdir -p "$SCRIPTS_DIR"

echo "Copying VST3..."
cp -R "$ARTEFACTS/VST3/HybridEQ.vst3" "$PKG_ROOT/Library/Audio/Plug-Ins/VST3/"

echo "Copying AU..."
cp -R "$ARTEFACTS/AU/HybridEQ.component" "$PKG_ROOT/Library/Audio/Plug-Ins/Components/"

if [ "$INCLUDE_AAX" = "1" ]; then
    echo "Copying AAX..."
    mkdir -p "$PKG_ROOT/Library/Application Support/Avid/Audio/Plug-Ins"
    cp -R "$ARTEFACTS/AAX/HybridEQ.aaxplugin" "$PKG_ROOT/Library/Application Support/Avid/Audio/Plug-Ins/"
else
    echo "Skipping AAX (not redistributable without an Avid developer agreement; set INCLUDE_AAX=1 to include it)."
fi

echo "Copying Standalone..."
cp -R "$ARTEFACTS/Standalone/HybridEQ.app" "$PKG_ROOT/Applications/"

cat > "$SCRIPTS_DIR/postinstall" << 'POSTINSTALL'
#!/bin/bash
/usr/bin/killall -9 "AudioComponentRegistrar" 2>/dev/null || true
exit 0
POSTINSTALL
chmod +x "$SCRIPTS_DIR/postinstall"

AAX_LI=""
if [ "$INCLUDE_AAX" = "1" ]; then
    AAX_LI="<li><b>AAX</b> &rarr; /Library/Application Support/Avid/Audio/Plug-Ins/</li>"
fi

cat > "$STAGING/welcome.html" << HTML
<html><body style="font-family:-apple-system,Helvetica,Arial,sans-serif;padding:20px;">
<h1>HybridEQ Installer</h1>
<p>This installer will place the following on your system:</p>
<ul>
<li><b>VST3</b> &rarr; /Library/Audio/Plug-Ins/VST3/</li>
<li><b>Audio Unit</b> &rarr; /Library/Audio/Plug-Ins/Components/</li>
$AAX_LI
<li><b>Standalone App</b> &rarr; /Applications/</li>
</ul>
<p>Version $VERSION &mdash; macOS ARM (Apple Silicon)</p>
</body></html>
HTML

cp "$PROJECT_DIR/LICENSE" "$STAGING/license.txt"

cat > "$STAGING/conclusion.html" << 'HTML'
<html><body style="font-family:-apple-system,Helvetica,Arial,sans-serif;padding:20px;">
<h1>Installation Complete</h1>
<p>HybridEQ has been installed successfully.</p>
<p>Please restart your DAW to load the new plug-in.</p>
</body></html>
HTML

cat > "$STAGING/distribution.xml" << DIST
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<installer-gui-script minSpecVersion="2">
    <title>HybridEQ $VERSION</title>
    <welcome file="welcome.html" mime-type="text/html"/>
    <license file="license.txt" mime-type="text/plain"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <installation-check script="installCheck();"/>
    <script>
function installCheck() {
    if (!(system.compareVersions(system.version.ProductVersion, '10.13') &gt;= 0)) {
        my.result.title = 'Unsupported macOS';
        my.result.message = 'HybridEQ requires macOS 10.13 or later.';
        my.result.type = 'Fatal';
        return false;
    }
    return true;
}
    </script>
    <choices-outline>
        <line choice="default">
            <line choice="com.hybridaudio.hybrideq.pkg"/>
        </line>
    </choices-outline>
    <choice id="default"/>
    <choice id="com.hybridaudio.hybrideq.pkg" visible="false">
        <pkg-ref id="com.hybridaudio.hybrideq.pkg"/>
    </choice>
    <pkg-ref id="com.hybridaudio.hybrideq.pkg" version="$VERSION" auth="Root" onConclusion="none">HybridEQ.pkg</pkg-ref>
</installer-gui-script>
DIST

AAX_DICT=""
if [ "$INCLUDE_AAX" = "1" ]; then
AAX_DICT='    <dict>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Library/Application Support/Avid/Audio/Plug-Ins/HybridEQ.aaxplugin</string>
    </dict>'
fi

cat > "$STAGING/component.plist" << COMPONENTPLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <dict>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Library/Audio/Plug-Ins/VST3/HybridEQ.vst3</string>
    </dict>
    <dict>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Library/Audio/Plug-Ins/Components/HybridEQ.component</string>
    </dict>
$AAX_DICT
    <dict>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleOverwriteAction</key>
        <string>upgrade</string>
        <key>RootRelativeBundlePath</key>
        <string>Applications/HybridEQ.app</string>
    </dict>
</array>
</plist>
COMPONENTPLIST

echo "Building component package..."
pkgbuild \
    --root "$PKG_ROOT" \
    --component-plist "$STAGING/component.plist" \
    --scripts "$SCRIPTS_DIR" \
    --identifier "$IDENTIFIER" \
    --version "$VERSION" \
    --ownership recommended \
    --install-location "/" \
    "$STAGING/HybridEQ.pkg"

echo "Building product installer..."
productbuild \
    --distribution "$STAGING/distribution.xml" \
    --resources "$STAGING" \
    --package-path "$STAGING" \
    "$OUTPUT"

echo ""
echo "Installer created: $OUTPUT"
echo "Size: $(du -sh "$OUTPUT" | cut -f1)"

rm -rf "$STAGING"
