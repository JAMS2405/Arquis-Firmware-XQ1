# SPDX-License-Identifier: LGPL-3.0-or-later
"""ARQUIS Non-Planar: ONDULADO de un G-code normal (capas no planas con VOLUMEN). EXPERIMENTAL, desactivado por defecto, sin pruebas fisicas.

Idea (tecnica publicada por Moritz Walter, repositorio makertum/non-planar-layer-fdm, licencia AGPL-3.0): tomar el G-code de una pieza
normal, trocear cada movimiento en segmentos cortos y sumarle a Z una onda que crece con la altura, compensando la extrusion segun el
espesor local de cada capa. Las capas quedan onduladas y entrelazadas: la traccion entre capas (lo debil en FDM) pasa a ser esfuerzo
cortante. ESTA IMPLEMENTACION ES PROPIA (no se ha copiado su codigo): misma idea, formulas escritas aqui, funcion de onda de media cero.

Desplazamiento:  z' = z + rampa(z) * f(x, y)
  f  = funcion de onda de media cero y amplitud pico a pico ``amplitude`` (onda | ondulacion radial | expresion segura propia)
  rampa(z) sube de 0 a 1 entre z_in y z_in + ramp y, si se define z_out, baja de 1 a 0 entre z_out - ramp y z_out (cara superior plana)
Extrusion:       E' = E * (1 + (dz(x,y,z) - dz(x,y,z - h)) / h)   (h = espesor nominal de la capa: la capa se vuelve mas gruesa o mas fina)

Lo que este modulo anade (el script original no lo tiene) y que bloquea la exportacion si falla: pendiente maxima de la trayectoria, espesor
de capa admisible, velocidad y aceleracion de Z del firmware, margen frente al cabezal (bloque, ducto, hexagono), limites de la maquina,
y una verificacion independiente del texto resultante.

Limitaciones: solo G-code de Cura/Marlin con XYZ absoluto dentro de la pieza y sin arcos; no modifica el manifiesto ARQUIS (sus Z quedan
sin deformar: los estados de objeto «pendiente/imprimiendo» pueden adelantarse o retrasarse hasta ~la amplitud); la compatibilidad con la
cancelacion de objetos (M486) NO esta probada; la pieza real queda deformada (hay que pre-deformar el diseno si se quiere recta).
"""
import ast
import hashlib
import json
import math
import os
from dataclasses import asdict, dataclass, field, replace
from typing import Callable, Dict, List, Optional, Tuple

from . import nonplanar_engine as npe
from .gcode_scan_engine import parse_cmd

MARK = ";ARQUIS_NP_WARP"
HARD_MAX_AMPLITUDE_MM = 4.0           # tope duro de la amplitud pico a pico
ZCAP_FRAC = 0.9


class WarpRefused(RuntimeError):
    """El G-code no se puede ondular con seguridad (estructura no soportada o parametros inaceptables)."""


@dataclass
class WarpConfig:
    enabled: bool = False                       # DESACTIVADO POR DEFECTO
    amplitude: float = 1.0                      # pico a pico de f (mm)
    wavelength: float = 20.0                    # mm
    function: str = "wave"                      # "wave" | "radial" | expresion de x, y, r, A, L (ver safe_expr)
    center_x: float = 150.0                     # origen de la onda (por defecto el centro de la cama de 300 x 300)
    center_y: float = 150.0
    z_in: float = 0.8                           # Z donde empieza a deformarse (por encima de la primera capa)
    ramp: float = 5.0                           # mm de transicion
    z_out: Optional[float] = None               # Z donde termina; None = sin rampa de salida (la cara superior tambien ondula)
    flat_top: bool = False                      # True: z_out = Z de la ultima capa (la cara superior queda plana); ignorado si z_out esta definido
    max_seg: float = 1.0                        # mm: longitud maxima de los segmentos
    nozzle_d: float = 0.4
    max_slope_deg: float = 15.0
    head: npe.HeadModel = field(default_factory=npe.HeadModel)
    machine: npe.MachineMotion = field(default_factory=npe.MachineMotion)

    @property
    def eff_max_slope_deg(self) -> float:
        return min(self.max_slope_deg, self.head.nozzle_clearance_deg - self.head.safety_margin_deg)


def warp_config_from_dict(d: dict) -> WarpConfig:
    d = dict(d)
    head = npe.HeadModel(**{**asdict(npe.HeadModel()), **{k: (tuple(tuple(e) for e in v) if k == "envelopes" else v) for k, v in d.pop("head", {}).items()}})
    d.pop("machine", None)
    return WarpConfig(head=head, **d)


# ---------------------------------------------------------------- expresion segura (sustituye al eval de Perl del original)
_ALLOWED_FUNCS = {"sin": math.sin, "cos": math.cos, "abs": abs, "min": min, "max": max, "sqrt": math.sqrt, "tan": math.tan, "exp": math.exp, "atan2": math.atan2}
_ALLOWED_NAMES = ("x", "y", "r", "A", "L", "pi")


def safe_expr(src: str) -> Callable[[float, float, float, float, float], float]:
    """Compila una expresion matematica de x, y, r, A, L, pi con una lista BLANCA de nodos (sin atributos, sin llamadas arbitrarias, sin indices)."""
    try:
        tree = ast.parse(src, mode="eval")
    except SyntaxError as e:
        raise WarpRefused("Expresion no valida: %s" % e)
    for node in ast.walk(tree):
        if isinstance(node, (ast.Expression, ast.BinOp, ast.UnaryOp, ast.Add, ast.Sub, ast.Mult, ast.Div, ast.Pow, ast.Mod, ast.USub, ast.UAdd, ast.Load)):
            continue
        if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)) and not isinstance(node.value, bool):
            continue
        if isinstance(node, ast.Name) and (node.id in _ALLOWED_NAMES or node.id in _ALLOWED_FUNCS):
            continue
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id in _ALLOWED_FUNCS and not node.keywords:
            continue
        raise WarpRefused("Expresion no permitida (solo numeros, x y r A L pi, + - * / ** %% y sin cos tan sqrt exp abs min max atan2): %s" % type(node).__name__)
    code = compile(tree, "<funcion>", "eval")

    def fn(x, y, r, A, L):
        env = {"x": x, "y": y, "r": r, "A": A, "L": L, "pi": math.pi}
        env.update(_ALLOWED_FUNCS)
        return float(eval(code, {"__builtins__": {}}, env))        # noqa: S307  (AST ya validado con lista blanca)
    return fn


def make_f(cfg: WarpConfig) -> Callable[[float, float], float]:
    A, L, cx, cy = cfg.amplitude, cfg.wavelength, cfg.center_x, cfg.center_y
    if cfg.function == "wave":                                     # media cero, pico a pico = A
        k = 2 * math.pi / L
        return lambda x, y: A / 4.0 * (math.sin((x - cx) * k) + math.sin((y - cy) * k))
    if cfg.function == "radial":
        k = 2 * math.pi / L
        return lambda x, y: A / 2.0 * math.sin(math.hypot(x - cx, y - cy) * k)
    fn = safe_expr(cfg.function)
    return lambda x, y: fn(x - cx, y - cy, math.hypot(x - cx, y - cy), A, L)


def ramp_at(cfg: WarpConfig, z: float) -> float:
    a = max(min((z - cfg.z_in) / cfg.ramp, 1.0), 0.0)
    if cfg.z_out is None:
        return a
    return a * (1.0 - max(min((z - cfg.z_out + cfg.ramp) / cfg.ramp, 1.0), 0.0))


# ---------------------------------------------------------------- procesado
@dataclass
class WarpResult:
    cfg: WarpConfig
    text: str
    findings: List[npe.Finding]
    stats: Dict[str, float]
    digest: str = ""
    input_id: str = ""          # huella corta del G-code de ENTRADA (para saber si cambio entre dos exportaciones)
    cfg_id: str = ""            # huella corta de los AJUSTES
    stable_code: str = ""       # codigo ESTABLE (ajustes + n.o de capas + altura): no cambia por diferencias minimas entre dos laminados de la misma pieza
    sig: Dict[str, float] = field(default_factory=dict)   # firma de la pieza para comparar con tolerancia (caja XY, capas, altura, extrusion)

    @property
    def ok(self) -> bool:
        return not any(f.level == "ERROR" for f in self.findings)


def _fmt(v: float, nd: int = 4) -> str:
    return ("%.*f" % (nd, v)).rstrip("0").rstrip(".") or "0"


def _zone(lines: List[str]) -> Tuple[int, int]:
    first = next((i for i, ln in enumerate(lines) if ln.startswith(";LAYER:")), None)
    last = next((i for i in range(len(lines) - 1, -1, -1) if lines[i].startswith(";TIME_ELAPSED:")), None)
    if first is None or last is None or last <= first:
        raise WarpRefused("No encuentro ';LAYER:' y ';TIME_ELAPSED:' de Cura: este ondulador solo entiende G-code de Cura/Marlin con esa estructura.")
    return first, last


def _z_top(lines: List[str], z0i: int, z1i: int) -> float:
    """Mayor Z a la que hay una EXTRUSION con movimiento XY dentro de la pieza (los Z-hops de viaje no cuentan)."""
    z, top = 0.0, 0.0
    for ln in lines[z0i:z1i + 1]:
        c = parse_cmd(ln)
        if c is None or c.name not in ("G0", "G1"):
            continue
        if "Z" in c.words:
            z = c.words["Z"]
        if c.name == "G1" and c.words.get("E", 0.0) != 0.0 and ("X" in c.words or "Y" in c.words):
            top = max(top, z)
    return top


def warp_gcode(text: str, cfg: WarpConfig, stable: bool = False) -> WarpResult:
    if not cfg.enabled:
        raise npe.NotEnabled("El ondulador esta desactivado por defecto: activa cfg.enabled=True expresamente (experimental, sin pruebas fisicas).")
    if not (0 < cfg.amplitude <= HARD_MAX_AMPLITUDE_MM) or cfg.wavelength <= 0 or cfg.ramp <= 0 or cfg.max_seg <= 0:
        raise WarpRefused("Parametros fuera de rango (amplitud 0..%.1f mm, longitud de onda, rampa y segmento > 0)." % HARD_MAX_AMPLITUDE_MM)
    f = make_f(cfg)
    lines = text.splitlines()
    z0i, z1i = _zone(lines)
    if cfg.flat_top and cfg.z_out is None:
        cfg = replace(cfg, z_out=_z_top(lines, z0i, z1i))
    m = cfg.machine
    zcap = ZCAP_FRAC * m.max_feedrate[2]
    out: List[str] = []
    # estado del G-code de ENTRADA
    x = y = z = 0.0
    e_in = e_out = 0.0
    abs_xyz, abs_e = True, True
    feed = 0.0                      # F de entrada (mm/min)
    emitted_f: Optional[float] = None
    lifted = False
    layer_z: List[float] = []       # Z nominales de las capas con extrusion
    h_cur = 0.2
    # estadisticas
    st = {"mov_entrada": 0, "mov_salida": 0, "e_in": 0.0, "e_out": 0.0, "dz_max": 0.0, "mult_min": 1e9, "mult_max": -1e9, "pend_max": 0.0, "caps_z": 0, "z_min": 1e9, "z_max": -1e9,
          "x_min": 1e9, "x_max": -1e9, "y_min": 1e9, "y_max": -1e9}
    refs: List[str] = []

    for idx, ln in enumerate(lines):
        c = parse_cmd(ln)
        inside = z0i <= idx <= z1i
        if idx > z1i and not lifted and st["mov_salida"] > 0:
            # El final del usuario suele ser RELATIVO (G91 + G1 Z5) y partiria de la posicion ondulada. Antes de CUALQUIER linea posterior a la pieza
            # (aun en modo absoluto) se sube, nunca se baja, por encima de la cresta mas alta.
            zl = min(z + st["dz_max"] + 0.5, m.z_max)
            out.append("G0 Z%s F%s ; ARQUIS: subida segura al terminar la pieza ondulada" % (_fmt(zl), _fmt(zcap * 60.0, 1)))
            emitted_f = zcap * 60.0
            lifted = True
        if c is None:
            out.append(ln)
            continue
        w = c.words
        n = c.name
        if n == "G90":
            abs_xyz = True; out.append(ln); continue
        if n == "G91":
            if inside:
                raise WarpRefused("Linea %d: G91 dentro de la pieza: no se ondula." % (idx + 1))
            abs_xyz = False; out.append(ln); continue
        if n == "M82":
            abs_e = True; out.append(ln); continue
        if n == "M83":
            abs_e = False; out.append(ln); continue
        if n == "G92":
            if inside and any(k in w for k in "XYZ"):
                raise WarpRefused("Linea %d: G92 con X/Y/Z dentro de la pieza: no se ondula." % (idx + 1))
            if "E" in w:
                e_in = e_out = w["E"]
            if "X" in w: x = w["X"]
            if "Y" in w: y = w["Y"]
            if "Z" in w: z = w["Z"]
            out.append(ln); continue
        if n in ("G2", "G3") and inside:
            raise WarpRefused("Linea %d: arco (%s) dentro de la pieza: no se ondula." % (idx + 1, n))
        if n not in ("G0", "G1"):
            out.append(ln); continue
        if "F" in w:
            feed = w["F"]
        nx = (w["X"] if abs_xyz else x + w["X"]) if "X" in w else x
        ny = (w["Y"] if abs_xyz else y + w["Y"]) if "Y" in w else y
        nz = (w["Z"] if abs_xyz else z + w["Z"]) if "Z" in w else z
        if "E" in w:
            de = (w["E"] - e_in) if abs_e else w["E"]
            e_in_new = w["E"] if abs_e else e_in + w["E"]
        else:
            de, e_in_new = 0.0, e_in
        if not inside:
            if emitted_f is not None and abs(emitted_f - feed) > 1e-6:
                out.append("G1 F%s" % _fmt(feed, 1))          # devolver la velocidad modal de la entrada (la ultima del ondulado pudo ser menor)
                emitted_f = feed
            # fuera de la pieza: solo se reescribe E si ya hay desfase acumulado; XYZ intactos
            if "E" in w and abs_e and abs(e_out - e_in) > 1e-12:
                e_out_new = e_out + de
                out.append(_rewrite_e(ln, e_out_new))
                e_out = e_out_new
            else:
                out.append(ln)
                if "E" in w:
                    e_out = e_out + de if abs_e else e_out
            e_in = e_in_new
            x, y, z = nx, ny, nz
            continue
        if not any(k in w for k in "XYZ"):
            # Solo E y/o F (retraccion, cebado): no hay movimiento que ondular. Pasa tal cual, con su velocidad, y solo se ajusta E si hay desfase acumulado.
            if "F" not in w and emitted_f is not None and abs(emitted_f - feed) > 1e-6:
                out.append("G1 F%s" % _fmt(feed, 1))
                emitted_f = feed
            elif "F" in w:
                emitted_f = w["F"]
            if "E" in w and abs_e and abs(e_out - e_in) > 1e-12:
                e_out = e_out + de
                out.append(_rewrite_e(ln, e_out))
            else:
                out.append(ln)
                if "E" in w:
                    e_out = e_out + de if abs_e else e_out
            e_in = e_in_new
            continue
        st["mov_entrada"] += 1
        extruding = n == "G1" and de > 1e-9 and ("X" in w or "Y" in w)
        horiz = math.hypot(nx - x, ny - y)
        if "Z" in w and not ("X" in w or "Y" in w):
            # cambio de capa / Z-hop: un solo segmento con el desplazamiento en el XY actual
            segs = 1
        else:
            segs = max(int(math.ceil(horiz / cfg.max_seg - 1e-9)), 1)
        if extruding and abs(nz - z) < 1e-9:
            if not layer_z or abs(layer_z[-1] - nz) > 1e-6:
                layer_z.append(nz)
                h_cur = nz - (layer_z[-2] if len(layer_z) > 1 else 0.0)
                if h_cur <= 1e-6:
                    h_cur = 0.2
        px, py, pz_disp = round(x, 4), round(y, 4), round(z + ramp_at(cfg, z) * f(x, y), 4)
        for k in range(1, segs + 1):
            sx, sy, sz = x + (nx - x) * k / segs, y + (ny - y) * k / segs, z + (nz - z) * k / segs
            r_now = ramp_at(cfg, sz)
            dz = r_now * f(sx, sy)
            zz = sz + dz
            mult = 1.0
            if extruding:
                dz_prev = ramp_at(cfg, sz - h_cur) * f(sx, sy)
                mult = 1.0 + (dz - dz_prev) / h_cur
                st["mult_min"] = min(st["mult_min"], mult); st["mult_max"] = max(st["mult_max"], mult)
            seg_de = de / segs * mult
            sx, sy, zz = round(sx, 4), round(sy, 4), round(zz, 4)        # los limites se calculan sobre lo que REALMENTE se escribe (4 decimales)
            # velocidad: la pedida, el tope de Z del firmware (90 %) y los de X/Y
            fs = feed
            L3 = math.sqrt((sx - px) ** 2 + (sy - py) ** 2 + (zz - pz_disp) ** 2)
            if L3 > 1e-9:
                if abs(zz - pz_disp) > 1e-9:
                    cap = zcap * 60.0 * L3 / abs(zz - pz_disp)
                    if cap < fs - 1e-6 or fs <= 0:
                        fs = cap if fs <= 0 else min(fs, cap); st["caps_z"] += 1
                horiz_s = math.hypot(sx - px, sy - py)
                if horiz_s > 1e-9:
                    fs = min(fs, m.max_feedrate[0] * 60.0 * L3 / horiz_s)
                if extruding and horiz_s > 1e-9:
                    st["pend_max"] = max(st["pend_max"], math.degrees(math.atan(abs(zz - pz_disp) / horiz_s)))
            words = []
            if "X" in w or "Y" in w:
                words += ["X" + _fmt(sx), "Y" + _fmt(sy)] if ("X" in w and "Y" in w) else (["X" + _fmt(sx)] if "X" in w else ["Y" + _fmt(sy)])
            words.append("Z" + _fmt(zz))
            if "E" in w:
                if abs_e:
                    e_out = e_out + seg_de
                    words.append("E" + _fmt(e_out, 5))
                else:
                    words.append("E" + _fmt(seg_de, 5))
                    e_out = e_out + seg_de
                if seg_de > 0 and extruding:
                    st["e_out"] += seg_de
            if fs > 0 and (emitted_f is None or abs(fs - emitted_f) > 1e-6):
                words.append("F" + _fmt(fs, 1)); emitted_f = fs
            out.append("%s %s" % (n, " ".join(words)))
            st["mov_salida"] += 1
            st["dz_max"] = max(st["dz_max"], abs(dz))
            st["z_min"], st["z_max"] = min(st["z_min"], zz), max(st["z_max"], zz)
            if extruding:
                st["x_min"], st["x_max"] = min(st["x_min"], sx), max(st["x_max"], sx)
                st["y_min"], st["y_max"] = min(st["y_min"], sy), max(st["y_max"], sy)
            px, py, pz_disp = sx, sy, zz
        if extruding:
            st["e_in"] += de
        e_in = e_in_new
        x, y, z = nx, ny, nz

    F = _analyze(cfg, f, st, layer_z)
    full = "\n".join(out) + "\n"
    F += verify_warped(text, full, cfg)
    stats = {k: (float(v) if not isinstance(v, str) else v) for k, v in st.items()}
    stats["capas"] = float(len(layer_z))
    res = WarpResult(cfg, full, F, stats)
    res.input_id = hashlib.sha256(text.encode()).hexdigest()[:8].upper()
    res.cfg_id = hashlib.sha256(json.dumps(asdict(cfg), sort_keys=True, default=list).encode()).hexdigest()[:8].upper()
    # El codigo une ENTRADA + AJUSTES + el TEXTO EXACTO que se exportara: si cambia el algoritmo, cambia el codigo (un codigo aceptado no vale para otro archivo).
    exact = hashlib.sha256((hashlib.sha256(text.encode()).hexdigest() + json.dumps(asdict(cfg), sort_keys=True, default=list) + hashlib.sha256(full.encode()).hexdigest()).encode()).hexdigest()[:12].upper()
    res.sig = {"capas": float(len(layer_z)), "z_top": round(max(layer_z), 2), "x_min": st["x_min"], "x_max": st["x_max"], "y_min": st["y_min"], "y_max": st["y_max"], "e_in": st["e_in"]}
    res.stable_code = hashlib.sha256((json.dumps(asdict(cfg), sort_keys=True, default=list) + "|%d|%.2f" % (len(layer_z), max(layer_z))).encode()).hexdigest()[:12].upper()
    res.digest = res.stable_code if stable else exact
    banner = "%s A=%g L=%g IN=%g RAMP=%g CODE=%s" % (MARK, cfg.amplitude, cfg.wavelength, cfg.z_in, cfg.ramp, res.digest)
    # el banner va DESPUES de las lineas ;ARQUIS iniciales (el firmware busca su marca en los primeros 512 B)
    ls = full.split("\n")
    k = 0
    while k < len(ls) and ls[k].startswith(";ARQUIS"):
        k += 1
    res.text = "\n".join(ls[:k] + [banner] + ls[k:])
    return res


def _rewrite_e(line: str, e_new: float) -> str:
    c = parse_cmd(line)
    code = line.split(";")[0]
    import re
    return re.sub(r"E[-+]?\d*\.?\d+", "E" + _fmt(e_new, 5), code, count=1).rstrip() + ((" ;" + line.split(";", 1)[1]) if ";" in line else "")


# ---------------------------------------------------------------- analisis de seguridad
def _footprint_grid(st: Dict, step: float) -> List[Tuple[float, float]]:
    if st["x_min"] > st["x_max"]:
        return []
    xs = [st["x_min"] + i * step for i in range(int((st["x_max"] - st["x_min"]) / step) + 2)]
    ys = [st["y_min"] + j * step for j in range(int((st["y_max"] - st["y_min"]) / step) + 2)]
    return [(a, b) for a in xs for b in ys]


def _analyze(cfg: WarpConfig, f, st: Dict, layer_z: List[float]) -> List[npe.Finding]:
    F: List[npe.Finding] = []
    m = cfg.machine
    if st["mov_entrada"] == 0 or not layer_z:
        F.append(npe.Finding("ERROR", "WARP_VACIO", "No hay movimientos de extrusion dentro de la pieza."))
        return F
    if cfg.amplitude > npe.HARD_MAX_Z_DEV_MM * 2:
        F.append(npe.Finding("WARN", "WARP_AMP", "Amplitud %.2f mm por encima de %.1f mm." % (cfg.amplitude, npe.HARD_MAX_Z_DEV_MM * 2)))
    pts = _footprint_grid(st, 2.0)
    fvals = [f(a, b) for a, b in pts]
    fmin, fmax = min(fvals), max(fvals)
    # pendiente maxima de la superficie (gradiente de f por diferencias finitas) x rampa maxima
    rmax = max(ramp_at(cfg, z) for z in layer_z + [layer_z[-1] + 1.0])
    gmax = 0.0
    for a, b in pts:
        gx = (f(a + 0.25, b) - f(a - 0.25, b)) / 0.5
        gy = (f(a, b + 0.25) - f(a, b - 0.25)) / 0.5
        gmax = max(gmax, math.hypot(gx, gy))
    slope = math.degrees(math.atan(rmax * gmax))
    st["pend_superficie_deg"] = slope
    lim = cfg.eff_max_slope_deg
    if slope > lim:
        F.append(npe.Finding("ERROR", "WARP_PENDIENTE", "Pendiente de la superficie ondulada %.1f deg > limite %.1f deg (min(max_slope %.1f, cono %.1f - margen %.1f))." % (slope, lim, cfg.max_slope_deg, cfg.head.nozzle_clearance_deg, cfg.head.safety_margin_deg)))
    if cfg.z_out is not None and cfg.z_out < cfg.z_in + 2 * cfg.ramp - 1e-9:
        F.append(npe.Finding("WARN", "WARP_RAMPAS", "La rampa de entrada (hasta Z %.1f) y la de salida (desde Z %.1f) se solapan: la onda no llega a su amplitud completa." % (cfg.z_in + cfg.ramp, cfg.z_out - cfg.ramp)))
    # espesor de capa: t = h + (rampa(z) - rampa(z-h)) f
    t_lo, t_hi = max(0.05, 0.4 * min(0.2, cfg.nozzle_d)), 0.75 * cfg.nozzle_d
    tmin, tmax = 1e9, -1e9
    prev = 0.0
    for z in layer_z:
        h = z - prev
        prev = z
        if h <= 1e-6:
            continue
        dr = ramp_at(cfg, z) - ramp_at(cfg, z - h)
        for fv in (fmin, fmax):
            t = h + dr * fv
            tmin, tmax = min(tmin, t), max(tmax, t)
    st["espesor_min"], st["espesor_max"] = tmin, tmax
    if tmin < t_lo - 1e-9 or tmax > t_hi + 1e-9:
        F.append(npe.Finding("ERROR", "WARP_ESPESOR", "Espesor de capa resultante %.3f..%.3f mm fuera de %.3f..%.3f mm: sube la rampa (mas lenta), baja la amplitud o aleja z_in." % (tmin, tmax, t_lo, t_hi)))
    # Z: nunca por debajo de la primera capa ni por encima de la maquina
    zlow = min(z + ramp_at(cfg, z) * fmin for z in layer_z)
    if zlow < min(layer_z) * 0.5:
        F.append(npe.Finding("ERROR", "WARP_Z_BAJO", "Una capa quedaria demasiado cerca de la cama (Z %.2f mm)." % zlow))
    if st["z_max"] > m.z_max:
        F.append(npe.Finding("ERROR", "WARP_Z_ALTO", "Z maxima %.2f mm supera la maquina (%.0f mm)." % (st["z_max"], m.z_max)))
    if st["x_min"] < 0 or st["x_max"] > m.bed_x or st["y_min"] < 0 or st["y_max"] > m.bed_y:
        F.append(npe.Finding("ERROR", "WARP_CAMA", "La pieza se sale de la cama (X %.1f..%.1f, Y %.1f..%.1f)." % (st["x_min"], st["x_max"], st["y_min"], st["y_max"])))
    # cabezal: la capa nueva puede quedar mas alta que el material cercano en rampa x (max f en el disco - f en el punto)
    margin = 0.0
    for name, rad, hgt in cfg.head.envelopes:
        step = max(2.0, rad / 5.0)
        worst = 0.0
        for a, b in _footprint_grid(st, max(step, 3.0)):
            fa = f(a, b)
            n_s = max(6, int(2 * math.pi * rad / step))
            for ring in (rad / 2.0, rad):
                for q in range(n_s):
                    t = 2 * math.pi * q / n_s
                    worst = max(worst, f(a + ring * math.cos(t), b + ring * math.sin(t)) - fa)
        need = rmax * worst
        margin = max(margin, need)
        if need > hgt:
            F.append(npe.Finding("ERROR", "WARP_CABEZAL", "%s: el material cercano puede quedar %.2f mm mas alto que la punta, mas que su holgura (%.2f mm)." % (name, need, hgt)))
    st["holgura_cabezal_usada"] = margin
    # aceleracion de Z: la mayor pendiente por la velocidad maxima de Z
    F.append(npe.Finding("INFO", "WARP_ACEL_Z", "La velocidad se reduce donde Z lo exige (%d segmentos limitados al 90 %% del maximo de Z: %.1f mm/s). La aceleracion de Z (maximo %.0f mm/s2) se comprueba solo en el texto, sin simular el planificador." % (st["caps_z"], ZCAP_FRAC * m.max_feedrate[2], m.max_accel[2])))
    F.append(npe.Finding("INFO", "WARP_MODELO", "Choque con el cabezal calculado con el MODELO (%s)." % ("confirmado por el usuario" if cfg.head.confirmed else "SIN CONFIRMAR")))
    F.append(npe.Finding("WARN", "WARP_MANIFIESTO", "No se deforma el manifiesto ARQUIS: los estados de objeto pueden adelantarse o retrasarse hasta ~%.1f mm de Z. La cancelacion de objetos (M486) NO esta probada con este archivo." % (cfg.amplitude / 2.0)))
    if m.leveling_bilinear:
        F.append(npe.Finding("INFO", "WARP_NIVELACION", "Se conserva el inicio de tu G-code (tu nivelacion); no se toca nada por debajo de z_in = %.2f mm." % cfg.z_in))
    return F


def verify_warped(src: str, out: str, cfg: WarpConfig) -> List[npe.Finding]:
    """Lee el TEXTO resultante (no el modelo interno): limites, velocidades por eje, E, ordenes nuevas y conservacion del volumen."""
    m = cfg.machine
    F: List[npe.Finding] = []
    x = y = z = 0.0
    e = 0.0
    abs_xyz, abs_e = True, True
    feed = 0.0
    e_pos = 0.0
    bad_feed = 0
    out_of = 0
    seg_over_z = 0
    n_moves = 0
    lines = out.splitlines()
    z0i, z1i = _zone(lines)
    for i, ln in enumerate(lines):
        c = parse_cmd(ln)
        if c is None:
            continue
        w = c.words
        if c.name == "G90": abs_xyz = True; continue
        if c.name == "G91": abs_xyz = False; continue
        if c.name == "M82": abs_e = True; continue
        if c.name == "M83": abs_e = False; continue
        if c.name == "G92":
            e = w.get("E", e); x, y, z = w.get("X", x), w.get("Y", y), w.get("Z", z); continue
        if c.name not in ("G0", "G1"):
            continue
        if "F" in w:
            feed = w["F"] / 60.0
        nx = (w["X"] if abs_xyz else x + w["X"]) if "X" in w else x
        ny = (w["Y"] if abs_xyz else y + w["Y"]) if "Y" in w else y
        nz = (w["Z"] if abs_xyz else z + w["Z"]) if "Z" in w else z
        if z0i <= i <= z1i:
            n_moves += 1
            if not (-1e-6 <= nx <= m.bed_x + 1e-6 and -1e-6 <= ny <= m.bed_y + 1e-6 and -1e-6 <= nz <= m.z_max):
                out_of += 1
            d = math.sqrt((nx - x) ** 2 + (ny - y) ** 2 + (nz - z) ** 2)
            if d > 1e-9 and feed > 0:
                if feed * abs(nz - z) / d > m.max_feedrate[2] + 1e-6 or feed * abs(nx - x) / d > m.max_feedrate[0] + 1e-6 or feed * abs(ny - y) / d > m.max_feedrate[1] + 1e-6:
                    bad_feed += 1
        if "E" in w:
            ne = w["E"] if abs_e else e + w["E"]
            if ne > e and (z0i <= i <= z1i):
                e_pos += ne - e
            e = ne
        x, y, z = nx, ny, nz
    if out_of:
        F.append(npe.Finding("ERROR", "WARP_G_LIMITES", "%d movimientos del texto resultante salen de la maquina." % out_of))
    if bad_feed:
        F.append(npe.Finding("ERROR", "WARP_G_VEL", "%d movimientos del texto resultante superan la velocidad por eje del firmware." % bad_feed))
    # conservacion del volumen: la E total extruida debe ser casi la de la entrada
    e_src = _total_e(src)
    if e_src > 0:
        ratio = e_pos / e_src
        if abs(ratio - 1.0) > 0.08:
            F.append(npe.Finding("ERROR" if abs(ratio - 1.0) > 0.2 else "WARN", "WARP_VOLUMEN", "La extrusion total cambia un %.1f %% respecto al original (se espera < 8 %%): revisa la funcion, la amplitud y la rampa." % (100 * (ratio - 1.0))))
    return F


def _total_e(text: str) -> float:
    lines = text.splitlines()
    z0i, z1i = _zone(lines)
    e = 0.0
    tot = 0.0
    abs_e = True
    for i, ln in enumerate(lines):
        c = parse_cmd(ln)
        if c is None:
            continue
        if c.name == "M82": abs_e = True; continue
        if c.name == "M83": abs_e = False; continue
        if c.name == "G92":
            e = c.words.get("E", e); continue
        if c.name in ("G0", "G1") and "E" in c.words:
            ne = c.words["E"] if abs_e else e + c.words["E"]
            if ne > e and z0i <= i <= z1i:
                tot += ne - e
            e = ne
    return tot


def export_warped(res: WarpResult, accept_code: str) -> str:
    cfg = res.cfg
    if not cfg.enabled:
        raise npe.NotEnabled("Ondulador desactivado.")
    if not res.ok:
        raise npe.ExportRefused("Hay hallazgos ERROR: " + "; ".join(sorted({f.code for f in res.findings if f.level == "ERROR"})))
    if not cfg.head.confirmed:
        raise npe.ExportRefused("Las dimensiones del cabezal no estan confirmadas (head.confirmed=False).")
    if (accept_code or "").strip().upper() != res.digest:
        raise npe.ExportRefused("El codigo de vista previa no coincide (%s): genera la vista previa, revisala y devuelve su codigo." % res.digest)
    if not res.text.startswith(";ARQUIS") and MARK not in res.text:
        raise npe.ExportRefused("Falta la marca del ondulador.")
    return res.text


def render_warp_preview(res: WarpResult, text_in: Optional[str] = None) -> str:
    """SVG: mapa de la funcion f(x, y) sobre la huella, perfil Z de varias capas a lo largo de X y resumen/hallazgos."""
    cfg, st = res.cfg, res.stats
    f = make_f(cfg)
    x0, x1, y0, y1 = st["x_min"], st["x_max"], st["y_min"], st["y_max"]
    W, H = 1100, 640
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="sans-serif" font-size="12">' % (W, H), '<rect width="100%" height="100%" fill="#0f1620"/>',
           '<text x="16" y="24" fill="#ffb020" font-size="15" font-weight="bold">ARQUIS Non-Planar EXPERIMENTAL - ondulado (vista previa; NO probado en la impresora)</text>',
           '<text x="16" y="44" fill="#cfd8e3">Codigo de vista previa: %s   funcion=%s  A=%g mm  L=%g mm  z_in=%g  rampa=%g</text>' % (res.digest, npe._esc(cfg.function), cfg.amplitude, cfg.wavelength, cfg.z_in, cfg.ramp)]
    ox, oy = 16, 70
    sc = min(440.0 / max(x1 - x0, 1e-6), 250.0 / max(y1 - y0, 1e-6))
    out.append('<text x="%d" y="%d" fill="#9fb3c8">f(x,y) sobre la huella de la pieza (rojo = sube, azul = baja)</text>' % (ox, oy - 4))
    cell = 2.0
    a = x0
    while a < x1:
        b = y0
        while b < y1:
            v = f(a + cell / 2, b + cell / 2) / (cfg.amplitude / 2.0 + 1e-9)
            v = max(-1.0, min(1.0, v))
            col = "#%02x%02x%02x" % (int(110 + 120 * max(v, 0)), int(110 - 30 * abs(v)), int(110 + 120 * max(-v, 0)))
            out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"/>' % (ox + (a - x0) * sc, oy + (y1 - b - cell) * sc, cell * sc + 0.4, cell * sc + 0.4, col))
            b += cell
        a += cell
    # perfil: DESPLAZAMIENTO de Z (z' - z) a lo largo de X, en 7 alturas repartidas entre z_in y la Z maxima, AMPLIADO para que se vea
    px, py, pw, ph = 560, 90, 430, 250
    zt = st["z_max"]
    yc = (y0 + y1) / 2
    rows = 7
    band = ph / rows
    sc = (band * 0.42) / max(cfg.amplitude / 2.0, 1e-9)
    out.append('<text x="%d" y="%d" fill="#9fb3c8">Desplazamiento de Z (z\' - z) a lo largo de X en %d alturas; 1 mm = %.0f px (ampliado)</text>' % (px, py - 10, rows, sc))
    for k in range(rows):
        zl = cfg.z_in + k * (zt - cfg.z_in) / (rows - 1)
        base = py + band * (k + 0.5)
        out.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#2a3441"/>' % (px, base, px + pw, base))
        pts = " ".join("%.1f,%.1f" % (px + pw * i / 100.0, base - ramp_at(cfg, zl) * f(x0 + (x1 - x0) * i / 100.0, yc) * sc) for i in range(101))
        out.append('<polyline points="%s" fill="none" stroke="#4aa3ff" stroke-width="1.4"/>' % pts)
        out.append('<text x="%d" y="%.1f" fill="#9fb3c8" font-size="10">Z %.1f (rampa %.2f)</text>' % (px + pw + 4, base + 3, zl, ramp_at(cfg, zl)))
    y = 360
    lines = ["pendiente de la superficie %.1f deg (limite efectivo %.1f)   espesor de capa %.3f..%.3f mm   multiplicador de E %.2f..%.2f   segmentos %d -> %d" %
             (st.get("pend_superficie_deg", 0), cfg.eff_max_slope_deg, st.get("espesor_min", 0), st.get("espesor_max", 0), st["mult_min"], st["mult_max"], st["mov_entrada"], st["mov_salida"]),
             "desplazamiento max %.2f mm   Z %.2f..%.2f mm   holgura de cabezal usada %.2f mm   segmentos con la velocidad limitada por Z: %d" % (st["dz_max"], st["z_min"], st["z_max"], st.get("holgura_cabezal_usada", 0), st["caps_z"]),
             "Cabezal: %s" % ("CONFIRMADO por el usuario" if cfg.head.confirmed else "SIN CONFIRMAR: no se podra exportar")]
    for ln in lines:
        out.append('<text x="16" y="%d" fill="#cfd8e3">%s</text>' % (y, npe._esc(ln))); y += 18
    for fd in res.findings:
        col = {"ERROR": "#ff6b6b", "WARN": "#ffb020", "INFO": "#8fb4d9"}[fd.level]
        out.append('<text x="16" y="%d" fill="%s">[%s] %s: %s</text>' % (y, col, fd.level, fd.code, npe._esc(fd.msg[:165]))); y += 16
        if y > H - 8:
            break
    out.append('</svg>')
    return "\n".join(out)


# ---------------------------------------------------------------- uso desde el script de post-procesado de Cura
@dataclass
class CuraOutcome:
    text: str                  # G-code a devolver a Cura (el ORIGINAL si no se aplica el ondulado)
    applied: bool
    level: str                 # "none" | "info" | "warning" | "error"
    message: str
    code: str = ""


SIG_TOL_MM = 1.0        # caja XY de la pieza
SIG_TOL_Z = 0.05        # altura de la ultima capa
SIG_TOL_E = 0.03        # cantidad de filamento (3 %)


def sig_mismatch(saved: Dict[str, float], cur: Dict[str, float]) -> str:
    """'' si la pieza actual es la misma que la de la vista previa guardada (dentro de tolerancia); si no, que cambio. Dos laminados de la misma pieza
    pueden diferir en milesimas, asi que se compara con tolerancia en vez de byte a byte."""
    try:
        if int(saved["capas"]) != int(cur["capas"]):
            return "numero de capas %d -> %d" % (saved["capas"], cur["capas"])
        if abs(saved["z_top"] - cur["z_top"]) > SIG_TOL_Z:
            return "altura %.2f -> %.2f mm" % (saved["z_top"], cur["z_top"])
        for k in ("x_min", "x_max", "y_min", "y_max"):
            if abs(saved[k] - cur[k]) > SIG_TOL_MM:
                return "posicion o tamano (%s %.1f -> %.1f mm)" % (k, saved[k], cur[k])
        if saved["e_in"] > 0 and abs(cur["e_in"] - saved["e_in"]) / saved["e_in"] > SIG_TOL_E:
            return "cantidad de filamento %.0f -> %.0f mm" % (saved["e_in"], cur["e_in"])
    except (KeyError, TypeError, ValueError):
        return "firma de la vista previa ilegible"
    return ""


def _read_state(path: str) -> Optional[dict]:
    try:
        with open(path, encoding="utf-8") as f:
            d = json.load(f)
        return d if isinstance(d, dict) else None
    except (OSError, ValueError):
        return None


def apply_for_cura(text: str, s: dict, head_path: str, out_dir: str) -> CuraOutcome:
    """Reproduce DENTRO de Cura las barreras de la linea de comandos. El ondulado solo se aplica si: el ajuste esta activado (apagado por defecto),
    no hay errores, el cabezal esta confirmado y el usuario ha escrito el CODIGO de la vista previa (que esta funcion escribe en ``out_dir``).
    En cualquier otro caso devuelve el G-code ORIGINAL sin tocar y explica por que."""
    if not s.get("warp_on"):
        return CuraOutcome(text, False, "none", "")
    try:
        with open(head_path, encoding="utf-8") as f:
            head = json.load(f)["head"]
        cfg = warp_config_from_dict({"enabled": True, "amplitude": float(s["amplitude"]), "wavelength": float(s["wavelength"]), "function": str(s["function"]).strip() or "wave",
                                     "z_in": float(s["z_in"]), "ramp": float(s["ramp"]), "flat_top": bool(s["flat_top"]), "head": head})
        res = warp_gcode(text, cfg, stable=True)
    except (WarpRefused, npe.NotEnabled) as e:
        return CuraOutcome(text, False, "error", "Ondulado NO aplicado (el G-code se exporta sin cambios): %s" % e)
    except Exception as e:                                           # nunca romper la exportacion
        return CuraOutcome(text, False, "error", "Ondulado NO aplicado (error interno; el G-code se exporta sin cambios): %s" % e)
    svg_path = os.path.join(out_dir, "arquis_nonplanar_vista.svg")
    state_path = os.path.join(out_dir, "arquis_nonplanar_vista.json")
    try:
        with open(svg_path, "w", encoding="utf-8") as f:
            f.write(render_warp_preview(res))
    except OSError:
        svg_path = "(no se pudo escribir la vista previa)"
    if not res.ok:
        return CuraOutcome(text, False, "error", "Ondulado NO aplicado (el G-code se exporta sin cambios): " + "; ".join("%s: %s" % (f.code, f.msg) for f in res.findings if f.level == "ERROR") +
                           "\nVista previa: %s" % svg_path, res.stable_code)
    if not cfg.head.confirmed:
        return CuraOutcome(text, False, "error", "Ondulado NO aplicado: el cabezal no esta confirmado.", res.stable_code)
    typed = str(s.get("accept_code", "")).strip().upper()
    order = "" if ";ARQUIS_M486" in text else ("  AVISO DE ORDEN: este G-code aun no tiene la marca de ARQUIS M486. Si el script ARQUIS M486 va DESPUES de este, vera extrusiones con X, Y y Z a la vez y "
                                                "avisara de que cancelar un objeto en esos tramos puede afectar a la altura (NO esta probado). Pon 'ARQUIS Ondulado no planar' el ULTIMO de la lista.")
    why = ""
    if typed == res.stable_code:
        saved = _read_state(state_path)
        if saved is None or saved.get("code") != res.stable_code:
            why = "No hay una vista previa guardada con ese codigo (se vuelve a generar)."
        else:
            bad = sig_mismatch(saved.get("sig", {}), res.sig)
            if not bad:
                warns = [f.msg for f in res.findings if f.level == "WARN"]
                return CuraOutcome(res.text, True, "warning" if warns else "info", "Ondulado APLICADO (experimental, sin pruebas fisicas). " + " | ".join(warns) + order + "\nVista previa: %s" % svg_path, res.stable_code)
            why = "La pieza ha cambiado respecto a la vista previa que revisaste (%s): revisa la nueva vista previa." % bad
    try:
        with open(state_path, "w", encoding="utf-8") as f:
            json.dump({"code": res.stable_code, "sig": res.sig}, f)
    except OSError:
        pass
    return CuraOutcome(text, False, "warning", "Ondulado NO aplicado todavia (el G-code se exporta sin cambios). %s1) Abre y revisa la vista previa: %s  2) Si estas de acuerdo, escribe el codigo %s en el ajuste "
                       "'Codigo de vista previa aceptado' y exporta de nuevo. Puedes volver a laminar: el codigo solo cambia si cambian los ajustes del ondulado, el numero de capas o la altura de la pieza; "
                       "si la pieza cambia de posicion, tamano o cantidad de material se te pedira revisar de nuevo.  Pieza: %d capas, altura %.2f mm.%s" %
                       ((why + "  ") if why else "", svg_path, res.stable_code, int(res.sig["capas"]), res.sig["z_top"], order), res.stable_code)
