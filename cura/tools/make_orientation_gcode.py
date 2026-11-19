#!/usr/bin/env python3
"""Escribe el G-code de la prueba de orientacion fisica (5 piezas) con M486 y manifiesto ARQUIS.
Uso: python3 cura/tools/make_orientation_gcode.py [--bed 300x300] [--nozzle 200] [--bedtemp 60] [--out prueba_orientacion.gcode]"""
import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "plugins", "ARQUISPlugin"))
import arquis_bootstrap  # noqa: E402

arquis_bootstrap.load_engines()
from arquis_engines import orientation_test  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("--bed", default="300x300")
ap.add_argument("--nozzle", type=int, default=200)
ap.add_argument("--bedtemp", type=int, default=60)
ap.add_argument("--out", default="prueba_orientacion.gcode")
a = ap.parse_args()
w, d = (float(v) for v in a.bed.lower().split("x"))
res = orientation_test.make(w, d, nozzle=a.nozzle, bed_temp=a.bedtemp)
if res.status != "ok":
    print(res.summary())
    sys.exit(1)
with open(a.out, "w", encoding="utf-8", newline="\r\n") as f:                         # CRLF, como exporta Cura en Windows
    f.write("".join(res.chunks))
print("escrito %s: 5 piezas, M486 T5, manifiesto %d objetos" % (a.out, len(res.manifest.objects)))
