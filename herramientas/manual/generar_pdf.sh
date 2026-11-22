#!/usr/bin/env bash
# Genera docs/arquis/Manual_de_uso_ARQUIS_XQ1.pdf a partir de docs/arquis/MANUAL_DE_USO.md (con sus imagenes).
# Requisitos: pandoc (>= 2.19), Node.js con el modulo 'playwright' y Chromium (npx playwright install chromium), fuentes Inter y DejaVu.
# Uso: herramientas/manual/generar_pdf.sh
set -e
export LC_ALL=C.UTF-8 LANG=C.UTF-8   # los metadatos de la portada llevan acentos
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
DOCS="$ROOT/docs/arquis"; OUT="$DOCS/Manual_de_uso_ARQUIS_XQ1.pdf"; TMP="${TMPDIR:-/tmp}/arquis_manual"; mkdir -p "$TMP"
VER="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo sin-git)"
FECHA="$(date +%Y-%m-%d)"
pandoc "$DOCS/MANUAL_DE_USO.md" -f markdown -t html5 --standalone --embed-resources --toc --toc-depth=2 \
  --resource-path="$DOCS" --css "$HERE/manual.css" --metadata lang=es \
  --metadata pagetitle="Manual de uso ARQUIS XQ1" \
  --metadata title="Manual de uso" --metadata subtitle="Arquis XQ1 · pantalla táctil TS35 y ajustes de la impresora" \
  --metadata author="ARQUIS · Soluciones Tecnológicas" --metadata date="Versión del firmware $VER · $FECHA" \
  -o "$TMP/manual.html"
node "$HERE/imprimir_pdf.js" "$TMP/manual.html" "$OUT" "ARQUIS XQ1 — Manual de uso"
echo "PDF: $OUT ($(du -h "$OUT" | cut -f1))"
