#!/usr/bin/env bash
# Pruebas del firmware unico XQ1 / Aquila D1. Uso: tests/machines/run.sh
# La pantalla (asistente y Ajustes > Maquina): herramientas/arquis_ui/sim/build_machine_sim.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; OUT="${TMPDIR:-/tmp}/arquis_machine_tests"; mkdir -p "$OUT"
MKS="$ROOT/Marlin/src/lcd/extui/mks_ui"; RC=0
echo "== 1/3 configuracion, tabla de perfiles y restricciones (estatico)"; python3 "$HERE/test_profiles.py" || RC=1
echo "== 2/3 logica pura REAL: perfiles, rangos, registro EEPROM (CRC, 2 copias, cortes, migracion) (ASan+UBSan)"
g++ -std=gnu++14 -Wall -Wextra -fsanitize=address,undefined -I"$MKS" "$HERE/machine_logic_test.cpp" "$MKS/arquis_machine_logic.cpp" -o "$OUT/logic" && "$OUT/logic" || RC=1
echo "== 3/3 pegamento REAL (arquis_machine.cpp) sobre una maquina simulada: EEPROM, TMC2209 con fallos, M502, M9050 (ASan+UBSan)"
T="$OUT/glue_tree"; rm -rf "$T"; mkdir -p "$T"; STUB="$ROOT/herramientas/arquis_ui/stub_machine"
cp -r "$STUB/Marlin" "$T/"; cp "$STUB"/arq_fake_hw.* "$T/"; D="$T/Marlin/src/lcd/extui/mks_ui"
cp "$MKS"/arquis_machine.h "$MKS"/arquis_machine.cpp "$MKS"/arquis_machine_logic.h "$MKS"/arquis_machine_logic.cpp "$D/"
g++ -std=gnu++17 -Wall -Wextra -Werror -Wno-extra -fsanitize=address,undefined -fno-sanitize-recover=undefined -I"$D" -I"$T" \
    "$HERE/glue_test.cpp" "$T/arq_fake_hw.cpp" "$D/arquis_machine.cpp" "$D/arquis_machine_logic.cpp" -o "$OUT/glue" && "$OUT/glue" || RC=1
[ $RC = 0 ] && echo "TODO OK" || echo "HAY FALLOS"; exit $RC
