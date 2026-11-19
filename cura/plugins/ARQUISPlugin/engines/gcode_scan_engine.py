"""Analisis del G-code de CuraEngine 5.11: tramos ``;MESH:``, cajas XY de extrusion y marcadores de capa.

Formato que se asume (leido en CuraEngine 5.11.0, ``LayerPlan.cpp`` / ``gcodeExport.cpp``):
  * ``;LAYER:<n>`` abre cada capa. Al empezar la capa el contexto es implicitamente NONMESH (``current_mesh`` es nulo) y solo se
    escribe ``;MESH:<nombre>`` cuando cambia la malla de la ruta (``;MESH:NONMESH`` si pasa a ser nula).
  * ``;TIME_ELAPSED:<s>`` cierra cada capa; el G-code final de la maquina (y ``M106 S0``) va despues de la ultima, sin marcador propio.
  * El nombre de ``;MESH:`` es el nombre del nodo de Cura: las copias de un mismo modelo lo comparten.

El escaneo es de una sola pasada y solo lee; no modifica nada.
"""
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

LAYER_RE = re.compile(r"^;LAYER:(-?\d+)\s*$")
MESH_PREFIX = ";MESH:"
TIME_PREFIX = ";TIME_ELAPSED:"
NONMESH = "NONMESH"

_WORD_RE = re.compile(r"([A-Za-z])\s*([-+]?(?:\d+\.?\d*|\.\d+))?")
_CMD_RE = re.compile(r"^([GgMmTt])\s*(\d+)")

MOVE_CMDS = ("G0", "G1", "G2", "G3")
# Comandos que mueven filamento sin ser G0/G1: el planificador tambien los descarta mientras un objeto esta cancelado
FILAMENT_CMDS = ("G10", "G11", "M600", "M601", "M603", "M701", "M702")


@dataclass
class Cmd:
    name: str                       # "G1", "M486"...
    words: Dict[str, float]         # primera aparicion de cada letra (flag sin valor = 0.0)
    flags: Tuple[str, ...]          # letras sin valor (p. ej. la C de "M486 C")


def split_code(line: str) -> Tuple[str, str]:
    """(codigo, comentario) de una linea; el comentario conserva el ';'."""
    i = line.find(";")
    if i < 0:
        return line.strip(), ""
    return line[:i].strip(), line[i:]


def parse_cmd(line: str) -> Optional[Cmd]:
    code, _ = split_code(line)
    if not code:
        return None
    m = _CMD_RE.match(code)
    if not m:
        return None
    name = m.group(1).upper() + str(int(m.group(2)))
    words: Dict[str, float] = {}
    flags: List[str] = []
    for w in _WORD_RE.finditer(code[m.end():]):
        letter = w.group(1).upper()
        if letter in words or letter in flags:
            continue
        if w.group(2) is None:
            flags.append(letter)
        else:
            words[letter] = float(w.group(2))
    return Cmd(name, words, tuple(flags))


@dataclass
class Section:
    index: int
    kind: str                       # "prologue" | "layer" | "mesh" | "gap"
    mesh: Optional[str]             # nombre de malla; None = NONMESH / sin malla
    layer: Optional[int]
    pos: Tuple[int, int]            # (chunk, linea) de la linea que abre el tramo
    bbox: Optional[List[float]] = None   # [x0, y0, x1, y1] de las extrusiones (incluye el punto de partida de cada una)
    extrude_moves: int = 0
    travel_moves: int = 0
    z_moves: int = 0
    z_extrude_moves: int = 0        # G1 con X/Y, Z y E>0 a la vez (modo jarron / no planar)
    filament_only_moves: int = 0
    travel_e_moves: int = 0         # viaje XY con E (retraccion/preparacion mientras se viaja)
    z_first: Optional[float] = None


@dataclass
class ScanResult:
    sections: List[Section]
    start_at: Dict[Tuple[int, int], int]       # (chunk, linea) -> indice de tramo que empieza ahi
    first_layer_pos: Optional[Tuple[int, int]]
    layer_count: int
    time_markers: int
    foreign_m486: List[Tuple[int, int, str]]   # (chunk, linea, texto) de M486 ya presentes
    relative_xy_in_model: bool = False
    arcs_in_model: bool = False
    has_arquis_marker: Optional[str] = None
    flags: Dict[str, int] = field(default_factory=dict)


def _grow(box: Optional[List[float]], x: float, y: float) -> List[float]:
    if box is None:
        return [x, y, x, y]
    if x < box[0]: box[0] = x
    if y < box[1]: box[1] = y
    if x > box[2]: box[2] = x
    if y > box[3]: box[3] = y
    return box


def scan_gcode(chunks: Sequence[str]) -> ScanResult:
    sections: List[Section] = []
    start_at: Dict[Tuple[int, int], int] = {}
    foreign: List[Tuple[int, int, str]] = []
    marker: Optional[str] = None
    first_layer: Optional[Tuple[int, int]] = None
    layers = 0
    times = 0
    rel_xy_model = False
    arcs_model = False
    in_model = False                # ya se vio el primer ;LAYER:

    def open_section(kind: str, mesh: Optional[str], layer: Optional[int], pos: Tuple[int, int]) -> Section:
        s = Section(len(sections), kind, mesh, layer, pos)
        sections.append(s)
        start_at[pos] = s.index
        return s

    cur = open_section("prologue", None, None, (0, 0))
    layer_nr: Optional[int] = None

    abs_xyz = True
    e_abs = True
    x = y = z = e = None            # type: Optional[float]

    for ci, chunk in enumerate(chunks):
        for li, line in enumerate(chunk.split("\n")):
            if line.startswith(";"):
                m = LAYER_RE.match(line)
                if m:
                    layer_nr = int(m.group(1))
                    layers += 1
                    in_model = True
                    if first_layer is None:
                        first_layer = (ci, li)
                    cur = open_section("layer", None, layer_nr, (ci, li))
                elif line.startswith(MESH_PREFIX):
                    name = line[len(MESH_PREFIX):].rstrip("\r")
                    cur = open_section("mesh", None if name == NONMESH else name, layer_nr, (ci, li))
                elif line.startswith(TIME_PREFIX):
                    times += 1
                    cur = open_section("gap", None, layer_nr, (ci, li))
                elif line.startswith(";ARQUIS_M486"):
                    marker = line.strip()
                continue

            cmd = parse_cmd(line)
            if cmd is None:
                continue
            n = cmd.name
            w = cmd.words
            if n == "M486":
                foreign.append((ci, li, line.strip()))
            elif n == "G90":
                abs_xyz = True
            elif n == "G91":
                abs_xyz = False
            elif n == "M82":
                e_abs = True
            elif n == "M83":
                e_abs = False
            elif n == "G92":
                if "X" in w: x = w["X"]
                if "Y" in w: y = w["Y"]
                if "Z" in w: z = w["Z"]
                if "E" in w: e = w["E"]
            elif n in FILAMENT_CMDS:
                cur.filament_only_moves += 1
            elif n in MOVE_CMDS:
                has_xy = "X" in w or "Y" in w
                has_z = "Z" in w
                has_e = "E" in w
                nx, ny, nz = x, y, z
                if abs_xyz:
                    if "X" in w: nx = w["X"]
                    if "Y" in w: ny = w["Y"]
                    if "Z" in w: nz = w["Z"]
                else:
                    if "X" in w: nx = None if x is None else x + w["X"]
                    if "Y" in w: ny = None if y is None else y + w["Y"]
                    if "Z" in w: nz = None if z is None else z + w["Z"]
                de = 0.0
                if has_e:
                    if e_abs:
                        de = w["E"] - (e if e is not None else w["E"])
                        e = w["E"]
                    else:
                        de = w["E"]
                        e = (e if e is not None else 0.0) + w["E"]
                # G0 con E (CuraEngine 5.11 lo usa para 'retraer/preparar mientras se viaja') NO es extrusion: es viaje con filamento
                extruding = n != "G0" and has_e and has_xy and de > 1e-9
                if in_model and not abs_xyz and has_xy and cur.kind == "mesh" and cur.mesh is not None:
                    rel_xy_model = True
                if n in ("G2", "G3") and extruding and cur.kind == "mesh":
                    arcs_model = True
                if extruding:
                    cur.extrude_moves += 1
                    if x is not None and y is not None:
                        cur.bbox = _grow(cur.bbox, x, y)
                    if nx is not None and ny is not None:
                        cur.bbox = _grow(cur.bbox, nx, ny)
                    if has_z:
                        cur.z_extrude_moves += 1
                else:
                    if has_xy:
                        cur.travel_moves += 1
                    if has_z and not has_xy:
                        cur.z_moves += 1
                    elif has_z and has_xy:
                        cur.z_moves += 1
                    if has_e and not has_xy:
                        cur.filament_only_moves += 1
                    elif has_e and has_xy:
                        cur.travel_e_moves += 1
                if has_z and cur.z_first is None:
                    cur.z_first = nz
                x, y, z = nx, ny, nz
            # los demas comandos no afectan a la posicion

    return ScanResult(sections=sections, start_at=start_at, first_layer_pos=first_layer, layer_count=layers,
                      time_markers=times, foreign_m486=foreign, relative_xy_in_model=rel_xy_model,
                      arcs_in_model=arcs_model, has_arquis_marker=marker)
