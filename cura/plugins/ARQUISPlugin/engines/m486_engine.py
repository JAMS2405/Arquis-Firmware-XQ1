"""ARQUIS M486 Engine: inserta ``M486 T``/``M486 S`` en el G-code de Cura con correspondencia verificada.

Semantica de Marlin 2.1.2.10 en la que se basa (feature/cancel_object.cpp, gcode/feature/cancel/M486.cpp, gcode/gcode.cpp):
  * ``M486 T<n>`` reinicia el estado y fija el numero de objetos; ``M486 S<i>`` activa el objeto i; ``M486 S-1`` = no cancelable.
  * Mientras el objeto activo esta cancelado, TODO G0/G1 (y G2/G3) pierde sus ejes X, Y y Z (``get_destination_from_command``) y el
    planificador descarta el movimiento de E (``position.e = target.e``).
De ahi las reglas de este motor (cada una tiene prueba en cura/tests):
  R1  Los movimientos que SOLO tocan Z, o filamento (E sin X/Y), se ejecutan SIEMPRE: van en contexto ``S-1``. Un G0/G1 que mezcla
      XY con Z o con E que no es extrusion (CuraEngine 5.11 escribe ``G0 X.. Y.. E<retraido>`` para retraer mientras viaja) se
      parte en "Z/E (en S-1)" + "XY (en el objeto)". Asi un objeto cancelado no puede dejar la boquilla a una altura equivocada
      ni descompensar retracciones (hallazgo con G-code REAL: sin esto quedaban 0,8 mm de filamento de mas por retraccion).
  R2  Todo lo que no sea un tramo de objeto verificado (prologo, inicio de capa, fin de capa, G-code final) va en ``S-1``; el
      G-code final de la maquina nunca se omite aunque el ultimo objeto este cancelado.
  R3  Un G1 que extruye con X/Y y Z a la vez (modo jarron, no planar) no se parte: se avisa.
  R4  Nunca se emite un M486 redundante, un indice fuera de [-1, N-1] ni mas de un ``M486 T``.
  R5  Si la correspondencia tramo<->instancia no es segura el G-code NO se modifica (salvo un comentario) y se explica el motivo.
"""
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

from . import protocol_limits as limits
from .gcode_scan_engine import FILAMENT_CMDS, MOVE_CMDS, ScanResult, parse_cmd, scan_gcode, split_code
from .matching_engine import MatchResult, match_sections
from .object_engine import ArquisObject, SceneInstance, build_objects
from .problems import ERROR, WARNING, Problem, has_errors

MARKER = ";ARQUIS_M486"
_NUM = r"[-+]?(?:\d+\.?\d*|\.\d+)"


@dataclass
class M486Options:
    capacity: int = limits.DESIGN_CAPACITY      # objetos cancelables del firmware destino (indices 0..capacity-1)
    tolerance_mm: float = 1.0                   # holgura al comprobar que un tramo cae dentro de su instancia
    protect_z: bool = True                      # R1 para Z
    protect_filament: bool = True               # R1 para movimientos de filamento sin XY
    id_order: str = "scene"                     # "scene" | "position"
    ceiling: int = limits.CAPACITY_CEILING      # mayor capacidad admitida (limits.json). Solo se cambia al ampliar el firmware.

    def __post_init__(self):
        if not 1 <= self.ceiling <= limits.PROTOCOL_MAX_OBJECTS:
            raise ValueError("ceiling fuera de rango (1..%d): %r" % (limits.PROTOCOL_MAX_OBJECTS, self.ceiling))
        if not 1 <= self.capacity <= self.ceiling:
            raise ValueError("capacity fuera de rango (1..%d): %r" % (self.ceiling, self.capacity))


@dataclass
class M486Result:
    status: str                                  # "ok" | "blocked" | "skipped"
    chunks: List[str]
    problems: List[Problem] = field(default_factory=list)
    objects: List[ArquisObject] = field(default_factory=list)
    stats: Dict[str, int] = field(default_factory=dict)

    @property
    def ok(self) -> bool:
        return self.status == "ok"

    def summary(self) -> str:
        head = "ARQUIS M486: %s" % self.status.upper()
        lines = [head] + ["  " + str(p) for p in self.problems]
        if self.stats:
            lines.append("  " + ", ".join("%s=%s" % kv for kv in sorted(self.stats.items())))
        return "\n".join(lines)


def _blocked(chunks: Sequence[str], problems: List[Problem], objects: List[ArquisObject], stats: Optional[Dict[str, int]] = None) -> M486Result:
    """R5: el G-code no cambia salvo un comentario de cabecera que dice por que no hay cancelacion de objetos."""
    codes = ",".join(sorted({p.code for p in problems if p.severity == ERROR}))
    out = list(chunks)
    if out:
        out[0] = "%s STATUS=BLOCKED CODES=%s\n%s" % (MARKER, codes, out[0])
    return M486Result("blocked", out, problems, objects, stats or {})


def process_gcode(chunks: Sequence[str], instances: Sequence[SceneInstance], options: Optional[M486Options] = None) -> M486Result:
    opt = options or M486Options()
    chunks = list(chunks)
    if not chunks:
        return M486Result("skipped", chunks, [Problem("E_EMPTY", ERROR, "G-code vacio")])

    scan = scan_gcode(chunks)
    if scan.has_arquis_marker:
        return M486Result("skipped", chunks, [Problem("W_ALREADY_PROCESSED", WARNING, "el G-code ya fue procesado (%s)" % scan.has_arquis_marker)])

    objects = build_objects(instances, opt.capacity, opt.id_order)
    problems: List[Problem] = []

    # --- comprobaciones previas -----------------------------------------------------------------------------------------------
    if not instances:
        problems.append(Problem("E_NO_INSTANCES", ERROR, "la escena no aporta ninguna instancia imprimible"))
    if scan.layer_count == 0 or scan.first_layer_pos is None:
        problems.append(Problem("E_NO_LAYERS", ERROR, "el G-code no tiene marcadores ';LAYER:' (no es una salida de CuraEngine)"))
    if scan.time_markers == 0:
        problems.append(Problem("E_NO_LAYER_END_MARKER", ERROR,
                                "el G-code no tiene ';TIME_ELAPSED:': no se puede situar con seguridad el fin de la ultima capa "
                                "(el G-code final de la maquina podria quedar dentro de un objeto cancelado)"))
    if scan.foreign_m486:
        problems.append(Problem("E_FOREIGN_M486", ERROR,
                                "el G-code ya contiene %d comandos M486 que no son de ARQUIS (otro script de cancelar objetos); "
                                "desactiva el otro script" % len(scan.foreign_m486)))
    if scan.relative_xy_in_model:
        problems.append(Problem("E_RELATIVE_XY", ERROR, "hay movimientos XY relativos (G91) dentro de las capas; el analisis de posicion no es fiable"))
    if scan.arcs_in_model:
        problems.append(Problem("W_ARCS", WARNING, "hay arcos G2/G3 extruyendo; su caja se aproxima por los extremos"))
    if has_errors(problems):
        return _blocked(chunks, problems, objects)

    match = match_sections(scan, objects, opt.tolerance_mm)
    problems.extend(match.problems)
    over = [o for o in objects if not o.cancelable]
    if over:
        problems.append(Problem("W_OVER_CAPACITY", WARNING,
                                "%d objetos superan la capacidad del firmware (%d): se imprimen pero NO son cancelables" % (len(over), opt.capacity)))
    if any(s.z_extrude_moves for s in scan.sections if s.kind == "mesh"):
        problems.append(Problem("W_Z_IN_EXTRUDING_MOVE", WARNING,
                                "hay movimientos que extruyen con X/Y y Z a la vez (modo jarron o no planar): no se parten; cancelar un objeto "
                                "en esos tramos puede afectar a la altura"))
    if has_errors(problems):
        return _blocked(chunks, problems, objects)

    n_cancel = sum(1 for o in objects if o.cancelable)
    out, stats = _rewrite(chunks, scan, match, n_cancel, opt)
    stats["objects"] = len(objects)
    stats["cancelable"] = n_cancel
    stats["tolerance_used"] = match.tolerance_used
    out[0] = "%s V=%d N=%d CAP=%d STATUS=OK\n%s" % (MARKER, limits.PROTOCOL_VERSION, n_cancel, opt.capacity, out[0])

    from .validation_engine import validate_m486        # import tardio: validacion y motor se prueban por separado
    post = validate_m486(out, capacity=opt.capacity, objects=objects, tolerance_mm=opt.tolerance_mm)
    if has_errors(post):
        # Defensa en profundidad: lo que acabamos de generar no supera nuestro propio validador => no se entrega.
        return _blocked(chunks, problems + [Problem("E_INTERNAL_VALIDATION", ERROR, "la salida no supera la validacion: " + "; ".join(p.message for p in post if p.severity == ERROR))], objects)
    return M486Result("ok", out, problems + [p for p in post if p.severity == WARNING], objects, stats)


def _is_xy(w: Dict[str, float]) -> bool:
    return "X" in w or "Y" in w


def _strip(code: str, *letters: str) -> str:
    """Quita de una linea de codigo las palabras de los ejes indicados (conserva el resto, F incluido)."""
    for l in letters:
        code = re.sub(r"\s*\b" + l + r"\s*" + _NUM, "", code, count=1, flags=re.IGNORECASE)
    return code


def _token(code: str, letter: str) -> str:
    m = re.search(r"\b(" + letter + r"\s*" + _NUM + r")", code, re.IGNORECASE)
    return m.group(1).replace(" ", "") if m else ""


def _rewrite(chunks: List[str], scan: ScanResult, match: MatchResult, n_cancel: int, opt: M486Options) -> Tuple[List[str], Dict[str, int]]:
    stats = {"m486_lines": 0, "z_splits": 0, "z_protected": 0, "filament_protected": 0, "travel_e_splits": 0}
    out_chunks: List[str] = []
    active = -1                                   # contexto M486 vigente en el G-code emitido (tras M486 T es -1)
    want = -1                                     # contexto del tramo actual
    t_done = False
    e_abs, e_cur = True, None                     # estado de E para distinguir extrusion de retraccion

    for ci, chunk in enumerate(chunks):
        out: List[str] = []

        def emit_ctx(ctx: int) -> None:
            nonlocal active
            if ctx != active:
                out.append("M486 S%d" % ctx)
                active = ctx
                stats["m486_lines"] += 1

        for li, line in enumerate(chunk.split("\n")):
            sec = scan.start_at.get((ci, li))
            if sec is not None:
                want = match.section_object.get(sec, -1)
            if not t_done and scan.first_layer_pos == (ci, li):
                out.append("M486 T%d" % n_cancel)
                stats["m486_lines"] += 1
                active = -1
                t_done = True

            if line.startswith(";"):
                out.append(line)
                continue
            cmd = parse_cmd(line)
            if cmd is None:
                out.append(line)
                continue
            n, w = cmd.name, cmd.words

            # estado de E (igual que el escaner)
            de = 0.0
            if n == "M82": e_abs = True
            elif n == "M83": e_abs = False
            elif n == "G92" and "E" in w: e_cur = w["E"]
            elif n in MOVE_CMDS and "E" in w:
                if e_abs:
                    de = w["E"] - (e_cur if e_cur is not None else w["E"])
                    e_cur = w["E"]
                else:
                    de = w["E"]
                    e_cur = (e_cur or 0.0) + w["E"]

            if n in FILAMENT_CMDS:
                if opt.protect_filament and want >= 0:
                    stats["filament_protected"] += 1
                emit_ctx(-1 if opt.protect_filament else want)
                out.append(line)
                continue
            if n not in MOVE_CMDS:
                out.append(line)
                continue

            if want < 0:                           # R2: fuera de un objeto todo se ejecuta
                emit_ctx(-1)
                out.append(line)
                continue

            has_xy, has_z, has_e = _is_xy(w), "Z" in w, "E" in w
            if not (has_xy or has_z or has_e):     # p. ej. "G0 F6000": no mueve nada, el contexto da igual
                out.append(line)
                continue
            extruding = n != "G0" and has_xy and has_e and de > 1e-9
            if extruding or n in ("G2", "G3"):     # extrusion del objeto (R3: con Z no se parte; los arcos tampoco)
                emit_ctx(want)
                out.append(line)
                continue

            # Viaje / movimiento de eje o filamento dentro de un objeto: lo que no debe omitirse (Z, E) va en S-1 (R1)
            prot = []
            if has_z and opt.protect_z:
                prot.append("Z")
            if has_e and opt.protect_filament:
                prot.append("E")
            if not prot:
                emit_ctx(want)
                out.append(line)
                continue
            code, comment = split_code(line)
            rest = _strip(code, *prot)
            rest_cmd = parse_cmd(rest)
            has_rest_xy = rest_cmd is not None and _is_xy(rest_cmd.words)
            if not has_rest_xy:                    # solo Z y/o E (y quiza F): la linea completa va en S-1
                emit_ctx(-1)
                out.append(line)
                stats["z_protected" if "Z" in prot else "filament_protected"] += 1
                continue
            ptoks = [t for t in (_token(code, l) for l in prot) if t]
            fl = _token(code, "F")
            pline = ("G1" if "E" in prot else n) + ((" " + fl) if fl else "") + " " + " ".join(ptoks) + " ;ARQ_" + "".join(prot)
            rline = rest + ((" " + comment) if comment else "")
            xy_first = prot == ["E"] and de > 1e-9           # "preparar mientras se viaja": primero el viaje, luego el filamento
            if "Z" in prot:
                stats["z_splits"] += 1
            if "E" in prot:
                stats["travel_e_splits"] += 1
            if xy_first:
                emit_ctx(want); out.append(rline)
                emit_ctx(-1); out.append(pline)
            else:
                emit_ctx(-1); out.append(pline)
                emit_ctx(want); out.append(rline)
        out_chunks.append("\n".join(out))
    return out_chunks, stats
