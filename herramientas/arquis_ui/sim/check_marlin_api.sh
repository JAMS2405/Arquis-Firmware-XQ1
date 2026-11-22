#!/usr/bin/env bash
# Verifica que arquis_data.cpp, arquis_nav.cpp y arquis_dashboard.cpp compilan contra las FIRMAS reales de Marlin 2.1.2.10.
# IMPORTANTE: usa cabeceras "stub" (herramientas/arquis_ui/stub) que replican esas firmas; NO es una compilacion de Marlin.
# La compilacion real la hace PlatformIO. Uso: ./check_marlin_api.sh [carpeta_lvgl]
set -e
cd "$(dirname "$0")"
LV=${1:-/tmp/arquis_lvgl}
MKS="$(cd ../../../Marlin/src/lcd/extui/mks_ui && pwd)"
T=$(mktemp -d); ./make_stub_tree.sh "$T"   # arbol con la disposicion del repositorio y las cabeceras stub
MKS="$(cd ../../../Marlin/src/lcd/extui/mks_ui && pwd)"
CXX=${CXX:-arm-none-eabi-g++}
for f in arquis_data arquis_nav arquis_dashboard arquis_print arquis_objects_view arquis_ctrl_view arquis_controls arquis_tools_view arquis_tools arquis_files arquis_files_view arquis_params_view arquis_params arquis_objects arquis_objects_sd draw_cancel_object draw_print_file_arq arquis_mesh_logic arquis_mesh_view arquis_mesh arquis_mesh_ui arquis_pid_logic arquis_pid_view arquis_pid arquis_pid_ui arquis_perf_logic arquis_perf_view arquis_perf arquis_perf_ui; do
  $CXX -mcpu=cortex-m4 -mthumb -std=gnu++17 -fno-exceptions -fno-rtti -Wall -Wextra -Wno-unused-parameter -I"$LV" -I"$T/Marlin/src/lcd/extui/mks_ui" -include stdint.h \
       -fsyntax-only "$T/Marlin/src/lcd/extui/mks_ui/$f.cpp" && echo "OK  $f.cpp compila contra las firmas de Marlin"
done
rm -rf "$T"
