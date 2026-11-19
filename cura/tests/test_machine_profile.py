# SPDX-License-Identifier: LGPL-3.0-or-later
"""Perfil REAL de la maquina ARQUIS (300 x 300 mm, la del firmware) frente a los G-code de muestra (perfil CR-10 Max, 450 x 450 mm).

Regla: un manifiesto con la cama equivocada NUNCA habilita el mapa ni la cancelacion visual:
  * Cura/plugin: el script no genera el manifiesto y la ventana del gestor no dibuja el mapa;
  * firmware: el lector lo rechaza (E_BED_MISMATCH) => sin mapa y sin cancelacion desde la pantalla.
Pruebas SIMULADAS en el PC (G-code sintetico y real de Cura 5.11); no prueban Cura ni la impresora."""
import json
import os
import re
import shutil
import subprocess
import sys

import pytest

from arquis_engines import machine_profile as mp
from arquis_engines import manifest_engine as me
from arquis_engines import object_manager_model as omm
from arquis_engines.m486_engine import M486Options, process_gcode
from fixtures import grid_objects, instances_from, make_gcode
from real_gcode import derive_scene, load_reduced, split_like_cura

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FW = os.path.join(ROOT, "tests", "firmware")


def _cfg(name):
    t = open(os.path.join(ROOT, "Marlin", "Configuration.h"), encoding="utf-8").read()
    m = re.search(r"^#define\s+%s\s+(\d+)\s*$" % name, t, re.M)
    assert m, name
    return int(m.group(1))


def test_machine_profile_is_exactly_what_the_firmware_is_configured_for():
    j = json.load(open(os.path.join(ROOT, "shared", "machines", "arquis_xq1.json"), encoding="utf-8"))
    assert (j["bed_x_mm"], j["bed_y_mm"], j["height_z_mm"], j["min_x_mm"], j["min_y_mm"]) == \
           (_cfg("X_BED_SIZE"), _cfg("Y_BED_SIZE"), _cfg("Z_MAX_POS"), _cfg("X_MIN_POS"), _cfg("Y_MIN_POS")) == (300, 300, 450, 0, 0)
    assert (mp.BED_X_MM, mp.BED_Y_MM) == (300, 300) and mp.CENTER_IS_ZERO is False


def test_orientation_is_confirmed_by_the_owner():
    # Confirmado por el propietario el 2026-11-19 (observacion de la impresion de 6 piezas), no con la prueba formal de 5 piezas.
    j = json.load(open(os.path.join(ROOT, "shared", "machines", "arquis_xq1.json"), encoding="utf-8"))
    assert mp.ORIGIN == "FL" and mp.ORIGIN_CONFIRMED is True and j["origin_confirmed"] is True and j["origin"] == "FL"


def test_cura_script_defaults_to_the_real_machine_bed():
    src = open(os.path.join(ROOT, "cura", "scripts", "ARQUIS_M486.py"), encoding="utf-8").read()
    for key, val in (("bed_x", mp.BED_X_MM), ("bed_y", mp.BED_Y_MM)):
        m = re.search(r'"%s":\s*\{.*?"default_value":\s*([0-9.]+)' % key, src, re.S)
        assert m and float(m.group(1)) == val, key


def _arquis_gcode():
    objs = grid_objects(30, "Llavero", w=20, d=20, gap=6, bed=300.0)
    assert all(o.x1 <= 300 and o.y1 <= 300 for o in objs)
    return make_gcode(objs), instances_from(objs)


def test_arquis_profile_300_generates_a_manifest_the_plugin_accepts():
    chunks, inst = _arquis_gcode()
    res = process_gcode(chunks, inst, M486Options(bed=(0, 0, 300, 300), expected_bed=(300, 300)))
    assert res.status == "ok" and res.manifest is not None
    text = "".join(res.chunks)
    assert me.parse_text(text, (3000, 3000)).ok


def test_wrong_bed_profile_never_produces_a_manifest():
    chunks, inst = _arquis_gcode()
    res = process_gcode(chunks, inst, M486Options(bed=(0, 0, 450, 450), expected_bed=(300, 300)))     # perfil CR-10 Max con la maquina ARQUIS
    assert res.status == "ok"                                                                          # el M486 se genera igual (no depende de la cama)
    assert res.manifest is None and any(p.code == "W_MANIFEST_OMITTED" for p in res.problems)
    assert me.parse_text("".join(res.chunks), (3000, 3000)).manifest is None


@pytest.mark.skipif(shutil.which("g++") is None, reason="necesita g++")
@pytest.mark.parametrize("name", ["real_3_distintos", "real_4_iguales"])
def test_firmware_refuses_the_real_450_gcode_on_the_300_machine(tmp_path, name):
    out = str(tmp_path / "arq_cli")
    subprocess.run([os.path.join(FW, "build_cli.sh"), out, "san"], check=True, capture_output=True)
    chunks = split_like_cura(load_reduced(name))
    res = process_gcode(chunks, derive_scene(chunks), M486Options(bed=(0, 0, 450, 450)))              # manifiesto FORZADO con la cama del perfil CR-10 Max
    p = tmp_path / "f.gcode"
    p.write_text("".join(res.chunks), encoding="utf-8")
    r = json.loads(subprocess.run([out, "parse", str(p), "512", str(mp.BED_X_MM * 10), str(mp.BED_Y_MM * 10)], capture_output=True, text=True, check=True).stdout)
    assert r["status"] == "bad" and r["errors"] == ["E_BED_MISMATCH"] and "objects" not in r        # sin tabla de objetos: ni mapa ni cancelacion visual
    ok = json.loads(subprocess.run([out, "parse", str(p), "512", "4500", "4500"], capture_output=True, text=True, check=True).stdout)
    assert ok["status"] == "ok"                                                                       # (el mismo archivo SI valdria en una maquina de 450)


def test_object_manager_does_not_draw_a_map_for_an_incompatible_bed():
    inst = instances_from(grid_objects(12, "Llavero", w=20, d=20, gap=6, bed=450.0))
    from arquis_engines.object_engine import build_objects
    objs = build_objects(inst, 30)
    good = omm.build_view_model(objs, (0.0, 0.0, 300.0, 300.0))
    assert good["mapValid"] and good["markers"] and good["bedMismatch"] == ""
    bad = omm.build_view_model(objs, (0.0, 0.0, 450.0, 450.0))                                         # perfil CR-10 Max
    assert not bad["mapValid"] and bad["markers"] == [] and bad["grid"] == [] and bad["_markers"] == []
    assert "450 x 450" in bad["bedMismatch"] and "300 x 300" in bad["bedMismatch"] and bad["warnings"][0] == bad["bedMismatch"]
    assert all(r["pos"] == "-" for r in bad["rows"]) and len(bad["rows"]) == 12                         # la lista (identificacion) sigue; las posiciones no
    assert omm.build_view_model(objs, (0.0, 0.0, 450.0, 450.0), machine_bed=None)["mapValid"]          # solo si se pide no comprobar (pruebas)
