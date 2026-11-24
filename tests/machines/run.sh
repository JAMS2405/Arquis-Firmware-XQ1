#!/usr/bin/env bash
# Pruebas del firmware unico XQ1 / Aquila D1. Uso: tests/machines/run.sh
# La pantalla (asistente y Ajustes > Maquina): herramientas/arquis_ui/sim/build_machine_sim.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; OUT="${TMPDIR:-/tmp}/arquis_machine_tests"; mkdir -p "$OUT"
MKS="$ROOT/Marlin/src/lcd/extui/mks_ui"; RC=0
echo "== 1/2 configuracion, tabla de perfiles y restricciones (estatico)"; python3 "$HERE/test_profiles.py" || RC=1
echo "== 2/2 logica pura REAL: perfiles, rangos, registro EEPROM (CRC, 2 copias, cortes, migracion) (ASan+UBSan)"
g++ -std=gnu++14 -Wall -Wextra -fsanitize=address,undefined -I"$MKS" "$HERE/machine_logic_test.cpp" "$MKS/arquis_machine_logic.cpp" -o "$OUT/logic" && "$OUT/logic" || RC=1
[ $RC = 0 ] && echo "TODO OK" || echo "HAY FALLOS"; exit $RC
