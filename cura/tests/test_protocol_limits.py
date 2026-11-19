# SPDX-License-Identifier: LGPL-3.0-or-later
import json
import os
import subprocess
import sys

import arquis_engines.protocol_limits as L

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SHARED = os.path.join(ROOT, "shared", "object_protocol")


def test_generated_files_are_up_to_date_with_limits_json():
    r = subprocess.run([sys.executable, os.path.join(SHARED, "gen_limits.py"), "--check"], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def test_limits_values_are_the_owner_decision():
    j = json.load(open(os.path.join(SHARED, "limits.json"), encoding="utf-8"))
    assert (L.DESIGN_CAPACITY, L.MAX_CANCELABLE_ID) == (j["design_capacity"], j["design_capacity"] - 1) == (30, 29)
    assert L.CAPACITY_CEILING == j["capacity_ceiling"] == 32           # mascara uint32_t del motor actual
    assert L.PROTOCOL_MAX_OBJECTS == j["protocol_max_objects"] == 128  # int8_t
    assert L.NAME_MAX_BYTES == j["name_max_bytes"] and L.COORD_UNIT_MM == j["coord_unit_mm"]
    assert 1 <= L.DESIGN_CAPACITY <= L.CAPACITY_CEILING <= L.PROTOCOL_MAX_OBJECTS


def test_script_settings_follow_limits():
    """Los JSON de los scripts de Cura no pueden leer limits.json en tiempo de definicion: esta prueba obliga a que coincidan."""
    import importlib.util, types
    sys.path.insert(0, os.path.dirname(__file__))
    from test_cura_scripts import load_script
    for name in ("ARQUIS_M486", "ARQUIS_Validation"):
        s = json.loads(getattr(load_script(name), name)().getSettingDataString())["settings"]["capacity"]
        assert (s["default_value"], s["maximum_value"], s["minimum_value"]) == (L.DESIGN_CAPACITY, L.CAPACITY_CEILING, 1)


def test_generated_header_has_the_same_numbers():
    h = open(os.path.join(SHARED, "generated", "arquis_object_limits.h"), encoding="utf-8").read()
    assert "#define ARQ_OBJ_DESIGN_CAPACITY    30" in h and "#define ARQ_OBJ_CAPACITY_CEILING   32" in h
