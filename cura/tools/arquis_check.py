#!/usr/bin/env python3
"""Comprueba un G-code YA EXPORTADO por Cura: cancelacion de objetos (M486) y manifiesto, tal como los vera el firmware.
Lee el manifiesto con la misma ventana de busqueda que el firmware (cabecera: 512 B iniciales; bloque: 16 KB finales) y lo contrasta
con el G-code. Uso: python3 cura/tools/arquis_check.py archivo.gcode [--bed 300x300] [--capacity 30]. Codigo de salida 0 = todo correcto."""
import argparse
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "plugins", "ARQUISPlugin"))
import arquis_bootstrap  # noqa: E402

arquis_bootstrap.load_engines()
from arquis_engines import manifest_engine as me  # noqa: E402
from arquis_engines import protocol_limits as limits  # noqa: E402
from arquis_engines.validation_engine import validate_m486, validate_manifest_vs_gcode  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument("file")
ap.add_argument("--bed", help="cama de la maquina, p. ej. 300x300 (mm)")
ap.add_argument("--capacity", type=int, default=limits.DESIGN_CAPACITY)
a = ap.parse_args()
raw = open(a.file, "rb").read()
text = raw.decode("utf-8", "replace")
bed = tuple(int(round(float(v) * 10)) for v in a.bed.lower().split("x")) if a.bed else None
rc = 0
print("archivo: %s (%d bytes, %s)" % (a.file, len(raw), "CRLF" if b"\r\n" in raw else "LF"))
pr = me.parse_text(text, bed)
if pr.manifest is None and all(p.code == "W_NO_MANIFEST" for p in pr.problems):
    print("manifiesto: NO HAY (G-code normal o antiguo: se imprime igual, sin mapa de objetos)")
elif not pr.ok:
    rc = 1
    print("manifiesto: INVALIDO -> sin mapa ni cancelacion visual")
    for p in pr.problems:
        print("  ", p)
else:
    m = pr.manifest
    print("manifiesto: OK, %d objetos, cama %.0f x %.0f mm, PID %08X, %s" % (len(m.objects), m.bed_w / 10, m.bed_d / 10, m.pid, "uno a uno" if m.seq else "capa a capa"))
    lf = text.replace("\r\n", "\n")
    chunks = [lf]
    probs = validate_m486(chunks, a.capacity) + validate_manifest_vs_gcode(chunks, m)
    for p in probs:
        print("  ", p)
    rc = 1 if any(p.severity == "error" for p in probs) else 0
    print("M486 y sincronizacion con el G-code:", "CORRECTO" if rc == 0 else "FALLA")
sys.exit(rc)
