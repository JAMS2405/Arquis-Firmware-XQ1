#!/usr/bin/env bash
# Estudio de capacidad: 30 objetos de diseno (IDs 0-29). Compila en carpetas temporales; no escribe en el firmware.
#   1) motor M486 REAL: el SIN modificar (tests/capacity/stock_marlin) y el de esta rama (Marlin/src): barrido exhaustivo de int16_t + valores hostiles de 32 bits
#   2) prototipo del motor endurecido para 30, manteniendo la mascara uint32_t
#   3) tamanos de las estructuras propuestas y del manifiesto
# Uso: tests/capacity/run.sh      (requiere g++ y python3)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
"$HERE/build_real_engine.sh" "$T/real" stock
python3 "$HERE/scan_real_engine.py" "$T/real/harness" stock
"$HERE/build_real_engine.sh" "$T/dev" dev
python3 "$HERE/scan_real_engine.py" "$T/dev/harness" dev
g++ -std=c++17 -O1 "$HERE/co30_prototype.cpp" -o "$T/co30" && "$T/co30"
g++ -std=gnu++17 -I"$HERE/../../Marlin/src/lcd/extui/mks_ui" "$HERE/sizes.cpp" -o "$T/sizes" && "$T/sizes"
python3 "$HERE/manifest_size.py"
