#!/usr/bin/env bash
# Estudio de capacidad: 30 objetos de diseno (IDs 0-29). NO modifica el firmware: compila en una carpeta temporal.
#   1) motor M486 REAL de Marlin (cancel_object.cpp + M486.cpp sin modificar): barrido exhaustivo de los 65 536 valores de int16_t
#   2) prototipo del motor endurecido para 30, manteniendo la mascara uint32_t
#   3) tamanos de las estructuras propuestas y del manifiesto
# Uso: tests/capacity/run.sh      (requiere g++ y python3)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
"$HERE/build_real_engine.sh" "$T/real"
python3 "$HERE/scan_real_engine.py" "$T/real/harness"
g++ -std=c++17 -O1 "$HERE/co30_prototype.cpp" -o "$T/co30" && "$T/co30"
g++ -std=c++17 "$HERE/sizes.cpp" -o "$T/sizes" && "$T/sizes"
python3 "$HERE/manifest_size.py"
