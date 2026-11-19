# SPDX-License-Identifier: LGPL-3.0-or-later
"""Probeta cubica para el ondulado no planar (generador propio, NO es Cura): estructura, limites y flujo completo con el perfil de la probeta."""
import importlib.util
import json
import math
import os
import re

import pytest

import arquis_bootstrap

arquis_bootstrap.load_engines()
import arquis_engines.nonplanar_engine as npe  # noqa: E402
import arquis_engines.nonplanar_warp as nw  # noqa: E402
from arquis_engines.marlin_model import simulate  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
_spec = importlib.util.spec_from_file_location("make_probe_cube", os.path.join(ROOT, "cura", "tools", "make_probe_cube.py"))
probe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(probe)


def profile():
    with open(os.path.join(ROOT, "cura", "profiles", "nonplanar_probeta_cubo_30mm.json"), encoding="utf-8") as f:
        d = {k: v for k, v in json.load(f).items() if not k.startswith("_")}
    d["enabled"] = True
    return nw.warp_config_from_dict(d)


def test_probe_is_deterministic_and_has_the_cura_structure_with_the_owners_start_and_end():
    a = probe.build(30.0)
    assert a == probe.build(30.0)
    L = a.splitlines()
    assert L[0] == ";FLAVOR:Marlin" and "M420 S1" in a and "M140 S70" in a and "G1 Z5 F3000" in a and a.rstrip().splitlines()[-1] == ";End of Gcode"
    assert sum(1 for l in L if l.startswith(";LAYER:")) == 150 and sum(1 for l in L if l.startswith(";TIME_ELAPSED:")) == 150 and ";LAYER_COUNT:150" in L
    assert a.index(";LAYER:0") > a.index("G1 F2100 E-0.8") and "M486" not in a                              # sin objetos cancelables: modo sin manifiesto


def test_probe_stays_inside_the_bed_and_the_cube_has_the_requested_size():
    a = probe.build(30.0)
    zone = a[a.index(";LAYER:0"):a.index("M140 S0\nM107")]
    xs = [float(m) for m in re.findall(r"X([\d.]+)", zone)]
    ys = [float(m) for m in re.findall(r"Y([\d.]+)", zone)]
    zs = [float(m) for m in re.findall(r"Z([\d.]+)", zone)]
    assert 135 <= min(xs) and max(xs) <= 165 and 135 <= min(ys) and max(ys) <= 165 and max(zs) == pytest.approx(30.0) and min(zs) == pytest.approx(0.2)
    assert simulate([a], capacity=30).final[2] == pytest.approx(35.0)                                        # el final del usuario levanta 5 mm sobre la ultima capa (30 mm)


def test_probe_extrusion_matches_the_geometry():
    a = probe.build(30.0)
    e_max = max(float(m) for m in re.findall(r" E([\d.]+)", a[a.index(";LAYER:0"):]))
    vol = e_max * math.pi * (1.75 / 2) ** 2
    assert 0.12 * 30 ** 3 < vol < 0.6 * 30 ** 3                                                              # 20 % de relleno + paredes + capas solidas: entre 12 % y 60 % del cubo


def test_warp_flow_on_the_probe_is_safe_volume_preserving_and_flat_on_top():
    src = probe.build(30.0)
    r = nw.warp_gcode(src, profile())
    assert r.ok, [f.msg for f in r.findings if f.level == "ERROR"]
    assert r.stats["e_out"] == pytest.approx(r.stats["e_in"], rel=0.005) and 0.85 <= r.stats["mult_min"] < 1.0 < r.stats["mult_max"] <= 1.15
    assert r.stats["z_max"] == pytest.approx(30.0, abs=1e-3)                                                 # la ultima capa queda plana: ni un decimo por encima de 30 mm
    assert r.stats["pend_superficie_deg"] < 10 and r.stats["espesor_min"] >= 0.15 and r.stats["espesor_max"] <= 0.25
    last = [l for l in r.text.splitlines() if l.startswith("G1") and " E" in l and "Z" in l][-200:]
    assert all(abs(float(re.search(r"Z([\d.]+)", l).group(1)) - 30.0) < 1e-3 for l in last)                  # capa superior plana (z_out = 30)
    # tramos planos al principio: nada se toca por debajo de z_in
    first = r.text[r.text.index(";LAYER:0"):r.text.index(";LAYER:3")]
    ext = [l for l in first.splitlines() if l.startswith("G1") and " E" in l and "Z" in l and not l.startswith("G1 F2100")]      # lineas que EXTRUYEN en las capas 0-2 (< z_in = 0.8)
    assert ext and {round(float(re.search(r"Z([\d.]+)", l).group(1)), 3) for l in ext} == {0.2, 0.4, 0.6}                    # solo sus Z nominales: sin deformar


def test_marlin_model_runs_both_files_with_the_same_filament_and_the_same_end():
    src = probe.build(30.0)
    r = nw.warp_gcode(src, profile())
    a, b = simulate([src], capacity=30), simulate([r.text], capacity=30)
    assert b.physical_e == pytest.approx(a.physical_e, rel=0.005) and (a.final[0], a.final[1]) == (b.final[0], b.final[1]) and b.final[2] >= a.final[2] - 1e-6
    assert len(r.text) < 8_000_000 and r.text.count("\n") < 140_000                                           # tamano manejable para la SD


def test_probe_export_needs_the_preview_code_and_a_confirmed_head():
    r = nw.warp_gcode(probe.build(30.0), profile())
    with pytest.raises(npe.ExportRefused, match="codigo"):
        nw.export_warped(r, "000000000000")
    assert nw.export_warped(r, r.digest) == r.text
    cfg = profile()
    cfg.head.confirmed = False
    unconf = nw.warp_gcode(probe.build(30.0), cfg)
    with pytest.raises(npe.ExportRefused, match="cabezal"):
        nw.export_warped(unconf, unconf.digest)
