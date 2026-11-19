"""Modelo ejecutable de la semantica de cancelacion de Marlin 2.1.2.10, para PROBAR el G-code generado sin impresora.

Reproduce solo lo que importa (cada punto cita el codigo real del firmware):
  * M486 T/S/P/U/C            -> feature/cancel_object.cpp, gcode/feature/cancel/M486.cpp
  * G0/G1/G2/G3 con skipping  -> gcode/gcode.cpp:get_destination_from_command(): X, Y y Z se ignoran (destination = current_position)
  * E con skipping            -> module/planner.cpp:3050: position.e = target.e (el extrusor NO se mueve)
  * G92 E, M82/M83, G90/G91   -> posicion logica
No modela tiempos, aceleraciones ni la cola de comandos. ``legacy=True`` reproduce el motor SIN modificar (mascara uint32_t y
estrechamiento a int8_t, que provoca los alias medidos en docs/slicer_suite/02_capacidad_64.md); ``legacy=False`` modela el motor
con capacidad ``capacity`` y rango validado (propuesta para la Etapa 3).
"""
from dataclasses import dataclass, field
from typing import Iterable, List, Optional, Set, Tuple

from .gcode_scan_engine import FILAMENT_CMDS, MOVE_CMDS, parse_cmd


def _to_int8(v: int) -> int:
    v &= 0xFF
    return v - 256 if v >= 128 else v


class MarlinCancel:
    def __init__(self, capacity: int = 64, legacy: bool = False):
        self.capacity = 32 if legacy else capacity
        self.legacy = legacy
        self.canceled: Set[int] = set()
        self.object_count = 0
        self.active = -1
        self.skipping = False
        self.overflow = False

    def reset(self) -> None:
        self.canceled.clear()
        self.object_count = 0
        self.overflow = False
        self.set_active(-1)

    def set_active(self, v: int) -> None:
        if self.legacy:
            v = _to_int8(v)                        # set_active_object(const int8_t) recibe parser.value_int()
            self.active = v
            if 0 <= v <= 31:
                if v >= self.object_count:
                    self.object_count = v + 1
                self.skipping = v in self.canceled
            else:
                self.skipping = False
            return
        if 0 <= v < self.capacity:
            self.active = v
            self.object_count = max(self.object_count, v + 1)
            self.skipping = v in self.canceled
        else:
            self.active = -1
            self.skipping = False
            if v >= self.capacity:
                self.overflow = True

    def cancel(self, v: int) -> None:
        if self.legacy:
            v = _to_int8(v)
            if 0 <= v <= 31:
                self.canceled.add(v)
                if v == self.active:
                    self.skipping = True
            return
        if 0 <= v < self.capacity:
            self.canceled.add(v)
            if v == self.active:
                self.skipping = True

    def uncancel(self, v: int) -> None:
        if self.legacy:
            v = _to_int8(v)
        if 0 <= v < (32 if self.legacy else self.capacity):
            self.canceled.discard(v)
            if v == self.active:
                self.skipping = False

    def m486(self, words: dict, flags: Tuple[str, ...]) -> None:
        if "T" in words or "T" in flags:
            self.reset()
            self.object_count = _to_int8(int(words.get("T", 1))) if self.legacy else max(0, min(int(words.get("T", 1)), self.capacity))
            if not self.legacy and int(words.get("T", 1)) > self.capacity:
                self.overflow = True
        if "S" in words:
            self.set_active(int(words["S"]))
        if "C" in flags or "C" in words:
            self.cancel(self.active)
        if "P" in words:
            self.cancel(int(words["P"]))
        if "U" in words:
            self.uncancel(int(words["U"]))


@dataclass
class Move:
    line: int                    # numero de linea global (0-based, concatenando los trozos)
    ctx: int                     # contexto M486 activo cuando se ejecuto
    x: float
    y: float
    z: float
    de: float                    # movimiento FISICO del extrusor (mm) en esta orden
    xy_moved: bool
    skipped: bool
    xy_cmd: bool = False         # la orden lleva X o Y (su destino XY es absoluto y debe coincidir con la ejecucion normal)


@dataclass
class Trace:
    moves: List[Move] = field(default_factory=list)
    final: Tuple[float, float, float] = (0.0, 0.0, 0.0)
    physical_e: float = 0.0
    overflow: bool = False


def simulate(chunks: Iterable[str], cancel: Iterable[int] = (), capacity: int = 64, legacy: bool = False,
             cancel_at_start: bool = True) -> Trace:
    """Ejecuta el G-code con los objetos ``cancel`` cancelados desde el principio (equivale a P<i> tras M486 T)."""
    cancel = list(cancel)
    mc = MarlinCancel(capacity, legacy)
    tr = Trace()
    x = y = z = 0.0
    e_cur = 0.0                  # posicion logica de E (current_position.e)
    e_planner = 0.0              # posicion de E en el planificador
    abs_xyz, e_abs = True, True
    lineno = -1
    for chunk in chunks:
        for line in chunk.split("\n"):
            lineno += 1
            cmd = parse_cmd(line)
            if cmd is None:
                continue
            n, w = cmd.name, cmd.words
            if n == "M486":
                mc.m486(w, cmd.flags)
                if "T" in w and cancel_at_start:
                    for c in cancel:
                        mc.cancel(c)
                continue
            if n == "G90": abs_xyz = True; continue
            if n == "G91": abs_xyz = False; continue
            if n == "M82": e_abs = True; continue
            if n == "M83": e_abs = False; continue
            if n == "G92":
                if "X" in w: x = w["X"]
                if "Y" in w: y = w["Y"]
                if "Z" in w: z = w["Z"]
                if "E" in w: e_cur = e_planner = w["E"]
                continue
            if n in FILAMENT_CMDS:
                # G10/G11/M600...: movimientos de E por el planificador; con skipping se descartan (planner.cpp:3050)
                continue
            if n not in MOVE_CMDS:
                continue
            nx, ny, nz = x, y, z
            skip = mc.skipping
            for axis in "XYZ":
                if axis in w:
                    v = w[axis]
                    cur = {"X": x, "Y": y, "Z": z}[axis]
                    tgt = cur if skip else (v if abs_xyz else cur + v)
                    if axis == "X": nx = tgt
                    elif axis == "Y": ny = tgt
                    else: nz = tgt
            ne = e_cur
            if "E" in w:
                ne = w["E"] if e_abs else e_cur + w["E"]
            de = 0.0
            if skip:
                e_planner = ne                      # position.e = target.e
            else:
                de = ne - e_planner
                e_planner = ne
            e_cur = ne
            moved_xy = (nx != x) or (ny != y)
            x, y, z = nx, ny, nz
            tr.physical_e += de
            tr.moves.append(Move(lineno, mc.active, x, y, z, de, moved_xy, skip, ("X" in w or "Y" in w) and abs_xyz))
    tr.final = (x, y, z)
    tr.overflow = mc.overflow
    return tr
