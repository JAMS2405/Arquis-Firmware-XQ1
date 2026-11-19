"""R5: si la correspondencia no es segura, el G-code no se toca (salvo un comentario) y se explica por que."""
import pytest

from arquis_engines.m486_engine import M486Options, process_gcode
from fixtures import Obj, grid_objects, instances_from, make_gcode
from helpers import flat, m486_lines


def _blocked(res, code, allowed_m486=0):
    assert res.status == "blocked", res.summary()
    assert code in {p.code for p in res.problems}, res.summary()
    assert len(m486_lines(res.chunks)) == allowed_m486                  # ARQUIS no anade ni un solo M486
    return res


def _unchanged_but_marker(res, original):
    assert res.chunks[0].startswith(";ARQUIS_M486 STATUS=BLOCKED")
    assert res.chunks[0].split("\n", 1)[1] == original[0]
    assert res.chunks[1:] == list(original[1:])


def test_scene_does_not_match_gcode():
    objs = grid_objects(3, "A", height=1.0)
    chunks = make_gcode(objs)
    moved = [Obj(o.name, o.x0 + 8, o.y0, o.x1 + 8, o.y1, o.height) for o in objs]
    res = _blocked(process_gcode(chunks, instances_from(moved), M486Options()), "E_SECTION_OUTSIDE_INSTANCE")
    _unchanged_but_marker(res, chunks)
    # la ejecucion repetida no vuelve a procesar
    assert process_gcode(res.chunks, instances_from(moved)).status == "skipped"


def test_instance_missing_from_scene_unique_name_and_duplicate_name():
    objs = [Obj("Soporte", 20, 20, 60, 50, 1.0), Obj("Llavero", 100, 20, 120, 40, 1.0), Obj("Llavero", 140, 20, 160, 40, 1.0)]
    chunks = make_gcode(objs)
    _blocked(process_gcode(chunks, instances_from(objs[1:]), M486Options()), "E_UNKNOWN_MESH")
    _blocked(process_gcode(chunks, instances_from(objs[:2]), M486Options()), "E_SECTION_OUTSIDE_INSTANCE")


def test_scene_instance_that_is_not_printed():
    objs = grid_objects(2, "A", height=1.0)
    chunks = make_gcode(objs)
    extra = objs + [Obj("A", 200, 200, 220, 220, 1.0)]
    _blocked(process_gcode(chunks, instances_from(extra), M486Options()), "E_INSTANCE_NOT_PRINTED")


def test_nested_same_name_objects_are_ambiguous():
    big = Obj("A", 10, 10, 110, 110, 1.0)
    small = Obj("A", 40, 40, 60, 60, 1.0)                               # dentro de la caja de 'big'
    chunks = make_gcode([big, small])
    _blocked(process_gcode(chunks, instances_from([big, small]), M486Options()), "E_AMBIGUOUS_INSTANCE")


def test_foreign_m486_present():
    objs = grid_objects(2, "A", height=1.0)
    chunks = make_gcode(objs)
    chunks[1] += "M486 T2 ;otro script\n"
    _blocked(process_gcode(chunks, instances_from(objs), M486Options()), "E_FOREIGN_M486", allowed_m486=1)


def test_missing_layer_end_marker():
    objs = grid_objects(2, "A", height=1.0)
    chunks = [c.replace(";TIME_ELAPSED:", ";X:") for c in make_gcode(objs)]
    _blocked(process_gcode(chunks, instances_from(objs), M486Options()), "E_NO_LAYER_END_MARKER")


def test_relative_xy_inside_layers():
    objs = grid_objects(2, "A", height=1.0)
    chunks = make_gcode(objs)
    chunks[3] = chunks[3].replace(";TYPE:WALL-OUTER", "G91\nG1 F1200 X1 Y1 E0.1\nG90\n;TYPE:WALL-OUTER", 1)
    _blocked(process_gcode(chunks, instances_from(objs), M486Options()), "E_RELATIVE_XY")


def test_object_named_nonmesh():
    objs = [Obj("NONMESH", 10, 10, 30, 30, 1.0)]
    _blocked(process_gcode(make_gcode(objs), instances_from(objs), M486Options()), "E_NONMESH_NAME")


def test_not_cura_output_and_empty_scene():
    objs = grid_objects(1, "A", height=1.0)
    _blocked(process_gcode(["G28\nG1 X10 Y10\n"], instances_from(objs), M486Options()), "E_NO_LAYERS")
    _blocked(process_gcode(make_gcode(objs), [], M486Options()), "E_NO_INSTANCES")
