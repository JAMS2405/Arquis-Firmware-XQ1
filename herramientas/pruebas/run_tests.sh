#!/usr/bin/env bash
# Pruebas de logica en el PC (no necesitan la impresora ni PlatformIO). Requiere g++ y python3.
# Usa el codigo REAL del firmware: extrae preview_fill_update() y compila feature/cancel_object.cpp con stubs.
set -e
cd "$(dirname "$0")"
SRC=../../Marlin/src
python3 - <<'PY'
src=open('../../Marlin/src/lcd/extui/mks_ui/draw_printing.cpp').read()
a=src.index('void preview_fill_update'); i=src.index('{',a); d=0
for j in range(i,len(src)):
    d+=(src[j]=='{')-(src[j]=='}')
    if d==0: break
open('fill_body.inc','w').write(src[a:j+1])
PY
cp $SRC/feature/cancel_object.h stub/feature/
sed 's#"../gcode/gcode.h"#"gcode/gcode.h"#;s#"../lcd/marlinui.h"#"lcd/marlinui.h"#;s#"cancel_object.h"#"feature/cancel_object.h"#;s#"../inc/MarlinConfig.h"#"prelude.h"#' $SRC/feature/cancel_object.cpp > co.cpp
g++ -std=c++17 -O1 -I. fill_test.cpp -o fill_test && ./fill_test
g++ -std=c++17 -include stub/prelude.h -Istub -I. co_test.cpp -o co_test && ./co_test
rm -f fill_test co_test co.cpp fill_body.inc stub/feature/cancel_object.h
