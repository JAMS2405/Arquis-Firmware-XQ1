# SPDX-License-Identifier: LGPL-3.0-or-later
"""Mapa proporcional de la cama: posicion fiel, orientacion, tamano minimo, zoom, seleccion y estados (aritmetica entera, paridad con el firmware)."""
import pytest

import arquis_engines.bed_map_engine as bm
from arquis_engines.bed_map_engine import Bed, View, marker, pick, overlaps, to_px, to_bed

VIEW = View(6, 36, 266, 266)           # mapa de la TS35: 266 x 266 px
BED300 = Bed(0, 0, 3000, 3000)


def test_proportional_positions_of_the_owners_example():
    """Ejemplo del enunciado en una cama de 220: objetos en (30,30), (110,30), (180,110), (50,180) mm."""
    bed, v = Bed(0, 0, 2200, 2200), VIEW
    pts = {0: (30, 30), 1: (110, 30), 2: (180, 110), 3: (50, 180)}
    px = {i: to_px(x * 10, y * 10, bed, v) for i, (x, y) in pts.items()}
    scale = (266 - 16) / 2200.0                                                   # px por 0,1 mm
    for i, (x, y) in pts.items():
        ex = 6 + 133 + (x * 10 - 1100) * scale
        ey = 36 + 133 - (y * 10 - 1100) * scale
        assert abs(px[i][0] - ex) <= 0.5 + 1e-9 and abs(px[i][1] - ey) <= 0.5 + 1e-9
    assert px[1][0] - px[0][0] == pytest.approx(80 * 10 * scale, abs=1)          # 80 mm de separacion en X = proporcional
    assert px[0][1] > px[3][1]                                                    # Y mayor (mas atras) -> mas arriba en pantalla


def test_orientation_five_zone_test():
    """Prueba de orientacion (esquina frontal izq./der., centro, posterior izq./der.): coincide con la vista desde el frente."""
    z = {"FL": (400, 400), "FR": (2600, 400), "C": (1500, 1500), "BL": (400, 2600), "BR": (2600, 2600)}
    p = {k: to_px(x, y, BED300, VIEW) for k, (x, y) in z.items()}
    assert p["FL"][0] < p["C"][0] < p["FR"][0] and p["BL"][0] < p["C"][0] < p["BR"][0]            # X: izquierda -> derecha
    assert p["BL"][1] < p["C"][1] < p["FL"][1] and p["BR"][1] < p["C"][1] < p["FR"][1]            # Y: atras ARRIBA, frente ABAJO
    assert p["FL"][0] == p["BL"][0] and p["FR"][0] == p["BR"][0] and p["FL"][1] == p["FR"][1]    # sin inclinacion ni mezcla de ejes
    cx, cy = 6 + 133, 36 + 133
    assert (p["C"][0], p["C"][1]) == (cx, cy)


def test_mapping_is_uniform_square_on_non_square_beds():
    bed = Bed(0, 0, 4000, 2000)                                                    # 400 x 200 mm
    a, b = to_px(0, 0, bed, VIEW), to_px(4000, 2000, bed, VIEW)
    assert (b[0] - a[0]) == 2 * (a[1] - b[1])                                      # escala unica: ancho = 2 x alto


@pytest.mark.parametrize("x,y", [(0, 0), (1234, 2999), (3000, 3000), (1500, 1)])
def test_to_bed_inverts_to_px(x, y):
    px, py = to_px(x, y, BED300, VIEW)
    bx, by = to_bed(px, py, BED300, VIEW)
    tol = 3000 / 250.0                                                              # 1 px
    assert abs(bx - x) <= tol and abs(by - y) <= tol


def test_marker_size_is_proportional_with_a_readable_minimum_and_never_moves():
    big = marker(1, 1500, 1500, 1000, 1000, "S", BED300, VIEW)                    # 100 mm
    small = marker(2, 400, 400, 40, 40, "S", BED300, VIEW)                         # 4 mm
    assert big.w == round(1000 * 250 / 3000) and big.w == big.h
    assert small.w == small.h == bm.MIN_MARKER_PX
    assert (small.cx, small.cy) == to_px(400, 400, BED300, VIEW)                    # el centro REAL: nunca se desplaza
    assert abs((small.x0 + small.x1) / 2 - small.cx) <= 1
    long_ = marker(3, 1500, 500, 2000, 150, "R", BED300, VIEW)
    assert long_.w > 3 * long_.h or long_.h == bm.MIN_MARKER_PX                    # alargado: rectangulo
    c = marker(4, 500, 500, 800, 300, "C", BED300, VIEW)
    assert c.w == c.h                                                               # circulo: caja cuadrada


def test_close_objects_keep_real_positions_and_can_be_picked():
    a = marker(0, 1000, 1000, 100, 100, "S", BED300, VIEW)
    b = marker(1, 1040, 1000, 100, 100, "S", BED300, VIEW)                         # 4 mm mas alla: se solapan en pantalla
    assert overlaps([a, b]) == [(0, 1)]
    assert (b.cx - a.cx) == to_px(1040, 1000, BED300, VIEW)[0] - to_px(1000, 1000, BED300, VIEW)[0]       # no se separaron artificialmente
    assert pick([a, b], a.cx, a.cy) in (0, 1)
    big = marker(2, 1500, 1500, 1500, 1500, "S", BED300, VIEW)
    inner = marker(3, 1500, 1500, 100, 100, "S", BED300, VIEW)
    assert pick([big, inner], inner.cx, inner.cy) == 3                              # la pieza pequena dentro de la grande se puede elegir
    assert pick([big, inner], big.x0, big.y0) == 2
    assert pick([big, inner], 0, 0) == -1


def test_zoom_keeps_center_and_scales_linearly():
    p1 = to_px(1500, 1500, BED300, View(6, 36, 266, 266, 1))
    for z in (2, 4):
        v = View(6, 36, 266, 266, z)
        assert to_px(1500, 1500, BED300, v) == p1                                   # el centro de la cama no se mueve al ampliar
        d1 = to_px(1800, 1500, BED300, View(6, 36, 266, 266, 1))[0] - p1[0]
        dz = to_px(1800, 1500, BED300, v)[0] - p1[0]
        assert abs(dz - z * d1) <= z
    v = View(6, 36, 266, 266, 4, 400, 400)                                          # desplazado a una esquina
    assert to_px(400, 400, BED300, v) == (6 + 133, 36 + 133)


def test_display_id_is_m486_index_plus_one():
    assert [bm.display_id(i) for i in (0, 29)] == [1, 30]


def test_states_have_text_and_badge_not_only_color():
    assert len(set(bm.STATE_TEXT.values())) == 5 and set(bm.STATE_BADGE) == set(bm.STATE_TEXT)
    assert len({bm.STATE_BADGE[s] for s in bm.STATE_BADGE if s != bm.PENDING}) == 4


S = dict(available=True, trusted=True, canceled=False, seq=False, z0=2, ztop=20, order=0, z_print=0, active_order=-1)


def st(**kw):
    return bm.object_state(**{**S, **kw})


def test_state_machine_all_at_once():
    assert st(z_print=0) == bm.PENDING and st(z_print=2) == bm.PRINTING and st(z_print=20) == bm.PRINTING and st(z_print=21) == bm.DONE
    assert st(canceled=True, z_print=5) == bm.CANCELED
    assert st(available=False) == bm.UNAVAILABLE and st(trusted=False, canceled=True) == bm.UNAVAILABLE      # sin verificar: nada se afirma


def test_state_machine_sequential():
    for order, active, want in ((0, -1, bm.PENDING), (0, 0, bm.PRINTING), (0, 1, bm.DONE), (2, 1, bm.PENDING), (1, 1, bm.PRINTING)):
        assert st(seq=True, order=order, active_order=active) == want
