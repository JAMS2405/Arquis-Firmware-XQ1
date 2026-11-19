#!/usr/bin/env bash
# Tamano (Flash y RAM estatica) de los modulos de la Etapa 3 frente a la version ANTERIOR (rama estable `main`), compilados con arm-none-eabi-g++
# (Cortex-M4, -Os, hilos y datos en secciones propias). NO es un enlace: es el tamano de cada objeto. La cifra real de Flash/RAM solo sale de
# compilar el firmware completo con PlatformIO (ver docs/slicer_suite/07_etapa3_firmware.md). Los objetos de la interfaz se compilan contra
# cabeceras "stub" de Marlin (herramientas/arquis_ui/stub): sirven para medir el codigo propio, no el de Marlin.
# Uso: tests/firmware/measure_size.sh [carpeta_lvgl]     (requiere arm-none-eabi-g++ y git; la rama estable debe llamarse `main`)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; LV="${1:-/tmp/arquis_lvgl}"
BASE="${BASE_REF:-main}"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
CXX=arm-none-eabi-g++; SZ=arm-none-eabi-size
FLAGS="-Os -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -std=gnu++17 -w -include stdint.h"
mkdir -p "$T/old"; ( cd "$ROOT" && git archive "$BASE" Marlin/src/lcd/extui/mks_ui | tar -x -C "$T/old" )
( cd "$ROOT" && git show "$BASE:Marlin/src/feature/cancel_object.h" > "$T/old/cancel_object.h" )
"$ROOT/herramientas/arquis_ui/sim/make_stub_tree.sh" "$T/new"
"$ROOT/herramientas/arquis_ui/sim/make_stub_tree.sh" "$T/oldtree" "$T/old/Marlin/src/lcd/extui/mks_ui" "$T/old/cancel_object.h"
size_of() {   # <arbol> <archivo.cpp>  -> "text data bss"
  local tree="$1" f="$2"
  [ -f "$tree/Marlin/src/lcd/extui/mks_ui/$f.cpp" ] || { echo "0 0 0"; return; }
  $CXX $FLAGS -I"$LV" -I"$tree/Marlin/src/lcd/extui/mks_ui" -c "$tree/Marlin/src/lcd/extui/mks_ui/$f.cpp" -o "$T/$f.o" 2>/dev/null || { echo "ERR ERR ERR"; return; }
  $SZ "$T/$f.o" | awk 'NR==2 {print $1, $2, $3}'
}
printf "%-28s %22s %22s\n" "modulo" "ANTES (text data bss)" "AHORA (text data bss)"
tt_o=0; tt_n=0; tb_o=0; tb_n=0
for f in arquis_objects arquis_objects_sd arquis_objects_view draw_cancel_object arquis_files; do
  o=$(size_of "$T/oldtree" $f); n=$(size_of "$T/new" $f)
  printf "%-28s %22s %22s\n" "$f" "$o" "$n"
  set -- $o; ot=$(( $1 + $2 )); ob=$3; set -- $n; nt=$(( $1 + $2 )); nb=$3
  tt_o=$((tt_o + ot)); tt_n=$((tt_n + nt)); tb_o=$((tb_o + ob)); tb_n=$((tb_n + nb))
done
# Motor de cancelacion (copia estable vs de esta rama), con las cabeceras minimas de tests/capacity
for which in stock dev; do
  "$ROOT/tests/capacity/build_real_engine.sh" "$T/eng_$which" "$which" >/dev/null 2>&1
  tot=0; bs=0
  for f in co m486; do
    $CXX $FLAGS -I"$T/eng_$which" -c "$T/eng_$which/$f.cpp" -o "$T/eng_$which/$f.o" 2>/dev/null
    read t d b <<< "$($SZ "$T/eng_$which/$f.o" | awk 'NR==2 {print $1, $2, $3}')"; tot=$((tot + t + d)); bs=$((bs + b))
  done
  eval "eng_$which=\"$tot $bs\""
done
printf "%-28s %22s %22s\n" "cancel_object+M486 (motor)" "$eng_stock" "$eng_dev"
set -- $eng_stock; tt_o=$((tt_o + $1)); tb_o=$((tb_o + $2)); set -- $eng_dev; tt_n=$((tt_n + $1)); tb_n=$((tb_n + $2))
echo "-------------------------------------------------------------------------------------------"
printf "%-28s %22s %22s\n" "TOTAL (Flash = text+data)" "$tt_o  bss $tb_o" "$tt_n  bss $tb_n"
echo "Delta: Flash $((tt_n - tt_o)) B ; RAM estatica (bss) $((tb_n - tb_o)) B"
echo "(los objetos compilados con -ffunction-sections: el enlazador descarta lo no usado; la cifra real exige enlazar el firmware)"
