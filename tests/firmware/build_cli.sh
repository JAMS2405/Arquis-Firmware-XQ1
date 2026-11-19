#!/usr/bin/env bash
# Compila el modulo REAL del firmware (arquis_objects.cpp) con el arnes de PC. Uso: build_cli.sh <salida> [san]   (san = ASan + UBSan)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"; OUT="$1"
MKS="$ROOT/Marlin/src/lcd/extui/mks_ui"
FLAGS="-O1 -std=gnu++17 -Wall -Wextra -Werror"
[ "$2" = san ] && FLAGS="$FLAGS -fsanitize=address,undefined -fno-sanitize-recover=undefined -g"
g++ $FLAGS -I"$MKS" "$HERE/arq_cli.cpp" "$MKS/arquis_objects.cpp" -o "$OUT"
