# SPDX-License-Identifier: LGPL-3.0-or-later
import os
import subprocess
import sys

import arquis_engines.manifest_engine as me
import arquis_engines.orientation_test as ot
from arquis_engines import bed_map_engine as bm
from arquis_engines.marlin_model import simulate
from arquis_engines.validation_engine import validate_manifest_vs_gcode
from helpers import check_cancel_k, flat

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TOOLS = os.path.join(ROOT, "tools")


def test_orientation_test_pieces_are_in_the_five_zones_and_the_map_agrees():
    res = ot.make(300.0, 300.0)
    assert res.status == "ok" and res.manifest is not None, res.summary()
    m = res.manifest
    assert [o.id for o in m.objects] == [0, 1, 2, 3, 4] and m.names[0] == "Frontal izquierda"
    c = {o.id: (o.cx / 10, o.cy / 10) for o in m.objects}
    assert c[0][0] < 150 > c[0][1] and c[1][0] > 150 > c[1][1]                      # frente = Y bajo
    assert c[3][0] < 150 < c[3][1] and c[4][0] > 150 < c[4][1]                      # atras = Y alto
    assert abs(c[2][0] - 150) < 1 and abs(c[2][1] - 150) < 1
    assert sorted(o.w for o in m.objects) == [200, 220, 240, 260, 280]               # un tamano distinto por pieza
    bed, view = bm.Bed(0, 0, 3000, 3000), bm.View(6, 36, 266, 266)
    px = {o.id: bm.to_px(o.cx, o.cy, bed, view) for o in m.objects}
    assert px[0][1] > px[3][1] and px[1][1] > px[4][1]                               # en pantalla: frente ABAJO, atras ARRIBA
    assert px[0][0] < px[1][0] and px[3][0] < px[4][0]                               # izquierda a la izquierda
    assert validate_manifest_vs_gcode(res.chunks, m) == []
    for k in range(5):
        check_cancel_k(res.chunks, k)


def test_orientation_test_checks_the_bed_and_runs_on_other_sizes():
    assert ot.make(220.0, 220.0).status == "ok"
    assert ot.make(450.0, 450.0).manifest.bed_w == 4500


def test_cli_tools_end_to_end(tmp_path):
    out = tmp_path / "orient.gcode"
    r = subprocess.run([sys.executable, os.path.join(TOOLS, "make_orientation_gcode.py"), "--out", str(out)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    raw = out.read_bytes()
    assert b"\r\n" in raw and b";ARQUIS_OBJECTS_BEGIN" in raw                          # exportado con CRLF, como Cura en Windows
    c = subprocess.run([sys.executable, os.path.join(TOOLS, "arquis_check.py"), str(out), "--bed", "300x300"], capture_output=True, text=True)
    assert c.returncode == 0, c.stdout + c.stderr
    assert "manifiesto: OK, 5 objetos" in c.stdout and "CORRECTO" in c.stdout
    bad = tmp_path / "bad.gcode"
    bad.write_bytes(raw.replace(b";ARQ:O 0 0 0", b";ARQ:O 0 0 1", 1))
    assert subprocess.run([sys.executable, os.path.join(TOOLS, "arquis_check.py"), str(bad)], capture_output=True, text=True).returncode == 1
    wrongbed = subprocess.run([sys.executable, os.path.join(TOOLS, "arquis_check.py"), str(out), "--bed", "220x220"], capture_output=True, text=True)
    assert wrongbed.returncode == 1 and "E_BED_MISMATCH" in wrongbed.stdout
    plain = tmp_path / "plain.gcode"
    plain.write_text(";FLAVOR:Marlin\nG28\nG1 X1 Y1\n", encoding="utf-8")
    p = subprocess.run([sys.executable, os.path.join(TOOLS, "arquis_check.py"), str(plain)], capture_output=True, text=True)
    assert p.returncode == 0 and "NO HAY" in p.stdout
