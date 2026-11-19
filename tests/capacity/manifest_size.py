"""Tamano REAL del manifiesto ARQUIS_OBJECTS_V1 con valores de longitud maxima, generado con el motor de referencia de Cura."""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "cura", "plugins", "ARQUISPlugin"))
import arquis_bootstrap  # noqa: E402

arquis_bootstrap.load_engines()
import arquis_engines.manifest_engine as me  # noqa: E402
import arquis_engines.protocol_limits as L  # noqa: E402


def make(n, names):
    m = me.Manifest(bed_w=3000, bed_d=3000, cap=L.DESIGN_CAPACITY, names=list(names))
    for i in range(n):                                   # valores de maxima anchura
        m.objects.append(me.ManifestObject(i, i % len(names), i // len(names), 2999, 2999, 3000, 3000, 359, "R", 4500, 4500, i))
    m.pid = 0xFFFFFFFF
    h, b = me.render_block(m)
    return h + b


worst = make(L.DESIGN_CAPACITY, ["N" * L.NAME_MAX_BYTES] * L.DESIGN_CAPACITY)
typical = make(L.DESIGN_CAPACITY, ["Llavero", "Soporte", "Tapa"])
print("manifiesto peor caso (%d objetos, %d nombres de %d B): %d B, linea mas larga %d B" % (L.DESIGN_CAPACITY, L.DESIGN_CAPACITY, L.NAME_MAX_BYTES, len(worst), max(map(len, worst.split("\n")))))
print("manifiesto tipico (%d piezas, 3 nombres): %d B" % (L.DESIGN_CAPACITY, len(typical)))
assert len(worst) < 2800 and len(typical) < 2000
