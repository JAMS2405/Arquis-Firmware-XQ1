# SPDX-License-Identifier: LGPL-3.0-or-later
"""El esquema JSON describe exactamente lo que produce y acepta el motor (validador minimo del subconjunto de JSON Schema que usa el esquema)."""
import json
import os
import re

import pytest

import arquis_engines.manifest_engine as me
import arquis_engines.protocol_limits as L
from fixtures import grid_objects, instances_from, make_gcode
from arquis_engines.m486_engine import M486Options, process_gcode

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCHEMA = json.load(open(os.path.join(ROOT, "shared", "object_protocol", "arquis_objects_v1.schema.json"), encoding="utf-8"))


def validate(x, s, path="$"):
    errs = []
    if "const" in s and x != s["const"]:
        errs.append("%s: debe ser %r" % (path, s["const"]))
    if "enum" in s and x not in s["enum"]:
        errs.append("%s: fuera de %r" % (path, s["enum"]))
    t = s.get("type")
    ok = {"object": dict, "array": list, "string": str, "boolean": bool}.get(t)
    if t == "integer":
        if not (isinstance(x, int) and not isinstance(x, bool)):
            return errs + ["%s: debe ser entero" % path]
    elif ok and not isinstance(x, ok):
        return errs + ["%s: debe ser %s" % (path, t)]
    if t == "integer":
        if "minimum" in s and x < s["minimum"]: errs.append("%s: menor que %s" % (path, s["minimum"]))
        if "maximum" in s and x > s["maximum"]: errs.append("%s: mayor que %s" % (path, s["maximum"]))
    if t == "string":
        if "pattern" in s and not re.search(s["pattern"], x): errs.append("%s: no cumple %s" % (path, s["pattern"]))
        if "minLength" in s and len(x) < s["minLength"]: errs.append("%s: vacio" % path)
        if "maxBytes" in s and len(x.encode("utf-8")) > s["maxBytes"]: errs.append("%s: mas de %d bytes" % (path, s["maxBytes"]))
    if t == "array":
        if "maxItems" in s and len(x) > s["maxItems"]: errs.append("%s: demasiados elementos" % path)
        for i, v in enumerate(x):
            errs += validate(v, s.get("items", {}), "%s[%d]" % (path, i))
    if t == "object":
        for k in s.get("required", []):
            if k not in x: errs.append("%s: falta %s" % (path, k))
        props = s.get("properties", {})
        if s.get("additionalProperties") is False:
            errs += ["%s: campo no permitido %s" % (path, k) for k in x if k not in props]
        for k, v in x.items():
            if k in props: errs += validate(v, props[k], "%s.%s" % (path, k))
    return errs


def good_dict():
    objs = grid_objects(30, "Llavero", height=1.0)
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options(bed=(0.0, 0.0, 300.0, 300.0)))
    return me.parse_text("".join(res.chunks)).manifest.to_dict()


def test_schema_accepts_what_the_engine_produces():
    d = good_dict()
    assert validate(d, SCHEMA) == []
    assert len(d["objects"]) == 30 and d["objects"][-1]["id"] == 29


def test_schema_limits_follow_limits_json():
    p = SCHEMA["properties"]
    assert p["objects"]["maxItems"] == L.DESIGN_CAPACITY and p["names"]["maxItems"] == L.DESIGN_CAPACITY
    assert p["objects"]["items"]["properties"]["id"]["maximum"] == L.MAX_CANCELABLE_ID
    assert p["objects"]["items"]["properties"]["name"]["maxBytes"] == L.NAME_MAX_BYTES == p["names"]["items"]["maxBytes"]
    assert p["capacity"]["maximum"] == L.CAPACITY_CEILING


@pytest.mark.parametrize("mut", [lambda d: d["objects"][0].update(id=30), lambda d: d["objects"][0].update(shape="X"),
                                 lambda d: d["objects"][0].update(rotation=360), lambda d: d.update(version=2),
                                 lambda d: d.update(print_id="zz"), lambda d: d["objects"][0].pop("z0"),
                                 lambda d: d["objects"][0].update(extra=1), lambda d: d["objects"].append(dict(d["objects"][0])),
                                 lambda d: d["names"].__setitem__(0, "x" * 25), lambda d: d["bed"].update(width=0)])
def test_schema_rejects_bad_documents(mut):
    d = good_dict()
    mut(d)
    assert validate(d, SCHEMA)
