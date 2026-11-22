# SPDX-License-Identifier: LGPL-3.0-or-later
"""Piezas de prueba: estancas y con volumen y angulos exactos por formula (puerta de la Fase 1: error <= 1 %)."""
import math

import pytest

from aite.mesh import analyze_mesh
from aite.overhang import facet_angles
from aite.parts import (cross_part_volume, cylinder_volume, make_box, make_cross_part, make_cylinder, make_extruded_profile,
                        make_overhang_wedge)


def test_cylinder_volume_matches_formula_and_circle():
    m = make_cylinder(10.0, 7.0, 96)
    r = analyze_mesh(m)
    assert r.watertight
    assert r.volume == pytest.approx(cylinder_volume(10.0, 7.0, 96), rel=1e-9)
    assert r.volume == pytest.approx(math.pi * 100 * 7.0, rel=1e-3)           # < 0,1 % del cilindro ideal


def test_cylinder_rejects_bad_arguments():
    for args in ((0, 1, 96), (1, 0, 96), (1, 1, 4)):
        with pytest.raises(ValueError):
            make_cylinder(*args)


def test_overhang_wedge_has_exact_angle_and_volume():
    for ang in (30.0, 45.0, 70.0):
        m = make_overhang_wedge(ang, base=3.0, height=3.0, width=2.0)
        r = analyze_mesh(m)
        assert r.watertight and not r.inverted
        run = 3.0 * math.tan(math.radians(ang))
        assert r.volume == pytest.approx((3.0 + run / 2) * 3.0 * 2.0)          # trapecio por el ancho
        facets, bed, _ = facet_angles(m)
        assert max(f.beta_deg for f in facets) == pytest.approx(ang, abs=1e-6)
        assert bed == pytest.approx(3.0 * 2.0)                                  # la cara inferior descansa en la cama


def test_wedge_with_zero_angle_is_a_box():
    assert analyze_mesh(make_overhang_wedge(0.0)).volume == pytest.approx(3 * 3 * 2)


def test_extruded_profile_rejects_clockwise_and_degenerate():
    with pytest.raises(ValueError):
        make_extruded_profile([(0, 0), (0, 1), (1, 1), (1, 0)], 0, 1)         # horario
    with pytest.raises(ValueError):
        make_extruded_profile([(0, 0), (1, 0)], 0, 1)
    with pytest.raises(ValueError):
        make_extruded_profile([(0, 0), (1, 0), (0, 1)], 1, 1)


def test_cross_part_default_is_the_report_part():
    m = make_cross_part()
    r = analyze_mesh(m)
    assert r.watertight and not r.inverted
    assert r.volume == pytest.approx(cross_part_volume())                       # 20x20x40 + 2 ramas de 15x10x10 = 19000 mm3
    assert r.volume == pytest.approx(19000.0)
    (x0, y0, z0), (x1, y1, z1) = r.bbox
    assert (x0, x1, y0, y1, z0, z1) == pytest.approx((-25.0, 25.0, -10.0, 10.0, 0.0, 40.0))


def test_cross_part_variants():
    one = analyze_mesh(make_cross_part(both=False, axis="y"))
    assert one.watertight and one.volume == pytest.approx(cross_part_volume(both=False))
    assert one.bbox[0][1] == pytest.approx(-10.0) and one.bbox[1][1] == pytest.approx(25.0)
    small = analyze_mesh(make_cross_part((6, 6, 8), 4, (3, 3), 3.0))
    assert small.watertight and small.volume == pytest.approx(6 * 6 * 8 + 2 * 4 * 3 * 3)


def test_cross_part_rejects_inconsistent_dimensions():
    with pytest.raises(ValueError):
        make_cross_part(branch_z0=33.0)                    # la rama no cabe en la altura del tronco
    with pytest.raises(ValueError):
        make_cross_part(branch_len=15.3)                   # no es multiplo de la celda
    with pytest.raises(ValueError):
        make_cross_part(axis="z")
    with pytest.raises(ValueError):
        make_box((0.0, 1.0, 1.0))
