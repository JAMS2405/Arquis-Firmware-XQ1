# SPDX-License-Identifier: LGPL-3.0-or-later
"""ARQUIS Non-Planar: LAMINA EN ESPIRAL (la tecnica de la flor). EXPERIMENTAL, desactivado por defecto, sin pruebas fisicas.

Imprime la CARA SUPERIOR de una superficie (de un STL o de una funcion) como una lamina de una sola capa que crece desde un punto de
arranque hacia afuera en espiral: cada vuelta se apoya en la anterior, asi que nada se imprime en el aire. NO imprime el volumen del modelo.

Que formas admite: las que se pueden recorrer como una espiral desde un punto (el punto de arranque debe estar dentro y desde el cada
direccion debe cruzar el material una sola vez: forma "estrellada", sin agujeros entre el centro y el borde). Un anillo con agujero NO
es de ese tipo y se rechaza con un mensaje; la estrategia para anillos esta pendiente.

Comprueba (y rechaza si falla): arranque sobre la cama, pendiente radial (la vuelta nueva debe poder apoyarse en la anterior), espaciado
entre vueltas, velocidad y aceleracion de Z, colisiones con el cabezal, limites de cama. Genera G-code con el mismo envoltorio y las mismas
barreras de exportacion que el adaptador (vista previa con codigo, cabezal confirmado, temperaturas y ventilador explicitos).
"""
import hashlib
import json
import math
from dataclasses import asdict, dataclass, replace
from typing import Callable, List, Optional, Tuple

from . import nonplanar_engine as npe
from .nonplanar_surface import Surface


class SheetRefused(npe.AdaptRefused):
    """La superficie no se puede imprimir como lamina en espiral con seguridad."""


@dataclass
class SheetField:
    z: Callable[[float, float], float]            # altura de la superficie en (x, y) (puede devolver cualquier valor fuera de la mascara)
    inside: Callable[[float, float], bool]        # hay material en (x, y)
    center: Tuple[float, float]                   # punto de arranque (dentro del material)
    reach: float                                  # radio maximo que se explora desde el centro (mm)
    name: str = "superficie"


def flower_field(r0: float = 13.0, dr: float = 2.0, petals: int = 5, height: float = 1.0, bump: float = 0.4) -> SheetField:
    """Flor parametrica de prueba: borde R(t) = r0 + dr cos(petalos t) y altura z = height (r/R)^2 (1 + bump cos(petalos t))."""
    R = lambda t: r0 + dr * math.cos(petals * t)
    def z(x, y):
        t = math.atan2(y, x)
        rho = min(math.hypot(x, y) / R(t), 1.0)
        return height * rho * rho * (1 + bump * math.cos(petals * t))
    return SheetField(z, lambda x, y: math.hypot(x, y) <= R(math.atan2(y, x)), (0.0, 0.0), r0 + dr + 1.0, "flor parametrica")


def field_from_surface(s: Surface, center: Optional[Tuple[float, float]] = None, name: str = "STL") -> SheetField:
    """Campo a partir de un STL ya proyectado (cara superior). Por defecto el arranque es el punto mas BAJO de la superficie."""
    def cell(x, y):
        return int(round((x - s.x0) / s.cell)), int(round((y - s.y0) / s.cell))

    def inside(x, y):
        i, j = cell(x, y)
        return 0 <= i < s.nx and 0 <= j < s.ny and s.top[j][i] is not None

    def z(x, y):                                   # interpolacion bilineal con las celdas que tengan material
        fx, fy = (x - s.x0) / s.cell, (y - s.y0) / s.cell
        i0, j0 = int(math.floor(fx)), int(math.floor(fy))
        tx, ty = fx - i0, fy - j0
        acc = wsum = 0.0
        for di, dj, w in ((0, 0, (1 - tx) * (1 - ty)), (1, 0, tx * (1 - ty)), (0, 1, (1 - tx) * ty), (1, 1, tx * ty)):
            i, j = i0 + di, j0 + dj
            if 0 <= i < s.nx and 0 <= j < s.ny and s.top[j][i] is not None and w > 0:
                acc += w * s.top[j][i]; wsum += w
        if wsum <= 0:
            i, j = cell(x, y)
            i, j = min(max(i, 0), s.nx - 1), min(max(j, 0), s.ny - 1)
            return s.top[j][i] if s.top[j][i] is not None else 0.0
        return acc / wsum

    cov = s.covered()
    if not cov:
        raise SheetRefused("El modelo no tiene superficie superior (celdas con material).")
    if center is None:
        i, j = min(cov, key=lambda c: s.top[c[1]][c[0]])
        center = (s.x0 + i * s.cell, s.y0 + j * s.cell)
    reach = math.hypot(s.nx * s.cell, s.ny * s.cell)
    return SheetField(z, inside, center, reach, name)


def radial_profile(f: SheetField, step: float = 0.25, n_theta: int = 720, gap_tol: float = 1.0) -> List[float]:
    """R(theta): distancia del centro al borde en cada direccion. Rechaza si el centro esta fuera o si algun rayo vuelve a entrar en el
    material despues de un hueco MAYOR que gap_tol (agujero o forma no estrellada). Los huecos de hasta gap_tol mm son ruido del borde
    (un STL rasterizado tiene el borde dentado) y se ignoran."""
    cx, cy = f.center
    if not f.inside(cx, cy):
        raise SheetRefused("El punto de arranque %s no esta dentro del material." % (tuple(round(c, 2) for c in f.center),))
    out = []
    for k in range(n_theta):
        t = 2 * math.pi * k / n_theta
        c, s_ = math.cos(t), math.sin(t)
        r, last_in = 0.0, 0.0
        while r <= f.reach:
            if f.inside(cx + r * c, cy + r * s_):
                if r - last_in > gap_tol + 1e-9:
                    raise SheetRefused("La forma no se puede recorrer como espiral desde el punto de arranque %s: en la direccion %.0f grados el material vuelve a aparecer a %.1f mm "
                                       "despues de un hueco de %.1f mm (agujero o forma no estrellada). Un anillo con agujero necesita otra estrategia (pendiente)."
                                       % (tuple(round(v, 1) for v in f.center), math.degrees(t), r, r - last_in))
                last_in = r
            r += step
        lo, hi = last_in, last_in + step                              # el borde esta entre lo (dentro) y hi: se afina por biseccion
        for _ in range(14):
            mid = (lo + hi) / 2
            if f.inside(cx + mid * c, cy + mid * s_):
                lo = mid
            else:
                hi = mid
        out.append(lo)
    return out


def generate_sheet(f: SheetField, cfg: npe.NPConfig, seg_len: float = 0.5) -> npe.AdaptedPlan:
    if not cfg.enabled:
        raise npe.NotEnabled("Motor desactivado.")
    if cfg.bed_temp is None or cfg.nozzle_temp is None or cfg.fan_speed is None:
        raise SheetRefused("Faltan bed_temp, nozzle_temp o fan_speed: el generador no asume el material.")
    for ln in cfg.custom_start_lines:
        cc = npe.parse_cmd(ln)
        if cc is None or cc.name not in npe.ALLOWED_CMDS or cc.name in ("G0", "G1"):
            raise SheetRefused("custom_start_lines: orden no permitida: %r" % ln)
    R = radial_profile(f)
    nth = len(R)
    def Rf(t):                                                   # interpolacion LINEAL en theta (el valor mas cercano mete escalones y falsea la aceleracion de Z)
        u = (t % (2 * math.pi)) / (2 * math.pi) * nth
        k = int(u)
        fr = u - k
        return R[k % nth] * (1 - fr) + R[(k + 1) % nth] * fr
    Rmax, Rmin = max(R), min(R)
    if Rmax < 3 * cfg.sheet_w:
        raise SheetRefused("La superficie es demasiado pequena (radio maximo %.1f mm) para una lamina de %.2f mm de ancho de linea." % (Rmax, cfg.sheet_w))
    N = int(math.ceil(Rmax / cfg.sheet_w))                     # numero de vueltas
    sp_min = Rmin / N
    F: List[npe.Finding] = []
    if sp_min < 0.6 * cfg.sheet_w:
        F.append(npe.Finding("ERROR", "SHEET_ESPACIADO", "El radio varia demasiado (min %.1f, max %.1f mm): en la zona estrecha las vueltas quedan a %.2f mm, menos del 60 %% del ancho de linea (%.2f): sobreextrusion." % (Rmin, Rmax, sp_min, cfg.sheet_w)))
    cx, cy = f.center
    # ---- puntos crudos de la espiral: r = rho * R(theta), rho = theta / (2 pi N)
    pts = []
    th, tmax = 0.0, 2 * math.pi * N
    while th < tmax:
        rho = th / tmax
        r = rho * Rf(th)
        x, y = cx + r * math.cos(th), cy + r * math.sin(th)
        pts.append((th, r, x, y, f.z(x, y)))
        th += min(0.3, seg_len / max(r, cfg.sheet_w))
    th = tmax
    x, y = cx + Rf(th) * math.cos(th), cy + Rf(th) * math.sin(th)
    pts.append((th, Rf(th), x, y, f.z(x, y)))
    zmin = min(p[4] for p in pts)
    z_first = cfg.sheet_t
    shift = z_first - zmin
    zs = [p[4] + shift for p in pts]
    # ---- arranque sobre la cama
    if zs[0] - z_first > 0.3:
        F.append(npe.Finding("ERROR", "SHEET_ARRANQUE", "El punto de arranque queda %.2f mm sobre la cama (la lamina arrancaria en el aire): elige como arranque la zona mas baja (--center) o rebaja la superficie." % (zs[0] - z_first)))
    # ---- apoyo: la vuelta nueva debe poder apoyarse en la anterior (pendiente radial)
    worst = 0.0
    for (t, r, x, y, _), z in zip(pts, zs):
        rho = t / tmax
        if rho > 1.0 / N:
            ri = (rho - 1.0 / N) * Rf(t)
            zi = f.z(cx + ri * math.cos(t), cy + ri * math.sin(t)) + shift
            sp = Rf(t) / N
            worst = max(worst, math.degrees(math.atan(abs(z - zi) / sp)))
    lim = cfg.eff_max_slope_deg
    if worst > lim:
        F.append(npe.Finding("ERROR", "SHEET_APOYO", "Pendiente entre vueltas %.1f deg > limite %.1f deg: la vuelta nueva no se apoya bien en la anterior y/o el cono rozaria." % (worst, lim)))
    # ---- segmentos, velocidad y extrusion
    segs: List[npe.Seg] = []
    for k in range(1, len(pts)):
        a, b = pts[k - 1], pts[k]
        sp_loc = max(min(Rf(b[0]) / N, cfg.sheet_w), 0.05)
        sg = npe.Seg("np", 1, a[2], a[3], zs[k - 1], b[2], b[3], zs[k], t=cfg.sheet_t)
        sg.e_w = sp_loc                                              # ancho efectivo de esta linea
        segs.append(sg)
    cfg_w = replace(cfg, line_w=cfg.sheet_w)
    sp = npe.adjust_speed(segs, cfg_w)
    fil = math.pi * (cfg.filament_d / 2) ** 2
    for sg in segs:
        sg.e = npe._area(sg.t, sg.e_w) * sg.length * cfg.flow / fil
    # ---- colocar el centro en la cama y escribir G-code
    dx, dy = cfg.place_x - cx, cfg.place_y - cy
    body = []
    for sg in segs:
        if sg.length <= 1e-9:
            continue
        body.append("G1 X%s Y%s Z%s E%s F%d" % (npe._fmt(sg.x1 + dx), npe._fmt(sg.y1 + dy), npe._fmt(sg.z1), npe._fmt(sg.e, 6), int(round(sg.feed * 60))))
    path = [(sg.x1 + dx, sg.y1 + dy, sg.z1) for sg in segs]
    start = (segs[0].x0 + dx, segs[0].y0 + dy, segs[0].z0)
    m = cfg.machine
    zspeed = int(0.5 * m.max_feedrate[2] * 60)
    z_end = min(max(p[2] for p in path) + 20.0, m.z_max)
    pre_lines = [npe.MARKER + " - lamina en espiral (%s): NO USAR SIN REVISAR LA VISTA PREVIA" % f.name, "; codigo de vista previa: @@CODIGO@@",
                 "; EXPERIMENTAL: sin pruebas fisicas. Una mano en el paro de emergencia. Imprime la cara superior como una lamina de una capa, no el volumen.",
                 "M140 S%g" % cfg.bed_temp, "M104 S%g" % min(150.0, cfg.nozzle_temp), "M190 S%g" % cfg.bed_temp, "M109 S%g" % min(150.0, cfg.nozzle_temp), "G28"] + list(cfg.custom_start_lines) + \
                ["G90", "M83", "G92 E0", "M220 S100", "M221 S100", "M106 S%d" % cfg.fan_speed, "M109 S%g" % cfg.nozzle_temp,
                 "G1 Z5 F%d" % zspeed, "G1 X%s Y%s F%d" % (npe._fmt(start[0]), npe._fmt(start[1]), cfg.speed_travel * 60), "G1 Z%s F%d" % (npe._fmt(start[2]), zspeed)]
    post_lines = ["G1 E-%g F2400" % cfg.retract_mm, "G1 Z%s F%d" % (npe._fmt(z_end), zspeed), "M104 S0", "M140 S0", "M107", "M84 X Y E", ";End of spiral sheet"]
    full = "\n".join(pre_lines + body + post_lines) + "\n"
    F += npe.verify_gcode(full, cfg)
    rep = npe.analyze_external_gcode(full, cfg)
    F += rep.findings
    F.append(npe.Finding("INFO", "SHEET_GEOM", "Lamina de %d vueltas, radio %.1f..%.1f mm, espaciado %.2f..%.2f mm, ancho de linea %.2f mm, espesor %.2f mm. Imprime la cara SUPERIOR del modelo, no su volumen." % (N, Rmin, Rmax, sp_min, Rmax / N, cfg.sheet_w, cfg.sheet_t)))
    if m.leveling_bilinear:
        F.append(npe.Finding("WARN", "NIVELACION", "El archivo NO activa tu malla de nivelacion tras el G28 y la primera vuelta va a Z %.2f mm. Si usas malla guardada, anade tu orden (p. ej. M420 S1) en custom_start_lines." % start[2]))
    F.append(npe.Finding("INFO", "SHEET_CEBADO", "No se hace purga: la primera vuelta arranca en el centro. Pon tu purga habitual en custom_start_lines."))
    stats = dict(rep.stats)
    stats.update({"vueltas": float(N), "pendiente_entre_vueltas_deg": worst, "radio_min": Rmin, "radio_max": Rmax, "e_total_mm": sum(sg.e for sg in segs), "movimientos": float(len(body))})
    ap = npe.AdaptedPlan(cfg, f.name, full, F, stats, path)
    ap.digest = hashlib.sha256((full.replace("@@CODIGO@@", "") + json.dumps(asdict(cfg), sort_keys=True, default=list)).encode()).hexdigest()[:12].upper()
    ap.text = full.replace("@@CODIGO@@", ap.digest)
    return ap
