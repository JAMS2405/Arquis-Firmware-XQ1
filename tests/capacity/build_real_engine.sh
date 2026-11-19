#!/usr/bin/env bash
# Compila el motor de cancelacion REAL de Marlin (sin modificarlo) con el arnes de tests/capacity. Uso: build_real_engine.sh <carpeta_salida>
# Escribe <salida>/harness. NO toca el firmware (solo lee Marlin/src).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; OUT="$1"
mkdir -p "$OUT/feature"
cp "$HERE/shim/prelude.h" "$OUT/"; mkdir -p "$OUT/gcode" "$OUT/lcd"; cp "$HERE/shim/gcode/gcode.h" "$OUT/gcode/"; cp "$HERE/shim/lcd/marlinui.h" "$OUT/lcd/"
cp "$ROOT/Marlin/src/feature/cancel_object.h" "$OUT/feature/"
sed 's#"../inc/MarlinConfig.h"#"prelude.h"#;s#"cancel_object.h"#"feature/cancel_object.h"#;s#"../gcode/gcode.h"#"gcode/gcode.h"#;s#"../lcd/marlinui.h"#"lcd/marlinui.h"#' \
  "$ROOT/Marlin/src/feature/cancel_object.cpp" > "$OUT/co.cpp"
sed 's#"../../../inc/MarlinConfig.h"#"prelude.h"#;s#"../../gcode.h"#"gcode/gcode.h"#;s#"../../../feature/cancel_object.h"#"feature/cancel_object.h"#' \
  "$ROOT/Marlin/src/gcode/feature/cancel/M486.cpp" > "$OUT/m486.cpp"
cp "$HERE/real_engine_harness.cpp" "$OUT/"
g++ -std=c++17 -O1 -I"$OUT" "$OUT/real_engine_harness.cpp" "$OUT/co.cpp" "$OUT/m486.cpp" -o "$OUT/harness"
