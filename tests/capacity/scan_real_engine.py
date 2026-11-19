#!/usr/bin/env python3
"""Barrido EXHAUSTIVO de los 65 536 valores de int16_t (+ valores hostiles de 32 bits) por el motor REAL de Marlin.
Uso: scan_real_engine.py <harness> [stock|dev]
  stock: motor SIN modificar de la rama estable -> documenta el defecto (alias fuera de 0..31)
  dev  : motor de esta rama -> exige que SOLO 0..29 sean validos y que nada fuera de rango active ni cancele otro objeto"""
import os
import subprocess
import sys
import tempfile

H = sys.argv[1]
MODE = sys.argv[2] if len(sys.argv) > 2 else "stock"
HOSTILE32 = [65536, 65537, 65567, 65536 + 29, 131072, 2 ** 31 - 1, -2 ** 31, 2 ** 32, 2 ** 32 + 5, 2 ** 40 + 7, -65536, -65537]
VALUES = list(range(-32768, 32768)) + HOSTILE32
lines = []
for v in VALUES:
    lines += ["M486 T30", "M486 P%d" % v, "G1 X1", "M486 T30", "M486 S%d" % v, "G1 X2"]
with tempfile.NamedTemporaryFile("w", suffix=".gcode", delete=False) as f:
    f.write("\n".join(lines))
out = subprocess.run([H, f.name, "-"], capture_output=True, text=True, check=True).stdout.split("\n")
os.unlink(f.name)
rows = [tuple(map(int, l.split())) for l in out if l]
assert len(rows) == 2 * len(VALUES)
valid_hi = 31 if MODE == "stock" else 29          # el motor sin modificar acepta 0..31 (mascara de 32 bits); el nuevo solo 0..29
ok_in = bad_in = alias_p = alias_s = inert_p = inert_s = 0
for i, v in enumerate(VALUES):
    (_, _, _, mask), (_, active, skipping, _) = rows[2 * i], rows[2 * i + 1]
    if 0 <= v <= valid_hi:
        if mask == (1 << v) and active == v: ok_in += 1
        else: bad_in += 1
    else:
        if mask != 0: alias_p += 1                     # P<v> cancelo ALGUN objeto
        else: inert_p += 1
        if active >= 0: alias_s += 1                   # S<v> activo ALGUN objeto
        else: inert_s += 1
design_ok = all(rows[2 * i][3] == (1 << v) for i, v in enumerate(VALUES) if 0 <= v < 30)
print("MOTOR %s sobre los 65 536 valores de int16_t + %d valores hostiles de 32 bits:" % ("SIN MODIFICAR (rama estable)" if MODE == "stock" else "DE ESTA RAMA (Etapa 3)", len(HOSTILE32)))
print("  indices 0..%d: %d correctos, %d incorrectos; los 30 de diseno (0..29) correctos: %s" % (valid_hi, ok_in, bad_in, design_ok))
print("  fuera de 0..%d: P<v> cancela OTRO objeto en %d, ignorado en %d; S<v> activa ALGUN objeto (indice >= 0) en %d, ignorado en %d" % (valid_hi, alias_p, inert_p, alias_s, inert_s))
assert design_ok and bad_in == 0
if MODE == "dev":
    assert alias_p == 0 and alias_s == 0, "el motor nuevo no debe cancelar ni activar nada fuera de 0..29"
