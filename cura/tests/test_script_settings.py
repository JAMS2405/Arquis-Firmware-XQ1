# SPDX-License-Identifier: LGPL-3.0-or-later
"""Cura 5.11 (Uranium SettingDefinition._deserialize_dict) EXIGE label, description, type y default_value en cada ajuste
de un script de post-procesado; si falta alguno lanza AttributeError y Cura se cierra al anadir el script
(fallo real observado: 'Setting bed_x is missing required property description')."""
import ast, json, pathlib
import pytest

SCRIPTS = sorted((pathlib.Path(__file__).resolve().parents[1] / "scripts").glob("ARQUIS_*.py"))
REQUIRED = ("label", "description", "type", "default_value")


def _setting_json(path):
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for fn in ast.walk(tree):
        if isinstance(fn, ast.FunctionDef) and fn.name == "getSettingDataString":
            for node in ast.walk(fn):
                if isinstance(node, ast.Constant) and isinstance(node.value, str):
                    return json.loads(node.value)
    return None


@pytest.mark.parametrize("path", SCRIPTS, ids=lambda p: p.name)
def test_every_setting_has_required_properties(path):
    data = _setting_json(path)
    if data is None:
        pytest.skip("sin ajustes")
    for key, s in data.get("settings", {}).items():
        for prop in REQUIRED:
            assert prop in s, "%s: el ajuste %s no tiene '%s' (Cura se cierra)" % (path.name, key, prop)
