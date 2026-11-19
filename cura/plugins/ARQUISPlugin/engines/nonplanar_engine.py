# SPDX-License-Identifier: LGPL-3.0-or-later
"""ARQUIS Non-Planar Engine (Etapa 5): motor EXPERIMENTAL, independiente y DESACTIVADO POR DEFECTO.

Que es: genera trayectorias en las que Z cambia mientras la boquilla se mueve en XY (capas no horizontales), y verifica que sean
ejecutables con movimientos G1 X Y Z convencionales de Marlin 2.1.2.10 SIN modificar el firmware. No es Vase Mode ni Spiralize.

Que NO es (limitaciones, ver docs/slicer_suite/09_etapa5_nonplanar.md):
  * La superficie de entrada es un CAMPO DE ALTURAS analitico (hoy: una onda senoidal). No lee STL ni escena de Cura, y no esta
    conectado al plugin ni a ningun script de Cura: nadie lo ejecuta salvo la herramienta cura/tools/nonplanar_demo.py.
  * Genera UNA pieza de prueba pequena (base plana + capas onduladas), no una impresion completa.
  * Las dimensiones del cabezal (bloque calefactor, ducto, angulo del cono) son PARAMETROS que el propietario debe medir:
    los valores por defecto son marcadores prudentes SIN medir, y sin ``head.confirmed`` no se exporta G-code.
  * Ninguna prueba fisica. La seguridad de movimientos se ha comprobado solo con calculos y con el modelo del propio motor.

Etapas internas (una funcion por etapa, en el orden pedido por el PROMPT MAESTRO):
  1 analyze_geometry  2 select_zones  3 generate_trajectories  4 adjust_z  5 adjust_speed  6 adjust_extrusion
  7 verify_limits     8 detect_collisions  9 render_preview  10 export_gcode (+ verify_gcode, comprobacion independiente del texto)
Barreras de seguridad: ``cfg.enabled`` (False por defecto), vista previa con codigo que el usuario debe devolver, cabezal confirmado,
temperaturas explicitas y cero hallazgos ERROR. Solo usa ordenes de Marlin ya presentes en este firmware (G0 G1 G28 G29 G90 G92
M82 M84 M104 M106 M107 M109 M140 M190 M420); no usa arcos, ni relativo, ni inventa comandos.
"""
import hashlib
import json
import math
from dataclasses import asdict, dataclass, field
from typing import Dict, List, Optional, Tuple

from .gcode_scan_engine import parse_cmd

EXPERIMENTAL = True
ENABLED_BY_DEFAULT = False
MARKER = ";ARQUIS_NONPLANAR EXPERIMENTAL"
ALLOWED_CMDS = ("G0", "G1", "G28", "G29", "G90", "G92", "M82", "M84", "M104", "M106", "M107", "M109", "M140", "M190", "M420")
SMALL_ENVELOPE_MM = 6.0          # volumenes del cabezal de hasta este radio se comprueban con la rejilla fina
HARD_MAX_Z_DEV_MM = 2.0          # tope absoluto de desviacion en Z, aunque el usuario pida mas


class NotEnabled(RuntimeError):
    """El motor esta desactivado por defecto: hay que activarlo expresamente (cfg.enabled = True)."""


class ExportRefused(RuntimeError):
    """No se exporta G-code: falta una barrera de seguridad o hay hallazgos ERROR."""


# ---------------------------------------------------------------- configuracion
@dataclass(frozen=True)
class MachineMotion:
    """Limites del firmware ARQUIS XQ1. Una prueba los compara con Marlin/Configuration.h (si cambian alla, la prueba falla)."""
    bed_x: float = 300.0
    bed_y: float = 300.0
    z_max: float = 450.0
    max_feedrate: Tuple[float, float, float, float] = (250.0, 250.0, 5.0, 25.0)         # X, Y, Z, E (mm/s): DEFAULT_MAX_FEEDRATE
    max_accel: Tuple[float, float, float, float] = (1200.0, 1200.0, 100.0, 10000.0)      # X, Y, Z, E (mm/s2): DEFAULT_MAX_ACCELERATION
    leveling_bilinear: bool = True                                                       # AUTO_BED_LEVELING_BILINEAR
    fade_height: float = 10.0                                                            # DEFAULT_LEVELING_FADE_HEIGHT (mm)


@dataclass
class HeadModel:
    """Geometria del cabezal. VALORES POR DEFECTO SIN MEDIR: el propietario debe medirlos en su hotend y poner ``confirmed=True``."""
    nozzle_clearance_deg: float = 20.0      # pendiente maxima (desde la horizontal) de la superficie por delante sin que la toque el cono de la boquilla
    safety_margin_deg: float = 5.0
    # (nombre, radio en mm alrededor de la punta, altura en mm por encima de la punta a la que empieza ese volumen)
    envelopes: Tuple[Tuple[str, float, float], ...] = (("bloque calefactor", 12.0, 3.0), ("ducto de ventilacion", 25.0, 2.0))
    confirmed: bool = False


@dataclass
class NPConfig:
    enabled: bool = False                    # DESACTIVADO POR DEFECTO
    # pieza de prueba (mm); por defecto centrada en la cama
    x0: float = 135.0
    y0: float = 140.0
    size_x: float = 30.0
    size_y: float = 20.0
    base_layers: int = 5
    layer_h: float = 0.2
    np_layers: int = 8
    amplitude: float = 0.6                   # desviacion maxima en Z de la ultima capa (mm)
    wavelength: float = 20.0
    phase: float = 0.0
    # extrusion
    nozzle_d: float = 0.4
    line_w: float = 0.42
    filament_d: float = 1.75                 # sin medir (valor habitual); comprobar en el perfil de material
    flow: float = 1.0
    max_flow_mm3s: float = 8.0               # caudal volumetrico maximo permitido (sin medir en tu hotend)
    # velocidades (mm/s)
    speed_planar: float = 30.0
    speed_np: float = 20.0
    speed_travel: float = 100.0
    seg_len: float = 1.0
    # limites
    max_z_dev: float = 1.0
    max_slope_deg: float = 15.0
    bed_margin: float = 5.0
    grid_mm: float = 3.0
    retract_mm: float = 0.8
    # temperaturas: None obliga al usuario a elegirlas (no se asume el material)
    bed_temp: Optional[float] = None
    nozzle_temp: Optional[float] = None
    custom_start_lines: Tuple[str, ...] = ()  # p. ej. ("M420 S1",) si el usuario usa malla guardada; se validan contra ALLOWED_CMDS
    head: HeadModel = field(default_factory=HeadModel)
    machine: MachineMotion = field(default_factory=MachineMotion)

    @property
    def z_base(self) -> float:
        return self.base_layers * self.layer_h

    @property
    def eff_max_slope_deg(self) -> float:
        return min(self.max_slope_deg, self.head.nozzle_clearance_deg - self.head.safety_margin_deg)


def config_from_dict(d: dict) -> NPConfig:
    d = dict(d)
    head = HeadModel(**{**asdict(HeadModel()), **{k: (tuple(tuple(e) for e in v) if k == "envelopes" else v) for k, v in d.pop("head", {}).items()}})
    d.pop("machine", None)           # la maquina NO se configura: es la del firmware
    if "custom_start_lines" in d:
        d["custom_start_lines"] = tuple(d["custom_start_lines"])
    return NPConfig(head=head, **d)


# ---------------------------------------------------------------- tipos
@dataclass(frozen=True)
class WaveField:
    amplitude: float
    wavelength: float
    phase: float
    x_origin: float

    def f(self, x: float, y: float) -> float:
        return self.amplitude * math.sin(2 * math.pi * (x - self.x_origin) / self.wavelength + self.phase)

    def grad(self, x: float, y: float) -> Tuple[float, float]:
        return (self.amplitude * 2 * math.pi / self.wavelength * math.cos(2 * math.pi * (x - self.x_origin) / self.wavelength + self.phase), 0.0)


@dataclass
class Finding:
    level: str          # ERROR | WARN | INFO
    code: str
    msg: str


@dataclass
class Seg:
    kind: str           # planar | np | zstep | travel
    layer: int
    x0: float
    y0: float
    z0: float
    x1: float
    y1: float
    z1: float
    t: float = 0.0      # espesor de la linea (mm)
    feed: float = 0.0   # mm/s
    e: float = 0.0      # filamento (mm) de este segmento

    @property
    def length(self) -> float:
        return math.sqrt((self.x1 - self.x0) ** 2 + (self.y1 - self.y0) ** 2 + (self.z1 - self.z0) ** 2)

    @property
    def printing(self) -> bool:
        return self.kind in ("planar", "np")


@dataclass
class Analysis:
    field: WaveField
    cells: List[Tuple[float, float, float, float, bool]]   # x, y, pendiente (deg), espesor_max, compatible
    max_slope_deg: float
    t_min: float
    t_max: float
    t_lo: float
    t_hi: float
    slope_limit_deg: float


@dataclass
class Zones:
    fraction: float
    incompatible: List[Tuple[float, float]]


@dataclass
class Plan:
    cfg: NPConfig
    analysis: Analysis
    zones: Zones
    segs: List[Seg]
    findings: List[Finding]
    stats: Dict[str, float]
    digest: str = ""

    @property
    def ok(self) -> bool:
        return not any(f.level == "ERROR" for f in self.findings)


# ---------------------------------------------------------------- 1. analisis geometrico
def make_field(cfg: NPConfig) -> WaveField:
    return WaveField(cfg.amplitude, cfg.wavelength, cfg.phase, cfg.x0)


def thickness_limits(cfg: NPConfig) -> Tuple[float, float]:
    return max(0.05, 0.4 * cfg.layer_h), 0.75 * cfg.nozzle_d


def analyze_geometry(cfg: NPConfig) -> Analysis:
    """Pendiente y espesor de capa en una rejilla de 1 mm sobre la huella de la pieza. La ultima capa tiene la pendiente maxima
    (la amplitud crece capa a capa, de modo que el espesor de cada capa es layer_h + F/np_layers y la base es plana)."""
    fld = make_field(cfg)
    t_lo, t_hi = thickness_limits(cfg)
    lim = cfg.eff_max_slope_deg
    cells = []
    max_slope = 0.0
    nx, ny = int(cfg.size_x), int(cfg.size_y)
    for i in range(nx + 1):
        for j in range(ny + 1):
            x, y = cfg.x0 + i * cfg.size_x / max(nx, 1), cfg.y0 + j * cfg.size_y / max(ny, 1)
            gx, gy = fld.grad(x, y)
            slope = math.degrees(math.atan(math.hypot(gx, gy)))
            tt = cfg.layer_h + fld.f(x, y) / cfg.np_layers
            ok = slope <= lim + 1e-9 and t_lo - 1e-9 <= tt <= t_hi + 1e-9
            cells.append((x, y, slope, tt, ok))
            max_slope = max(max_slope, slope)
    ts = [c[3] for c in cells]
    return Analysis(fld, cells, max_slope, min(ts), max(ts), t_lo, t_hi, lim)


# ---------------------------------------------------------------- 2. zonas compatibles
def select_zones(an: Analysis) -> Zones:
    bad = [(c[0], c[1]) for c in an.cells if not c[4]]
    return Zones(1.0 - len(bad) / len(an.cells), bad)


# ---------------------------------------------------------------- 3. trayectorias
def _positions(lo: float, hi: float, w: float) -> List[float]:
    n = max(2, int(round((hi - lo) / w)) + 1)
    return [lo + i * (hi - lo) / (n - 1) for i in range(n)]


def _points(a: float, b: float, seg_len: float) -> List[float]:
    m = max(1, int(math.ceil(abs(b - a) / seg_len - 1e-9)))
    return [a + (b - a) * j / m for j in range(m + 1)]


def _raster(cfg, orient, cur, zfn, tfn, layer, kind, segs):
    w = cfg.line_w
    x_lo, x_hi = cfg.x0 + w / 2, cfg.x0 + cfg.size_x - w / 2
    y_lo, y_hi = cfg.y0 + w / 2, cfg.y0 + cfg.size_y - w / 2
    if orient == "X":
        c_lo, c_hi, a_lo, a_hi, c_cur, a_cur = y_lo, y_hi, x_lo, x_hi, cur[1], cur[0]
    else:
        c_lo, c_hi, a_lo, a_hi, c_cur, a_cur = x_lo, x_hi, y_lo, y_hi, cur[0], cur[1]
    pos = _positions(c_lo, c_hi, w)
    if abs(c_cur - c_hi) < abs(c_cur - c_lo):
        pos.reverse()
    start_low = abs(a_cur - a_lo) <= abs(a_cur - a_hi)
    prev = None
    for i, p in enumerate(pos):
        a_from, a_to = (a_lo, a_hi) if ((i % 2 == 0) == start_low) else (a_hi, a_lo)
        pts = [((a, p) if orient == "X" else (p, a)) for a in _points(a_from, a_to, cfg.seg_len)]
        seq = ([prev] if prev is not None else []) + pts           # conexion con la fila anterior (mismo extremo)
        for (xa, ya), (xb, yb) in zip(seq, seq[1:]):
            segs.append(Seg(kind, layer, xa, ya, zfn(xa, ya), xb, yb, zfn(xb, yb), tfn((xa + xb) / 2, (ya + yb) / 2)))
        prev = pts[-1]
    return prev


def generate_trajectories(cfg: NPConfig, zones: Zones) -> List[Seg]:
    """Base plana (lineas alternando X/Y) y capas onduladas en lineas X. Entre capas, un paso Z sin extrusion en el mismo XY.
    (Hoy genera siempre sobre toda la huella; si ``zones`` tiene zonas incompatibles, verify_limits lo marca ERROR y no se exporta.)"""
    fld = make_field(cfg)
    h, K = cfg.layer_h, cfg.np_layers
    segs: List[Seg] = []
    cur = (cfg.x0 + cfg.line_w / 2, cfg.y0 + cfg.line_w / 2)
    z_cur = 0.0
    layer = 0
    for k in range(1, cfg.base_layers + 1):
        layer += 1
        z = k * h
        if z_cur != z:
            segs.append(Seg("zstep", layer, cur[0], cur[1], z_cur, cur[0], cur[1], z))
        cur = _raster(cfg, "X" if k % 2 else "Y", cur, lambda x, y, z=z: z, lambda x, y: h, layer, "planar", segs)
        z_cur = z
    for k in range(1, K + 1):
        layer += 1
        zfn = lambda x, y, k=k: cfg.z_base + k * h + (k / K) * fld.f(x, y)
        tfn = lambda x, y: h + fld.f(x, y) / K
        z_new = zfn(cur[0], cur[1])
        segs.append(Seg("zstep", layer, cur[0], cur[1], z_cur, cur[0], cur[1], z_new))
        cur = _raster(cfg, "X", cur, zfn, tfn, layer, "np", segs)
        z_cur = zfn(cur[0], cur[1])
    return segs


# ---------------------------------------------------------------- 4. ajuste de Z
def adjust_z(segs: List[Seg], cfg: NPConfig, findings: List[Finding]) -> None:
    """Comprueba la altura Z de cada punto (no la altera en silencio): desviacion respecto a la capa plana, limites de la maquina y tope duro."""
    # desviacion = onda de la ultima capa
    z_dev = abs(cfg.amplitude)
    limit = min(cfg.max_z_dev, HARD_MAX_Z_DEV_MM)
    if cfg.max_z_dev > HARD_MAX_Z_DEV_MM:
        findings.append(Finding("WARN", "ZDEV_TOPE", "max_z_dev=%.2f mm supera el tope duro de %.1f mm: se aplica el tope." % (cfg.max_z_dev, HARD_MAX_Z_DEV_MM)))
    if z_dev > limit + 1e-9:
        findings.append(Finding("ERROR", "ZDEV", "Desviacion maxima en Z %.2f mm > limite %.2f mm." % (z_dev, limit)))
    zs = [z for s in segs for z in (s.z0, s.z1)]
    if zs and (min(zs) < -1e-9 or max(zs) > cfg.machine.z_max):
        findings.append(Finding("ERROR", "Z_RANGE", "Z fuera de 0..%.0f mm." % cfg.machine.z_max))
    top = max(zs) if zs else 0.0
    if cfg.machine.leveling_bilinear and top > cfg.machine.fade_height:
        findings.append(Finding("WARN", "FADE", "La pieza supera la altura de desvanecimiento de la nivelacion (%.0f mm): la compensacion se atenua." % cfg.machine.fade_height))
    if cfg.machine.leveling_bilinear:
        findings.append(Finding("INFO", "NIVELACION", "Nivelacion bilineal activa en el firmware (fade %.0f mm). El archivo NO activa la malla por si mismo: "
                                "si usas una malla guardada, anade tu orden (p. ej. M420 S1) en custom_start_lines." % cfg.machine.fade_height))


# ---------------------------------------------------------------- 5. velocidad
def adjust_speed(segs: List[Seg], cfg: NPConfig) -> Dict[str, float]:
    """Velocidad por segmento (mm/s) = minimo de: la del tipo, el tope del eje Z (90 % de su maximo), el caudal volumetrico maximo.
    Marlin aplica F a la longitud XYZ del movimiento (planner.cpp: millimeters = sqrt(x2+y2+z2)). Despues limita la aceleracion de Z."""
    zmax = cfg.machine.max_feedrate[2]
    amax = cfg.machine.max_accel[2]
    for s in segs:
        L = s.length
        if L <= 0:
            continue
        if s.kind == "zstep":
            s.feed = 0.5 * zmax
            continue
        v = cfg.speed_planar if s.kind == "planar" else cfg.speed_np
        dz = abs(s.z1 - s.z0)
        if dz > 1e-9:
            v = min(v, 0.9 * zmax * L / dz)
        area = _area(s.t, cfg.line_w)
        if area > 0:
            v = min(v, cfg.max_flow_mm3s / area)
        s.feed = v
    # aceleracion de Z estimada entre segmentos consecutivos (velocidad constante en cada uno); si pasa del 80 % del limite se baja la velocidad.
    # Es PESIMISTA en las esquinas: el planificador de Marlin frena ademas en cada cambio de direccion (JUNCTION_DEVIATION).
    def worst_az() -> float:
        w = 0.0
        for a, b in zip(segs, segs[1:]):
            if a.printing and b.printing and a.length and b.length:
                va, vb = a.feed * (a.z1 - a.z0) / a.length, b.feed * (b.z1 - b.z0) / b.length
                dt = (a.length + b.length) / 2 / max((a.feed + b.feed) / 2, 1e-9)
                w = max(w, abs(vb - va) / dt)
        return w
    for _ in range(6):
        if worst_az() <= 0.8 * amax:
            break
        for a, b in zip(segs, segs[1:]):
            if a.printing and b.printing and a.length and b.length:
                va, vb = a.feed * (a.z1 - a.z0) / a.length, b.feed * (b.z1 - b.z0) / b.length
                dt = (a.length + b.length) / 2 / max((a.feed + b.feed) / 2, 1e-9)
                az = abs(vb - va) / dt
                if az > 0.8 * amax:
                    f = max(0.5, math.sqrt(0.8 * amax / az))
                    a.feed *= f
                    b.feed *= f
    return {"az_max_est": worst_az(), "az_limit": amax}


def _area(t: float, w: float) -> float:
    """Seccion de una linea extruida (mm2): rectangulo con extremos redondos (modelo habitual de Slic3r/Cura), t = espesor, w = ancho."""
    if t <= 0:
        return 0.0
    if t >= w:
        return w * w * math.pi / 4
    return t * (w - t) + math.pi * (t / 2) ** 2


# ---------------------------------------------------------------- 6. extrusion
def adjust_extrusion(segs: List[Seg], cfg: NPConfig) -> float:
    """E (mm de filamento) de cada segmento = seccion * longitud 3D * flujo / seccion del filamento. Devuelve el total."""
    fil = math.pi * (cfg.filament_d / 2) ** 2
    total = 0.0
    for s in segs:
        s.e = _area(s.t, cfg.line_w) * s.length * cfg.flow / fil if s.printing else 0.0
        total += s.e
    return total


# ---------------------------------------------------------------- 7. limites
def verify_limits(segs: List[Seg], cfg: NPConfig, an: Analysis, zones: Zones, findings: List[Finding]) -> None:
    m = cfg.machine
    xs = [c for s in segs for c in (s.x0, s.x1)]
    ys = [c for s in segs for c in (s.y0, s.y1)]
    if min(xs) < cfg.bed_margin or max(xs) > m.bed_x - cfg.bed_margin or min(ys) < cfg.bed_margin or max(ys) > m.bed_y - cfg.bed_margin:
        findings.append(Finding("ERROR", "BED", "La pieza se acerca a menos de %.0f mm del borde de la cama %gx%g." % (cfg.bed_margin, m.bed_x, m.bed_y)))
    worst_slope = 0.0
    for s in segs:
        L = s.length
        if L <= 0:
            continue
        if s.printing:
            horiz = math.hypot(s.x1 - s.x0, s.y1 - s.y0)
            if horiz > 1e-9:
                worst_slope = max(worst_slope, math.degrees(math.atan(abs(s.z1 - s.z0) / horiz)))
            vx, vy, vz = (s.feed * abs(s.x1 - s.x0) / L, s.feed * abs(s.y1 - s.y0) / L, s.feed * abs(s.z1 - s.z0) / L)
            if vx > m.max_feedrate[0] or vy > m.max_feedrate[1] or vz > m.max_feedrate[2] + 1e-9:
                findings.append(Finding("ERROR", "FEED", "Velocidad por eje por encima del maximo del firmware (vz=%.2f mm/s, maximo %.1f)." % (vz, m.max_feedrate[2])))
                break
    if worst_slope > cfg.eff_max_slope_deg + 1e-6:
        findings.append(Finding("ERROR", "SLOPE", "Pendiente de trayectoria %.1f deg > limite %.1f deg (min(max_slope %.1f, holgura del cono %.1f - margen %.1f))."
                                % (worst_slope, cfg.eff_max_slope_deg, cfg.max_slope_deg, cfg.head.nozzle_clearance_deg, cfg.head.safety_margin_deg)))
    t_lo, t_hi = an.t_lo, an.t_hi
    if an.t_min < t_lo - 1e-9 or an.t_max > t_hi + 1e-9:
        findings.append(Finding("ERROR", "ESPESOR", "Espesor de capa %.3f..%.3f mm fuera de %.3f..%.3f mm." % (an.t_min, an.t_max, t_lo, t_hi)))
    if zones.incompatible:
        findings.append(Finding("ERROR", "ZONA", "%d puntos de la huella son incompatibles (pendiente o espesor): el motor no deja huecos, asi que no se exporta." % len(zones.incompatible)))
    flow = max((_area(s.t, cfg.line_w) * s.feed for s in segs if s.printing), default=0.0)
    if flow > cfg.max_flow_mm3s + 1e-9:
        findings.append(Finding("ERROR", "CAUDAL", "Caudal %.2f mm3/s > maximo %.2f." % (flow, cfg.max_flow_mm3s)))
    if cfg.head.nozzle_clearance_deg - cfg.head.safety_margin_deg <= 0:
        findings.append(Finding("ERROR", "CONO", "La holgura del cono menos el margen no es positiva."))
    # extruder: la velocidad de E por segmento
    for s in segs:
        if s.printing and s.length > 0 and s.e / (s.length / s.feed) > m.max_feedrate[3]:
            findings.append(Finding("ERROR", "FEED_E", "Velocidad del extrusor por encima del maximo del firmware (%.0f mm/s)." % m.max_feedrate[3]))
            break
    findings.append(Finding("INFO", "RETRACCION", "Sin retracciones dentro de las capas onduladas; al final se retrae %.1f mm. Sin viajes entre piezas." % cfg.retract_mm))
    findings.append(Finding("INFO", "ACELERACION", "El G-code no cambia aceleraciones ni jerk (el firmware usa JUNCTION_DEVIATION, no CLASSIC_JERK)."))


# ---------------------------------------------------------------- 8. colisiones
_DISKS: Dict[Tuple[float, float], List[Tuple[int, int]]] = {}


def _disk(rad: float, cell: float) -> List[Tuple[int, int]]:
    key = (rad, cell)
    if key not in _DISKS:
        n = int(math.ceil(rad / cell)) + 1
        _DISKS[key] = [(i, j) for i in range(-n, n + 1) for j in range(-n, n + 1) if math.hypot(i * cell, j * cell) <= rad + cell * 0.7071]
    return _DISKS[key]


def detect_collisions(segs: List[Seg], cfg: NPConfig, findings: List[Finding]) -> Dict[str, float]:
    """Mapa de alturas de lo YA impreso, en orden de impresion, con DOS rejillas:
      * gruesa (grid_mm): para los volumenes del cabezal (radio r, altura h sobre la punta): ningun material impreso dentro de r puede
        estar mas alto que punta + h. Conservadora (suma media diagonal de celda).
      * fina (0,5 mm): para la punta: no puede quedar por debajo del material impreso JUSTO debajo de ella (con tolerancia de dos
        espesores + la pendiente de la celda). Imprimir junto a una pared mas alta NO es colision.
    Los segmentos se marcan a lo largo de todo su recorrido (no solo en los extremos). Coste O(muestras x celdas impresas): valido para
    una pieza pequena de prueba, no para una impresion completa."""
    g, fg = cfg.grid_mm, 0.5
    printed: Dict[Tuple[int, int], float] = {}
    fine: Dict[Tuple[int, int], float] = {}
    tol = cfg.layer_h * 2 + fg * math.tan(math.radians(max(cfg.eff_max_slope_deg, 0.0)))
    half_diag = g * 0.7071
    r = cfg.line_w / 2
    worst_dive = 0.0
    n_checked = 0
    bad: Dict[str, Tuple[float, float, float]] = {}
    for idx, s in enumerate(segs):
        if not s.printing:
            continue
        tip = (s.x1, s.y1, s.z1)
        if idx % 2 == 0:
            n_checked += 1
            under = fine.get((int(tip[0] // fg), int(tip[1] // fg)), 0.0) - tip[2]
            worst_dive = max(worst_dive, under)
            if under > tol and "TIP" not in bad:
                bad["TIP"] = (tip[0], tip[1], under)
            for name, rad, hgt in cfg.head.envelopes:
                if name in bad:
                    continue
                if rad <= SMALL_ENVELOPE_MM:         # volumen pequeno (p. ej. el hexagono de la boquilla): rejilla fina, sin margen de celda gruesa
                    ci, cj = int(tip[0] // fg), int(tip[1] // fg)
                    for di, dj in _disk(rad, fg):
                        top = fine.get((ci + di, cj + dj))
                        if top is not None and top > tip[2] + hgt:
                            bad[name] = (tip[0], tip[1], top - (tip[2] + hgt))
                            break
                    continue
                for (cx, cy), top in printed.items():
                    if top > tip[2] + hgt and math.hypot((cx + 0.5) * g - tip[0], (cy + 0.5) * g - tip[1]) - half_diag <= rad:
                        bad[name] = (tip[0], tip[1], top - (tip[2] + hgt))
                        break
        horiz = math.hypot(s.x1 - s.x0, s.y1 - s.y0)
        n = max(1, int(math.ceil(horiz / (fg / 2))))
        for k in range(n + 1):
            f = k / n
            px, py, pz = s.x0 + (s.x1 - s.x0) * f, s.y0 + (s.y1 - s.y0) * f, s.z0 + (s.z1 - s.z0) * f
            for dx in (-r, 0.0, r):
                for dy in (-r, 0.0, r):
                    c = (int((px + dx) // g), int((py + dy) // g))
                    printed[c] = max(printed.get(c, 0.0), pz)
                    cf = (int((px + dx) // fg), int((py + dy) // fg))
                    fine[cf] = max(fine.get(cf, 0.0), pz)
    for name, (x, y, over) in bad.items():
        if name == "TIP":
            findings.append(Finding("ERROR", "COLISION_PUNTA", "La punta queda %.2f mm por debajo del material impreso justo debajo de ella en (%.1f, %.1f)." % (over, x, y)))
        else:
            findings.append(Finding("ERROR", "COLISION_CABEZAL", "%s: material impreso %.2f mm por encima de su volumen en (%.1f, %.1f)." % (name, over, x, y)))
    top_part = max(printed.values()) if printed else 0.0
    findings.append(Finding("INFO", "COLISION_MODELO", "Colisiones comprobadas con el MODELO del cabezal (%d volumenes, valores %s). "
                            "No se ha comprobado con tu hotend real." % (len(cfg.head.envelopes), "CONFIRMADOS por el usuario" if cfg.head.confirmed else "SIN MEDIR")))
    return {"muestras": float(n_checked), "inmersion_max_mm": worst_dive, "altura_pieza_mm": top_part}


# ---------------------------------------------------------------- plan completo (etapas 1-8)
def build_plan(cfg: NPConfig) -> Plan:
    if not cfg.enabled:
        raise NotEnabled("El motor no planar esta desactivado por defecto: activa cfg.enabled=True expresamente (experimental, sin pruebas fisicas).")
    an = analyze_geometry(cfg)
    zones = select_zones(an)
    segs = generate_trajectories(cfg, zones)
    findings: List[Finding] = []
    adjust_z(segs, cfg, findings)
    sp = adjust_speed(segs, cfg)
    e_total = adjust_extrusion(segs, cfg)
    verify_limits(segs, cfg, an, zones, findings)
    col = detect_collisions(segs, cfg, findings)
    if sp["az_max_est"] > sp["az_limit"]:
        findings.append(Finding("ERROR", "ACEL_Z", "Aceleracion de Z estimada %.0f mm/s2 > maximo %.0f." % (sp["az_max_est"], sp["az_limit"])))
    printing = [s for s in segs if s.printing]
    stats = {"segmentos": float(len(segs)), "segmentos_impresion": float(len(printing)), "e_total_mm": e_total,
             "pendiente_max_deg": an.max_slope_deg, "espesor_min": an.t_min, "espesor_max": an.t_max,
             "vz_max_mm_s": max((s.feed * abs(s.z1 - s.z0) / s.length for s in printing if s.length), default=0.0),
             "az_est_mm_s2": sp["az_max_est"], "feed_min_mm_s": min((s.feed for s in printing), default=0.0),
             "caudal_max_mm3_s": max((_area(s.t, cfg.line_w) * s.feed for s in printing), default=0.0),
             "zonas_compatibles": zones.fraction, "z_max_mm": max(max(s.z0, s.z1) for s in segs), **col}
    plan = Plan(cfg, an, zones, segs, findings, stats)
    plan.digest = _digest(plan)
    return plan


def _digest(plan: Plan) -> str:
    """Codigo de la vista previa: cambia si cambia cualquier parametro o cualquier trayectoria. El usuario lo devuelve para poder exportar."""
    blob = json.dumps({"cfg": asdict(plan.cfg), "n": len(plan.segs), "e": round(plan.stats["e_total_mm"], 4),
                       "sum": round(sum(s.x1 + 2 * s.y1 + 3 * s.z1 for s in plan.segs), 3)}, sort_keys=True, default=list)
    return hashlib.sha256(blob.encode()).hexdigest()[:12].upper()


# ---------------------------------------------------------------- 9. vista previa (SVG, sin dependencias)
def render_preview(plan: Plan) -> str:
    cfg, an = plan.cfg, plan.analysis
    W, H = 980, 640
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" font-family="sans-serif" font-size="12">' % (W, H, W, H),
           '<rect width="100%" height="100%" fill="#0f1620"/>',
           '<text x="16" y="24" fill="#ffb020" font-size="15" font-weight="bold">ARQUIS Non-Planar EXPERIMENTAL - vista previa (NO es G-code, NO se ha probado en la impresora)</text>',
           '<text x="16" y="44" fill="#cfd8e3">Codigo de vista previa: %s   (hay que devolverlo para poder exportar)</text>' % plan.digest]
    # vista superior: celdas coloreadas por pendiente; verde = compatible
    ox, oy, sc = 16, 70, 12.0
    out.append('<text x="%d" y="%d" fill="#9fb3c8">Vista superior (X derecha, Y arriba): pendiente por zona</text>' % (ox, oy - 6))
    for x, y, slope, t, ok in an.cells:
        px, py = ox + (x - cfg.x0) * sc, oy + (cfg.size_y - (y - cfg.y0)) * sc
        frac = min(slope / max(an.slope_limit_deg, 1e-6), 1.0)
        col = "#2ecc71" if ok else "#e74c3c"
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s" fill-opacity="%.2f"/>' % (px - sc / 2, py - sc / 2, sc, sc, col, 0.25 + 0.6 * frac))
    out.append('<rect x="%d" y="%d" width="%.0f" height="%.0f" fill="none" stroke="#9fb3c8"/>' % (ox, oy, cfg.size_x * sc, cfg.size_y * sc))
    # perfil lateral: z frente a x, capa por capa, escala vertical exagerada
    px0, py0 = 420, 70
    zscale = 90.0
    out.append('<text x="%d" y="%d" fill="#9fb3c8">Perfil lateral (Z exagerado x%.1f respecto a X)</text>' % (px0, py0 - 6, zscale / 16.0))
    fld = an.field
    xs = [cfg.x0 + i * cfg.size_x / 120 for i in range(121)]
    zmax = cfg.z_base + cfg.np_layers * cfg.layer_h + cfg.amplitude
    base_y = py0 + 330
    out.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#555"/>' % (px0, base_y, px0 + cfg.size_x * 16, base_y))
    for k in range(0, cfg.np_layers + 1):
        pts = " ".join("%.1f,%.1f" % (px0 + (x - cfg.x0) * 16, base_y - (cfg.z_base + k * cfg.layer_h + (k / cfg.np_layers) * fld.f(x, 0)) * zscale) for x in xs)
        out.append('<polyline points="%s" fill="none" stroke="%s" stroke-width="%s"/>' % (pts, "#4aa3ff" if k else "#9fb3c8", 2 if k == cfg.np_layers else 1))
    for k in range(1, cfg.base_layers + 1):
        yy = base_y - k * cfg.layer_h * zscale
        out.append('<line x1="%d" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#666"/>' % (px0, yy, px0 + cfg.size_x * 16, yy))
    out.append('<text x="%d" y="%.1f" fill="#9fb3c8">Z max de la pieza = %.2f mm</text>' % (px0, base_y + 18, zmax))
    # resumen y hallazgos
    y = 470
    st = plan.stats
    lines = ["Pendiente maxima %.1f deg (limite efectivo %.1f deg)   espesor de capa %.3f..%.3f mm   zonas compatibles %.0f %%" % (st["pendiente_max_deg"], an.slope_limit_deg, st["espesor_min"], st["espesor_max"], 100 * st["zonas_compatibles"]),
             "vz max %.2f mm/s (firmware %.1f)   az estimada %.0f mm/s2 (firmware %.0f)   caudal max %.2f mm3/s   velocidad min %.1f mm/s   %d segmentos" % (st["vz_max_mm_s"], cfg.machine.max_feedrate[2], st["az_est_mm_s2"], cfg.machine.max_accel[2], st["caudal_max_mm3_s"], st["feed_min_mm_s"], st["segmentos"]),
             "Cabezal: %s" % ("CONFIRMADO por el usuario" if cfg.head.confirmed else "SIN CONFIRMAR (valores por defecto sin medir): no se podra exportar")]
    for ln in lines:
        out.append('<text x="16" y="%d" fill="#cfd8e3">%s</text>' % (y, _esc(ln)))
        y += 18
    for f in plan.findings:
        col = {"ERROR": "#ff6b6b", "WARN": "#ffb020", "INFO": "#8fb4d9"}[f.level]
        out.append('<text x="16" y="%d" fill="%s">[%s] %s: %s</text>' % (y, col, f.level, f.code, _esc(f.msg[:150])))
        y += 16
        if y > H - 8:
            break
    out.append('</svg>')
    return "\n".join(out)


def _esc(s: str) -> str:
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


# ---------------------------------------------------------------- 10. exportacion
def export_gcode(plan: Plan, accept_code: str) -> str:
    """Solo si: el motor esta activo, no hay ERROR, el cabezal esta confirmado, las temperaturas son explicitas, el usuario devuelve
    el codigo de la vista previa y el G-code resultante pasa verify_gcode (comprobacion independiente del texto)."""
    cfg = plan.cfg
    if not cfg.enabled:
        raise NotEnabled("Motor desactivado.")
    if not plan.ok:
        raise ExportRefused("Hay hallazgos ERROR: " + "; ".join(f.code for f in plan.findings if f.level == "ERROR"))
    if not cfg.head.confirmed:
        raise ExportRefused("Las dimensiones del cabezal no estan confirmadas (head.confirmed=False): mide tu hotend y confirmalo.")
    if cfg.bed_temp is None or cfg.nozzle_temp is None:
        raise ExportRefused("Falta bed_temp / nozzle_temp: el motor no asume el material.")
    if (accept_code or "").strip().upper() != plan.digest:
        raise ExportRefused("El codigo de vista previa no coincide (%s): genera la vista previa, revisala y devuelve su codigo." % plan.digest)
    for ln in cfg.custom_start_lines:
        c = parse_cmd(ln)
        if c is None or c.name not in ALLOWED_CMDS or c.name in ("G0", "G1"):
            raise ExportRefused("custom_start_lines: orden no permitida: %r" % ln)
    m = cfg.machine
    zspeed = 0.5 * m.max_feedrate[2] * 60
    first = plan.segs[0]
    L = [MARKER + " - NO USAR SIN REVISAR LA VISTA PREVIA", "; codigo de vista previa: %s" % plan.digest,
         "; pieza: %gx%g mm, base %d capas, %d capas onduladas, onda %.2f mm / %.1f mm, Z max %.2f mm" % (cfg.size_x, cfg.size_y, cfg.base_layers, cfg.np_layers, cfg.amplitude, cfg.wavelength, plan.stats["z_max_mm"]),
         "; EXPERIMENTAL: sin pruebas fisicas. Una mano en el paro de emergencia.",
         "M140 S%g" % cfg.bed_temp, "M104 S%g" % cfg.nozzle_temp, "M190 S%g" % cfg.bed_temp, "M109 S%g" % cfg.nozzle_temp, "G28"]
    L += list(cfg.custom_start_lines)
    L += ["G90", "M82", "G92 E0", "G1 Z5 F%d" % zspeed, "G1 X%.3f Y%.3f F%d" % (first.x0, first.y0, cfg.speed_travel * 60), "G1 Z%.3f F%d" % (cfg.layer_h, zspeed)]
    e = 0.0
    cur_layer = 0
    for s in plan.segs:
        if s.layer != cur_layer:
            cur_layer = s.layer
            L.append(";LAYER %d (%s)" % (cur_layer, "no planar" if s.kind == "np" or (s.kind == "zstep" and cur_layer > cfg.base_layers) else "plana"))
        e += s.e
        if s.kind == "zstep":
            L.append("G1 Z%.4f F%d" % (s.z1, int(round(s.feed * 60))))
        else:
            L.append("G1 X%.3f Y%.3f Z%.4f E%.5f F%d" % (s.x1, s.y1, s.z1, e, int(round(s.feed * 60))))
    last = plan.segs[-1]
    L += ["G1 E%.5f F2400" % (e - cfg.retract_mm), "G1 Z%.3f F%d" % (min(last.z1 + 5, m.z_max), zspeed),
          "M104 S0", "M140 S0", "M107", "M84 X Y E", ";End of non-planar test"]
    text = "\n".join(L) + "\n"
    errs = [f for f in verify_gcode(text, cfg, plan) if f.level == "ERROR"]
    if errs:
        raise ExportRefused("El G-code generado no pasa la verificacion independiente: " + "; ".join(f.code for f in errs))
    return text


def verify_gcode(text: str, cfg: NPConfig, plan: Optional[Plan] = None) -> List[Finding]:
    """Lee el TEXTO (no el modelo interno) y comprueba: marca experimental, solo ordenes permitidas, absoluto, limites de cama y Z,
    velocidades por eje <= firmware, E sin retroceso salvo la retraccion final, y (si hay plan) que E total y numero de movimientos coincidan."""
    m = cfg.machine
    F: List[Finding] = []
    lines = text.splitlines()
    if not lines or not lines[0].startswith(MARKER):
        F.append(Finding("ERROR", "G_MARCA", "Falta la marca experimental en la primera linea."))
    x = y = z = e = 0.0
    feed = 0.0
    seen_xyz = False
    n_print = 0
    e_max = 0.0
    for ln in lines:
        c = parse_cmd(ln)
        if c is None:
            continue
        if c.name not in ALLOWED_CMDS:
            F.append(Finding("ERROR", "G_ORDEN", "Orden no permitida: %s" % c.name))
            continue
        w = c.words
        if c.name == "G92":
            e = w.get("E", e)
            continue
        if c.name not in ("G0", "G1"):
            continue
        nx, ny, nz, ne = w.get("X", x), w.get("Y", y), w.get("Z", z), w.get("E", e)
        if "F" in w:
            feed = w["F"] / 60.0
        if not (0 <= nx <= m.bed_x and 0 <= ny <= m.bed_y and 0 <= nz <= m.z_max):
            F.append(Finding("ERROR", "G_LIMITES", "Movimiento fuera de la maquina: X%.2f Y%.2f Z%.2f" % (nx, ny, nz)))
        d = math.sqrt((nx - x) ** 2 + (ny - y) ** 2 + (nz - z) ** 2)
        if d > 1e-9:
            if feed <= 0:
                F.append(Finding("ERROR", "G_SIN_F", "Movimiento sin velocidad."))
            else:
                v = (feed * abs(nx - x) / d, feed * abs(ny - y) / d, feed * abs(nz - z) / d)
                if v[0] > m.max_feedrate[0] + 1e-6 or v[1] > m.max_feedrate[1] + 1e-6 or v[2] > m.max_feedrate[2] + 1e-6:
                    F.append(Finding("ERROR", "G_VEL", "Velocidad por eje sobre el maximo del firmware: %s" % ln.strip()))
        if "E" in w:
            if ne > e + 1e-9 and d > 1e-9:
                n_print += 1
            if ne < e_max - cfg.retract_mm - 1e-6:
                F.append(Finding("ERROR", "G_E", "Retroceso de E mayor que la retraccion final."))
            e_max = max(e_max, ne)
        x, y, z, e = nx, ny, nz, ne
        seen_xyz = True
    if not seen_xyz:
        F.append(Finding("ERROR", "G_VACIO", "El G-code no contiene movimientos."))
    if plan is not None:
        want = sum(1 for s in plan.segs if s.printing and s.e > 0)
        if abs(e_max - plan.stats["e_total_mm"]) > 0.005 * plan.stats["e_total_mm"] + 0.01:
            F.append(Finding("ERROR", "G_E_TOTAL", "E total %.3f no coincide con el plan %.3f." % (e_max, plan.stats["e_total_mm"])))
        if n_print != want:
            F.append(Finding("ERROR", "G_N", "Movimientos con extrusion %d != plan %d." % (n_print, want)))
    return F


# ---------------------------------------------------------------- analisis de un G-code EXTERNO (solo lectura)
@dataclass
class ExternalReport:
    findings: List[Finding]
    stats: Dict[str, float]

    @property
    def ok(self) -> bool:
        return not any(f.level == "ERROR" for f in self.findings)


def analyze_external_gcode(text: str, cfg: NPConfig) -> ExternalReport:
    """Mide un G-code NO generado por este motor (p. ej. FullControl) contra los limites de TU firmware y el modelo de TU cabezal.
    No lo modifica ni lo exporta. Sigue G92 X/Y (desplaza el origen), M82/M83 y G90/G91; los limites de cama se miden en coordenadas FISICAS."""
    m = cfg.machine
    F: List[Finding] = []
    x = y = z = e = 0.0
    off_x = off_y = 0.0
    feed = 0.0
    absxyz, abse = True, True
    foreign: Dict[str, int] = {}
    risky: List[str] = []
    segs: List[Seg] = []
    worst_slope = 0.0
    vz_max = vx_max = vy_max = 0.0
    n_over_travel = 0
    phys = [1e9, -1e9, 1e9, -1e9, 1e9, -1e9]          # todos los movimientos (viajes incluidos)
    ext = [1e9, -1e9, 1e9, -1e9, 1e9, -1e9]           # solo los que extruyen
    for ln in text.splitlines():
        c = parse_cmd(ln)
        if c is None:
            continue
        w = c.words
        name = c.name
        if name not in ALLOWED_CMDS:
            foreign[name] = foreign.get(name, 0) + 1
        if name == "G90":
            absxyz = True; continue
        if name == "G91":
            absxyz = False; risky.append("G91 (coordenadas relativas)"); continue
        if name == "M82":
            abse = True; continue
        if name == "M83":
            abse = False; continue
        if name == "M106" and ("P" in w and w["P"] != 0):
            risky.append("M106 P%d (ventilador auxiliar: tu firmware puede no tenerlo)" % int(w["P"]))
        if name == "M900":
            risky.append("M900 (cambia el Linear Advance del firmware hasta reiniciar)")
        if name == "G92":
            if "X" in w:
                off_x = (x + off_x) - w["X"]; x = w["X"]
            if "Y" in w:
                off_y = (y + off_y) - w["Y"]; y = w["Y"]
            if "Z" in w:
                z = w["Z"]
            if "X" in w or "Y" in w:
                risky.append("G92 con X/Y (desplaza el origen: la pieza queda en otro sitio de la cama)")
            continue
        if name not in ("G0", "G1"):
            continue
        if "F" in w:
            feed = w["F"] / 60.0
        nx = (w["X"] if absxyz else x + w["X"]) if "X" in w else x
        ny = (w["Y"] if absxyz else y + w["Y"]) if "Y" in w else y
        nz = (w["Z"] if absxyz else z + w["Z"]) if "Z" in w else z
        de = (w["E"] if not abse else w["E"] - e) if "E" in w else 0.0
        if "E" in w and abse:
            e = w["E"]
        d = math.sqrt((nx - x) ** 2 + (ny - y) ** 2 + (nz - z) ** 2)
        px, py = nx + off_x, ny + off_y
        extruding = name == "G1" and de > 0 and d > 1e-9
        for i, v in enumerate((px, py, nz)):
            phys[2 * i] = min(phys[2 * i], v); phys[2 * i + 1] = max(phys[2 * i + 1], v)
            if extruding:
                ext[2 * i] = min(ext[2 * i], v); ext[2 * i + 1] = max(ext[2 * i + 1], v)
        if d > 1e-9 and feed > 0:
            vx, vy, vz = feed * abs(nx - x) / d, feed * abs(ny - y) / d, feed * abs(nz - z) / d
            vz_max, vx_max, vy_max = max(vz_max, vz), max(vx_max, vx), max(vy_max, vy)
            if vz > m.max_feedrate[2] + 1e-9 or vx > m.max_feedrate[0] + 1e-9 or vy > m.max_feedrate[1] + 1e-9:
                n_over_travel += 1
        if name == "G1" and de > 0 and d > 1e-9:
            horiz = math.hypot(nx - x, ny - y)
            if horiz > 1e-9:
                worst_slope = max(worst_slope, math.degrees(math.atan(abs(nz - z) / horiz)))
            segs.append(Seg("np", 0, x + off_x, y + off_y, z, px, py, nz, t=cfg.nozzle_d, feed=feed))
        x, y, z = nx, ny, nz
    if not segs:
        F.append(Finding("ERROR", "EXT_VACIO", "No hay movimientos con extrusion."))
        return ExternalReport(F, {})
    if phys[0] < 0 or phys[1] > m.bed_x or phys[2] < 0 or phys[3] > m.bed_y or phys[4] < -1e-9 or phys[5] > m.z_max:
        F.append(Finding("ERROR", "EXT_LIMITES", "Hay movimientos fuera de la maquina (X %.1f..%.1f, Y %.1f..%.1f, Z %.1f..%.1f; cama %gx%g, Z max %g)."
                         % (phys[0], phys[1], phys[2], phys[3], phys[4], phys[5], m.bed_x, m.bed_y, m.z_max)))
    if n_over_travel:
        F.append(Finding("WARN", "EXT_VEL", "%d movimientos piden mas velocidad por eje que el firmware (max Z %.1f mm/s): Marlin los limitara, no se ejecutan como estan escritos."
                         % (n_over_travel, m.max_feedrate[2])))
    pz = max((s.feed * abs(s.z1 - s.z0) / s.length for s in segs if s.length), default=0.0)
    if pz > 0.9 * m.max_feedrate[2]:
        F.append(Finding("WARN", "EXT_VZ", "La extrusion pide hasta %.2f mm/s en Z (firmware: %.1f): sin margen." % (pz, m.max_feedrate[2])))
    lim = cfg.eff_max_slope_deg
    if worst_slope > lim:
        F.append(Finding("ERROR", "EXT_PENDIENTE", "Pendiente maxima %.1f deg > limite efectivo %.1f deg (min(max_slope %.1f, cono %.1f - margen %.1f))."
                         % (worst_slope, lim, cfg.max_slope_deg, cfg.head.nozzle_clearance_deg, cfg.head.safety_margin_deg)))
    col: List[Finding] = []
    detect_collisions(segs, cfg, col)
    F += [f for f in col if f.level != "INFO"]
    for r in sorted(set(risky)):
        F.append(Finding("WARN", "EXT_ORDEN", "Orden delicada para tu impresora: %s" % r))
    other = {k: v for k, v in foreign.items() if k not in ("G91", "M900", "M83")}
    if other:
        F.append(Finding("INFO", "EXT_OTRAS", "Ordenes fuera de la lista del motor: " + ", ".join("%s x%d" % kv for kv in sorted(other.items()))))
    F.append(Finding("INFO", "EXT_MODELO", "Colisiones comprobadas con el MODELO del cabezal (%s)." % ("confirmado por el usuario" if cfg.head.confirmed else "SIN CONFIRMAR")))
    stats = {"movimientos_extrusion": float(len(segs)), "x_min": ext[0], "x_max": ext[1], "y_min": ext[2], "y_max": ext[3], "z_min": ext[4], "z_max": ext[5],
             "pendiente_max_deg": worst_slope, "vz_extrusion_mm_s": pz, "vx_max": vx_max, "vy_max": vy_max}
    return ExternalReport(F, stats)
