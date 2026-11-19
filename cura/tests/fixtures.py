"""Generador de G-code SINTETICO con la forma de la salida de CuraEngine 5.11 (leida en LayerPlan.cpp / gcodeExport.cpp).

No es G-code real de Cura: reproduce las propiedades que importan a ARQUIS (ver docs/slicer_suite/03_etapa1_m486.md):
  * ';LAYER:n' abre la capa con contexto implicito NONMESH; ';MESH:nombre' solo al cambiar de malla; ';TIME_ELAPSED:' cierra la capa.
  * El viaje de entrada a un objeto (retraccion + elevacion en Z + viaje + des-retraccion) pertenece al tramo de ESE objeto.
  * Opcionalmente el ultimo viaje de la capa ("subir a la altura de la siguiente") queda dentro del ultimo tramo.
  * El orden de los objetos en cada capa NO es el de la escena (vecino mas proximo, como calculateMeshOrder).
"""
import math
from dataclasses import dataclass
from typing import Callable, List, Optional, Sequence, Tuple

from arquis_engines.object_engine import SceneInstance


@dataclass
class Obj:
    name: str
    x0: float
    y0: float
    x1: float
    y1: float
    height: float = 2.0
    group: Optional[int] = None
    group_name: Optional[str] = None


def instances_from(objs: Sequence[Obj]) -> List[SceneInstance]:
    return [SceneInstance(i, o.name, o.x0, o.y0, o.x1, o.y1, o.group, o.group_name) for i, o in enumerate(objs)]


def grid_objects(n: int, name: str = "Llavero", w: float = 20.0, d: float = 20.0, gap: float = 6.0, bed: float = 300.0,
                 height: float = 2.0, x_start: float = 10.0, y_start: float = 10.0) -> List[Obj]:
    cols = max(1, int((bed - 2 * x_start + gap) // (w + gap)))
    out = []
    for i in range(n):
        cx, cy = i % cols, i // cols
        x0 = x_start + cx * (w + gap)
        y0 = y_start + cy * (d + gap)
        out.append(Obj(name, x0, y0, x0 + w, y0 + d, height))
    return out


class _G:
    def __init__(self, e_mode: str):
        self.e_mode = e_mode
        self.e = 0.0
        self.x = 0.0
        self.y = 0.0
        self.z = 0.0
        self.lines: List[str] = []

    def w(self, s: str) -> None:
        self.lines.append(s)

    def e_move(self, delta: float, f: int = 2400) -> None:       # retraccion / des-retraccion (solo E)
        self.e += delta
        self.w("G1 F%d E%.5f" % (f, delta if self.e_mode == "rel" else self.e))

    def line_to(self, x: float, y: float, f: int = 1800, k: float = 0.04) -> None:   # extrusion
        d = math.hypot(x - self.x, y - self.y)
        de = d * k
        self.e += de
        self.w("G1 F%d X%.3f Y%.3f E%.5f" % (f, x, y, de if self.e_mode == "rel" else self.e))
        self.x, self.y = x, y

    def travel(self, x: Optional[float] = None, y: Optional[float] = None, z: Optional[float] = None, f: int = 6000) -> None:
        parts = ["G0 F%d" % f]
        if x is not None: parts.append("X%.3f" % x); self.x = x
        if y is not None: parts.append("Y%.3f" % y); self.y = y
        if z is not None: parts.append("Z%.3f" % z); self.z = z
        self.w(" ".join(parts))


def make_gcode(objs: Sequence[Obj], layer_h: float = 0.2, first_h: float = 0.3, e_mode: str = "abs", skirt: bool = True,
               tower: Optional[Tuple[float, float, float, float]] = None, support_nonmesh: bool = False,
               final_z_in_last_mesh: bool = True, g92_each_layer: bool = True, zhop: float = 0.4,
               order: Optional[Callable[[int, List[int]], List[int]]] = None, one_at_a_time: bool = False,
               mesh_name_of: Optional[Callable[[int], str]] = None, header_extra: str = "",
               travel_in: str = "mesh") -> List[str]:
    """Devuelve la lista de trozos como ``scene.gcode_dict``: [cabecera, inicio, capa0, capa1, ..., final, ';SETTING_3']."""
    mesh_name_of = mesh_name_of or (lambda i: objs[i].name)
    g = _G(e_mode)
    n_layers = max(int(round((o.height - first_h) / layer_h)) + 1 for o in objs)
    header = ";FLAVOR:Marlin\n;TIME:1234\n;Filament used: 1m\n;Layer height: %.2f\n;Generated with Cura_SteamEngine 5.11.0\n%s;LAYER_COUNT:%d\n" % (layer_h, header_extra, n_layers)
    start = "M140 S60\nM105\nM190 S60\nM104 S200\nM109 S200\nM82 ;absolute extrusion mode\nG28 ;Home\nG90\n" + \
            ("M83\n" if e_mode == "rel" else "") + "G92 E0\n"
    g.e = 0.0
    chunks = [header, start]

    def z_of(layer: int) -> float:
        return first_h + layer * layer_h

    def present(layer: int, i: int) -> bool:
        return z_of(layer) <= objs[i].height + 1e-6

    def section_for(layer: int, i: int, first_in_layer: bool, last_in_layer: bool, z_next: float) -> None:
        o = objs[i]
        z = z_of(layer)
        sx, sy = o.x0 + 0.2, o.y0 + 0.2
        if travel_in == "nonmesh":                                      # variante: el viaje de entrada se escribe bajo NONMESH
            g.w(";MESH:NONMESH")
            if not first_in_layer or layer > 0:
                g.e_move(-1.0)
            g.travel(sx, sy, z if first_in_layer else z + zhop)
            g.w(";MESH:%s" % mesh_name_of(i))
            if not first_in_layer:
                g.travel(z=z, f=600)
            if not first_in_layer or layer > 0:
                g.e_move(1.0)
        else:                                                           # variante: el viaje de entrada pertenece al objeto
            g.w(";MESH:%s" % mesh_name_of(i))
            if not first_in_layer or layer > 0:
                g.e_move(-1.0)                                          # retraccion (pertenece al tramo entrante)
            if first_in_layer:
                g.travel(sx, sy, z)                                     # cambio de capa: XY y Z en el mismo G0
            else:
                g.travel(sx, sy, z + zhop)                              # z-hop
                g.travel(z=z, f=600)
            if not first_in_layer or layer > 0:
                g.e_move(1.0)
        g.w(";TYPE:WALL-OUTER")
        for x, y in ((o.x1 - 0.2, o.y0 + 0.2), (o.x1 - 0.2, o.y1 - 0.2), (o.x0 + 0.2, o.y1 - 0.2), (o.x0 + 0.2, o.y0 + 0.2)):
            g.line_to(x, y, 1200)
        g.w(";TYPE:FILL")
        ym = (o.y0 + o.y1) / 2
        g.travel(o.x0 + 1.0, ym)
        g.line_to(o.x1 - 1.0, ym)
        if last_in_layer and final_z_in_last_mesh and z_next is not None:
            g.travel(z=z_next, f=600)                                   # ultimo viaje: subir a la altura de la siguiente capa

    def nonmesh_extrusions(layer: int) -> None:
        z = z_of(layer)
        if layer == 0 and skirt:
            g.w(";TYPE:SKIRT")
            xs = [o.x0 for o in objs] + [o.x1 for o in objs]
            ys = [o.y0 for o in objs] + [o.y1 for o in objs]
            x0, y0, x1, y1 = min(xs) - 3, min(ys) - 3, max(xs) + 3, max(ys) + 3
            g.travel(x0, y0, z)
            for x, y in ((x1, y0), (x1, y1), (x0, y1), (x0, y0)):
                g.line_to(x, y)
        if tower is not None:
            tx0, ty0, tx1, ty1 = tower
            g.w(";MESH:NONMESH")
            g.w(";TYPE:PRIME-TOWER")
            g.e_move(-1.0); g.travel(tx0, ty0, z + zhop); g.travel(z=z, f=600); g.e_move(1.0)
            for x, y in ((tx1, ty0), (tx1, ty1), (tx0, ty1), (tx0, ty0)):
                g.line_to(x, y)
        if support_nonmesh and layer > 0:
            g.w(";MESH:NONMESH")
            g.w(";TYPE:SUPPORT")

    groups: List[List[int]] = []
    if one_at_a_time:
        groups = [[i] for i in range(len(objs))]
    else:
        groups = [list(range(len(objs)))]

    layer_counter = 0
    for grp in groups:
        gl = max(int(round((objs[i].height - first_h) / layer_h)) + 1 for i in grp)
        for layer in range(gl):
            g.lines = []
            g.w(";LAYER:%d" % layer_counter)
            if g92_each_layer and e_mode == "abs":
                g.w("G92 E0"); g.e = 0.0
            nonmesh_extrusions(layer)
            ids = [i for i in grp if present(layer, i)]
            if order:
                ids = order(layer, ids)
            else:                                                       # vecino mas proximo desde el cabezal (no es el orden de la escena)
                rest, cur, seq = ids[:], (g.x, g.y), []
                while rest:
                    j = min(rest, key=lambda k: (math.hypot(objs[k].x0 - cur[0], objs[k].y0 - cur[1]), k))
                    rest.remove(j); seq.append(j); cur = (objs[j].x1, objs[j].y1)
                ids = seq
            z_next = z_of(layer + 1)
            for k, i in enumerate(ids):
                section_for(layer, i, first_in_layer=(k == 0), last_in_layer=(k == len(ids) - 1), z_next=z_next)
            g.w(";TIME_ELAPSED:%d.0" % (layer + 1))
            chunks.append("\n".join(g.lines) + "\n")
            layer_counter += 1
    end = "M106 S0\nM104 S0\nM140 S0\nG91 ;relative\nG1 F1800 E-3\nG1 F3000 Z10 ;lift\nG90 ;absolute\nG1 F3000 X5 Y295 ;park\nM84 X Y E\nM82\n;End of Gcode\n"
    chunks.append(end)
    chunks.append(";SETTING_3 {\"global_quality\": \"[general]\\nversion = 4\"}\n")
    return chunks
