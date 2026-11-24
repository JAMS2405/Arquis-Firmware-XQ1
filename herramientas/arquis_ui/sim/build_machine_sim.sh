#!/usr/bin/env bash
# Simulador del asistente del primer arranque y de Ajustes > Maquina (codigo REAL sobre LVGL 6.1.1-MKS REAL).
# Capturas PNG en ./capturas_maquina. Uso: ./build_machine_sim.sh [carpeta_lvgl]
# Se compila con el compilador del PC (64 bits): la memoria de LVGL medida es MAYOR que en la placa (ARM de 32 bits).
set -e
cd "$(dirname "$0")"
MKS="$(cd ../../../Marlin/src/lcd/extui/mks_ui && pwd)"
LV=${1:-/tmp/arquis_lvgl}
if [ ! -f "$LV/lvgl.h" ]; then mkdir -p "$LV"; (cd "$LV" && python3 "$OLDPWD/fetch_lvgl.py"); fi
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
# Arbol con la disposicion del repositorio: stubs de Marlin (../stub_machine) + archivos REALES de la interfaz
cp -r ../stub_machine/Marlin "$T/"
D="$T/Marlin/src/lcd/extui/mks_ui"
cp "$MKS"/arquis_machine*.h "$MKS"/arquis_machine_ui.cpp "$MKS"/arquis_machine_logic.cpp "$MKS"/arquis_params.h "$MKS"/arquis_params_view.* \
   "$MKS"/arquis_widgets.* "$MKS"/arquis_theme.* "$MKS"/arquis_logic.* "$MKS"/arquis_logo.* "$MKS"/arquis_icons.* "$MKS"/arq_font_*.c "$D/"
mkdir -p build_machine capturas_maquina
if ! ls build_machine/lv_*.o >/dev/null 2>&1; then
  for f in $(find "$LV/src" -name '*.c'); do gcc -Os -w -c -I"$LV" "$f" -o build_machine/lv_$(echo "$f" | md5sum | cut -c1-10).o; done
fi
(cd "$D" && gcc -Os -w -c -I"$LV" -I. arquis_icons.c arquis_logo.c arq_font_*.c)
g++ -Os -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -std=gnu++17 -I"$LV" -I"$D" \
  machine_sim.cpp "$D/arquis_machine_ui.cpp" "$D/arquis_machine_logic.cpp" "$D/arquis_params_view.cpp" "$D/arquis_widgets.cpp" \
  "$D/arquis_theme.cpp" "$D/arquis_logic.cpp" "$D"/*.o build_machine/lv_*.o -o build_machine/machine_sim
rm -f capturas_maquina/*
set +e; ./build_machine/machine_sim capturas_maquina; RC=$?; set -e
python3 - <<'PY'
from PIL import Image; import glob
for f in glob.glob('capturas_maquina/*.ppm'):
    Image.open(f).save(f.replace('.ppm', '.png'))
PY
rm -f capturas_maquina/*.ppm
exit $RC
