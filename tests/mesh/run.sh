#!/usr/bin/env bash
# Pruebas de la nivelacion manual 5x5. Uso: tests/mesh/run.sh   (no necesita Marlin compilado ni LVGL)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; OUT="${TMPDIR:-/tmp}/arquis_mesh_tests"; mkdir -p "$OUT"
MKS="$ROOT/Marlin/src/lcd/extui/mks_ui"; RC=0
echo "== 1/3 configuracion y codigo (estatico)"; python3 "$HERE/test_config.py" || RC=1
echo "== 2/3 logica pura REAL (ASan+UBSan)"
g++ -std=gnu++17 -Wall -Wextra -fsanitize=address,undefined -I"$MKS" "$HERE/mesh_logic_test.cpp" "$MKS/arquis_mesh_logic.cpp" -o "$OUT/logic" && "$OUT/logic" || RC=1
echo "== 3/3 interpolacion bilineal y fade REALES de Marlin"
"$HERE/build_bilinear.sh" "$OUT/bil" && "$OUT/bil/bilinear_harness" || RC=1
[ $RC = 0 ] && echo "TODO OK" || echo "HAY FALLOS"
exit $RC
