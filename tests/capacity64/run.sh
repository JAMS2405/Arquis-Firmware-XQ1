#!/usr/bin/env bash
# Estudio de capacidad 64 (docs/slicer_suite/02_capacidad_64.md). NO modifica el firmware: compila en una carpeta temporal.
#   1) motor M486 REAL (Marlin/src/feature/cancel_object.cpp) con stubs: alias de indices >= 32
#   2) prototipo del motor con capacidad 64 y rango validado: exhaustivo sobre los 65 536 valores de int16_t
#   3) tamanos de las estructuras propuestas
# Uso: tests/capacity64/run.sh      (requiere g++)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
cp -r "$ROOT/herramientas/pruebas/stub" "$T/stub"; mkdir -p "$T/stub/feature"
cp "$ROOT/Marlin/src/feature/cancel_object.h" "$T/stub/feature/"
sed 's#"../gcode/gcode.h"#"gcode/gcode.h"#;s#"../lcd/marlinui.h"#"lcd/marlinui.h"#;s#"cancel_object.h"#"feature/cancel_object.h"#;s#"../inc/MarlinConfig.h"#"prelude.h"#' \
  "$ROOT/Marlin/src/feature/cancel_object.cpp" > "$T/co.cpp"
cp "$HERE/stock_engine_scan.cpp" "$T/"
( cd "$T" && g++ -std=c++17 -include stub/prelude.h -Istub -I. stock_engine_scan.cpp -o stock && ./stock )
g++ -std=c++17 -O1 "$HERE/co64_prototype.cpp" -o "$T/co64" && "$T/co64"
g++ -std=c++17 "$HERE/sizes.cpp" -o "$T/sizes" && "$T/sizes"
python3 "$HERE/manifest_size.py"
