"""El validador debe detectar cada tipo de G-code inseguro (mutaciones de una salida correcta)."""
import pytest

from arquis_engines.m486_engine import M486Options, process_gcode
from arquis_engines.validation_engine import validate_m486
from fixtures import grid_objects, instances_from, make_gcode


@pytest.fixture(scope="module")
def good():
    objs = grid_objects(4, "A", height=1.5)
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options())
    assert res.status == "ok"
    return res


def codes(chunks, objects):
    return {p.code for p in validate_m486(chunks, 30, objects) if p.severity == "error"}


def mutate(good, fn):
    text = "\n".join(good.chunks)
    return [fn(text)]


def test_good_output_has_no_errors(good):
    assert codes(good.chunks, good.objects) == set()


def test_redundant_s(good):
    assert "V_S_REDUNDANT" in codes(mutate(good, lambda t: t.replace("M486 S0\n", "M486 S0\nM486 S0\n", 1)), good.objects)


def test_index_out_of_range(good):
    assert "V_S_RANGE" in codes(mutate(good, lambda t: t.replace("M486 S3\n", "M486 S99\n", 1)), good.objects)


def test_two_t(good):
    assert "V_T_COUNT" in codes(mutate(good, lambda t: t.replace("M486 T4\n", "M486 T4\nM486 T4\n", 1)), good.objects)


def test_t_over_capacity(good):
    assert "V_T_OVER_CAPACITY" in {p.code for p in validate_m486(mutate(good, lambda t: t.replace("M486 T4", "M486 T31")), 30, good.objects)}


def test_z_inside_object(good):
    assert "V_Z_IN_OBJECT" in codes(mutate(good, lambda t: t.replace(";TYPE:FILL", "G0 F600 Z9\n;TYPE:FILL", 1)), good.objects)


def test_filament_move_inside_object(good):
    assert "V_E_ONLY_IN_OBJECT" in codes(mutate(good, lambda t: t.replace(";TYPE:FILL", "G1 F2400 E-1\n;TYPE:FILL", 1)), good.objects)


def test_motion_under_object_in_gcode_final(good):
    assert "V_OBJECT_IN_NONMESH" in codes(mutate(good, lambda t: t.replace("M106 S0\n", "M486 S0\nM106 S0\nG1 X1 Y1\n", 1)), good.objects)


def test_p_u_c_not_allowed(good):
    assert "V_FOREIGN_M486_CMD" in codes(mutate(good, lambda t: t.replace("M486 T4\n", "M486 T4\nM486 P1\n", 1)), good.objects)


def test_extrusion_outside_object_box(good):
    # intercambia dos ids: las extrusiones del objeto 1 pasan a ser del 0 y salen de su caja
    def swap(t):
        return t.replace("M486 S1\n", "M486 STMP\n").replace("M486 S0\n", "M486 S1\n").replace("M486 STMP\n", "M486 S0\n")
    assert "V_OUTSIDE_BBOX" in codes(mutate(good, swap), good.objects)


def test_unused_id(good):
    assert "V_ID_UNUSED" in codes(mutate(good, lambda t: t.replace("M486 T4", "M486 T5")), good.objects)
