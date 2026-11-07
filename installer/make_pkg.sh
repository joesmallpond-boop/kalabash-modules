#!/bin/bash
# Builds one installer for all five devices:
#   VST3 -> /Library/Audio/Plug-Ins/VST3, AU -> /Library/Audio/Plug-Ins/Components,
#   DICEBAG (Max for Live) -> your Ableton User Library (via postinstall).
set -euo pipefail
VERSION="${1:-dev}"; VERSION="${VERSION#v}"
[ "$VERSION" = "main" ] && VERSION="1.0.0"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$ROOT/stage"; WORK="$ROOT/pkgwork"; DIST="$ROOT/dist"
rm -rf "$WORK" "$DIST"; mkdir -p "$WORK"/{vst3,au,m4l,scripts} "$DIST"

cp -R "$STAGE/vst3/"* "$WORK/vst3/"
cp -R "$STAGE/au/"* "$WORK/au/"
mkdir -p "$WORK/m4l/kalabash/DICEBAG"
cp -R "$STAGE/m4l/DICEBAG/"* "$WORK/m4l/kalabash/DICEBAG/"

cat > "$WORK/scripts/postinstall" <<'POST'
#!/bin/bash
USER_NAME=$(stat -f%Su /dev/console)
USER_HOME=$(dscl . -read /Users/"$USER_NAME" NFSHomeDirectory | awk '{print $2}')
DEST="$USER_HOME/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/kalabash"
mkdir -p "$DEST"
rm -rf "$DEST/DICEBAG"
cp -R "/Users/Shared/kalabash/DICEBAG" "$DEST/"
chown -R "$USER_NAME" "$DEST"
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
    <line choice="vst3"/><line choice="au"/><line choice="m4l"/>
  </choices-outline>
  <choice id="vst3" title="VST3 plugins (KNOTWORK, ALLOY, FOGBANK, ABACUS)"><pkg-ref id="com.kalabash.modules.vst3"/></choice>
  <choice id="au" title="Audio Unit plugins"><pkg-ref id="com.kalabash.modules.au"/></choice>
  <choice id="m4l" title="DICEBAG Max for Live device"><pkg-ref id="com.kalabash.modules.m4l"/></choice>
  <pkg-ref id="com.kalabash.modules.vst3">vst3.pkg</pkg-ref>
  <pkg-ref id="com.kalabash.modules.au">au.pkg</pkg-ref>
  <pkg-ref id="com.kalabash.modules.m4l">m4l.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$DIST/kalabash-modules-$VERSION.pkg"
(cd "$STAGE" && zip -qry "$DIST/kalabash-modules-$VERSION-manual.zip" vst3 au m4l)
echo "built $DIST/kalabash-modules-$VERSION.pkg"
