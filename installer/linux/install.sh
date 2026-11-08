#!/usr/bin/env bash
# CUPIDO — instalação no Linux (usuário atual). Uso: ./install.sh  [--uninstall]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ART="${CUPIDO_ARTEFACTS:-$HERE}"
if [[ "${1:-}" == "--uninstall" ]]; then
  rm -rf ~/.vst3/CUPIDO.vst3 ~/.lv2/CUPIDO.lv2 ~/.local/bin/CUPIDO ~/.local/share/applications/cupido.desktop ~/.local/share/icons/cupido.png
  echo "CUPIDO removido."; exit 0
fi
need() { ldconfig -p 2>/dev/null | grep -q "$1" || echo "Aviso: biblioteca '$1' não encontrada (instale: $2)"; }
need libwebkit2gtk-4.1 "sudo apt install libwebkit2gtk-4.1-0"
need libcurl "sudo apt install libcurl4"
mkdir -p ~/.vst3 ~/.lv2 ~/.local/bin ~/.local/share/applications ~/.local/share/icons
[[ -d "$ART/VST3/CUPIDO.vst3" ]] && cp -R "$ART/VST3/CUPIDO.vst3" ~/.vst3/ && echo "VST3  -> ~/.vst3"
[[ -d "$ART/LV2/CUPIDO.lv2" ]]   && cp -R "$ART/LV2/CUPIDO.lv2" ~/.lv2/   && echo "LV2   -> ~/.lv2"
if [[ -f "$ART/Standalone/CUPIDO" ]]; then
  cp "$ART/Standalone/CUPIDO" ~/.local/bin/ && chmod +x ~/.local/bin/CUPIDO
  [[ -f "$HERE/icon.png" ]] && cp "$HERE/icon.png" ~/.local/share/icons/cupido.png
  cat > ~/.local/share/applications/cupido.desktop <<EOF
[Desktop Entry]
Name=CUPIDO
Comment=Suíte vocal com IA
Exec=$HOME/.local/bin/CUPIDO
Icon=$HOME/.local/share/icons/cupido.png
Type=Application
Categories=AudioVideo;Audio;
EOF
  echo "App   -> ~/.local/bin/CUPIDO (no menu de aplicativos)"
fi
echo "Pronto! Reescaneie os plugins no seu DAW (Reaper, Bitwig, Ardour...)."
