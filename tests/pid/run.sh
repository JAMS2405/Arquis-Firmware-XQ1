#!/usr/bin/env bash
# Pruebas del autoajuste PID. Uso: tests/pid/run.sh   (la pantalla: herramientas/arquis_ui/sim/build_pid_sim.sh)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; OUT="${TMPDIR:-/tmp}/arquis_pid_tests"; mkdir -p "$OUT"
MKS="$ROOT/Marlin/src/lcd/extui/mks_ui"; RC=0
echo "== 1/2 configuracion y codigo (estatico)"; python3 "$HERE/test_config.py" || RC=1
echo "== 2/2 logica pura REAL (ASan+UBSan)"
g++ -std=gnu++17 -Wall -Wextra -fsanitize=address,undefined -I"$MKS" "$HERE/pid_logic_test.cpp" "$MKS/arquis_pid_logic.cpp" -o "$OUT/logic" && "$OUT/logic" || RC=1
[ $RC = 0 ] && echo "TODO OK" || echo "HAY FALLOS"; exit $RC
