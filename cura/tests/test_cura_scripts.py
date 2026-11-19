# SPDX-License-Identifier: LGPL-3.0-or-later
"""Los scripts de post-procesado se cargan como los carga Cura (modulo dentro de un paquete con ``Script``) y se ejecutan con una escena falsa."""
import importlib.util
import json
import os
import re
import sys
import types

import pytest

from fixtures import Obj, grid_objects, instances_from, make_gcode

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PLUGIN = os.path.join(ROOT, "plugins", "ARQUISPlugin")


class FakeScript:
    """Imita UM/Script de Cura: valores de los ajustes por defecto del JSON del propio script."""
    settings = {}

    def getSettingValueByKey(self, key):
        d = json.loads(self.getSettingDataString())["settings"][key]
        return self.settings.get(key, d["default_value"])


def load_script(name):
    pkg = types.ModuleType("fakepp")
    pkg.__path__ = []
    sub = types.ModuleType("fakepp.sub")
    sub.__path__ = []
    mod = types.ModuleType("fakepp.Script")
    mod.Script = FakeScript
    sys.modules.update({"fakepp": pkg, "fakepp.sub": sub, "fakepp.Script": mod})
    spec = importlib.util.spec_from_file_location("fakepp.sub." + name, os.path.join(ROOT, "scripts", name + ".py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


@pytest.fixture(autouse=True)
def plugin_path(monkeypatch):
    monkeypatch.setenv("ARQUIS_PLUGIN_PATH", PLUGIN)


def test_scripts_settings_are_valid_json_and_have_key_equal_to_class_name():
    for name in ("ARQUIS_M486", "ARQUIS_Validation"):
        m = load_script(name)
        s = getattr(m, name)()
        d = json.loads(s.getSettingDataString())
        assert d["key"] == name and "name" in d and d["settings"]["capacity"]["default_value"] == 30


def test_m486_script_end_to_end_with_fake_scene(monkeypatch):
    m = load_script("ARQUIS_M486")
    objs = grid_objects(6, "Llavero", height=1.0)
    _, scene = m._load_arquis()
    monkeypatch.setattr(scene, "capture_scene", lambda: instances_from(objs))
    monkeypatch.setattr(scene, "capture_bed", lambda: (0.0, 0.0, 300.0, 300.0))
    out = m.ARQUIS_M486().execute(make_gcode(objs))
    text = "\n".join(out)
    assert text.startswith(";ARQUIS_M486 V=1 N=6 CAP=30 STATUS=OK") and "M486 T6" in text


def test_m486_script_never_breaks_export_when_scene_fails(monkeypatch):
    m = load_script("ARQUIS_M486")
    _, scene = m._load_arquis()

    def boom():
        raise RuntimeError("sin escena")
    monkeypatch.setattr(scene, "capture_scene", boom)
    data = make_gcode(grid_objects(2, "A", height=1.0))
    assert m.ARQUIS_M486().execute(list(data)) == data           # G-code intacto


def test_m486_script_blocks_and_leaves_gcode_without_m486(monkeypatch):
    m = load_script("ARQUIS_M486")
    _, scene = m._load_arquis()
    objs = grid_objects(3, "A", height=1.0)
    wrong = [Obj(o.name, o.x0 + 20, o.y0, o.x1 + 20, o.y1, o.height) for o in objs]
    monkeypatch.setattr(scene, "capture_scene", lambda: instances_from(wrong))
    monkeypatch.setattr(scene, "capture_bed", lambda: (0.0, 0.0, 300.0, 300.0))
    out = "\n".join(m.ARQUIS_M486().execute(make_gcode(objs)))
    assert "STATUS=BLOCKED" in out and "\nM486 " not in out


def test_validation_script_does_not_modify_gcode():
    m = load_script("ARQUIS_Validation")
    data = make_gcode(grid_objects(2, "A", height=1.0))
    assert m.ARQUIS_Validation().execute(list(data)) == data


def test_m486_script_emits_manifest_and_omits_it_when_the_cura_bed_is_not_the_firmware_bed(monkeypatch):
    m = load_script("ARQUIS_M486")
    objs = grid_objects(5, "Llavero", height=1.0)
    _, scene = m._load_arquis()
    monkeypatch.setattr(scene, "capture_scene", lambda: instances_from(objs))
    monkeypatch.setattr(scene, "capture_bed", lambda: (0.0, 0.0, 300.0, 300.0))
    text = "".join(m.ARQUIS_M486().execute(make_gcode(objs)))
    assert ";ARQUIS_OBJECTS_BEGIN" in text and ";ARQUIS_TRAILER" in text
    monkeypatch.setattr(scene, "capture_bed", lambda: (0.0, 0.0, 450.0, 450.0))        # perfil de Cura CR-10 Max, firmware 300x300
    text2 = "".join(m.ARQUIS_M486().execute(make_gcode(objs)))
    assert "M486 T5" in text2 and ";ARQUIS_OBJECTS_BEGIN" not in text2                   # M486 si, mapa no
    FakeScript.settings = {"emit_manifest": False}
    try:
        assert ";ARQUIS_OBJECTS_BEGIN" not in "".join(m.ARQUIS_M486().execute(make_gcode(objs)))
    finally:
        FakeScript.settings = {}


# ---------------------------------------------------------------- ARQUIS_NonPlanar (ondulado)
def _np_setup(monkeypatch, tmp_path, **settings):
    monkeypatch.setenv("ARQUIS_PLUGIN_PATH", PLUGIN)
    monkeypatch.setenv("HOME", str(tmp_path)); monkeypatch.setenv("USERPROFILE", str(tmp_path))
    m = load_script("ARQUIS_NonPlanar")
    sc = m.ARQUIS_NonPlanar()
    sc.settings = dict(settings)
    notes = []
    m._notify = lambda text, level: notes.append((level, text))
    return m, sc, notes


def _cura_chunks(layers=24):
    sys.path.insert(0, os.path.dirname(__file__))
    from test_nonplanar_warp import cura_like
    return [c + "\n" for c in cura_like(layers=layers).rstrip("\n").split("\n")]


def test_nonplanar_script_is_off_by_default_and_does_not_touch_anything(monkeypatch, tmp_path):
    m, sc, notes = _np_setup(monkeypatch, tmp_path)
    data = _cura_chunks()
    assert json.loads(sc.getSettingDataString())["settings"]["warp_on"]["default_value"] is False
    assert sc.execute(data) is data and notes == []                                      # el mismo objeto: ni se copia
    assert not (tmp_path / "arquis_nonplanar_vista.svg").exists()


def test_nonplanar_script_first_export_only_writes_the_preview_and_the_code(monkeypatch, tmp_path):
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True)
    data = _cura_chunks()
    out = sc.execute(data)
    assert out is data                                                                   # G-code ORIGINAL: no se aplica nada sin el codigo
    assert (tmp_path / "arquis_nonplanar_vista.svg").read_text().startswith("<svg")
    lvl, msg = notes[-1]
    code = re.search(r"escribe el codigo ([0-9A-F]{12})", msg).group(1)
    assert lvl == "warning" and "NO aplicado todavia" in msg and "arquis_nonplanar_vista.svg" in msg and code
    assert "Pieza: 24 capas" in msg and "SIN CAMBIAR NADA" not in msg and "Puedes volver a laminar" in msg                      # el mensaje ya no exige exportar identico


def test_nonplanar_script_applies_only_with_the_right_code_and_keeps_the_header(monkeypatch, tmp_path):
    data = _cura_chunks()
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True)
    sc.execute(data)
    code = re.search(r"escribe el codigo ([0-9A-F]{12})", notes[-1][1]).group(1)
    m, bad, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code="AAAAAAAAAAAA")
    assert bad.execute(data) is data                                                     # codigo equivocado: nada
    m, ok, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code=code.lower())
    out = ok.execute(data)
    assert out is not data and len(out) == 1 and notes[-1][0] in ("info", "warning") and "APLICADO" in notes[-1][1]
    lines = out[0].splitlines()
    assert lines[0].startswith(";ARQUIS_M486") and lines[1].startswith(";ARQUIS_OBJECTS=1") and lines[2].startswith(";ARQUIS_NP_WARP") and code in lines[2]
    # cualquier cambio de ajuste invalida el codigo aceptado
    m, changed, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code=code, amplitude=1.2)
    assert changed.execute(data) is data and "NO aplicado todavia" in notes[-1][1]


def test_nonplanar_script_returns_the_original_gcode_on_errors(monkeypatch, tmp_path):
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, amplitude=3.9, ramp=0.5)          # ondulado inaceptable: espesor y pendiente
    data = _cura_chunks()
    assert sc.execute(data) is data and notes[-1][0] == "error" and "NO aplicado" in notes[-1][1]
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, function="__import__('os').system('x')")
    assert sc.execute(data) is data and notes[-1][0] == "error" and "no permitida" in notes[-1][1].lower()
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True)
    other = ["G28\n", "G1 X10 Y10 E1\n"]                                                 # sin ;LAYER: ni ;TIME_ELAPSED: de Cura
    assert sc.execute(other) is other and notes[-1][0] == "error" and "LAYER" in notes[-1][1]
    monkeypatch.setenv("ARQUIS_PLUGIN_PATH", str(tmp_path / "no_existe"))
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True)
    monkeypatch.setenv("ARQUIS_PLUGIN_PATH", str(tmp_path / "no_existe"))
    assert sc.execute(data) is data and "no esta instalado" in notes[-1][1]


def test_nonplanar_script_refuses_when_the_head_is_not_confirmed(monkeypatch, tmp_path):
    import shutil
    plug = tmp_path / "plug"
    shutil.copytree(PLUGIN, plug, ignore=shutil.ignore_patterns("__pycache__"))
    h = json.loads((plug / "nonplanar_head.json").read_text())
    h["head"]["confirmed"] = False
    (plug / "nonplanar_head.json").write_text(json.dumps(h))
    monkeypatch.setenv("ARQUIS_PLUGIN_PATH", str(plug))
    monkeypatch.setenv("HOME", str(tmp_path)); monkeypatch.setenv("USERPROFILE", str(tmp_path))
    sys.modules.pop("arquis_bootstrap", None)
    m = load_script("ARQUIS_NonPlanar")
    sc = m.ARQUIS_NonPlanar(); sc.settings = {"warp_on": True}
    notes = []; m._notify = lambda t, l: notes.append((l, t))
    data = _cura_chunks()
    assert sc.execute(data) is data and "no esta confirmado" in notes[-1][1]



# ---- el codigo sobrevive a laminados distintos de la MISMA pieza (diferencias minimas) y se invalida si la pieza cambia de verdad
def _first_code(monkeypatch, tmp_path, data, **settings):
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, **settings)
    out = sc.execute(data)
    return out, re.search(r"escribe el codigo ([0-9A-F]{12})", notes[-1][1]).group(1), notes


def _perturb(data, dx=0.003, e_scale=1.0001):
    def fix(chunk):
        chunk = re.sub(r"X(\d+\.?\d*)", lambda m: "X%.3f" % (float(m.group(1)) + dx), chunk)
        return re.sub(r" E(\d+\.\d+)", lambda m: " E%.5f" % (float(m.group(1)) * e_scale), chunk)
    return [fix(c) if c.startswith(("G0", "G1")) else c for c in data]


def test_code_survives_a_slightly_different_slice_of_the_same_part(monkeypatch, tmp_path):
    data = _cura_chunks()
    _, code, _ = _first_code(monkeypatch, tmp_path, data)
    again = _perturb(data)                                                                # otro laminado de la misma pieza: X +0,003 mm y E +0,01 %
    assert "".join(again) != "".join(data)
    m, ok, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code=code)
    out = ok.execute(again)
    assert out is not again and "APLICADO" in notes[-1][1] and ";ARQUIS_NP_WARP" in out[0].splitlines()[2] and code in out[0].splitlines()[2]


def test_code_is_invalidated_when_the_part_really_changes(monkeypatch, tmp_path):
    data = _cura_chunks()
    _, code, _ = _first_code(monkeypatch, tmp_path, data)
    moved = _perturb(data, dx=5.0, e_scale=1.0)                                           # la pieza se movio 5 mm: el codigo es el mismo pero hay que revisar de nuevo
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code=code)
    assert sc.execute(moved) is moved and "NO aplicado todavia" in notes[-1][1] and "ha cambiado" in notes[-1][1] and "posicion o tamano" in notes[-1][1]
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code=code)
    out = sc.execute(moved)                                                                 # tras revisar la nueva vista previa (mismo codigo), ahora si
    assert out is not moved and "APLICADO" in notes[-1][1]
    _first_code(monkeypatch, tmp_path, data)                                                # vuelve a guardarse la vista previa de la pieza original
    more = _perturb(data, dx=0.0, e_scale=1.2)                                              # 20 % mas de filamento: tambien se pide revisar
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code=code)
    assert sc.execute(more) is more and "filamento" in notes[-1][1]


def test_code_changes_with_the_settings_and_with_the_number_of_layers_or_height(monkeypatch, tmp_path):
    data = _cura_chunks(layers=24)
    _, code, _ = _first_code(monkeypatch, tmp_path, data)
    _, other_amp, _ = _first_code(monkeypatch, tmp_path, data, amplitude=1.2)
    _, taller, _ = _first_code(monkeypatch, tmp_path, _cura_chunks(layers=30))
    assert len({code, other_amp, taller}) == 3
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code=code)
    assert sc.execute(_cura_chunks(layers=30)) is not None and "NO aplicado todavia" in notes[-1][1]       # el codigo de 24 capas no vale para 30


def test_a_right_code_without_a_saved_preview_is_not_enough(monkeypatch, tmp_path):
    data = _cura_chunks()
    _, code, _ = _first_code(monkeypatch, tmp_path, data)
    (tmp_path / "arquis_nonplanar_vista.json").unlink()                                    # nadie reviso ninguna vista previa para esta pieza
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code=code)
    assert sc.execute(data) is data and "No hay una vista previa guardada" in notes[-1][1]
    (tmp_path / "arquis_nonplanar_vista.json").write_text("no es json")
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True, accept_code=code)
    assert sc.execute(data) is data and "NO aplicado todavia" in notes[-1][1]


def test_script_warns_when_it_runs_before_arquis_m486(monkeypatch, tmp_path):
    data = _cura_chunks()
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True)
    sc.execute(data)
    assert "AVISO DE ORDEN" not in notes[-1][1]                                           # con la marca ;ARQUIS_M486 delante: sin aviso
    raw = [c for c in data if not c.startswith(";ARQUIS")]                                # G-code crudo de Cura: M486 aun no se ejecuto
    m, sc, notes = _np_setup(monkeypatch, tmp_path, warp_on=True)
    assert sc.execute(raw) is raw and "AVISO DE ORDEN" in notes[-1][1] and "ULTIMO" in notes[-1][1]
