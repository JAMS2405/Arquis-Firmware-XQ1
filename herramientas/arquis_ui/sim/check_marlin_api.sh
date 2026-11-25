#!/usr/bin/env bash
# Verifica que los archivos de la interfaz ARQUIS compilan (solo sintaxis, ARM Cortex-M4) contra cabeceras "stub" que
# replican las firmas de Marlin 2.1.2.10 que usan. NO es una compilacion de Marlin: la compilacion real la hace PlatformIO.
#  - Interfaz (29 archivos): cabeceras de herramientas/arquis_ui/stub (make_stub_tree.sh).
#  - Firmware unico (arquis_machine_logic / arquis_machine / arquis_machine_ui): cabeceras de herramientas/arquis_ui/stub_machine,
#    las mismas que usan la prueba del pegamento (tests/machines/glue_test.cpp) y el simulador (build_machine_sim.sh).
# Falla (codigo != 0) si falta el compilador o LVGL, si un archivo no compila o si no se comprobaron TODOS los archivos:
# nunca da por buena una comprobacion que no se ha ejecutado. Uso: ./check_marlin_api.sh [carpeta_lvgl]   (CXX=... opcional)
set -u
cd "$(dirname "$0")"
LV=${1:-/tmp/arquis_lvgl}
CXX=${CXX:-arm-none-eabi-g++}
if ! command -v "$CXX" >/dev/null 2>&1; then
  echo "FALLO: no se encuentra el compilador '$CXX' (arm-none-eabi-g++). No se ha comprobado nada." >&2; exit 2
fi
if [ ! -f "$LV/lvgl.h" ]; then
  echo "FALLO: no esta LVGL en '$LV' (falta lvgl.h). Descargalo con: (mkdir -p $LV && cd $LV && python3 $(pwd)/fetch_lvgl.py)" >&2; exit 2
fi
MKS="$(cd ../../../Marlin/src/lcd/extui/mks_ui && pwd)"
T=$(mktemp -d); T2=$(mktemp -d); trap 'rm -rf "$T" "$T2"' EXIT
./make_stub_tree.sh "$T" || { echo "FALLO: no se pudo crear el arbol de cabeceras stub" >&2; exit 2; }
# Arbol del firmware unico: stubs de la maquina + archivos REALES (como build_machine_sim.sh)
cp -r ../stub_machine/Marlin "$T2/"; D2="$T2/Marlin/src/lcd/extui/mks_ui"
cp "$MKS"/arquis_machine*.h "$MKS"/arquis_machine.cpp "$MKS"/arquis_machine_logic.cpp "$MKS"/arquis_machine_ui.cpp "$MKS"/arquis_params.h \
   "$MKS"/arquis_params_view.h "$MKS"/arquis_widgets.h "$MKS"/arquis_theme.h "$MKS"/arquis_logic.h "$MKS"/arquis_logo.h "$MKS"/arquis_icons.h "$D2/"
FLAGS="-mcpu=cortex-m4 -mthumb -std=gnu++17 -fno-exceptions -fno-rtti -Wall -Wextra -Wno-unused-parameter -include stdint.h -fsyntax-only"
UI="arquis_data arquis_nav arquis_dashboard arquis_print arquis_objects_view arquis_ctrl_view arquis_controls arquis_tools_view arquis_tools arquis_files arquis_files_view arquis_params_view arquis_params arquis_objects arquis_objects_sd draw_cancel_object draw_print_file_arq arquis_mesh_logic arquis_mesh_view arquis_mesh arquis_mesh_ui arquis_pid_logic arquis_pid_view arquis_pid arquis_pid_ui arquis_perf_logic arquis_perf_view arquis_perf arquis_perf_ui"
MACHINE="arquis_machine_logic arquis_machine arquis_machine_ui"
ok=0; bad=0; want=0
check() {   # <arbol mks_ui> <archivo>
  want=$((want + 1))
  if [ ! -f "$1/$2.cpp" ]; then echo "FALLA $2.cpp: no esta en el arbol"; bad=$((bad + 1)); return; fi
  if "$CXX" $FLAGS -I"$LV" -I"$1" "$1/$2.cpp"; then echo "OK    $2.cpp"; ok=$((ok + 1)); else echo "FALLA $2.cpp"; bad=$((bad + 1)); fi
}
for f in $UI; do check "$T/Marlin/src/lcd/extui/mks_ui" "$f"; done
for f in $MACHINE; do check "$D2" "$f"; done
echo "check_marlin_api: $ok de $want archivos compilan, $bad fallos ($CXX)"
[ "$bad" = 0 ] && [ "$ok" = "$want" ] && [ "$want" = 32 ] || exit 1
