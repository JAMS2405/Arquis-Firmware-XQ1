#!/usr/bin/env bash
# Compila el motor de cancelacion REAL de Marlin con el arnes de tests/capacity. NO toca el firmware (solo lee).
# Uso: build_real_engine.sh <carpeta_salida> [stock|dev]
#   stock (por defecto): tests/capacity/stock_marlin/ = el motor SIN modificar de la rama estable (referencia "antes")
#   dev                : Marlin/src/... = el motor de esta rama de desarrollo (Etapa 3: rechazo de indices fuera de 0-29)
# Escribe <salida>/harness.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; OUT="$1"; WHICH="${2:-stock}"
if [ "$WHICH" = dev ]; then
  H="$ROOT/Marlin/src/feature/cancel_object.h"; C="$ROOT/Marlin/src/feature/cancel_object.cpp"; M="$ROOT/Marlin/src/gcode/feature/cancel/M486.cpp"
else
  H="$HERE/stock_marlin/cancel_object.h"; C="$HERE/stock_marlin/cancel_object.cpp"; M="$HERE/stock_marlin/M486.cpp"
fi
mkdir -p "$OUT/feature" "$OUT/gcode" "$OUT/lcd"
cp "$HERE/shim/prelude.h" "$OUT/"; cp "$HERE/shim/gcode/gcode.h" "$OUT/gcode/"; cp "$HERE/shim/lcd/marlinui.h" "$OUT/lcd/"
cp "$ROOT/shared/object_protocol/generated/arquis_object_limits.h" "$OUT/"
sed 's#"../../../shared/object_protocol/generated/arquis_object_limits.h"#"arquis_object_limits.h"#' "$H" > "$OUT/feature/cancel_object.h"
sed 's#"../inc/MarlinConfig.h"#"prelude.h"#;s#"cancel_object.h"#"feature/cancel_object.h"#;s#"../gcode/gcode.h"#"gcode/gcode.h"#;s#"../lcd/marlinui.h"#"lcd/marlinui.h"#' "$C" > "$OUT/co.cpp"
sed 's#"../../../inc/MarlinConfig.h"#"prelude.h"#;s#"../../gcode.h"#"gcode/gcode.h"#;s#"../../../feature/cancel_object.h"#"feature/cancel_object.h"#' "$M" > "$OUT/m486.cpp"
cp "$HERE/real_engine_harness.cpp" "$OUT/"
g++ -std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -I"$OUT" "$OUT/real_engine_harness.cpp" "$OUT/co.cpp" "$OUT/m486.cpp" -o "$OUT/harness"
