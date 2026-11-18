#!/usr/bin/env bash
# Verifica que arquis_data.cpp, arquis_nav.cpp y arquis_dashboard.cpp compilan contra las FIRMAS reales de Marlin 2.1.2.10.
# IMPORTANTE: usa cabeceras "stub" (herramientas/arquis_ui/stub) que replican esas firmas; NO es una compilacion de Marlin.
# La compilacion real la hace PlatformIO. Uso: ./check_marlin_api.sh [carpeta_lvgl]
set -e
cd "$(dirname "$0")"
LV=${1:-/tmp/arquis_lvgl}
MKS="$(cd ../../../Marlin/src/lcd/extui/mks_ui && pwd)"
T=$(mktemp -d); mkdir -p "$T/src/lcd/extui/mks_ui"
cp -r ../stub/inc ../stub/module ../stub/sd ../stub/gcode ../stub/feature ../stub/MarlinCore.h "$T/src/"
cp ../stub/draw_ui.h ../stub/SPI_TFT.h ../stub/tft_lvgl_configuration.h "$T/src/lcd/extui/mks_ui/"
cp $MKS/arquis_*.h $MKS/arquis_data.cpp $MKS/arquis_nav.cpp $MKS/arquis_dashboard.cpp $MKS/arquis_dashboard_view.cpp $MKS/arquis_print.cpp $MKS/arquis_print_view.cpp $MKS/arquis_objects_view.cpp $MKS/arquis_ctrl_view.cpp $MKS/arquis_controls.cpp $MKS/arquis_tools_view.cpp $MKS/arquis_tools.cpp $MKS/arquis_widgets.cpp $MKS/arquis_theme.cpp $MKS/arquis_logic.cpp "$T/src/lcd/extui/mks_ui/" 2>/dev/null
cp $MKS/arquis_icons.h $MKS/arquis_logo.h $MKS/draw_cancel_object.cpp "$T/src/lcd/extui/mks_ui/"
cp "$MKS/../../../feature/cancel_object.h" "$T/src/feature/"   # cabecera REAL de Marlin
python3 - "$MKS/draw_print_file.h" "$T/src/lcd/extui/mks_ui/print_file_types.h" <<'PY'
import sys,re
s=open(sys.argv[1]).read(); m=re.search(r"typedef struct \{\n  int cur_page_first_offset.*?extern LIST_FILE list_file;", s, re.S)
open(sys.argv[2],'w').write('#pragma once\n#include <stdint.h>\n'+m.group(0)+'\n')
PY
cp $MKS/tft_multi_language.h $MKS/tft_Language_*.h $MKS/arquis_params.cpp $MKS/arquis_params_view.cpp "$T/src/lcd/extui/mks_ui/"   # tablas de idioma REALES de MKS
cp $MKS/arquis_files.cpp $MKS/arquis_files.h $MKS/arquis_files_view.h $MKS/arquis_files_view.cpp ../stub/print_file_harness.h "$T/src/lcd/extui/mks_ui/"
# bloques ARQUIS de draw_print_file.cpp (entre '#else   // ---- explorador ARQUIS' y '#endif // ARQUIS_UI'), dentro de un arnes con las firmas reales
python3 - "$MKS/draw_print_file.cpp" "$T/src/lcd/extui/mks_ui/draw_print_file_arq.cpp" <<'PY'
import sys,re
s=open(sys.argv[1]).read(); out=['#include "../../../inc/MarlinConfigPre.h"','#include "print_file_harness.h"']
for m in re.finditer(r"#else   // ---- explorador ARQUIS[^\n]*\n(.*?)#endif // ARQUIS_UI", s, re.S): out.append(m.group(1))
open(sys.argv[2],"w").write("\n".join(out))
PY
CXX=${CXX:-arm-none-eabi-g++}
for f in arquis_data arquis_nav arquis_dashboard arquis_print arquis_objects_view arquis_ctrl_view arquis_controls arquis_tools_view arquis_tools arquis_files arquis_files_view arquis_params_view arquis_params draw_cancel_object draw_print_file_arq; do
  $CXX -mcpu=cortex-m4 -mthumb -std=gnu++17 -fno-exceptions -fno-rtti -Wall -Wextra -Wno-unused-parameter -I"$LV" -I"$T/src/lcd/extui/mks_ui" -include stdint.h \
       -fsyntax-only "$T/src/lcd/extui/mks_ui/$f.cpp" && echo "OK  $f.cpp compila contra las firmas de Marlin"
done
rm -rf "$T"
