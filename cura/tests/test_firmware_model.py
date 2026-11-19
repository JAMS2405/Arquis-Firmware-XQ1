"""Comportamiento del motor de cancelacion de Marlin (modelo) y compatibilidad con el firmware ACTUAL (mascara uint32_t)."""
import pytest

from arquis_engines.m486_engine import M486Options, process_gcode
from arquis_engines.marlin_model import MarlinCancel, simulate
from fixtures import grid_objects, instances_from, make_gcode
from helpers import check_cancel_k


def test_stock_firmware_alias_defect_vs_range_checked_engine():
    stock, new = MarlinCancel(legacy=True), MarlinCancel(capacity=30)
    for m in (stock, new):
        m.m486({"T": 30}, ())
    stock.m486({"S": 256}, ()); new.m486({"S": 256}, ())
    assert stock.active == 0                      # DEFECTO del motor actual (medido en docs/slicer_suite/02_capacidad_30.md)
    assert new.active == -1 and new.overflow      # el motor con rango validado nunca alias a otro objeto


@pytest.mark.parametrize("cap", [30, 32])
def test_range_checked_engine_never_aliases_any_int16_value(cap):
    for v in range(-32768, 32768):
        m = MarlinCancel(capacity=cap)
        m.m486({"T": cap}, ())
        m.m486({"S": v}, ())
        assert m.active == (v if 0 <= v < cap else -1)
        m.m486({"P": v}, ())
        assert m.canceled == ({v} if 0 <= v < cap else set())


def test_current_firmware_supports_the_30_design_objects_in_the_model():
    """Con la mascara uint32_t actual, los IDs 0-29 caben: el G-code de 30 objetos funciona SIN cambiar el firmware (modelo)."""
    objs = grid_objects(30, "Pieza", height=1.0)
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options())
    assert res.status == "ok"
    for k in range(30):
        check_cancel_k(res.chunks, k, legacy=True)


def test_gcode_never_uses_indices_that_could_alias_on_current_firmware():
    objs = grid_objects(40, "Pieza", height=1.0)
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options())
    ids = {int(l.split(" S")[1].split()[0]) for l in "\n".join(res.chunks).split("\n") if l.startswith("M486 S")}
    assert max(ids) == 29 and min(ids) == -1                   # nunca >= 30: ni siquiera 30/31, que el motor actual aceptaria


def test_expansion_gcode_on_current_firmware_is_inert_beyond_31_never_wrong():
    """Si algun dia se sube el techo en Cura pero el firmware no se ha ampliado: 0-31 funcionan, 32+ no se cancelan (nunca otro objeto)."""
    objs = grid_objects(40, "Pieza", height=1.0)
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options(capacity=40, ceiling=64))
    assert res.status == "ok"
    for k in (0, 31):
        check_cancel_k(res.chunks, k, capacity=64, legacy=True)
    base = simulate(res.chunks, [], 64, legacy=True)
    for k in (32, 39):
        canc = simulate(res.chunks, [k], 64, legacy=True)
        assert [(m.x, m.y, m.z, m.de, m.skipped) for m in base.moves] == [(m.x, m.y, m.z, m.de, m.skipped) for m in canc.moves]
