#!/usr/bin/env bash
# Compila la interpolacion bilineal REAL de Marlin (texto extraido de bbl.cpp y planner.h) con un arnes. Uso: build_bilinear.sh <salida>
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; OUT="$1"; mkdir -p "$OUT"
python3 - "$ROOT" "$OUT" <<'PY'
import sys,re
root,out=sys.argv[1:3]
b=open(root+'/Marlin/src/feature/bedlevel/abl/bbl.cpp',encoding='utf-8').read()
i=b.index('#if ENABLED(ABL_BILINEAR_SUBDIVISION)\n  #define ABL_BG_SPACING')
j=b.index('#if IS_CARTESIAN && DISABLED(SEGMENT_LEVELED_MOVES)\n\n  #define CELL_INDEX')
body=b[i:j]
assert 'float LevelingBilinear::get_z_correction' in body
open(out+'/bbl_real.inc','w').write(body)
p=open(root+'/Marlin/src/module/planner.h',encoding='utf-8').read()
i=p.index('static float fade_scaling_factor_for_z(const_float_t rz) {')
k=p.index('\n      }\n',i)+len('\n      }\n')
fn=p[i:k].replace('static float fade_scaling_factor_for_z(const_float_t rz) {','float Planner::fade_scaling_factor_for_z(const float rz) {')
fn=fn.replace('static float z_fade_factor = 1;','static float z_fade_factor = 1;')
open(out+'/fade_real.inc','w').write(fn)
PY
cp "$HERE/bilinear_prelude.h" "$HERE/bilinear_harness.cpp" "$OUT/"
g++ -std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -I"$OUT" "$OUT/bilinear_harness.cpp" -o "$OUT/bilinear_harness"
