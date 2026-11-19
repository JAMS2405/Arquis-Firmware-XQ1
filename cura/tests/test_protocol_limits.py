import json
import os

import arquis_engines.protocol_limits as L

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def test_limits_match_shared_json():
    with open(os.path.join(ROOT, "shared", "object_protocol", "limits.json"), encoding="utf-8") as f:
        j = json.load(f)
    assert j["protocol"] == L.PROTOCOL_NAME
    assert j["version"] == L.PROTOCOL_VERSION
    assert j["target_capacity"] == L.TARGET_CAPACITY == 64
    assert j["legacy_firmware_capacity"] == L.LEGACY_FIRMWARE_CAPACITY == 32
    assert j["max_protocol_index"] == L.MAX_PROTOCOL_INDEX == 127
    assert j["name_max_bytes"] == L.NAME_MAX_BYTES
    assert j["coord_unit_mm"] == L.COORD_UNIT_MM
