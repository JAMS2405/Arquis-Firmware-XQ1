# SPDX-License-Identifier: LGPL-3.0-or-later
"""Lectura de superficies STL para el motor no planar (solo analisis). Modelos GENERADOS aqui (anillo ondulado, placas); no son la pieza
de referencia del propietario. Pruebas de calculo: no validan nada en Cura ni en la impresora."""
import importlib.util
import math
import os
import struct

import pytest

import arquis_bootstrap

arquis_bootstrap.load_engines()
import arquis_engines.nonplanar_surface as nps  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def wavy_ring(r_in=10.0, r_out=25.0, z0=3.0, amp=0.5, lobes=3, thick=1.0, nr=30, nt=240):
    """Anillo cuya cara superior ondula: z = z0 + amp*sin(lobes*theta). Cara superior con normal +Z y cara inferior con normal -Z."""
    def p(r, t, dz=0.0):
        return (r * math.cos(t), r * math.sin(t), z0 + amp * math.sin(lobes * t) - dz)
    tris = []
    for i in range(nr):
        r0, r1 = r_in + (r_out - r_in) * i / nr, r_in + (r_out - r_in) * (i + 1) / nr
        for k in range(nt):
            t0, t1 = 2 * math.pi * k / nt, 2 * math.pi * (k + 1) / nt
            a, b, c, d = p(r0, t0), p(r1, t0), p(r1, t1), p(r0, t1)
            tris += [(a, b, c), (a, c, d)]                                      # antihorario visto desde +Z: normal arriba
            a, b, c, d = p(r0, t0, thick), p(r1, t0, thick), p(r1, t1, thick), p(r0, t1, thick)
            tris += [(a, c, b), (a, d, c)]                                      # inferior: normal abajo
    return tris


def to_binary(tris):
    out = bytearray(80) + struct.pack("<I", len(tris))
    for a, b, c in tris:
        out += struct.pack("<12fH", 0, 0, 0, *a, *b, *c, 0)
    return bytes(out)


def to_ascii(tris):
    s = "solid t\n"
    for a, b, c in tris:
        s += "facet normal 0 0 0\nouter loop\n" + "".join("vertex %r %r %r\n" % v for v in (a, b, c)) + "endloop\nendfacet\n"
    return (s + "endsolid t\n").encode()


def test_binary_and_ascii_stl_read_the_same_triangles():
    t = wavy_ring(nr=3, nt=12)
    b, a = nps.read_stl(to_binary(t)), nps.read_stl(to_ascii(t))
    assert len(b) == len(a) == len(t)
    assert all(abs(x - y) < 1e-4 for tb, ta in zip(b, a) for vb, va in zip(tb, ta) for x, y in zip(vb, va))


@pytest.mark.parametrize("bad", [b"", b"hola", to_binary(wavy_ring(nr=2, nt=6))[:-7], b"solid x\nvertex 1 2\nendsolid", b"solid x\nendsolid x\n",
                                 b"solid x\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 nan\nendsolid x\n"])
def test_bad_stl_files_are_rejected(bad):
    with pytest.raises(nps.StlError):
        nps.read_stl(bad)


def test_wavy_ring_surface_is_measured_correctly():
    amp, lobes, r_in, r_out = 0.5, 3, 10.0, 25.0
    s = nps.rasterize(nps.read_stl(to_binary(wavy_ring(amp=amp, lobes=lobes, r_in=r_in, r_out=r_out))), cell=1.0)
    rep = nps.analyze_surface(s, slope_limit_deg=15.0)
    area = math.pi * (r_out ** 2 - r_in ** 2)
    assert abs(rep.area_mm2 - area) / area < 0.06                                      # el area proyectada coincide con la del anillo
    assert rep.z_min == pytest.approx(3.0 - amp, abs=0.05) and rep.z_max == pytest.approx(3.0 + amp, abs=0.05) and rep.relief == pytest.approx(2 * amp, abs=0.1)
    steepest = math.degrees(math.atan(lobes * amp / r_in))                              # pendiente tangencial maxima en el borde interior
    assert 0.55 * steepest < rep.slope_max_deg < 1.1 * steepest
    assert rep.thick_min == pytest.approx(1.0, abs=0.05) and rep.thick_max == pytest.approx(1.0, abs=0.05)
    assert rep.stacked == 0 and rep.bad_cells == 0 and rep.ok
    expected_hole = math.pi * (r_in - 1.5) ** 2
    assert 0.6 * expected_hole < rep.holes < 1.1 * math.pi * r_in ** 2                 # el agujero central se detecta
    assert any(f.startswith("INFO") and "agujeros" in f for f in rep.findings)


def test_steep_surface_is_an_error():
    s = nps.rasterize(nps.read_stl(to_binary(wavy_ring(amp=1.5, lobes=5, r_in=8.0))), cell=1.0)
    rep = nps.analyze_surface(s, slope_limit_deg=15.0)
    assert not rep.ok and rep.bad_cells > 0 and any("pendiente maxima" in f for f in rep.findings)
    assert any("relieve" in f for f in nps.analyze_surface(s, 60.0, max_relief=1.0).findings)


def test_stacked_plates_are_not_a_25d_surface():
    def plate(z, n=4):
        return [((0, 0, z), (10, 0, z), (10, 10, z)), ((0, 0, z), (10, 10, z), (0, 10, z))]
    s = nps.rasterize(plate(0.0) + plate(3.0), cell=1.0)
    rep = nps.analyze_surface(s, 15.0)
    assert rep.stacked > 0 and not rep.ok and any("2,5D" in f for f in rep.findings)


def test_preview_is_svg_and_marks_steep_cells():
    s = nps.rasterize(nps.read_stl(to_binary(wavy_ring(amp=1.5, lobes=5, r_in=8.0))), cell=1.0)
    rep = nps.analyze_surface(s, 15.0)
    svg = nps.render_surface_preview(s, rep, "anillo.stl")
    assert svg.startswith("<svg") and svg.rstrip().endswith("</svg>") and "SOLO analisis" in svg or "solo analisis" in svg.lower()
    assert "#e74c3c" in svg and "anillo.stl" in svg


def test_cli_analyzes_an_stl_without_generating_gcode(tmp_path, capsys):
    spec = importlib.util.spec_from_file_location("nonplanar_demo", os.path.join(ROOT, "cura", "tools", "nonplanar_demo.py"))
    cli = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cli)
    stl, svg = tmp_path / "anillo.stl", tmp_path / "a.svg"
    stl.write_bytes(to_binary(wavy_ring()))
    assert cli.main(["--analyze-stl", str(stl)]) == 2                                       # sin --enable-experimental no hace nada
    assert cli.main(["--enable-experimental", "--analyze-stl", str(stl), "--preview", str(svg)]) == 0 and svg.exists()
    out = capsys.readouterr().out
    assert "SOLO ANALISIS" in out and "relieve" in out
    assert not list(tmp_path.glob("*.gcode"))
