#!/usr/bin/env bash
# Compila el simulador de ARQUIS UI contra LVGL 6.1.1-MKS real y genera capturas PNG en ./capturas
# Uso: ./build_sim.sh [carpeta_lvgl]   (si no existe, se descarga con fetch_lvgl.py)
set -e
# Se compila en ARM de 32 bits (como la placa) y se ejecuta con qemu-user, para que los tamanos de memoria de LVGL sean los reales.
# Sin compilador ARM: CROSS= ./build_sim.sh   (usa el compilador del PC; la memoria medida sera mayor que la real)
CROSS=${CROSS-arm-linux-gnueabihf-}
if [ -n "$CROSS" ]; then RUN="qemu-arm"; STATIC="-static"; else RUN=""; STATIC=""; fi
CC=${CROSS}gcc; CXX=${CROSS}g++
cd "$(dirname "$0")"
MKS="$(cd ../../../Marlin/src/lcd/extui/mks_ui && pwd)"
LV=${1:-/tmp/arquis_lvgl}
if [ ! -f "$LV/lvgl.h" ]; then mkdir -p "$LV"; (cd "$LV" && python3 "$OLDPWD/fetch_lvgl.py"); fi
# La fuente roboto_22 de la configuracion del fabricante y su macro de fuente china no se usan en el simulador.
mkdir -p build capturas; rm -f build/*.o
for f in $(find "$LV/src" -name '*.c'); do $CC -Os -w -c -I"$LV" "$f" -o build/$(echo "$f" | md5sum | cut -c1-10).o; done
$CC -Os -w -c -I"$LV" -I"$MKS" $MKS/arquis_icons.c $MKS/arquis_logo.c $MKS/arq_font_*.c 2>&1 | head; mv *.o build/
$CXX $STATIC -Os -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -std=gnu++17 -I"$LV" -I"$MKS" arquis_sim.cpp "$MKS/arquis_theme.cpp" "$MKS/arquis_widgets.cpp" "$MKS/arquis_logic.cpp" "$MKS/arquis_dashboard_view.cpp" "$MKS/arquis_print_view.cpp" "$MKS/arquis_objects_view.cpp" "$MKS/arquis_ctrl_view.cpp" build/*.o -o build/arquis_sim
$RUN ./build/arquis_sim capturas
python3 - <<'PY'
from PIL import Image; import glob
for f in glob.glob('capturas/*.ppm'):
    im = Image.open(f); im.save(f.replace('.ppm', '.png')); im.resize((960, 640), Image.LANCZOS).save(f.replace('.ppm', '_x2.png'))
PY
rm -f capturas/*.ppm; ls capturas
# Pruebas de logica (nativas, sin LVGL ni Marlin)
g++ -std=gnu++17 -Wall -Wextra -I"$MKS" logic_test.cpp "$MKS/arquis_logic.cpp" -o build/logic_test && ./build/logic_test
