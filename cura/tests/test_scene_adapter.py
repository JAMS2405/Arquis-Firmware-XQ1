# SPDX-License-Identifier: LGPL-3.0-or-later
from types import SimpleNamespace

import arquis_bootstrap
from arquis_engines.object_engine import build_objects

arquis_bootstrap.load_adapters()
import arquis_adapters.cura_scene_adapter as adapters  # noqa: E402


class Box:
    def __init__(self, left, right, front, back, top=10.0):
        self.left, self.right, self.front, self.back, self.top = left, right, front, back, top


class Node:
    def __init__(self, name, box, deco=None, parent=None, outside=False, mesh=True):
        self._name, self._box, self._deco, self._parent = name, box, deco or {}, parent
        self._outside_buildarea = outside
        self._mesh = SimpleNamespace(getVertices=lambda: [1]) if mesh else None

    def getName(self): return self._name
    def getBoundingBox(self): return self._box
    def getMeshData(self): return self._mesh
    def getParent(self): return self._parent
    def callDecoration(self, n, *a):
        d = {"isSliceable": True, "isNonPrintingMesh": False, "getBuildPlateNumber": 0, "isGroup": False}
        d.update(self._deco)
        return d.get(n)


def test_axis_conversion_front_is_y0():
    # objeto pegado al frente (z grande) a la izquierda (x negativo) de una cama 300x300 con origen en la esquina
    n = Node("A", Box(left=-100, right=-80, front=110, back=90))
    [i] = adapters.instances_from_nodes([n], 300, 300, False, 0)
    assert (i.x0, i.x1) == (50, 70)
    assert (i.y0, i.y1) == (40, 60)                    # y = -z + 150: front(110) -> 40 (cerca del frente), back(90) -> 60


def test_center_is_zero_has_no_offset():
    [i] = adapters.instances_from_nodes([Node("A", Box(-10, 10, 20, -20))], 200, 200, True, 0)
    assert (i.x0, i.y0, i.x1, i.y1) == (-10, -20, 10, 20)


def test_filters_like_startslicejob():
    nodes = [
        Node("ok", Box(0, 10, 10, 0)),
        Node("modificador", Box(0, 10, 10, 0), {"isNonPrintingMesh": True}),
        Node("otra_placa", Box(0, 10, 10, 0), {"getBuildPlateNumber": 2}),
        Node("fuera", Box(0, 10, 10, 0), outside=True),
        Node("sin_malla", Box(0, 10, 10, 0), mesh=False),
        Node("no_rebanable", Box(0, 10, 10, 0), {"isSliceable": False}),
    ]
    out = adapters.instances_from_nodes(nodes, 300, 300, False, 0)
    assert [i.name for i in out] == ["ok"] and out[0].scene_index == 0


def test_copies_keep_same_name_and_distinct_scene_index_and_positions():
    nodes = [Node("Llavero", Box(-50 + 30 * k, -30 + 30 * k, 10, -10)) for k in range(4)]
    inst = adapters.instances_from_nodes(nodes, 300, 300, False, 0)
    assert [i.name for i in inst] == ["Llavero"] * 4
    assert len({i.scene_index for i in inst}) == 4 and len({i.x0 for i in inst}) == 4
    objs = build_objects(inst, 30)
    assert [(o.name, o.instance, o.id) for o in objs] == [("Llavero", k, k) for k in range(4)]


def test_group_children_share_group():
    g = Node("Grupo", Box(0, 100, 10, 0), {"isGroup": True}, mesh=False)
    a = Node("a", Box(0, 10, 10, 0), parent=g)
    b = Node("b", Box(20, 30, 10, 0), parent=g)
    inst = adapters.instances_from_nodes([g, a, b], 300, 300, False, 0)
    assert len(inst) == 2 and inst[0].group == inst[1].group == 0 and inst[0].group_name == "Grupo"
    assert len(build_objects(inst, 30)) == 1


class Q:
    def __init__(self, deg):
        import math
        h = math.radians(deg) / 2
        self.x, self.y, self.z, self.w = 0.0, math.sin(h), 0.0, math.cos(h)


class Hull:
    def __init__(self, pts):
        self._p = pts

    def getPoints(self):
        return self._p


def test_rotation_shape_and_top_height_are_captured():
    import math
    circ = [(10 * math.cos(2 * math.pi * i / 24), 10 * math.sin(2 * math.pi * i / 24)) for i in range(24)]
    n = Node("Disco", Box(-10, 10, 10, -10, top=7.5), {"getConvexHull": Hull(circ)})
    n.getWorldOrientation = lambda: Q(90)
    [i] = adapters.instances_from_nodes([n], 300, 300, False, 0)
    assert i.shape == "C" and i.rot == 90 and i.z1 == 7.5
    r = Node("Barra", Box(-30, 30, 5, -5), {"getConvexHull": Hull([(-30, -5), (30, -5), (30, 5), (-30, 5)])})
    r.getWorldOrientation = lambda: Q(-30)
    [j] = adapters.instances_from_nodes([r], 300, 300, False, 0)
    assert j.shape == "R" and j.rot == 330


def test_shape_falls_back_to_the_box_when_cura_gives_no_hull_and_never_fails():
    sq = Node("Cubo", Box(-10, 10, 10, -10))
    [i] = adapters.instances_from_nodes([sq], 300, 300, False, 0)
    assert i.shape == "S" and i.rot == 0                                  # sin orientacion ni huella: valores neutros
    bar = Node("Barra", Box(-40, 40, 5, -5))
    assert adapters.instances_from_nodes([bar], 300, 300, False, 0)[0].shape == "R"


def test_bed_in_gcode_frame():
    assert adapters.bed_from_machine(300, 300, False) == (0.0, 0.0, 300, 300)
    assert adapters.bed_from_machine(300, 200, True) == (-150.0, -100.0, 300, 200)
