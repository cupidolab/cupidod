#!/usr/bin/env bash
# =============================================================================
#  CUPIDO — instalador macOS (.pkg com tela de boas-vindas e escolha de componentes)
#  Uso: installer/macos/build_pkg.sh build/Cupido_artefacts/Release 1.0.0
#  Assinatura (opcional): exporte DEV_ID_APP="Developer ID Application: Nome (TEAMID)"
#                          e DEV_ID_INSTALLER="Developer ID Installer: Nome (TEAMID)"
#  Notarização (opcional): APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD
# =============================================================================
set -euo pipefail
ART="${1:-build/Cupido_artefacts/Release}"
VERSION="${2:-1.0.0}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
WORK="$(mktemp -d)"
OUT="$ROOT/dist"; mkdir -p "$OUT"

sign() {  # assina com Developer ID se houver; senão assinatura ad-hoc (funciona localmente)
  local target="$1"
  if [[ -n "${DEV_ID_APP:-}" ]]; then
    codesign --force --deep --options runtime --timestamp --sign "$DEV_ID_APP" "$target"
  else
    codesign --force --deep --sign - "$target"
  fi
}

stage() { # nome, origem, destino-no-sistema
  local name="$1" src="$2" dest="$3"
  [[ -e "$src" ]] || { echo "pulando $name (não encontrado: $src)"; return 0; }
  mkdir -p "$WORK/$name/root$dest"
  cp -R "$src" "$WORK/$name/root$dest/"
  sign "$WORK/$name/root$dest/$(basename "$src")"
  pkgbuild --root "$WORK/$name/root" --identifier "com.cupidoaudio.cupido.$name" --version "$VERSION" \
           --install-location "/" "$WORK/$name.pkg"
}

stage vst3       "$ART/VST3/CUPIDO.vst3"            "/Library/Audio/Plug-Ins/VST3"
stage au         "$ART/AU/CUPIDO.component"         "/Library/Audio/Plug-Ins/Components"
stage aax        "$ART/AAX/CUPIDO.aaxplugin"        "/Library/Application Support/Avid/Audio/Plug-Ins"
stage standalone "$ART/Standalone/CUPIDO.app"       "/Applications"

# distribution.xml só com os componentes que existem
CHOICES=""; OUTLINE=""
for c in vst3 au aax standalone; do
  [[ -f "$WORK/$c.pkg" ]] || continue
  case $c in
    vst3) T="Plugin VST3 (Ableton, Cubase, Studio One, Reaper, Bitwig, FL Studio)";;
    au) T="Plugin Audio Unit (Logic Pro, GarageBand)";;
    aax) T="Plugin AAX (Pro Tools)";;
    standalone) T="Aplicativo CUPIDO (independente)";;
  esac
  OUTLINE+="    <line choice=\"$c\"/>\n"
  CHOICES+="  <choice id=\"$c\" title=\"$T\" visible=\"true\" start_selected=\"true\"><pkg-ref id=\"com.cupidoaudio.cupido.$c\"/></choice>\n  <pkg-ref id=\"com.cupidoaudio.cupido.$c\" version=\"$VERSION\">$c.pkg</pkg-ref>\n"
done

mkdir -p "$WORK/res"
cp "$HERE/welcome.html" "$HERE/conclusion.html" "$HERE/background.png" "$WORK/res/"
cp "$ROOT/LICENSE.txt" "$WORK/res/license.txt"

printf '%b' "<?xml version=\"1.0\" encoding=\"utf-8\"?>
<installer-gui-script minSpecVersion=\"2\">
  <title>CUPIDO $VERSION</title>
  <welcome file=\"welcome.html\" mime-type=\"text/html\"/>
  <license file=\"license.txt\"/>
  <conclusion file=\"conclusion.html\" mime-type=\"text/html\"/>
  <background file=\"background.png\" alignment=\"bottomleft\" scaling=\"none\"/>
  <background-darkAqua file=\"background.png\" alignment=\"bottomleft\" scaling=\"none\"/>
  <options customize=\"allow\" require-scripts=\"false\" hostArchitectures=\"arm64,x86_64\"/>
  <domains enable_localSystem=\"true\"/>
  <choices-outline>
$OUTLINE  </choices-outline>
$CHOICES</installer-gui-script>
" > "$WORK/distribution.xml"

PKG="$OUT/CUPIDO-$VERSION-macOS-Instalador.pkg"
if [[ -n "${DEV_ID_INSTALLER:-}" ]]; then
  productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" --package-path "$WORK" --sign "$DEV_ID_INSTALLER" "$PKG"
else
  productbuild --distribution "$WORK/distribution.xml" --resources "$WORK/res" --package-path "$WORK" "$PKG"
fi

if [[ -n "${APPLE_ID:-}" && -n "${APPLE_TEAM_ID:-}" && -n "${APPLE_APP_PASSWORD:-}" && -n "${DEV_ID_INSTALLER:-}" ]]; then
  xcrun notarytool submit "$PKG" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD" --wait
  xcrun stapler staple "$PKG"
fi
echo "Instalador pronto: $PKG"
