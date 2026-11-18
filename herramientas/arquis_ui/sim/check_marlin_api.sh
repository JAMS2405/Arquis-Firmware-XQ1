#!/usr/bin/env bash
# Verifica que arquis_data.cpp, arquis_nav.cpp y arquis_dashboard.cpp compilan contra las FIRMAS reales de Marlin 2.1.2.10.
# IMPORTANTE: usa cabeceras "stub" (herramientas/arquis_ui/stub) que replican esas firmas; NO es una compilacion de Marlin.
# La compilacion real la hace PlatformIO. Uso: ./check_marlin_api.sh [carpeta_lvgl]
set -e
cd "$(dirname "$0")"
LV=${1:-/tmp/arquis_lvgl}
MKS="$(cd ../../../Marlin/src/lcd/extui/mks_ui && pwd)"
T=$(mktemp -d); mkdir -p "$T/src/lcd/extui/mks_ui"
cp -r ../stub/inc ../stub/module ../stub/sd ../stub/MarlinCore.h "$T/src/"
cp ../stub/draw_ui.h "$T/src/lcd/extui/mks_ui/"
cp $MKS/arquis_*.h $MKS/arquis_data.cpp $MKS/arquis_nav.cpp $MKS/arquis_dashboard.cpp $MKS/arquis_dashboard_view.cpp $MKS/arquis_widgets.cpp $MKS/arquis_theme.cpp $MKS/arquis_logic.cpp "$T/src/lcd/extui/mks_ui/" 2>/dev/null
cp $MKS/arquis_icons.h $MKS/arquis_logo.h "$T/src/lcd/extui/mks_ui/"
CXX=${CXX:-arm-none-eabi-g++}
for f in arquis_data arquis_nav arquis_dashboard; do
  $CXX -mcpu=cortex-m4 -mthumb -std=gnu++17 -fno-exceptions -fno-rtti -Wall -Wextra -Wno-unused-parameter -I"$LV" -I"$T/src/lcd/extui/mks_ui" -include stdint.h \
       -fsyntax-only "$T/src/lcd/extui/mks_ui/$f.cpp" && echo "OK  $f.cpp compila contra las firmas de Marlin"
done
rm -rf "$T"
