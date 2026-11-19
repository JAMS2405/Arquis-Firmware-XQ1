# SPDX-License-Identifier: LGPL-3.0-or-later
"""El diagnostico dentro de Cura (``cura_diagnostics``) y la herramienta que lo verifica (``tools/verify_cura_capture.py``).

IMPORTANTE: estas pruebas usan NODOS FALSOS. Prueban que la herramienta detecta errores y que NUNCA da por validada la API de Cura con un
volcado ``fake``. No validan Cura: eso lo hace el usuario ejecutando el plugin en Cura 5.11 (cura/VALIDACION_EN_CURA.md)."""
import contextlib
import copy
import importlib.util
import io
import json
import os
from types import SimpleNamespace

import pytest

import arquis_bootstrap
from arquis_engines.m486_engine import M486Options, process_gcode
from arquis_engines.object_engine import SceneInstance
from fixtures import grid_objects, instances_from, make_gcode

arquis_bootstrap.load_adapters()
import arquis_adapters.cura_diagnostics as diag  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
_spec = importlib.util.spec_from_file_location("verify_cura_capture", os.path.join(ROOT, "cura", "tools", "verify_cura_capture.py"))
verify = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(verify)


class Box:
    def __init__(self, left, right, front, back, top=10.0, bottom=0.0):
        self.left, self.right, self.front, self.back, self.top, self.bottom = left, right, front, back, top, bottom


class Node:
    """Nodo falso con TODA la superficie de API que lee el diagnostico (para que no haya errores de API espurios)."""
    def __init__(self, name, box, deco=None, parent=None):
        self._name, self._box, self._deco, self._parent = name, box, deco or {}, parent
        self._outside_buildarea = False
        self._mesh = SimpleNamespace(getVertices=lambda: [1])

    def getName(self): return self._name
    def getBoundingBox(self): return self._box
    def getMeshData(self): return self._mesh
    def getParent(self): return self._parent
    def getWorldPosition(self): return SimpleNamespace(x=(self._box.left + self._box.right) / 2, y=0.0, z=(self._box.front + self._box.back) / 2)
    def getWorldOrientation(self): return SimpleNamespace(w=1.0, x=0.0, y=0.0, z=0.0)
    def callDecoration(self, n, *a):
        d = {"isSliceable": True, "isNonPrintingMesh": False, "getBuildPlateNumber": 0, "isGroup": False, "getConvexHull": None}
        d.update(self._deco)
        return d.get(n)


def scene_nodes(objs, W=300.0, D=300.0):
    """Cajas de escena de Cura que corresponden a las cajas de G-code ``objs`` (inversa de x = X + W/2, y = -Z + D/2)."""
    names = {}
    out = []
    for o in objs:
        k = names.get(o.name, 0)
        names[o.name] = k + 1
        out.append(Node(o.name if k == 0 else "%s(%d)" % (o.name, k), Box(o.x0 - W / 2, o.x1 - W / 2, D / 2 - o.y0, D / 2 - o.y1)))
    return out


def make_dump(objs, W=300.0, D=300.0, source="fake"):
    env = {"plugin_version": "0.2.0", "cura": {"version": "5.11.0"}}
    return diag.build_dump(scene_nodes(objs, W, D), W, D, False, 0, env, source)


def run_tool(tmp_path, dump, gcode_text=None, extra=()):
    jp = tmp_path / "cap.json"
    jp.write_text(json.dumps(dump), encoding="utf-8")
    args = [str(jp)]
    if gcode_text is not None:
        gp = tmp_path / "out.gcode"
        gp.write_text(gcode_text, encoding="utf-8")
        args.append(str(gp))
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = verify.main(args + list(extra))
    return rc, buf.getvalue()


def exported(objs, bed=(0, 0, 300, 300), expected=(300, 300)):
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options(bed=bed, expected_bed=expected))
    assert res.status == "ok"
    return "".join(res.chunks)


def objs_with_copies():
    return grid_objects(4, "pinon.stl", w=20, d=20, gap=10) + [
        type(o)(**{**o.__dict__, "name": "Soporte"}) for o in grid_objects(2, "x", w=30, d=15, gap=10, y_start=100)]


def test_the_dump_of_a_scene_has_raw_cura_numbers_and_no_api_errors():
    d = make_dump(objs_with_copies())
    assert d["schema"] == diag.SCHEMA and d["source"] == "fake" and not d["errors"] and all(not n["errors"] for n in d["nodes"])
    assert len(d["nodes"]) == len(d["instances"]) == 6 and d["nodes"][0]["bbox_scene"]["left"] is not None
    assert [n["name"] for n in d["nodes"]][:3] == ["pinon.stl", "pinon.stl(1)", "pinon.stl(2)"]          # Cura renombra las copias


def test_an_api_failure_is_reported_instead_of_hidden():
    class Broken(Node):
        def getConvexHull(self): raise RuntimeError("x")
        def callDecoration(self, n, *a):
            if n == "getConvexHull":
                raise AttributeError("sin huella")
            return super().callDecoration(n, *a)
    d = diag.build_dump([Broken("A", Box(0, 10, 10, 0))], 300, 300, False, 0, {}, "fake")
    assert any("getConvexHull" in e for e in d["nodes"][0]["errors"])


def test_tool_passes_a_consistent_scene_with_its_gcode_and_never_validates_a_fake_dump(tmp_path):
    objs = objs_with_copies()
    rc, out = run_tool(tmp_path, make_dump(objs), exported(objs))
    assert rc == 0 and "FALLA" not in out.replace("FALLA (ver", ""), out
    assert "pinon x4" in out and "NO VALIDA la integracion con Cura" in out
    assert "PENDIENTE" in out and "orientacion" in out.lower()


def test_tool_declares_validation_only_for_a_real_dump_with_nothing_unexercised(tmp_path):
    objs = objs_with_copies()
    rc, out = run_tool(tmp_path, make_dump(objs, source="cura"), exported(objs))                      # (solo prueba la logica de la herramienta)
    assert rc == 0 and "VALIDADO EN CURA REAL" in out
    rc, out = run_tool(tmp_path, make_dump(grid_objects(3, "A", w=20, d=20, gap=10), source="cura"))   # sin copias y sin G-code: queda sin ejercitar
    assert "VALIDADO EN CURA REAL" not in out and "NO EJERCITADO" in out and "SIN EJERCITAR" in out


def test_tool_detects_a_wrong_axis_conversion(tmp_path):
    objs = objs_with_copies()
    d = make_dump(objs)
    d["instances"][2]["bbox"][1] += 5.0; d["instances"][2]["bbox"][3] += 5.0                          # el plugin habria capturado una Y desplazada
    rc, out = run_tool(tmp_path, d)
    assert rc == 1 and "[FALLA" in out and "conversion de ejes" in out


def test_tool_detects_copies_with_the_same_box_and_a_missing_instance(tmp_path):
    objs = objs_with_copies()
    d = make_dump(objs)
    d["instances"][1]["bbox"] = list(d["instances"][0]["bbox"])
    rc, out = run_tool(tmp_path, d)
    assert rc == 1 and "MISMA caja" in out
    d2 = make_dump(objs)
    d2["instances"].pop()
    rc, out = run_tool(tmp_path, d2)
    assert rc == 1 and "una instancia por malla imprimible" in out


def test_tool_blocks_the_450_profile_on_the_300_machine(tmp_path):
    objs = grid_objects(6, "A", w=30, d=30, gap=20, bed=450.0)
    d = make_dump(objs, 450.0, 450.0)
    rc, out = run_tool(tmp_path, d)
    assert rc == 1 and "450 x 450" in out and "Perfil de Cura = maquina ARQUIS" in out
    gcode = exported(objs, bed=(0, 0, 450, 450), expected=None)                                         # manifiesto con la cama equivocada
    rc, out = run_tool(tmp_path, make_dump(objs), gcode)
    assert rc == 1 and "E_BED_MISMATCH" in out


def test_tool_detects_a_manifest_that_does_not_match_the_cura_scene(tmp_path):
    objs = objs_with_copies()
    shifted = [type(o)(**{**o.__dict__, "x0": o.x0 + 8, "x1": o.x1 + 8}) for o in objs]
    gcode = exported(shifted)                                                                           # el G-code/manifiesto sale de OTRA disposicion
    rc, out = run_tool(tmp_path, make_dump(objs), gcode)
    assert rc == 1 and "Manifiesto = escena capturada" in out and "FALLA" in out


def test_tool_reports_a_missing_arquis_marker(tmp_path):
    objs = objs_with_copies()
    rc, out = run_tool(tmp_path, make_dump(objs), "".join(make_gcode(objs)))                           # el script ARQUIS_M486 no se ejecuto
    assert rc == 1 and "sin marca" in out


def test_dump_is_json_serializable_and_roundtrips(tmp_path):
    d = make_dump(objs_with_copies())
    p = diag.save_dump(d, str(tmp_path / "x.json"))
    assert json.load(open(p, encoding="utf-8"))["instances"] == json.loads(json.dumps(d))["instances"]
