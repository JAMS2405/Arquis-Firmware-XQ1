# SPDX-License-Identifier: LGPL-3.0-or-later
"""ARQUIS Object Manifest (ARQUIS_OBJECTS_V1): generacion, serializacion, lectura y validacion. Especificacion: shared/object_protocol/ARQUIS_OBJECTS_V1.md

Texto del bloque (comentarios del G-code, ASCII salvo los nombres, UTF-8). Fin de linea "\\n" o "\\r\\n" (Cura en Windows escribe CRLF):
    ;ARQUIS_OBJECTS=1 PID=3FA9C21B N=30                   <- cabecera (primeras lineas del archivo; deteccion rapida)
    ;ARQUIS_OBJECTS_BEGIN
    ;ARQ:H V=1 PID=3FA9C21B N=30 BED=3000,3000 MIN=0,0 ORG=FL SEQ=0 CAP=30
    ;ARQ:M <idx> <nombre base>
    ;ARQ:O <id> <idx_nombre> <inst> <cx> <cy> <ancho> <prof> <rot> <tipo> <z0> <ztop> <orden>
    ;ARQUIS_OBJECTS_END CRC=1A2B3C4D
    ;ARQUIS_TRAILER CRC=1A2B3C4D PID=3FA9C21B N=30          <- al final del archivo (despues viene el bloque ;SETTING_ de Cura)
El CRC-32 (IEEE, el de zlib) cubre las lineas entre BEGIN y END, cada una normalizada como ``texto + "\\n"`` (sin "\\r"). No se usan
longitudes ni desplazamientos en bytes: no cambian si el archivo se guarda con CRLF.
"""
import re
import zlib
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

from . import protocol_limits as limits
from .object_engine import ArquisObject, split_name
from .problems import ERROR, WARNING, Problem, has_errors

BEGIN = ";ARQUIS_OBJECTS_BEGIN"
END = ";ARQUIS_OBJECTS_END"
TRAILER = ";ARQUIS_TRAILER"
HEADER = ";ARQUIS_OBJECTS="
TAIL_WINDOW = 16 * 1024            # el firmware busca el bloque en los ultimos 16 KB (detras va el ;SETTING_ de Cura, unos KB)
HEADER_WINDOW = 512                # la cabecera debe caer en los primeros 512 B (como ;simage: de las miniaturas)
UNIT = 10                          # 0,1 mm por mm


@dataclass
class ManifestObject:
    id: int
    name_idx: int
    inst: int
    cx: int            # centro (0,1 mm)
    cy: int
    w: int             # ancho X (0,1 mm)
    d: int             # profundidad Y
    rot: int           # grados 0..359 (informativo)
    shape: str         # C | S | R
    z0: int            # Z de la primera capa con extrusion (0,1 mm)
    ztop: int          # Z de la ultima capa con extrusion
    order: int         # orden de impresion (0 = el primero que aparece en el G-code)


@dataclass
class Manifest:
    version: int = limits.PROTOCOL_VERSION
    pid: int = 0
    bed_w: int = 0     # 0,1 mm
    bed_d: int = 0
    min_x: int = 0
    min_y: int = 0
    origin: str = "FL"
    seq: int = 0       # 1 = un objeto cada vez
    cap: int = limits.DESIGN_CAPACITY
    names: List[str] = field(default_factory=list)
    objects: List[ManifestObject] = field(default_factory=list)

    def display_name(self, o: ManifestObject) -> str:
        """Texto de la lista: nombre base y, si hay varias copias, su numero (Llavero 2)."""
        n = self.names[o.name_idx]
        same = sum(1 for p in self.objects if p.name_idx == o.name_idx)
        return "%s %d" % (n, o.inst + 1) if same > 1 else n

    def to_dict(self) -> dict:
        return {"format": limits.PROTOCOL_NAME, "version": self.version, "print_id": "%08X" % self.pid,
                "bed": {"width": self.bed_w, "depth": self.bed_d, "min_x": self.min_x, "min_y": self.min_y, "origin": self.origin},
                "sequential": bool(self.seq), "capacity": self.cap, "names": list(self.names),
                "objects": [{"id": o.id, "name": self.names[o.name_idx], "instance": o.inst, "x": o.cx, "y": o.cy, "width": o.w,
                             "depth": o.d, "rotation": o.rot, "shape": o.shape, "z0": o.z0, "ztop": o.ztop, "order": o.order}
                            for o in self.objects]}


def crc32_lines(lines: Sequence[str]) -> int:
    """CRC-32 de las lineas normalizadas (texto sin CR/LF + '\\n'), en UTF-8."""
    crc = 0
    for l in lines:
        crc = zlib.crc32((l.rstrip("\r\n") + "\n").encode("utf-8"), crc)
    return crc & 0xFFFFFFFF


def clean_name(name: str) -> str:
    """Nombre para el manifiesto: sin controles ni ';', y como mucho NAME_MAX_BYTES bytes UTF-8 (sin partir un caracter)."""
    s = "".join(" " if (ord(c) < 32 or c in ";\x7f") else c for c in name).strip()
    s = re.sub(r"\s+", " ", s)
    out = ""
    for ch in s:
        if len((out + ch).encode("utf-8")) > limits.NAME_MAX_BYTES:
            break
        out += ch
    return out.strip() or "objeto"


def _dmm(mm: float) -> int:
    return int(round(mm * UNIT))


def _pid(m: Manifest, body: Sequence[str]) -> int:
    return crc32_lines(["BED=%d,%d,%d,%d" % (m.bed_w, m.bed_d, m.min_x, m.min_y)] + list(body))


def body_lines(m: Manifest, with_pid: bool = True) -> List[str]:
    lines = [";ARQ:H V=%d PID=%08X N=%d BED=%d,%d MIN=%d,%d ORG=%s SEQ=%d CAP=%d" %
             (m.version, m.pid if with_pid else 0, len(m.objects), m.bed_w, m.bed_d, m.min_x, m.min_y, m.origin, m.seq, m.cap)]
    lines += [";ARQ:M %d %s" % (i, n) for i, n in enumerate(m.names)]
    lines += [";ARQ:O %d %d %d %d %d %d %d %d %s %d %d %d" % (o.id, o.name_idx, o.inst, o.cx, o.cy, o.w, o.d, o.rot, o.shape, o.z0, o.ztop, o.order)
              for o in m.objects]
    return lines


def build_manifest(objects: Sequence[ArquisObject], stats: Dict[int, dict], bed: Tuple[float, float, float, float],
                   sequential: bool = False, capacity: int = limits.DESIGN_CAPACITY) -> Manifest:
    """Manifiesto de los objetos CANCELABLES (id >= 0). ``stats``: por id, {'z0','ztop','order'} medidos EN el G-code ya procesado.

    bed = (min_x, min_y, ancho, profundo) en mm, coordenadas de G-code. Posicion y tamano salen de la caja de la escena (lo que el
    usuario ve en Cura); Z y orden de impresion, del G-code. Los objetos sin id (fuera de capacidad) NO se publican: la pantalla
    no inventa objetos que no puede cancelar.
    """
    m = Manifest(bed_w=_dmm(bed[2]), bed_d=_dmm(bed[3]), min_x=_dmm(bed[0]), min_y=_dmm(bed[1]), seq=1 if sequential else 0, cap=capacity)
    name_ix: Dict[str, int] = {}
    for o in sorted((o for o in objects if o.cancelable), key=lambda o: o.id):
        base = clean_name(o.base_name)
        if base not in name_ix:
            name_ix[base] = len(m.names)
            m.names.append(base)
        x0, y0, x1, y1 = o.bbox
        st = stats[o.id]
        m.objects.append(ManifestObject(o.id, name_ix[base], o.instance, _dmm((x0 + x1) / 2), _dmm((y0 + y1) / 2),
                                        max(1, _dmm(x1 - x0)), max(1, _dmm(y1 - y0)), int(o.rot) % 360, o.shape,
                                        st["z0"], st["ztop"], st["order"]))
    m.pid = _pid(m, body_lines(m, with_pid=False)[1:])
    return m


def render_block(m: Manifest) -> Tuple[str, str]:
    """(marca de cabecera, bloque final). Ambas terminan en '\\n'."""
    body = body_lines(m)
    block = "\n".join([BEGIN] + body + ["%s CRC=%08X" % (END, crc32_lines(body))]) + "\n"
    crc = crc32_lines(body)
    block += "%s CRC=%08X PID=%08X N=%d\n" % (TRAILER, crc, m.pid, len(m.objects))
    return "%s1 PID=%08X N=%d\n" % (HEADER, m.pid, len(m.objects)), block


# --------------------------------------------------------------------------------------------------------------------------------
# Lectura y validacion (reproduce lo que hara el firmware con la misma ventana de busqueda)
# --------------------------------------------------------------------------------------------------------------------------------
@dataclass
class ParseResult:
    manifest: Optional[Manifest]
    problems: List[Problem]
    header_found: bool = False
    block_found: bool = False

    @property
    def ok(self) -> bool:
        return self.manifest is not None and not has_errors(self.problems)


def _err(ps: List[Problem], code: str, msg: str) -> None:
    ps.append(Problem(code, ERROR, msg))


# Rangos de almacenamiento del firmware (arquis_objects.h): un valor que no cabe en su campo hace que la linea sea invalida.
I16 = (-32768, 32767)
U16 = (0, 65535)


def _fits(*pairs) -> bool:
    return all(lo <= v <= hi for v, (lo, hi) in pairs)


def parse_text(text: str, bed_check: Optional[Tuple[int, int]] = None) -> ParseResult:
    """Lee el manifiesto de un G-code completo (texto). Busca el bloque SOLO en los ultimos TAIL_WINDOW bytes y la cabecera SOLO en
    los primeros HEADER_WINDOW bytes, como el firmware. ``bed_check``: (ancho, prof) en 0,1 mm de la maquina, si se quiere comprobar."""
    raw = text.encode("utf-8")
    head = raw[:HEADER_WINDOW].decode("utf-8", "ignore")
    tail = raw[-TAIL_WINDOW:].decode("utf-8", "ignore")
    return parse_regions(head, tail, bed_check)


def parse_regions(head: str, tail: str, bed_check: Optional[Tuple[int, int]] = None) -> ParseResult:
    ps: List[Problem] = []
    hdr = re.search(r"^;ARQUIS_OBJECTS=(\d+) PID=([0-9A-F]{8}) N=(\d+)\s*$", head, re.M)
    lines = [l.rstrip("\r") for l in tail.split("\n")]
    b = max((i for i, l in enumerate(lines) if l == BEGIN), default=None)
    if b is None:
        return ParseResult(None, [Problem("W_NO_MANIFEST", WARNING, "sin manifiesto ARQUIS en el archivo")], hdr is not None, False)
    e = next((i for i in range(b + 1, len(lines)) if lines[i].startswith(END)), None)
    if e is None:
        _err(ps, "E_MANIFEST_TRUNCATED", "falta ;ARQUIS_OBJECTS_END")
        return ParseResult(None, ps, hdr is not None, True)
    body = lines[b + 1:e]
    m_end = re.match(r"^;ARQUIS_OBJECTS_END CRC=([0-9A-F]{8})$", lines[e])
    if not m_end:
        _err(ps, "E_MANIFEST_FORMAT", "linea END mal formada")
        return ParseResult(None, ps, hdr is not None, True)
    if int(m_end.group(1), 16) != crc32_lines(body):
        _err(ps, "E_MANIFEST_CRC", "CRC del manifiesto incorrecto (datos corruptos o modificados)")
        return ParseResult(None, ps, hdr is not None, True)
    tr = next((re.match(r"^;ARQUIS_TRAILER CRC=([0-9A-F]{8}) PID=([0-9A-F]{8}) N=(\d+)$", l) for l in lines[e + 1:] if l.startswith(TRAILER)), None)
    if tr is None:
        _err(ps, "E_MANIFEST_TRAILER", "falta o esta mal formada la linea ;ARQUIS_TRAILER tras el bloque")
        return ParseResult(None, ps, hdr is not None, True)

    m = Manifest()
    names: Dict[int, str] = {}
    objs: List[ManifestObject] = []
    h_seen = False
    for l in body:
        if l.startswith(";ARQ:H "):
            mm = re.match(r"^;ARQ:H V=(\d+) PID=([0-9A-F]{8}) N=(\d+) BED=(\d+),(\d+) MIN=(-?\d+),(-?\d+) ORG=([A-Z]{2}) SEQ=([01]) CAP=(\d+)$", l)
            if not mm or h_seen:
                _err(ps, "E_MANIFEST_FORMAT", "linea H mal formada o repetida")
                return ParseResult(None, ps, hdr is not None, True)
            h_seen = True
            m.version, m.pid, n_decl = int(mm.group(1)), int(mm.group(2), 16), int(mm.group(3))
            m.bed_w, m.bed_d, m.min_x, m.min_y, m.origin, m.seq, m.cap = (int(mm.group(4)), int(mm.group(5)), int(mm.group(6)),
                                                                          int(mm.group(7)), mm.group(8), int(mm.group(9)), int(mm.group(10)))
            if not _fits((m.bed_w, U16), (m.bed_d, U16), (m.min_x, I16), (m.min_y, I16)):
                _err(ps, "E_MANIFEST_FORMAT", "valor de la linea H fuera del rango de almacenamiento")
                return ParseResult(None, ps, hdr is not None, True)
        elif l.startswith(";ARQ:M "):
            mm = re.match(r"^;ARQ:M (\d+) (.+)$", l)
            if not mm or not h_seen or objs:
                _err(ps, "E_MANIFEST_FORMAT", "linea M mal formada o fuera de orden (H, luego M, luego O): %r" % l)
                return ParseResult(None, ps, hdr is not None, True)
            idx, nm = int(mm.group(1)), mm.group(2)
            if idx >= limits.DESIGN_CAPACITY:
                _err(ps, "E_NAME_INDEX", "indice de nombre fuera de 0..%d: %d" % (limits.DESIGN_CAPACITY - 1, idx))
            if idx in names:
                _err(ps, "E_NAME_DUPLICATE", "indice de nombre repetido: %d" % idx)
            if len(nm.encode("utf-8")) > limits.NAME_MAX_BYTES:
                _err(ps, "E_NAME_TOO_LONG", "nombre de mas de %d bytes: %r" % (limits.NAME_MAX_BYTES, nm))
            names[idx] = nm
        elif l.startswith(";ARQ:O "):
            mm = re.match(r"^;ARQ:O (\d+) (\d+) (\d+) (-?\d+) (-?\d+) (\d+) (\d+) (\d+) ([CSR]) (-?\d+) (-?\d+) (\d+)$", l)
            if not mm or not h_seen:
                _err(ps, "E_MANIFEST_FORMAT", "linea O mal formada o antes de la H: %r" % l)
                return ParseResult(None, ps, hdr is not None, True)
            g = mm.groups()
            if not _fits((int(g[2]), (0, 255)), (int(g[3]), I16), (int(g[4]), I16), (int(g[5]), U16), (int(g[6]), U16), (int(g[9]), U16), (int(g[10]), U16)):
                _err(ps, "E_MANIFEST_FORMAT", "valor de la linea O fuera del rango de almacenamiento: %r" % l)
                return ParseResult(None, ps, hdr is not None, True)
            objs.append(ManifestObject(int(g[0]), int(g[1]), int(g[2]), int(g[3]), int(g[4]), int(g[5]), int(g[6]), int(g[7]), g[8],
                                       int(g[9]), int(g[10]), int(g[11])))
        else:
            _err(ps, "E_MANIFEST_FORMAT", "linea desconocida en el bloque: %r" % l)
            return ParseResult(None, ps, hdr is not None, True)
    if not h_seen:
        _err(ps, "E_MANIFEST_FORMAT", "falta la linea H")
        return ParseResult(None, ps, hdr is not None, True)
    if m.version != limits.PROTOCOL_VERSION:
        _err(ps, "E_VERSION", "version de manifiesto desconocida: %d (soportada: %d)" % (m.version, limits.PROTOCOL_VERSION))
        return ParseResult(None, ps, hdr is not None, True)
    m.names = [names[i] for i in sorted(names)]
    if sorted(names) != list(range(len(names))):
        _err(ps, "E_NAME_INDEX", "los indices de nombre no son contiguos desde 0")
    m.objects = objs

    # ---- validaciones semanticas ----
    if n_decl != len(objs):
        _err(ps, "E_COUNT", "N=%d pero hay %d objetos" % (n_decl, len(objs)))
    if len(objs) > limits.DESIGN_CAPACITY or len(objs) > m.cap:
        _err(ps, "E_OVER_CAPACITY", "%d objetos superan la capacidad (%d)" % (len(objs), min(limits.DESIGN_CAPACITY, m.cap)))
    if m.cap > limits.CAPACITY_CEILING:
        _err(ps, "E_CAP", "CAP=%d supera el techo del motor (%d)" % (m.cap, limits.CAPACITY_CEILING))
    seen = set()
    for o in objs:
        if o.id in seen:
            _err(ps, "E_ID_DUPLICATE", "id repetido: %d" % o.id)
        seen.add(o.id)
        if o.id >= min(len(objs), m.cap) or o.id >= limits.DESIGN_CAPACITY:
            _err(ps, "E_ID_RANGE", "id %d fuera de 0..%d" % (o.id, min(len(objs), m.cap) - 1))
        if o.name_idx not in names:
            _err(ps, "E_NAME_INDEX", "objeto %d usa un nombre inexistente (%d)" % (o.id, o.name_idx))
        if o.w <= 0 or o.d <= 0:
            _err(ps, "E_SIZE", "objeto %d con tamano no positivo" % o.id)
        if o.z0 > o.ztop:
            _err(ps, "E_Z", "objeto %d con z0 > ztop" % o.id)
        if o.rot > 359:
            _err(ps, "E_ROT", "objeto %d con rotacion fuera de 0..359" % o.id)
        x0, x1, y0, y1 = o.cx - o.w // 2, o.cx + o.w // 2, o.cy - o.d // 2, o.cy + o.d // 2
        if x1 < m.min_x or y1 < m.min_y or x0 > m.min_x + m.bed_w or y0 > m.min_y + m.bed_d:
            _err(ps, "E_OUTSIDE_BED", "objeto %d fuera de la cama" % o.id)
    if sorted(o.order for o in objs) != list(range(len(objs))) and objs:
        _err(ps, "E_ORDER", "los ordenes de impresion no son una permutacion de 0..N-1")
    if int(tr.group(1), 16) != crc32_lines(body) or int(tr.group(2), 16) != m.pid or int(tr.group(3)) != len(objs):
        _err(ps, "E_TRAILER_MISMATCH", "el trailer no coincide con el bloque (CRC/PID/N)")
    if hdr is not None and (int(hdr.group(1)) != m.version or int(hdr.group(2), 16) != m.pid or int(hdr.group(3)) != len(objs)):
        _err(ps, "E_HEADER_MISMATCH", "la cabecera ;ARQUIS_OBJECTS= no coincide con el bloque")
    if bed_check is not None and (abs(m.bed_w - bed_check[0]) > UNIT or abs(m.bed_d - bed_check[1]) > UNIT):
        _err(ps, "E_BED_MISMATCH", "la cama del manifiesto (%.1f x %.1f mm) no coincide con la de la maquina (%.1f x %.1f mm)"
             % (m.bed_w / UNIT, m.bed_d / UNIT, bed_check[0] / UNIT, bed_check[1] / UNIT))
    return ParseResult(m if not has_errors(ps) else None, ps, hdr is not None, True)
