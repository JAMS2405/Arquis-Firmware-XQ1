#!/usr/bin/env bash
# Compila y ejecuta el simulador de la pantalla de nivelacion 5x5 contra LVGL 6.1.1-MKS real. Capturas PNG en ./capturas_pid
# Uso: [CROSS=] ./build_pid_sim.sh [carpeta_lvgl]     (sin CROSS usa el compilador del PC: la memoria medida es mayor que la real)
set -e
CROSS=${CROSS-}
if [ -n "$CROSS" ]; then RUN="qemu-arm"; STATIC="-static"; else RUN=""; STATIC=""; fi
CC=${CROSS}gcc; CXX=${CROSS}g++
cd "$(dirname "$0")"
MKS="$(cd ../../../Marlin/src/lcd/extui/mks_ui && pwd)"
LV=${1:-/tmp/arquis_lvgl}
if [ ! -f "$LV/lvgl.h" ]; then mkdir -p "$LV"; (cd "$LV" && python3 "$OLDPWD/fetch_lvgl.py"); fi
mkdir -p build capturas_pid
if ! ls build/*.o >/dev/null 2>&1; then
  for f in $(find "$LV/src" -name '*.c'); do $CC -Os -w -c -I"$LV" "$f" -o build/$(echo "$f" | md5sum | cut -c1-10).o; done
  $CC -Os -w -c -I"$LV" -I"$MKS" $MKS/arquis_icons.c $MKS/arquis_logo.c $MKS/arq_font_*.c; mv *.o build/
fi
$CXX $STATIC -Os -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -std=gnu++17 -I"$LV" -I"$MKS" pid_sim.cpp \
  "$MKS/arquis_theme.cpp" "$MKS/arquis_widgets.cpp" "$MKS/arquis_logic.cpp" "$MKS/arquis_pid_logic.cpp" "$MKS/arquis_pid_view.cpp" build/*.o -o build/pid_sim
rm -f capturas_pid/*
set +e; $RUN ./build/pid_sim capturas_pid; RC=$?; set -e
python3 - <<'PY'
from PIL import Image; import glob
for f in glob.glob('capturas_pid/*.ppm'):
    Image.open(f).save(f.replace('.ppm', '.png'))
PY
rm -f capturas_pid/*.ppm; ls capturas_pid
exit $RC
