# SPDX-License-Identifier: LGPL-3.0-or-later
"""Columnas, mapas de bits y pila de capas."""
import math

import pytest

from aite.columns import (ColumnGrid, LayerStack, and_, and_not, components, count_bits, dilate_disk, or_, rows_to_set, set_to_rows)
from aite.mesh import Mesh
from aite.parts import cylinder_volume, make_box, make_cross_part, make_cylinder


def _merge(a: Mesh, b: Mesh) -> Mesh:
    n = len(a.vertices)
    return Mesh(a.vertices + b.vertices, a.triangles + [(x + n, y + n, z + n) for x, y, z in b.triangles])


def test_box_volume_is_exact():
    g = ColumnGrid(make_box((4.0, 4.0, 4.0)), 0.25)
    assert g.volume() == pytest.approx(64.0, rel=1e-9)
    assert not g.bad_columns


def test_cylinder_volume_within_one_percent():
    g = ColumnGrid(make_cylinder(5.0, 3.0, 96), 0.25)
    assert g.volume() == pytest.approx(cylinder_volume(5.0, 3.0, 96), rel=0.01)


def test_cross_part_volume_error_depends_on_grid_alignment():
    """Con la celda alineada a las aristas el volumen es exacto; con una celda que no encaja (3/0,4 no es entero) el muestreo en XY
    cuantiza el contorno y el error es del orden de celda x perimetro: aqui 1,3 %. Es una propiedad del metodo, no un fallo."""
    m = make_cross_part((6, 6, 8), 4, (3, 3), 3.0)
    exact = 6 * 6 * 8 + 2 * 4 * 3 * 3
    assert ColumnGrid(m, 0.25).volume() == pytest.approx(exact, rel=1e-9)
    assert ColumnGrid(m, 0.4).volume() == pytest.approx(exact, rel=0.03)
    assert ColumnGrid(m, 0.1).volume() == pytest.approx(exact, rel=1e-9)


def test_open_mesh_marks_bad_columns():
    m = make_box((4.0, 4.0, 4.0))
    # quita las dos facetas de la tapa superior (z = 4)
    m.triangles = [t for t in m.triangles if not all(abs(m.vertices[i][2] - 4.0) < 1e-9 for i in t)]
    g = ColumnGrid(m, 0.5)
    assert g.bad_columns and not g.intervals


def test_overhang_columns_have_a_floor_above_zero():
    m = make_cross_part((6, 6, 8), 4, (3, 3), 3.0)
    g = ColumnGrid(m, 0.5)
    i = int((5.0 - g.x0) / g.cell)                         # x = 5: bajo la rama derecha (de 3 a 7)
    j = int((0.0 - g.y0) / g.cell)
    ivs = g.intervals[(i, j)]
    assert len(ivs) == 1 and ivs[0] == pytest.approx((3.0, 6.0))
    i_t = int((0.0 - g.x0) / g.cell)
    assert g.intervals[(i_t, j)][0][0] == pytest.approx(0.0)


def test_two_intervals_in_one_column():
    a = make_box((2.0, 2.0, 1.0))
    b = make_box((2.0, 2.0, 1.0), (0.0, 0.0, 2.0))
    g = ColumnGrid(_merge(a, b), 0.5)
    assert all(len(v) == 2 for v in g.intervals.values())
    assert g.volume() == pytest.approx(8.0)


def test_layer_stack_counts_and_volume():
    g = ColumnGrid(make_box((4.0, 4.0, 1.0)), 0.25)
    s = LayerStack.from_grid(g, 0.2)
    assert s.n == 5
    assert all(s.count(k) == 256 for k in range(s.n))
    assert s.volume() == pytest.approx(16.0)
    assert s.z_mid(0) == pytest.approx(0.1)
    assert s.area_mm2(0) == pytest.approx(16.0)


def test_layer_stack_with_overhang_starts_at_branch_layer():
    g = ColumnGrid(make_cross_part((6, 6, 8), 4, (3, 3), 3.0), 0.25)
    s = LayerStack.from_grid(g, 0.2)
    assert s.n == 40
    assert s.count(14) == 24 * 24 and s.count(15) == 24 * 24 + 2 * 16 * 12      # a z = 3,0 aparecen las dos ramas
    assert s.count(29) == 24 * 24 + 2 * 16 * 12 and s.count(30) == 24 * 24       # las ramas terminan en z = 6,0


def test_dilate_disk_shapes():
    one = set_to_rows([(5, 5)], 11)
    assert count_bits(dilate_disk(one, 0, 11)) == 1
    assert rows_to_set(dilate_disk(one, 1, 11)) == {(5, 5), (4, 5), (6, 5), (5, 4), (5, 6)}
    assert count_bits(dilate_disk(one, 2, 11)) == 13
    edge = set_to_rows([(10, 0)], 3)
    d = rows_to_set(dilate_disk(edge, 1, 11))
    assert all(0 <= i < 11 and 0 <= j < 3 for i, j in d)                     # no se sale de la rejilla
    assert (10, 0) in d and (9, 0) in d


def test_set_operations():
    a = set_to_rows([(0, 0), (1, 0), (2, 1)], 3)
    b = set_to_rows([(1, 0), (2, 2)], 3)
    assert rows_to_set(and_(a, b)) == {(1, 0)}
    assert rows_to_set(and_not(a, b)) == {(0, 0), (2, 1)}
    assert rows_to_set(or_(a, b)) == {(0, 0), (1, 0), (2, 1), (2, 2)}


def test_components_use_four_connectivity():
    cells = {(0, 0), (1, 0), (2, 0), (5, 5), (6, 6)}
    comps = sorted(components(cells), key=len, reverse=True)
    assert [len(c) for c in comps] == [3, 1, 1]
    assert components(set()) == []
