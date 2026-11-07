#!/bin/bash
# Builds one installer for everything, then one zip containing the installer plus
# a manual-install folder:
#   VST3 -> /Library/Audio/Plug-Ins/VST3, AU -> /Library/Audio/Plug-Ins/Components
#   Max for Live devices -> your Ableton User Library (postinstall)
#   Max externals -> ~/Documents/Max 8|9/Packages/kalabash/externals (postinstall)
set -euo pipefail
VERSION="${1:-dev}"; VERSION="${VERSION#v}"
[ "$VERSION" = "main" ] && VERSION="1.1.0"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$ROOT/stage"; WORK="$ROOT/pkgwork"; DIST="$ROOT/dist"
rm -rf "$WORK" "$DIST"; mkdir -p "$WORK"/{vst3,au,m4l,scripts} "$DIST"

cp -R "$STAGE/vst3/"* "$WORK/vst3/"
cp -R "$STAGE/au/"* "$WORK/au/"
mkdir -p "$WORK/m4l/kalabash"
cp -R "$STAGE/m4l/"* "$WORK/m4l/kalabash/"

cat > "$WORK/scripts/postinstall" <<'POST'
#!/bin/bash
SRC="/Users/Shared/kalabash"
USER_NAME=$(stat -f%Su /dev/console)
USER_HOME=$(dscl . -read /Users/"$USER_NAME" NFSHomeDirectory | awk '{print $2}')
LIB="$USER_HOME/Music/Ableton/User Library/Presets"
install_dev() {  # name, category folder
  local dest="$LIB/$2/kalabash"
  mkdir -p "$dest"; rm -rf "$dest/$1"; cp -R "$SRC/$1" "$dest/"
}
install_dev DICEBAG "MIDI Effects/Max MIDI Effect"
install_dev KNOTWORK "Instruments/Max Instrument"
install_dev ALLOY "Instruments/Max Instrument"
install_dev FOGBANK "Audio Effects/Max Audio Effect"
install_dev ABACUS "Audio Effects/Max Audio Effect"
for MAXV in "Max 8" "Max 9"; do
  PKG="$USER_HOME/Documents/$MAXV/Packages/kalabash"
  mkdir -p "$PKG/externals"
  cp -R "$SRC/externals/"*.mxo "$PKG/externals/"
  chown -R "$USER_NAME" "$USER_HOME/Documents/$MAXV"
done
chown -R "$USER_NAME" "$LIB"
xattr -dr com.apple.quarantine "$LIB" "$USER_HOME/Documents/Max 8/Packages/kalabash" "$USER_HOME/Documents/Max 9/Packages/kalabash" 2>/dev/null || true
exit 0
POST
chmod +x "$WORK/scripts/postinstall"

pkgbuild --root "$WORK/vst3" --install-location "/Library/Audio/Plug-Ins/VST3" \
  --identifier com.kalabash.modules.vst3 --version "$VERSION" "$WORK/vst3.pkg"
pkgbuild --root "$WORK/au" --install-location "/Library/Audio/Plug-Ins/Components" \
  --identifier com.kalabash.modules.au --version "$VERSION" "$WORK/au.pkg"
pkgbuild --root "$WORK/m4l" --install-location "/Users/Shared" --scripts "$WORK/scripts" \
  --identifier com.kalabash.modules.m4l --version "$VERSION" "$WORK/m4l.pkg"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>kalabash modules $VERSION</title>
  <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <choices-outline>
    <line choice="m4l"/><line choice="vst3"/><line choice="au"/>
  </choices-outline>
  <choice id="m4l" title="Max for Live devices (DICEBAG, KNOTWORK, ALLOY, FOGBANK, ABACUS)"><pkg-ref id="com.kalabash.modules.m4l"/></choice>
  <choice id="vst3" title="VST3 plugins (KNOTWORK, ALLOY, FOGBANK, ABACUS)"><pkg-ref id="com.kalabash.modules.vst3"/></choice>
  <choice id="au" title="Audio Unit plugins"><pkg-ref id="com.kalabash.modules.au"/></choice>
  <pkg-ref id="com.kalabash.modules.vst3">vst3.pkg</pkg-ref>
  <pkg-ref id="com.kalabash.modules.au">au.pkg</pkg-ref>
  <pkg-ref id="com.kalabash.modules.m4l">m4l.pkg</pkg-ref>
</installer-gui-script>
XML
PKG="kalabash-modules-$VERSION.pkg"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$WORK/$PKG"

# One zip: installer + manual-install folder + notes
Z="$WORK/zip/kalabash-modules-$VERSION"
mkdir -p "$Z/Manual install/VST3" "$Z/Manual install/Components" "$Z/Manual install/Max for Live" \
         "$Z/Manual install/Max package/kalabash/externals"
cp "$WORK/$PKG" "$Z/"
cp -R "$STAGE/vst3/"* "$Z/Manual install/VST3/"
cp -R "$STAGE/au/"* "$Z/Manual install/Components/"
for d in DICEBAG KNOTWORK ALLOY FOGBANK ABACUS; do cp -R "$STAGE/m4l/$d" "$Z/Manual install/Max for Live/"; done
cp -R "$STAGE/m4l/externals/"*.mxo "$Z/Manual install/Max package/kalabash/externals/"
cp "$ROOT/installer/READ ME.txt" "$Z/"
(cd "$WORK/zip" && ditto -c -k --sequesterRsrc --keepParent "kalabash-modules-$VERSION" "$DIST/kalabash-modules-$VERSION.zip")
echo "built $DIST/kalabash-modules-$VERSION.zip"
