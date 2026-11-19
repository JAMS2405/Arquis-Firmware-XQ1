"""Los scripts de post-procesado se cargan como los carga Cura (modulo dentro de un paquete con ``Script``) y se ejecutan con una escena falsa."""
import importlib.util
import json
import os
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
