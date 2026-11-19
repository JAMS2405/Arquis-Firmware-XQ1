"""Comportamiento del motor de cancelacion de Marlin (modelo) y compatibilidad con el firmware actual (capacidad 32)."""
import pytest

from arquis_engines.m486_engine import M486Options, process_gcode
from arquis_engines.marlin_model import MarlinCancel, simulate
from fixtures import grid_objects, instances_from, make_gcode
from helpers import check_cancel_k


def test_stock_firmware_alias_defect_vs_range_checked_engine():
    stock, new = MarlinCancel(legacy=True), MarlinCancel(capacity=64)
    for m in (stock, new):
        m.m486({"T": 50}, ())
    stock.m486({"S": 256}, ()); new.m486({"S": 256}, ())
    assert stock.active == 0                      # DEFECTO del motor actual (medido en docs/slicer_suite/02_capacidad_64.md)
    assert new.active == -1 and new.overflow      # el motor con rango validado nunca alias a otro objeto


def test_new_engine_model_never_aliases_any_int16_value():
    for v in range(-32768, 32768):
        m = MarlinCancel(capacity=64)
        m.m486({"T": 64}, ())
        m.m486({"S": v}, ())
        assert m.active == (v if 0 <= v < 64 else -1)
        m.m486({"P": v}, ())
        assert m.canceled == ({v} if 0 <= v < 64 else set())


def test_current_firmware_capacity_32_works_for_the_first_32():
    objs = grid_objects(40, "Pieza", height=1.0)
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options(capacity=32))
    assert res.status == "ok"
    for k in (0, 5, 31):
        check_cancel_k(res.chunks, k, legacy=True)


def test_gcode_for_capacity_64_on_current_firmware_is_inert_beyond_31_never_wrong():
    objs = grid_objects(40, "Pieza", height=1.0)
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options(capacity=64))
    assert res.status == "ok"
    for k in (0, 31):
        check_cancel_k(res.chunks, k, legacy=True)            # 0..31 siguen funcionando
    base = simulate(res.chunks, [], 64, legacy=True)
    for k in (32, 39):                                        # 32..39: el firmware actual no puede cancelarlos, pero tampoco cancela otro
        canc = simulate(res.chunks, [k], 64, legacy=True)
        assert [(m.x, m.y, m.z, m.de, m.skipped) for m in base.moves] == [(m.x, m.y, m.z, m.de, m.skipped) for m in canc.moves]
