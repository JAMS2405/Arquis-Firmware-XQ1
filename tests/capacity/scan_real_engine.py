#!/usr/bin/env python3
"""Barrido EXHAUSTIVO de los 65 536 valores de int16_t (todo lo que acepta parser.value_int()) por el motor REAL de Marlin
(cancel_object.cpp + M486.cpp sin modificar). Uso: scan_real_engine.py <harness>"""
import subprocess
import sys
import tempfile
import os

H = sys.argv[1]
lines = []
for v in range(-32768, 32768):
    lines += ["M486 T30", "M486 P%d" % v, "G1 X1", "M486 T30", "M486 S%d" % v, "G1 X2"]
with tempfile.NamedTemporaryFile("w", suffix=".gcode", delete=False) as f:
    f.write("\n".join(lines))
out = subprocess.run([H, f.name, "-"], capture_output=True, text=True, check=True).stdout.split("\n")
os.unlink(f.name)
rows = [tuple(map(int, l.split())) for l in out if l]
assert len(rows) == 2 * 65536
ok_in = bad_in = alias_p = alias_s = inert_p = 0
design_ok = True
for i, v in enumerate(range(-32768, 32768)):
    (_, _, _, mask), (_, active, _, _) = rows[2 * i], rows[2 * i + 1]
    if 0 <= v <= 31:                                   # indices que el motor actual acepta (mascara de 32 bits)
        if mask == (1 << v) and active == v: ok_in += 1
        else: bad_in += 1
        if v < 30 and not (mask == (1 << v) and active == v): design_ok = False
    else:
        if mask != 0: alias_p += 1                     # P<v> cancelo OTRO objeto
        else: inert_p += 1
        if 0 <= active <= 31: alias_s += 1             # S<v> activo OTRO objeto (0..31)
print("MOTOR REAL (Marlin 2.1.2.10) sobre los 65 536 valores de int16_t:")
print("  indices 0..31: %d correctos, %d incorrectos; los 30 de diseno (0..29) correctos: %s" % (ok_in, bad_in, design_ok))
print("  valores fuera de 0..31: P<v> cancela OTRO objeto en %d, no hace nada en %d; S<v> activa OTRO objeto (0..31) en %d" % (alias_p, inert_p, alias_s))
assert design_ok and bad_in == 0
