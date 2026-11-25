# SPDX-License-Identifier: LGPL-3.0-or-later
"""Detector de voladizos (facetas y capas) y solucionador de angulo por region."""
import pytest

from aite.columns import ColumnGrid, LayerStack
from aite.mesh import Mesh
from aite.overhang import (analyze_overhangs, facet_angles, facet_report, find_overhang_regions, support_radius_cells)
from aite.parts import make_box, make_cross_part, make_overhang_wedge

SMALL = dict(trunk=(6, 6, 8), branch_len=4, branch_section=(3, 3), branch_z0=3.0)
BETA_C = 55.0


def test_box_has_no_overhang_and_base_is_ignored():
    m = make_box((4.0, 4.0, 4.0))
    facets, bed, flipped = facet_angles(m)
    assert facets == [] and bed == pytest.approx(16.0) and not flipped
    rep = facet_report(m, BETA_C, 20.0)
    assert rep.overhang_area == 0 and rep.infeasible_area == 0 and rep.max_beta_deg == 0


def test_floating_box_underside_is_an_overhang_when_not_on_the_bed():
    # una caja con su base por encima de otra: la cara inferior de la superior (z = 2) mira hacia abajo y NO descansa en la cama
    a = make_box((4.0, 4.0, 1.0))
    b = make_box((4.0, 4.0, 1.0), (0.0, 0.0, 2.0))
    n = len(a.vertices)
    m = Mesh(a.vertices + b.vertices, a.triangles + [(x + n, y + n, z + n) for x, y, z in b.triangles])
    rep = facet_report(m, BETA_C, 20.0)
    assert rep.overhang_area == pytest.approx(16.0) and rep.max_beta_deg == pytest.approx(90.0)


def test_inverted_mesh_is_analyzed_with_flipped_normals_without_modifying_it():
    m = make_cross_part(**SMALL)
    ok = facet_report(m, BETA_C, 20.0)
    before = list(m.triangles)
    m.triangles = [(a, c, b) for a, b, c in m.triangles]
    inv = facet_report(m, BETA_C, 20.0)
    assert inv.normals_flipped and inv.overhang_area == pytest.approx(ok.overhang_area)
    assert m.triangles != before and m.triangles == [(a, c, b) for a, b, c in before]      # el modelo no se toco


def test_facet_report_of_cross_part_matches_underside_area():
    rep = facet_report(make_cross_part(**SMALL), BETA_C, 20.0)
    assert rep.overhang_area == pytest.approx(2 * 4 * 3)                        # las dos caras inferiores de las ramas
    assert rep.max_beta_deg == pytest.approx(90.0)
    assert rep.infeasible_area == pytest.approx(2 * 4 * 3)                      # 90 > 55 + 20
    assert rep.max_deficit_deg == pytest.approx(15.0)
    assert sum(a for _, _, a in rep.bins) == pytest.approx(2 * 4 * 3)


def test_support_radius_cells():
    assert support_radius_cells(0.2, 55.0, 0.25) == 1
    assert support_radius_cells(0.2, 55.0, 0.1) == 3
    assert support_radius_cells(0.2, 45.0, 0.1) == 2


def _stack(mesh, h=0.2, cell=0.25):
    return LayerStack.from_grid(ColumnGrid(mesh, cell), h)


def test_cross_part_has_two_cantilever_regions_at_the_branch_floor():
    res = analyze_overhangs(make_cross_part(**SMALL), BETA_C, 20.0, 0.2, 0.25)
    assert res.mesh_report.watertight and res.bad_columns == 0
    assert len(res.regions) == 2
    assert {r.layer for r in res.regions} == {15}
    assert all(r.kind == "voladizo" for r in res.regions)
    assert sorted(r.sides for r in res.regions) == ["E", "W"]
    for r in res.regions:
        assert 10.5 <= r.area_mm2 <= 12.0                                       # 4 x 3 mm menos la banda de apoyo junto al tronco
        assert 3.5 <= r.depth_mm <= 4.0
        assert r.z_mm == pytest.approx(3.0)
        assert r.beta_s_deg == pytest.approx(90.0)
        assert r.required_tilt_deg == pytest.approx(35.0)
        assert r.deficit_deg == pytest.approx(15.0) and r.feasible is False
    assert len(res.infeasible_regions) == 2 and res.max_deficit_deg == pytest.approx(15.0)


def test_cross_part_is_feasible_when_the_window_is_wide_enough():
    res = analyze_overhangs(make_cross_part(**SMALL), BETA_C, 40.0, 0.2, 0.25, margin_deg=3.0)
    assert not res.infeasible_regions and res.max_deficit_deg == 0.0
    assert all(r.required_tilt_deg == pytest.approx(38.0) and r.feasible for r in res.regions)


def test_regions_follow_the_branch_axis_and_single_branch():
    res = analyze_overhangs(make_cross_part(**SMALL, axis="y", both=False), BETA_C, 20.0, 0.2, 0.25)
    assert len(res.regions) == 1 and res.regions[0].sides == "S"                 # rama hacia +y: el tronco queda al sur


def test_island_and_bridge_classification():
    # puente: losa apoyada en dos pilares, separada del suelo; isla: caja flotante sin nada debajo
    from aite.parts import mesh_from_voxels
    vox = set()
    for i in range(0, 4):
        for j in range(0, 4):
            for k in range(0, 4):
                vox.add((i, j, k))
                vox.add((i + 8, j, k))
            for k in range(4, 6):
                for i2 in range(0, 12):
                    vox.add((i2, j, k))
    res = analyze_overhangs(mesh_from_voxels(vox, 0.5), BETA_C, 20.0, 0.2, 0.25)
    kinds = {r.kind for r in res.regions}
    assert "puente" in kinds
    a = make_box((2.0, 2.0, 1.0))
    b = make_box((2.0, 2.0, 1.0), (10.0, 10.0, 3.0))                             # flotando y sin tocar nada
    n = len(a.vertices)
    isl = analyze_overhangs(Mesh(a.vertices + b.vertices, a.triangles + [(x + n, y + n, z + n) for x, y, z in b.triangles]), BETA_C, 20.0, 0.2, 0.25)
    assert [r.kind for r in isl.regions] == ["isla"] and isl.regions[0].sides == "" and isl.regions[0].depth_mm == 0.0


def test_wedge_at_45_degrees_needs_no_inclination():
    # desplazamiento por capa = h*tan(45) = 0,2 mm < s_c = 0,286 mm
    res = analyze_overhangs(make_overhang_wedge(45.0), BETA_C, 20.0, 0.2, 0.1)
    assert res.regions == []
    assert res.facets.overhang_area == 0 and res.max_deficit_deg == 0


def test_wedge_at_70_degrees_has_a_region_in_every_layer_and_is_feasible():
    res = analyze_overhangs(make_overhang_wedge(70.0), BETA_C, 20.0, 0.2, 0.1)
    assert res.layers == 15 and len(res.regions) >= 12
    assert all(r.kind == "voladizo" for r in res.regions)
    assert all(r.beta_s_deg == pytest.approx(70.0, abs=0.5) for r in res.regions)      # toda la cara inclinada se asocia a cada capa
    assert all(r.required_tilt_deg == pytest.approx(15.0, abs=0.5) and r.feasible for r in res.regions)
    assert res.max_deficit_deg == 0.0


def test_wedge_at_80_degrees_is_outside_the_window_by_5_degrees():
    res = analyze_overhangs(make_overhang_wedge(80.0), BETA_C, 20.0, 0.2, 0.1)
    assert res.regions and all(r.deficit_deg == pytest.approx(5.0, abs=0.5) for r in res.regions)
    assert res.facets.max_deficit_deg == pytest.approx(5.0, abs=1e-6)
    assert analyze_overhangs(make_overhang_wedge(80.0), BETA_C, 30.0, 0.2, 0.1).max_deficit_deg == 0.0


def test_critical_angle_at_90_degrees_disables_layer_detection():
    assert find_overhang_regions(_stack(make_cross_part(**SMALL)), 89.95) == []


def test_min_cells_filters_noise():
    stack = _stack(make_cross_part(**SMALL))
    assert len(find_overhang_regions(stack, BETA_C, min_cells=2)) == 2
    assert find_overhang_regions(stack, BETA_C, min_cells=10 ** 6) == []
