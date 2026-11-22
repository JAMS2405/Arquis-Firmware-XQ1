# SPDX-License-Identifier: LGPL-3.0-or-later
"""Lectores STL/3MF, estanqueidad y volumen. Ninguna prueba repara ni modifica el modelo."""
import struct
import zipfile

import pytest

from aite.mesh import (Mesh, MeshError, analyze_mesh, load_3mf, load_mesh, load_stl, parse_stl, write_3mf, write_stl_binary)
from aite.parts import make_box


def _box():
    return make_box((2.0, 3.0, 4.0), (1.0, 1.0, 1.0))


def test_box_is_watertight_with_exact_volume_and_outward_normals():
    m = _box()
    r = analyze_mesh(m)
    assert r.watertight and not r.inverted and r.problems() == []
    assert r.n_triangles == 12 and r.n_vertices == 8
    assert r.volume == pytest.approx(24.0)
    assert m.surface_area() == pytest.approx(2 * (6 + 8 + 12))
    cx, cy, cz = 2.0, 2.5, 3.0
    for t in m.triangles:
        (nx, ny, nz), _ = m.normal_area(t)
        a, b, c = m.corners(t)
        mid = tuple(sum(p[i] for p in (a, b, c)) / 3 for i in range(3))
        assert nx * (mid[0] - cx) + ny * (mid[1] - cy) + nz * (mid[2] - cz) > 0      # apunta hacia fuera


def test_binary_stl_round_trip(tmp_path):
    p = tmp_path / "caja.stl"
    write_stl_binary(_box(), str(p))
    m = load_stl(str(p))
    r = analyze_mesh(m)
    assert r.watertight and r.volume == pytest.approx(24.0, rel=1e-6)
    assert load_mesh(str(p)).triangles == m.triangles


def test_ascii_stl():
    txt = ["solid t"]
    m = _box()
    for t in m.triangles:
        (nx, ny, nz), _ = m.normal_area(t)
        txt.append("facet normal %g %g %g\n outer loop" % (nx, ny, nz))
        for v in m.corners(t):
            txt.append("vertex %g %g %g" % v)
        txt.append("endloop\nendfacet")
    txt.append("endsolid t")
    mm = parse_stl("\n".join(txt).encode())
    assert analyze_mesh(mm).watertight and analyze_mesh(mm).volume == pytest.approx(24.0)


def test_stl_rejects_bad_files(tmp_path):
    good = tmp_path / "g.stl"
    write_stl_binary(_box(), str(good))
    data = good.read_bytes()
    with pytest.raises(MeshError):
        parse_stl(data[:-10])                                       # truncado
    bad = bytearray(data)
    struct.pack_into("<f", bad, 84 + 12, float("nan"))
    with pytest.raises(MeshError):
        parse_stl(bytes(bad))                                       # valor no finito
    with pytest.raises(MeshError):
        parse_stl(b"")
    with pytest.raises(MeshError):
        parse_stl(b"solid x\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\n")          # faceta incompleta
    with pytest.raises(MeshError):
        parse_stl(b"solid x\nfacet normal 0 0 1\nouter loop\nvertex 0 0 abc\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\n")
    with pytest.raises(MeshError):
        load_mesh(str(tmp_path / "x.obj"))
    empty = bytearray(84)
    with pytest.raises(MeshError):
        parse_stl(bytes(empty))                                     # cero triangulos


def test_open_mesh_is_detected():
    m = _box()
    m.triangles.pop()
    r = analyze_mesh(m)
    assert not r.watertight and r.open_edges > 0
    assert any("abiertas" in p for p in r.problems())


def test_flipped_triangle_is_inconsistent():
    m = _box()
    a, b, c = m.triangles[0]
    m.triangles[0] = (a, c, b)
    r = analyze_mesh(m)
    assert not r.watertight and r.inconsistent_edges > 0


def test_inverted_mesh_has_negative_volume():
    m = _box()
    m.triangles = [(a, c, b) for a, b, c in m.triangles]
    r = analyze_mesh(m)
    assert r.inverted and not r.watertight and r.volume == pytest.approx(-24.0)
    assert any("hacia dentro" in p for p in r.problems())


def test_duplicate_face_is_nonmanifold():
    m = _box()
    m.triangles.append(m.triangles[0])
    assert analyze_mesh(m).nonmanifold_edges > 0


def test_degenerate_triangle_is_counted():
    m = _box()
    m.vertices.append((0.0, 0.0, 0.0))
    m.vertices.append((1.0, 0.0, 0.0))
    m.vertices.append((2.0, 0.0, 0.0))
    n = len(m.vertices)
    m.triangles.append((n - 3, n - 2, n - 1))                       # tres puntos alineados
    r = analyze_mesh(m)
    assert r.degenerate_triangles == 1 and not r.watertight


def test_empty_mesh_reports():
    r = analyze_mesh(Mesh())
    assert not r.watertight and "malla vacia" in r.problems()


# ---------------------------------------------------------------- 3MF
NS = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
CUBE_V = "<vertices>" + "".join('<vertex x="%g" y="%g" z="%g"/>' % v for v in
                                [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0), (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)]) + "</vertices>"
CUBE_T = "<triangles>" + "".join('<triangle v1="%d" v2="%d" v3="%d"/>' % t for t in
                                 [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7), (1, 2, 6), (1, 6, 5)]) + "</triangles>"


def _write(path, model_xml):
    with zipfile.ZipFile(path, "w") as z:
        z.writestr("3D/3dmodel.model", model_xml)


def _model(body, unit="millimeter"):
    return '<?xml version="1.0"?><model unit="%s" xmlns="%s">%s</model>' % (unit, NS, body)


def test_3mf_round_trip_of_generated_part(tmp_path):
    p = tmp_path / "a.3mf"
    write_3mf(_box(), str(p))
    r = analyze_mesh(load_3mf(str(p)))
    assert r.watertight and r.volume == pytest.approx(24.0, rel=1e-5)


def test_3mf_transform_units_and_components(tmp_path):
    # objeto 1: cubo unidad; objeto 2: dos cubos como componentes (uno desplazado 2 en x); item con traslacion y escala en cm
    body = ('<resources><object id="1" type="model"><mesh>%s%s</mesh></object>'
            '<object id="2" type="model"><components><component objectid="1"/>'
            '<component objectid="1" transform="1 0 0 0 1 0 0 0 1 2 0 0"/></components></object></resources>'
            '<build><item objectid="2" transform="1 0 0 0 1 0 0 0 1 0 0 5"/></build>' % (CUBE_V, CUBE_T))
    p = tmp_path / "b.3mf"
    _write(str(p), _model(body, unit="centimeter"))
    m = load_3mf(str(p))
    r = analyze_mesh(m)
    assert r.watertight
    assert r.volume == pytest.approx(2 * 1000.0)                    # dos cubos de 1 cm = 10 mm de arista
    (x0, y0, z0), (x1, y1, z1) = r.bbox
    assert (x0, x1) == pytest.approx((0.0, 30.0))                   # 0..10 y 20..30
    assert (z0, z1) == pytest.approx((50.0, 60.0))                  # traslacion de 5 cm en z


def test_3mf_rotation_uses_row_vector_convention(tmp_path):
    # giro de 90 grados alrededor de z: (x, y) -> (-y, x). Con la convencion 3MF: m00=0 m01=1 m10=-1 m11=0
    body = ('<resources><object id="1" type="model"><mesh>%s%s</mesh></object></resources>'
            '<build><item objectid="1" transform="0 1 0 -1 0 0 0 0 1 0 0 0"/></build>' % (CUBE_V, CUBE_T))
    p = tmp_path / "r.3mf"
    _write(str(p), _model(body))
    m = load_3mf(str(p))
    (x0, y0, z0), (x1, y1, z1) = analyze_mesh(m).bbox
    assert (x0, x1) == pytest.approx((-1.0, 0.0)) and (y0, y1) == pytest.approx((0.0, 1.0))
    assert analyze_mesh(m).volume == pytest.approx(1.0)            # un giro propio conserva el signo


@pytest.mark.parametrize("body,unit", [
    ("<resources/>", "millimeter"),                                                                      # sin build
    ('<resources><object id="1"><mesh>%s%s</mesh></object></resources><build><item objectid="9"/></build>' % (CUBE_V, CUBE_T), "millimeter"),
    ('<resources><object id="1"><mesh>%s%s</mesh></object></resources><build><item objectid="1"/></build>' % (CUBE_V, CUBE_T), "parsec"),
    ('<resources><object id="1"><mesh>%s<triangles><triangle v1="0" v2="1" v3="99"/></triangles></mesh></object></resources>'
     '<build><item objectid="1"/></build>' % CUBE_V, "millimeter"),                                       # indice fuera de rango
    ('<resources><object id="1"><mesh>%s%s</mesh></object></resources><build><item objectid="1" transform="1 2 3"/></build>' % (CUBE_V, CUBE_T), "millimeter"),
])
def test_3mf_rejects_invalid(tmp_path, body, unit):
    p = tmp_path / "bad.3mf"
    _write(str(p), _model(body, unit))
    with pytest.raises(MeshError):
        load_3mf(str(p))


def test_3mf_rejects_bad_zip_and_xml(tmp_path):
    p = tmp_path / "x.3mf"
    p.write_bytes(b"no es un zip")
    with pytest.raises(MeshError):
        load_3mf(str(p))
    _write(str(p), "<model")
    with pytest.raises(MeshError):
        load_3mf(str(p))
    with zipfile.ZipFile(str(p), "w") as z:
        z.writestr("otra/cosa.txt", "x")
    with pytest.raises(MeshError):
        load_3mf(str(p))
