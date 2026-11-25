# SPDX-License-Identifier: LGPL-3.0-or-later
"""Las formulas de aite.physics reproducen las cifras del informe AITE (D.1 a D.11)."""
import math

import pytest

from aite import physics as P


def test_beta_of_normal_z():
    assert P.beta_of_normal_z(-1.0) == pytest.approx(90.0)
    assert P.beta_of_normal_z(-math.sqrt(0.5)) == pytest.approx(45.0)
    assert P.beta_of_normal_z(0.0) == 0.0            # pared vertical
    assert P.beta_of_normal_z(0.5) == 0.0            # cara que mira hacia arriba: no es voladizo
    assert P.beta_of_normal_z(-1.0000001) == pytest.approx(90.0)   # redondeo numerico
    with pytest.raises(ValueError):
        P.beta_of_normal_z(float("nan"))


def test_contact_fraction_table_of_report():
    assert P.contact_fraction(45, 0.2, 0.45) == pytest.approx(0.56, abs=0.005)
    assert P.contact_fraction(50, 0.2, 0.45) == pytest.approx(0.47, abs=0.005)
    assert P.contact_fraction(55, 0.2, 0.45) == pytest.approx(0.37, abs=0.005)
    assert P.contact_fraction(60, 0.2, 0.45) == pytest.approx(0.23, abs=0.005)
    assert P.contact_fraction(65, 0.16, 0.45) == pytest.approx(0.24, abs=0.005)


def test_45_degree_rule_is_half_overlap_only_when_width_is_twice_height():
    assert P.contact_fraction(45, 0.2, 0.4) == pytest.approx(0.5)
    assert P.contact_fraction(45, 0.2, 0.45) != pytest.approx(0.5, abs=0.01)


def test_beta_c_values_of_report():
    assert P.beta_c_deg(0.2, 0.45, 0.5) == pytest.approx(48.4, abs=0.05)
    assert P.beta_c_deg(0.2, 0.45, 0.4) == pytest.approx(53.5, abs=0.05)
    assert P.beta_c_deg(0.2, 0.45, 0.3) == pytest.approx(57.6, abs=0.05)
    assert P.beta_c_deg(0.16, 0.45, 0.4) == pytest.approx(59.3, abs=0.05)
    # inversa de contact_fraction
    bc = P.beta_c_deg(0.2, 0.45, 0.37)
    assert P.contact_fraction(bc, 0.2, 0.45) == pytest.approx(0.37)


def test_required_c_min_for_90_degrees_of_report():
    assert P.required_c_min_for_overhang(90, 33.7, 0.2, 0.45) == pytest.approx(0.33, abs=0.01)
    assert P.required_c_min_for_overhang(90, 33.7, 0.16, 0.45) == pytest.approx(0.47, abs=0.01)
    assert P.required_c_min_for_overhang(40, 60, 0.2, 0.45) == pytest.approx(1.0)       # voladizo menor que la inclinacion: apoyo total


def test_tip_step_per_layer():
    assert P.tip_step_per_layer(0.2, 36) == pytest.approx(0.34, abs=0.005)
    assert P.tip_step_per_layer(0.2, 45) == pytest.approx(0.283, abs=0.001)


def test_alpha_max_from_clearance_values_of_report():
    assert P.alpha_max_from_clearance(8.28, 15.715) == pytest.approx(27.8, abs=0.1)
    assert P.alpha_max_from_clearance(8.245, 20.15) == pytest.approx(22.25, abs=0.1)
    assert P.alpha_max_from_clearance(14.5, 39.6) == pytest.approx(20.1, abs=0.1)
    assert P.alpha_max_from_clearance(8.0, 16.13) == pytest.approx(26.4, abs=0.1)


def test_tilt_window():
    assert P.max_feasible_overhang(48.4, 20.0) == pytest.approx(68.4)
    assert P.required_tilt(90, 55) == 35.0
    assert P.required_tilt(90, 55, margin=3) == 38.0
    assert P.required_tilt(40, 55) == 0.0
    assert P.tilt_deficit(90, 48.4, 20.1) == pytest.approx(21.5)
    assert P.tilt_deficit(60, 55, 20) == 0.0


def test_bead_area_and_filament():
    assert P.bead_area(0.45, 0.2) == pytest.approx(0.0814, abs=0.0005)
    d = 1.75
    assert P.filament_per_mm(0.45, 0.2, d) == pytest.approx(P.bead_area(0.45, 0.2) / (math.pi * d * d / 4))
    assert P.filament_per_mm(0.45, 0.2, d, flow=1.1) == pytest.approx(1.1 * P.filament_per_mm(0.45, 0.2, d))


def test_path_speed_limit():
    assert P.path_speed_limit(8, 30) == pytest.approx(16.0)
    assert P.path_speed_limit(8, 39) == pytest.approx(12.7, abs=0.05)
    assert math.isinf(P.path_speed_limit(8, 0))


@pytest.mark.parametrize("call", [
    lambda: P.contact_fraction(45, 0.5, 0.4),        # w < t
    lambda: P.contact_fraction(90, 0.2, 0.45),       # beta_rel = 90
    lambda: P.beta_c_deg(0.2, 0.45, 1.5),
    lambda: P.bead_area(0.1, 0.2),
    lambda: P.tip_step_per_layer(0.2, 0),
    lambda: P.alpha_max_from_clearance(0, 10),
    lambda: P.path_speed_limit(0, 30),
    lambda: P.filament_per_mm(0.45, 0.2, filament_d=0),
])
def test_domain_errors(call):
    with pytest.raises(ValueError):
        call()
