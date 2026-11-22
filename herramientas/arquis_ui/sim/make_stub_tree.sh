#!/usr/bin/env bash
# Crea un arbol temporal con la misma disposicion que el repositorio (Marlin/src/..., shared/...) y las cabeceras "stub" de herramientas/arquis_ui/stub.
# Uso: make_stub_tree.sh <destino> [carpeta mks_ui de origen] [cancel_object.h de origen]
#   (los dos ultimos permiten construir el arbol con la version ANTERIOR del codigo para comparar tamanos: tests/firmware/measure_size.sh)
set -e
cd "$(dirname "$0")"
T="$1"; MKS="${2:-$(cd ../../../Marlin/src/lcd/extui/mks_ui && pwd)}"; CO_H="${3:-$MKS/../../../feature/cancel_object.h}"
mkdir -p "$T/Marlin/src/lcd/extui/mks_ui" "$T/shared/object_protocol/generated"
cp ../../../shared/object_protocol/generated/arquis_object_limits.h "$T/shared/object_protocol/generated/"
cp -r ../stub/inc ../stub/module ../stub/sd ../stub/gcode ../stub/feature ../stub/MarlinCore.h "$T/Marlin/src/"
cp ../stub/draw_ui.h ../stub/SPI_TFT.h ../stub/tft_lvgl_configuration.h "$T/Marlin/src/lcd/extui/mks_ui/"
cp $MKS/arquis_*.h $MKS/arquis_data.cpp $MKS/arquis_nav.cpp $MKS/arquis_dashboard.cpp $MKS/arquis_dashboard_view.cpp $MKS/arquis_print.cpp $MKS/arquis_print_view.cpp $MKS/arquis_objects_view.cpp $MKS/arquis_ctrl_view.cpp $MKS/arquis_controls.cpp $MKS/arquis_tools_view.cpp $MKS/arquis_tools.cpp $MKS/arquis_widgets.cpp $MKS/arquis_theme.cpp $MKS/arquis_logic.cpp $MKS/arquis_objects.cpp $MKS/arquis_objects.h $MKS/arquis_objects_sd.cpp $MKS/arquis_objects_sd.h $MKS/arquis_mesh.cpp $MKS/arquis_mesh_ui.cpp $MKS/arquis_mesh_view.cpp $MKS/arquis_mesh_logic.cpp "$T/Marlin/src/lcd/extui/mks_ui/" 2>/dev/null || true   # (en el arbol ANTERIOR faltan los archivos nuevos: es lo esperado)
cp $MKS/arquis_icons.h $MKS/arquis_logo.h $MKS/draw_cancel_object.cpp "$T/Marlin/src/lcd/extui/mks_ui/"
cp "$CO_H" "$T/Marlin/src/feature/cancel_object.h"   # cabecera REAL de Marlin
python3 - "$MKS/draw_print_file.h" "$T/Marlin/src/lcd/extui/mks_ui/print_file_types.h" <<'PY'
import sys,re
s=open(sys.argv[1]).read(); m=re.search(r"typedef struct \{\n  int cur_page_first_offset.*?extern LIST_FILE list_file;", s, re.S)
open(sys.argv[2],'w').write('#pragma once\n#include <stdint.h>\n'+m.group(0)+'\n')
PY
cp $MKS/tft_multi_language.h $MKS/tft_Language_*.h $MKS/arquis_params.cpp $MKS/arquis_params_view.cpp "$T/Marlin/src/lcd/extui/mks_ui/"   # tablas de idioma REALES de MKS
cp $MKS/arquis_files.cpp $MKS/arquis_files.h $MKS/arquis_files_view.h $MKS/arquis_files_view.cpp ../stub/print_file_harness.h "$T/Marlin/src/lcd/extui/mks_ui/"
# bloques ARQUIS de draw_print_file.cpp (entre '#else   // ---- explorador ARQUIS' y '#endif // ARQUIS_UI'), dentro de un arnes con las firmas reales
python3 - "$MKS/draw_print_file.cpp" "$T/Marlin/src/lcd/extui/mks_ui/draw_print_file_arq.cpp" <<'PY'
import sys,re
s=open(sys.argv[1]).read(); out=['#include "../../../inc/MarlinConfigPre.h"','#include "print_file_harness.h"']
for m in re.finditer(r"#else   // ---- explorador ARQUIS[^\n]*\n(.*?)#endif // ARQUIS_UI", s, re.S): out.append(m.group(1))
open(sys.argv[2],"w").write("\n".join(out))
PY
