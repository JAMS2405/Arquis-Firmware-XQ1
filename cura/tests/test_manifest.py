# SPDX-License-Identifier: LGPL-3.0-or-later
"""Etapa 2: manifiesto ARQUIS_OBJECTS_V1 — generacion, lectura, validacion y sincronizacion con el G-code."""
import re
import zlib

import pytest

import arquis_engines.manifest_engine as me
from arquis_engines.m486_engine import M486Options, process_gcode
from arquis_engines.object_engine import ArquisObject, SceneInstance, classify_shape, split_name
from arquis_engines.validation_engine import gcode_object_stats, print_order, validate_manifest_vs_gcode
from fixtures import Obj, grid_objects, instances_from, make_gcode
from helpers import flat, m486_lines
from real_gcode import derive_scene, load_reduced, split_like_cura

BED = (0.0, 0.0, 300.0, 300.0)


def build(objs, **kw):
    chunks = make_gcode(objs, **{k: v for k, v in kw.items() if k in ("one_at_a_time", "travel_e", "e_mode")})
    opt = M486Options(bed=BED, **{k: v for k, v in kw.items() if k in ("expected_bed", "capacity", "ceiling")})
    res = process_gcode(chunks, instances_from(objs), opt)
    return chunks, res


def text_of(res):
    return "".join(res.chunks)


def reparse(res):
    return me.parse_text(text_of(res))


# ---------------------------------------------------------------- generacion y sincronizacion -----------------------------------------
def test_thirty_identical_copies_each_has_its_own_manifest_entry():
    objs = grid_objects(30, "Llavero", height=1.0)
    _, res = build(objs)
    assert res.status == "ok" and res.manifest is not None
    pr = reparse(res)
    assert pr.ok, [str(p) for p in pr.problems]
    m = pr.manifest
    assert len(m.objects) == 30 and m.names == ["Llavero"]                       # un solo nombre en la tabla, 30 instancias
    assert [o.id for o in m.objects] == list(range(30)) and [o.inst for o in m.objects] == list(range(30))
    assert [m.display_name(o) for o in m.objects][:3] == ["Llavero 1", "Llavero 2", "Llavero 3"]
    for o, s in zip(m.objects, objs):                                             # posicion y tamano = los de la escena (0,1 mm)
        assert (o.cx, o.cy, o.w, o.d) == (round((s.x0 + s.x1) / 2 * 10), round((s.y0 + s.y1) / 2 * 10), round((s.x1 - s.x0) * 10), round((s.y1 - s.y0) * 10))
    assert (m.bed_w, m.bed_d, m.min_x, m.min_y, m.origin, m.seq, m.cap) == (3000, 3000, 0, 0, "FL", 0, 30)


def test_manifest_matches_gcode_exactly():
    objs = grid_objects(12, "Pieza", height=1.5)
    _, res = build(objs)
    pr = reparse(res)
    assert validate_manifest_vs_gcode(res.chunks, pr.manifest) == []
    stats = gcode_object_stats(res.chunks)
    assert set(stats) == set(range(12)) and print_order(stats) == {o.id: o.order for o in pr.manifest.objects}


def test_header_marker_is_in_first_512_bytes_and_block_at_the_end():
    objs = grid_objects(30, "A", height=1.0)
    _, res = build(objs)
    raw = text_of(res).encode("utf-8")
    assert re.search(rb"^;ARQUIS_OBJECTS=1 PID=[0-9A-F]{8} N=30$", raw[:me.HEADER_WINDOW], re.M)
    assert raw.rstrip().splitlines()[-1].startswith(b";ARQUIS_TRAILER")
    assert len(raw) - raw.index(b";ARQUIS_OBJECTS_BEGIN") < 2800                    # peor caso medido: 2 719 B con 30 nombres


def test_crc_is_standard_crc32_of_normalized_lines():
    objs = grid_objects(5, "A", height=1.0)
    _, res = build(objs)
    t = text_of(res)
    body = t[t.index(";ARQUIS_OBJECTS_BEGIN\n") + len(";ARQUIS_OBJECTS_BEGIN\n"):t.index(";ARQUIS_OBJECTS_END")]
    crc = re.search(r";ARQUIS_OBJECTS_END CRC=([0-9A-F]{8})", t).group(1)
    assert int(crc, 16) == zlib.crc32(body.encode("utf-8")) & 0xFFFFFFFF


def test_objects_over_capacity_are_not_published():
    objs = grid_objects(40, "Pieza", height=1.0)
    _, res = build(objs)
    m = reparse(res).manifest
    assert len(m.objects) == 30 and m.objects[-1].id == 29
    assert any(p.code == "W_OVER_CAPACITY" for p in res.problems)


def test_sequential_mode_is_detected_from_the_gcode_and_cross_checked():
    objs = grid_objects(4, "A", height=1.0)
    _, res = build(objs, one_at_a_time=True)
    m = reparse(res).manifest
    assert m.seq == 1 and [o.order for o in m.objects] == [0, 1, 2, 3]
    assert validate_manifest_vs_gcode(res.chunks, m) == []
    _, res2 = build(grid_objects(4, "A", height=1.0))
    assert reparse(res2).manifest.seq == 0


def test_manifest_is_deterministic_and_pid_depends_on_content():
    objs = grid_objects(6, "A", height=1.0)
    _, a = build(objs)
    _, b = build(objs)
    assert text_of(a) == text_of(b)
    moved = [Obj(o.name, o.x0 + 1, o.y0, o.x1 + 1, o.y1, o.height) for o in objs]
    _, c = build(moved)
    assert a.manifest.pid != c.manifest.pid


def test_gcode_without_bed_gets_no_manifest_but_keeps_m486():
    objs = grid_objects(3, "A", height=1.0)
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options())
    assert res.status == "ok" and res.manifest is None and "ARQUIS_OBJECTS" not in text_of(res)


def test_bed_mismatch_with_firmware_omits_only_the_manifest():
    objs = grid_objects(3, "A", height=1.0)
    _, res = build(objs, expected_bed=(220.0, 220.0))
    assert res.status == "ok" and res.manifest is None and "M486 T3" in m486_lines(res.chunks)
    w = [p for p in res.problems if p.code == "W_MANIFEST_OMITTED"]
    assert w and "E_BED_MISMATCH" in w[0].message and "ARQUIS_OBJECTS" not in text_of(res)
    _, ok = build(objs, expected_bed=(300.0, 300.0))
    assert ok.manifest is not None


def test_blocked_m486_never_emits_manifest():
    objs = grid_objects(3, "A", height=1.0)
    wrong = [Obj(o.name, o.x0 + 30, o.y0, o.x1 + 30, o.y1, o.height) for o in objs]
    res = process_gcode(make_gcode(objs), instances_from(wrong), M486Options(bed=BED))
    assert res.status == "blocked" and "ARQUIS_OBJECTS" not in text_of(res)


# ---------------------------------------------------------------- tolerancia a como Cura exporta ---------------------------------------
def test_crlf_export_is_read_identically():
    objs = grid_objects(8, "A", height=1.0)
    _, res = build(objs)
    t = text_of(res)
    a = me.parse_text(t)
    b = me.parse_text(t.replace("\n", "\r\n"))
    assert a.ok and b.ok and a.manifest == b.manifest


def test_cura_appends_a_settings_block_after_ours():
    objs = grid_objects(30, "A", height=1.0)
    _, res = build(objs)
    t = text_of(res)
    for kb in (1, 6, 12):                                                         # el ;SETTING_ de Cura: unos KB (en el G-code real, 1,1 KB)
        tail = "".join(";SETTING_3 " + "x" * 70 + "\n" for _ in range(kb * 1024 // 82))
        assert me.parse_text(t + tail).ok, kb
    huge = "".join(";SETTING_3 " + "x" * 70 + "\n" for _ in range(300))           # ~24 KB: fuera de la ventana de 16 KB
    r = me.parse_text(t + huge)
    assert r.manifest is None and r.problems[0].code == "W_NO_MANIFEST"           # degradacion elegante: sin mapa, impresion normal


def test_old_gcode_without_manifest_is_not_an_error():
    r = me.parse_text(";FLAVOR:Marlin\nG28\nG1 X1 Y1\n" * 50)
    assert r.manifest is None and [p.code for p in r.problems] == ["W_NO_MANIFEST"] and r.problems[0].severity == "warning"


def test_unicode_and_long_names_are_cut_on_character_boundaries():
    for name in ("Pieza ñandú muy larga de verdad", "日本語のモデル名がとても長い", "a;b\nc\x00d", "   "):
        c = me.clean_name(name)
        assert 1 <= len(c.encode("utf-8")) <= 20 and ";" not in c and "\n" not in c
        c.encode("utf-8").decode("utf-8")
    objs = [Obj("Pieza ñandú muy larga de verdad.stl", 10, 10, 30, 30, 1.0)]
    _, res = build(objs)
    assert reparse(res).ok


# ---------------------------------------------------------------- validacion: cada dato corrupto se rechaza ---------------------------
@pytest.fixture(scope="module")
def good():
    objs = grid_objects(4, "Llavero", height=1.0)
    _, res = build(objs)
    return res


def render(m):
    h, b = me.render_block(m)
    return h + b


def mutated(good, fn):
    import copy
    m = copy.deepcopy(good.manifest)
    fn(m)
    m.pid = me._pid(m, me.body_lines(m, with_pid=False)[1:])
    return me.parse_text(";FLAVOR\n" + render(m))


def codes(r):
    return {p.code for p in r.problems if p.severity == "error"}


def rebody(good, fn):
    """Reescribe el bloque con otro cuerpo PERO con CRC/trailer correctos: asi se prueban las validaciones que van mas alla del CRC."""
    lines = fn(me.body_lines(good.manifest))
    crc = me.crc32_lines(lines)
    return (";FLAVOR\n;ARQUIS_OBJECTS_BEGIN\n" + "\n".join(lines) + "\n%s CRC=%08X\n%s CRC=%08X PID=%08X N=%d\n"
            % (me.END, crc, me.TRAILER, crc, good.manifest.pid, len(good.manifest.objects)))


def test_roundtrip_equals(good):
    r = me.parse_text(";FLAVOR\n" + render(good.manifest))
    assert r.ok and r.manifest == good.manifest


def test_semantic_validations(good):
    assert "E_ID_DUPLICATE" in codes(mutated(good, lambda m: setattr(m.objects[1], "id", 0)))
    assert "E_ID_RANGE" in codes(mutated(good, lambda m: setattr(m.objects[3], "id", 9)))
    assert "E_OUTSIDE_BED" in codes(mutated(good, lambda m: setattr(m.objects[0], "cx", 99999)))
    assert "E_SIZE" in codes(mutated(good, lambda m: setattr(m.objects[0], "w", 0)))
    assert "E_Z" in codes(mutated(good, lambda m: setattr(m.objects[0], "z0", m.objects[0].ztop + 5)))
    assert "E_ROT" in codes(mutated(good, lambda m: setattr(m.objects[0], "rot", 400)))
    assert "E_NAME_INDEX" in codes(mutated(good, lambda m: setattr(m.objects[0], "name_idx", 7)))
    assert "E_NAME_TOO_LONG" in codes(mutated(good, lambda m: m.names.__setitem__(0, "x" * 40)))
    assert "E_ORDER" in codes(mutated(good, lambda m: setattr(m.objects[0], "order", 3)))
    assert "E_CAP" in codes(mutated(good, lambda m: setattr(m, "cap", 33)))
    assert "E_OVER_CAPACITY" in codes(mutated(good, lambda m: setattr(m, "cap", 3)))
    assert "E_VERSION" in codes(mutated(good, lambda m: setattr(m, "version", 2)))


def test_integrity_validations(good):
    t = ";FLAVOR\n" + render(good.manifest)
    assert "E_MANIFEST_CRC" in codes(me.parse_text(t.replace(";ARQ:O 0 0 0", ";ARQ:O 0 0 1", 1)))                 # un dato cambiado
    assert "E_MANIFEST_TRUNCATED" in codes(me.parse_text(t[:t.index(";ARQUIS_OBJECTS_END")]))
    assert "E_MANIFEST_TRAILER" in codes(me.parse_text(t[:t.index(";ARQUIS_TRAILER")]))
    assert "E_TRAILER_MISMATCH" in codes(me.parse_text(t.replace("PID=%08X N=4\n" % good.manifest.pid, "PID=DEADBEEF N=4\n")))
    assert "E_HEADER_MISMATCH" in codes(me.parse_text(t.replace("N=4\n;", "N=5\n;", 1)))                           # cabecera != bloque
    assert "E_MANIFEST_FORMAT" in codes(me.parse_text(rebody(good, lambda ls: ls + [";ARQ:X 1"])))          # CRC valido pero linea desconocida
    assert "E_MANIFEST_FORMAT" in codes(me.parse_text(rebody(good, lambda ls: [ls[0], ls[0]] + ls[1:])))      # linea H repetida
    assert "E_MANIFEST_FORMAT" in codes(me.parse_text(rebody(good, lambda ls: ls[1:])))                       # falta la H
    assert "E_COUNT" in codes(me.parse_text(rebody(good, lambda ls: [ls[0].replace("N=4", "N=9")] + ls[1:])))
    assert me.parse_text(t, bed_check=(3000, 3000)).ok
    assert "E_BED_MISMATCH" in codes(me.parse_text(t, bed_check=(2200, 2200)))


def test_manifest_vs_gcode_cross_check_detects_desync(good):
    objs = grid_objects(4, "Llavero", height=1.0)
    _, res = build(objs)
    m = me.parse_text(text_of(res)).manifest
    import copy
    a = copy.deepcopy(m); a.objects[0].cx += 600                                  # el objeto 0 esta 60 mm mas alla en el manifiesto
    assert any(p.code == "V_MANIFEST_BBOX" for p in validate_manifest_vs_gcode(res.chunks, a))
    b = copy.deepcopy(m); b.objects[1].ztop += 10
    assert any(p.code == "V_MANIFEST_Z" for p in validate_manifest_vs_gcode(res.chunks, b))
    c = copy.deepcopy(m); c.objects[0].order, c.objects[1].order = c.objects[1].order, c.objects[0].order
    assert any(p.code == "V_MANIFEST_ORDER" for p in validate_manifest_vs_gcode(res.chunks, c))
    d = copy.deepcopy(m); d.objects.pop()
    assert any(p.code in ("V_MANIFEST_T", "V_MANIFEST_IDS") for p in validate_manifest_vs_gcode(res.chunks, d))
    swapped = [c_.replace("M486 S1\n", "M486 STMP\n").replace("M486 S2\n", "M486 S1\n").replace("M486 STMP\n", "M486 S2\n") for c_ in res.chunks]
    assert validate_manifest_vs_gcode(swapped, m)                                 # ids intercambiados en el G-code: el manifiesto ya no describe el archivo


# ---------------------------------------------------------------- G-code REAL --------------------------------------------------------
@pytest.mark.parametrize("name,n", [("real_3_distintos", 3), ("real_4_iguales", 4)])
@pytest.mark.parametrize("crlf", [False, True])
def test_real_gcode_manifest_is_in_sync(name, n, crlf):
    text = load_reduced(name)
    chunks = split_like_cura(text)
    if crlf:
        chunks = [c.replace("\n", "\r\n") for c in chunks]
    res = process_gcode(chunks, derive_scene(split_like_cura(text)), M486Options(bed=(0, 0, 450, 450)))
    assert res.status == "ok" and res.manifest is not None, res.summary()
    full = "".join(res.chunks)
    pr = me.parse_text(full)
    assert pr.ok and len(pr.manifest.objects) == n
    assert validate_manifest_vs_gcode([c.replace("\r", "") for c in res.chunks], pr.manifest) == []
    if name == "real_4_iguales":
        assert pr.manifest.names == ["pinon"] and [o.inst for o in pr.manifest.objects] == [0, 1, 2, 3]
        assert len({(o.cx, o.cy) for o in pr.manifest.objects}) == 4                       # cuatro posiciones distintas


# ---------------------------------------------------------------- nombres y formas -------------------------------------------------
def test_split_name_follows_cura_renaming():
    assert split_name("pinon.stl") == ("pinon", None)
    assert split_name("pinon.stl(3)") == ("pinon", 3)
    assert split_name("Llavero (2).STL") == ("Llavero (2)", None)
    assert split_name("Grupo#2") == ("Grupo", 2)
    assert split_name("pinon rusvel 66 dientes.stl") == ("pinon rusvel 66 dientes", None)
    assert split_name(".stl")[0] == ".stl" or split_name(".stl")[0]


def circle(r=10, n=24):
    import math
    return [(r * math.cos(2 * math.pi * i / n), r * math.sin(2 * math.pi * i / n)) for i in range(n)]


def test_shape_classification_is_a_visual_hint():
    assert classify_shape(circle()) == "C"
    assert classify_shape([(0, 0), (20, 0), (20, 20), (0, 20)]) == "S"
    assert classify_shape([(0, 0), (60, 0), (60, 15), (0, 15)]) == "R"                 # alargada: rectangulo
    assert classify_shape([(0, 0), (20, 0), (22, 21), (0, 19)]) == "S"
    assert classify_shape([(0, 0), (1, 1)]) == "R"
    assert classify_shape([(x * 1.0, y * 0.5) for x, y in circle()]) == "R"            # elipse achatada: no es circulo
