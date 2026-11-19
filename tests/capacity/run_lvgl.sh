#!/usr/bin/env bash
# Laboratorio LVGL: heap de LVGL 6.1.1-MKS REAL al dibujar N marcadores en un solo objeto (N = 0, 16, 40, 64, 128).
# Descarga LVGL (raw.githubusercontent.com) y compila en una carpeta temporal; tarda unos minutos. Host de 64 bits (heap mayor que en la placa).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
MKS="$ROOT/Marlin/src/lcd/extui/mks_ui"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
mkdir -p "$T/lvgl" "$T/build"
( cd "$T/lvgl" && python3 "$ROOT/herramientas/arquis_ui/sim/fetch_lvgl.py" )
for f in $(find "$T/lvgl/src" -name '*.c'); do gcc -Os -w -c -I"$T/lvgl" "$f" -o "$T/build/$(echo "$f" | md5sum | cut -c1-10).o" & done; wait
( cd "$T/build" && gcc -Os -w -c -I"$T/lvgl" -I"$MKS" "$MKS"/arquis_icons.c "$MKS"/arquis_logo.c "$MKS"/arq_font_*.c )
g++ -Os -w -std=gnu++17 -I"$T/lvgl" -I"$MKS" "$HERE/lvgl_heap_lab.cpp" "$MKS/arquis_theme.cpp" "$MKS/arquis_widgets.cpp" "$MKS/arquis_logic.cpp" "$T"/build/*.o -o "$T/heap_lab"
"$T/heap_lab"
