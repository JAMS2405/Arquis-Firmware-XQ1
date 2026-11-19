# SPDX-License-Identifier: LGPL-3.0-or-later
"""Vectores compartidos (shared/object_protocol/test_vectors): estan al dia y describen lo que dice la especificacion."""
import json
import os
import subprocess
import sys
from fractions import Fraction
from math import floor

import arquis_engines.bed_map_engine as bm
import arquis_engines.manifest_engine as me

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
VEC = os.path.join(ROOT, "shared", "object_protocol", "test_vectors")


def load(name):
    return json.load(open(os.path.join(VEC, name), encoding="utf-8"))


def test_vectors_are_up_to_date():
    r = subprocess.run([sys.executable, os.path.join(ROOT, "cura", "tools", "gen_shared_vectors.py"), "--check"], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


def ref_div_round(a, b):                                   # implementacion independiente con fracciones exactas
    return floor(Fraction(2 * a + b, 2 * b))


def test_bed_map_vectors_against_an_independent_formula():
    v = load("bed_map_v1.json")
    n = 0
    for c in v["cases"]:
        if c["fn"] == "div_round":
            assert c["out"] == ref_div_round(*c["in"])
        elif c["fn"] == "to_px":
            mx, my, w, d = c["bed"]
            vx, vy, vw, vh, z, cx, cy = c["view"]
            cx = mx + w // 2 if cx is None else cx
            cy = my + d // 2 if cy is None else cy
            usable = min(vw, vh) - 16
            x, y = c["in"]
            ex = vx + vw // 2 + ref_div_round(usable * z * (x - cx), max(w, d))
            ey = vy + vh // 2 - ref_div_round(usable * z * (y - cy), max(w, d))
            assert c["out"] == [ex, ey]
        n += 1
    assert n > 300
    assert {c["fn"] for c in v["cases"]} == {"to_px", "marker", "pick", "overlaps", "div_round", "object_state"}


def test_manifest_vectors_have_the_expected_character():
    v = load("manifest_v1.json")
    names = [c["name"] for c in v["cases"]]
    assert len(names) == len(set(names)) and len(names) >= 25
    for c in v["cases"]:
        e = c["expect"]
        if c["name"].startswith("valid_"):
            assert e["ok"] and not e["errors"] and "manifest" in e
        elif c["name"].startswith("no_manifest"):
            assert not e["ok"] and not e["errors"] and e["warnings"] == ["W_NO_MANIFEST"]
        else:
            assert c["name"].startswith("bad_") and not e["ok"] and e["errors"], c["name"]
        r = me.parse_text(c["text"], tuple(c["bed_check"]) if c["bed_check"] else None)           # el parser de referencia reproduce el vector
        assert r.ok == e["ok"] and sorted({p.code for p in r.problems if p.severity == "error"}) == e["errors"]
    by = {c["name"]: c["expect"] for c in v["cases"]}
    assert by["valid_30_objects_3_names"]["manifest"]["objects"][29]["id"] == 29
    assert by["valid_30_crlf"]["manifest"] == by["valid_30_objects_3_names"]["manifest"]
    assert by["bad_dup_id"]["errors"] == ["E_ID_DUPLICATE"] and "E_OVER_CAPACITY" in by["bad_over_capacity_31"]["errors"]
