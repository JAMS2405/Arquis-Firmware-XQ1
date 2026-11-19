# SPDX-License-Identifier: LGPL-3.0-or-later
"""Lamina en espiral (tecnica de la flor) desde una funcion o un STL. EXPERIMENTAL: pruebas de calculo y de modelo; nada se ha probado en la impresora."""
import importlib.util
import json
import math
import os
import re
import struct

import pytest

import arquis_bootstrap

arquis_bootstrap.load_engines()
import arquis_engines.nonplanar_engine as npe  # noqa: E402
import arquis_engines.nonplanar_sheet as nsh  # noqa: E402
import arquis_engines.nonplanar_surface as nps  # noqa: E402
from arquis_engines.marlin_model import simulate  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def cfg(**kw):
    head = kw.pop("head", npe.HeadModel(nozzle_clearance_deg=40.0, confirmed=True))
    kw.setdefault("bed_temp", 50); kw.setdefault("nozzle_temp", 210); kw.setdefault("fan_speed", 255)
    return npe.NPConfig(enabled=True, head=head, **kw)


def errors(ap):
    return {f.code for f in ap.findings if f.level == "ERROR"}


def mesh_from_field(f, half, cell=0.5, thick=0.5, hole=None):
    """Triangula z=f(x,y) sobre la rejilla donde haya material (cara superior +Z y cara inferior -Z). ``hole(x,y)`` vacia una zona."""
    n = int(half / cell)
    ins = lambda x, y: f.inside(x, y) and not (hole and hole(x, y))
    tris = []
    for i in range(-n, n):
        for j in range(-n, n):
            xs, ys = (i * cell, (i + 1) * cell), (j * cell, (j + 1) * cell)
            if not all(ins(x, y) for x in xs for y in ys):
                continue
            a, b, c, d = [(x, y, f.z(x, y)) for x, y in ((xs[0], ys[0]), (xs[1], ys[0]), (xs[1], ys[1]), (xs[0], ys[1]))]
            tris += [(a, b, c), (a, c, d)]
            lo = lambda p: (p[0], p[1], p[2] - thick)
            tris += [(lo(a), lo(c), lo(b)), (lo(a), lo(d), lo(c))]
    return tris


def to_binary(tris):
    out = bytearray(80) + struct.pack("<I", len(tris))
    for a, b, c in tris:
        out += struct.pack("<12fH", 0, 0, 0, *a, *b, *c, 0)
    return bytes(out)


# ---------------------------------------------------------------- la flor parametrica
def test_parametric_flower_generates_a_safe_spiral_sheet():
    ap = nsh.generate_sheet(nsh.flower_field(), cfg())
    assert ap.ok, [f.msg for f in ap.findings if f.level == "ERROR"]
    st = ap.stats
    assert st["vueltas"] == 19 and 10 < st["radio_min"] < st["radio_max"] <= 15.01
    assert st["z_min"] == pytest.approx(0.3, abs=1e-3)                                    # la primera vuelta esta sobre la cama (espesor de la lamina)
    assert st["vz_extrusion_mm_s"] <= 0.9 * 5.0 and st["az_est_mm_s2"] < 100 and st["pendiente_entre_vueltas_deg"] < 15
    lines = ap.text.splitlines()
    assert lines[0].startswith(npe.MARKER) and npe.verify_gcode(ap.text, ap.cfg) == []
    names = {l.split()[0] for l in lines if l and not l.startswith(";")}
    assert names <= set(npe.ALLOWED_CMDS) and "G92 X" not in ap.text and "G91" not in ap.text


def test_the_spiral_grows_outward_each_turn_resting_on_the_previous_one():
    f = nsh.flower_field()
    ap = nsh.generate_sheet(f, cfg())
    cx, cy = cfg().place_x, cfg().place_y
    r = [math.hypot(p[0] - cx, p[1] - cy) for p in ap.path]
    assert r[0] < 1.0 and max(r) == pytest.approx(r[-1], abs=0.2) and r[-1] > 10                          # del centro hacia afuera
    # el radio al cruzar el angulo 0 crece en cada vuelta (cada vuelta queda por fuera de la anterior)
    ang = [math.atan2(p[1] - cy, p[0] - cx) % (2 * math.pi) for p in ap.path]
    cross = [r[k] for k in range(1, len(r)) if ang[k - 1] > 5.5 and ang[k] < 0.8]
    assert len(cross) >= int(ap.stats["vueltas"]) - 2 and all(b > a_ for a_, b in zip(cross, cross[1:]))
    # z sigue la superficie: el punto de mayor z esta en el borde de un petalo y es 0,3 + (zmax - zmin) de la superficie
    assert ap.stats["z_max"] == pytest.approx(0.3 + (max(f.z(x, y) for x, y in ((a * 0.25, b * 0.25) for a in range(-60, 61) for b in range(-60, 61) if f.inside(a * 0.25, b * 0.25))) - f.z(0.0, 0.0)), abs=0.1)


def test_extrusion_volume_matches_the_sheet():
    ap = nsh.generate_sheet(nsh.flower_field(), cfg())
    fil = math.pi * (1.75 / 2) ** 2
    vol = ap.stats["e_total_mm"] * fil
    area = 0.0
    n = 720
    f = nsh.flower_field()
    for k in range(n):                                                                   # area de la flor integrando R(theta)^2 / 2
        t = 2 * math.pi * k / n
        area += 0.5 * (13.0 + 2.0 * math.cos(5 * t)) ** 2 * (2 * math.pi / n)
    expected = area * 0.3 * (1 + 0.1)                                                    # espesor 0,3 mm (+ un poco por la pendiente y la forma de la seccion)
    assert 0.6 < vol / expected < 1.3, (vol, expected)


def test_gcode_is_executable_by_the_marlin_model_with_relative_e():
    ap = nsh.generate_sheet(nsh.flower_field(), cfg())
    tr = simulate([ap.text], capacity=30)
    x, y, z = tr.final
    assert 0 < x < 300 and 0 < y < 300 and z >= 20.0
    assert tr.physical_e == pytest.approx(ap.stats["e_total_mm"] - ap.cfg.retract_mm, abs=0.05)


# ---------------------------------------------------------------- STL propio
def test_stl_sheet_matches_the_parametric_one():
    f = nsh.flower_field()
    surf = nps.rasterize(nps.read_stl(to_binary(mesh_from_field(f, 16.0))), cell=0.5)
    fs = nsh.field_from_surface(surf, center=(0.0, 0.0), name="flor.stl")
    ap = nsh.generate_sheet(fs, cfg())
    ref = nsh.generate_sheet(f, cfg())
    assert ap.ok, [x.msg for x in ap.findings if x.level == "ERROR"]
    assert abs(ap.stats["vueltas"] - ref.stats["vueltas"]) <= 1
    assert ap.stats["radio_max"] == pytest.approx(ref.stats["radio_max"], abs=0.8) and ap.stats["z_max"] == pytest.approx(ref.stats["z_max"], abs=0.15)


def test_default_start_is_the_lowest_point_of_the_surface():
    f = nsh.flower_field()
    surf = nps.rasterize(nps.read_stl(to_binary(mesh_from_field(f, 16.0))), cell=0.5)
    fs = nsh.field_from_surface(surf)
    assert math.hypot(*fs.center) < 1.0                                                  # el centro de la flor (la zona mas baja)


def test_ring_with_a_hole_is_refused_with_an_explanation():
    f = nsh.flower_field(r0=20.0, dr=0.0)
    ring = mesh_from_field(f, 22.0, hole=lambda x, y: math.hypot(x, y) < 7.0)
    surf = nps.rasterize(nps.read_stl(to_binary(ring)), cell=0.5)
    with pytest.raises(nsh.SheetRefused, match="agujero|no estrellada"):
        nsh.generate_sheet(nsh.field_from_surface(surf), cfg())                         # el punto mas bajo cae en el anillo: el rayo cruza el agujero
    with pytest.raises(nsh.SheetRefused, match="no esta dentro"):
        nsh.generate_sheet(nsh.field_from_surface(surf, center=(0.0, 0.0)), cfg())     # arrancar en el agujero


def test_start_in_the_air_too_steep_and_elongated_shapes_are_errors():
    f = nsh.flower_field()
    surf = nps.rasterize(nps.read_stl(to_binary(mesh_from_field(f, 16.0))), cell=0.5)
    dome = nsh.SheetField(lambda x, y: 2.0 - 0.1 * math.hypot(x, y), lambda x, y: math.hypot(x, y) <= 10.0, (0.0, 0.0), 12.0, "cupula")     # lo mas bajo es el borde, no el centro
    assert "SHEET_ARRANQUE" in errors(nsh.generate_sheet(dome, cfg()))                  # arrancar en el centro seria empezar a 1 mm en el aire
    steep = nsh.generate_sheet(nsh.flower_field(height=4.0), cfg())
    assert {"SHEET_APOYO"} <= errors(steep) and not steep.ok
    ellipse = nsh.SheetField(lambda x, y: 0.0, lambda x, y: (x / 25.0) ** 2 + (y / 5.0) ** 2 <= 1.0, (0.0, 0.0), 30.0, "elipse")
    assert "SHEET_ESPACIADO" in errors(nsh.generate_sheet(ellipse, cfg()))
    tiny = nsh.SheetField(lambda x, y: 0.0, lambda x, y: math.hypot(x, y) <= 1.5, (0.0, 0.0), 5.0, "minima")
    with pytest.raises(nsh.SheetRefused, match="pequena"):
        nsh.generate_sheet(tiny, cfg())


def test_out_of_bed_and_head_collision_are_detected():
    ap = nsh.generate_sheet(nsh.flower_field(), cfg(place_x=5.0, place_y=5.0))
    assert "EXT_LIMITES" in errors(ap) or "BED" in errors(ap)
    low = nsh.generate_sheet(nsh.flower_field(height=2.0), cfg(max_slope_deg=40.0, head=npe.HeadModel(nozzle_clearance_deg=60.0, envelopes=(("bloque", 12.0, 0.5),), confirmed=True)))
    assert "COLISION_CABEZAL" in errors(low)


# ---------------------------------------------------------------- barreras
def test_disabled_by_default_and_material_is_explicit():
    with pytest.raises(npe.NotEnabled):
        nsh.generate_sheet(nsh.flower_field(), npe.NPConfig())
    with pytest.raises(nsh.SheetRefused, match="no asume el material"):
        nsh.generate_sheet(nsh.flower_field(), npe.NPConfig(enabled=True, head=npe.HeadModel(confirmed=True)))
    with pytest.raises(nsh.SheetRefused, match="no permitida"):
        nsh.generate_sheet(nsh.flower_field(), cfg(custom_start_lines=("G1 X10",)))


def test_export_needs_confirmed_head_and_the_preview_code():
    ap = nsh.generate_sheet(nsh.flower_field(), cfg())
    with pytest.raises(npe.ExportRefused, match="codigo"):
        npe.export_adapted(ap, "AAAAAAAAAAAA")
    unconf = nsh.generate_sheet(nsh.flower_field(), cfg(head=npe.HeadModel()))
    with pytest.raises(npe.ExportRefused, match="cabezal"):
        npe.export_adapted(unconf, unconf.digest)
    assert npe.export_adapted(ap, ap.digest) == ap.text and ap.digest in ap.text.splitlines()[1]
    assert ap.digest != nsh.generate_sheet(nsh.flower_field(), cfg(sheet_w=0.9)).digest


def test_cli_sheet_demo_and_stl_with_every_barrier(tmp_path, capsys):
    spec = importlib.util.spec_from_file_location("nonplanar_demo", os.path.join(ROOT, "cura", "tools", "nonplanar_demo.py"))
    cli = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cli)
    cfgf, out, svg, stl = tmp_path / "c.json", tmp_path / "o.gcode", tmp_path / "v.svg", tmp_path / "flor.stl"
    d = {"head": {"confirmed": False, "nozzle_clearance_deg": 40.0}, "bed_temp": 50, "nozzle_temp": 210, "fan_speed": 255}
    cfgf.write_text(json.dumps(d))
    assert cli.main(["--sheet-demo"]) == 2                                                # sin --enable-experimental no hace nada
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--sheet-demo", "--preview", str(svg), "--export", str(out), "--accept", "000000000000"]) == 3
    assert svg.exists() and not out.exists()
    d["head"]["confirmed"] = True
    cfgf.write_text(json.dumps(d))
    capsys.readouterr()
    cli.main(["--enable-experimental", "--config", str(cfgf), "--sheet-demo"])
    code = re.findall(r"CODIGO DE VISTA PREVIA: ([0-9A-F]{12})", capsys.readouterr().out)[-1]
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--sheet-demo", "--export", str(out), "--accept", code]) == 0 and out.exists()
    stl.write_bytes(to_binary(mesh_from_field(nsh.flower_field(), 16.0)))
    capsys.readouterr()
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--sheet-stl", str(stl), "--center", "0,0"]) == 0
    ring = tmp_path / "anillo.stl"
    ring.write_bytes(to_binary(mesh_from_field(nsh.flower_field(r0=20.0, dr=0.0), 22.0, hole=lambda x, y: math.hypot(x, y) < 7.0)))
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--sheet-stl", str(ring)]) == 3
    assert "RECHAZADO" in capsys.readouterr().out
