# SPDX-License-Identifier: LGPL-3.0-or-later
"""Verificador independiente del G-code del arbol AITE: lee SU TEXTO, no el plan interno del generador.

Comprueba (1) ordenes y limites del firmware (cama, Z, velocidades, temperaturas, extrusion), (2) holgura del cabezal contra todo lo ya
impreso en cada instante, con las piezas de ``data/head_arquis_xq1.json``, (3) que la boquilla no arrastre por encima de lo impreso al
desplazarse, (4) cobertura de la seccion central (plano y = centro del arbol): material que falta o sobra frente al arbol pedido y
planitud del piso de cada rama, y (5) el balance de volumen. Solo biblioteca estandar de Python.

El modelo del cabezal es CONSERVADOR: la pieza (eta, rho) prohibe todo material a distancia horizontal <= rho (en cualquier direccion)
cuya altura sobre la punta alcance eta. No sabe nada del apoyo del cordon ni de la fisica del plastico: eso lo decide la prueba.
"""
import math
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from .arbol import MAQUINA_XQ1, Constructor
from .head import HeadProfile, load_head_profile

PERMITIDAS = {"G1", "G4", "G21", "G28", "G90", "G92", "M82", "M104", "M109", "M140", "M190", "M106", "M107", "M204", "M220", "M221", "M84", "M501"}
CELDA = 1.0                     # mm: rejilla de alturas para la holgura del cabezal
PASO_MUESTRA = 2.0              # mm entre muestras de la trayectoria
MARGEN_HOLGURA = 0.5            # mm: por debajo de esto se avisa
RES = 0.025                     # mm: resolucion de la seccion central
AREA_HUECO_GRANDE = 0.04        # mm2: un hueco de la seccion mayor que esto no lo rellena el aplastamiento del cordon


@dataclass
class Informe:
    problemas: List[str] = field(default_factory=list)
    avisos: List[str] = field(default_factory=list)
    datos: Dict[str, float] = field(default_factory=dict)

    @property
    def ok(self) -> bool:
        return not self.problemas

    def texto(self) -> str:
        d = self.datos
        out = ["VERIFICACION DEL G-CODE (lee el texto del archivo)"]
        for clave, fmt in (("lineas", "Lineas de G-code: %d"), ("movimientos", "Movimientos G1: %d"), ("capas_planas", "Capas planas: %d"),
                           ("capas_inclinadas", "Capas inclinadas: %d"), ("extruido_mm", "Filamento extruido: %.1f mm"),
                           ("tiempo_min", "Tiempo estimado: %.0f min (aproximado: trapecios y esquinas del firmware; sin las esperas de temperatura)")):
            if clave in d:
                out.append("  " + fmt % d[clave])
        if "volumen_ratio" in d:
            out.append("  Volumen depositado / arbol pedido (con borde): %.3f (1,00 = exacto)" % d["volumen_ratio"])
        if "vz_max" in d:
            out.append("  Velocidad maxima de Z pedida: %.1f mm/s (maximo del firmware %.0f)" % (d["vz_max"], MAQUINA_XQ1["vmax_z"]))
        if "holgura_min" in d:
            out.append("  Holgura minima del cabezal contra lo impreso: %.2f mm (en x=%.1f y=%.1f z=%.2f, pieza que manda: %s)" % (d["holgura_min"], d["holgura_x"], d["holgura_y"], d["holgura_z"], d.get("holgura_pieza", "?")))
        if "falta_pct" in d:
            out.append("  Seccion central (plano y = centro): huecos entre cordones %.2f %% (normales: el plastico los rellena); huecos GRANDES %.2f %% (mayor %.2f mm2); sobra %.2f %%" % (d["falta_pct"], d["hueco_grande_pct"], d["hueco_mayor_mm2"], d["sobra_pct"]))
        for nombre in sorted(k for k in d if k.startswith("piso_")):
            out.append("  %s" % d[nombre] if isinstance(d[nombre], str) else "  %s: %.3f" % (nombre, d[nombre]))
        for a in self.avisos:
            out.append("  AVISO: " + a)
        if self.problemas:
            out.append("  PROBLEMAS (%d):" % len(self.problemas))
            out.extend("    - " + p for p in self.problemas[:25])
        else:
            out.append("  Sin problemas.")
        return "\n".join(out)


# ---------------------------------------------------------------------------------------------------------------------------
JD = 0.05                       # mm, JUNCTION_DEVIATION_MM del firmware


def estimar_tiempo(segmentos: List[Tuple[float, float, Tuple[float, float, float], float]]) -> float:
    """Segundos para una lista de (longitud, velocidad nominal, direccion unitaria, aceleracion de trayecto): trapecio por segmento con la
    velocidad de union de Marlin (desviacion de union) entre vecinos, sin mirar mas alla del vecino: es una COTA APROXIMADA."""
    n = len(segmentos)
    vj = [0.0] * (n + 1)
    for i in range(1, n):
        L0, v0, d0, a0 = segmentos[i - 1]
        L1, v1, d1, a1 = segmentos[i]
        cos = max(-1.0, min(1.0, d0[0] * d1[0] + d0[1] * d1[1] + d0[2] * d1[2]))
        tope = min(v0, v1)
        if cos > 0.9999:
            vj[i] = tope
        else:
            sh = math.sqrt(max(0.0, (1.0 - cos) / 2.0))
            vj[i] = min(tope, math.sqrt(min(a0, a1) * JD * sh / max(1e-9, 1.0 - sh)))
    total = 0.0
    for i, (L, v, _d, a) in enumerate(segmentos):
        vi, vo = min(vj[i], v), min(vj[i + 1], v)
        # velocidades de salida alcanzables
        vo = min(vo, math.sqrt(vi * vi + 2 * a * L))
        vi = min(vi, math.sqrt(vo * vo + 2 * a * L))
        d_acc = (v * v - vi * vi) / (2 * a)
        d_dec = (v * v - vo * vo) / (2 * a)
        if d_acc + d_dec <= L:
            total += (v - vi) / a + (v - vo) / a + (L - d_acc - d_dec) / v
        else:
            vp = math.sqrt(max(0.0, a * L + (vi * vi + vo * vo) / 2.0))
            total += max(0.0, vp - vi) / a + max(0.0, vp - vo) / a
    return total


def _palabras(linea: str) -> Tuple[str, Dict[str, float]]:
    w = linea.split()
    cmd = w[0].upper()
    val: Dict[str, float] = {}
    for t in w[1:]:
        if t[0].isalpha() and len(t) > 1:
            try:
                val[t[0].upper()] = float(t[1:])
            except ValueError:
                raise ValueError("valor no numerico en %r" % linea)
    return cmd, val


_TAG = re.compile(r"(\w+)=([-+0-9.]+)")


def _etiqueta(comentario: str) -> Optional[dict]:
    c = comentario.strip()
    if not c.startswith("AITE "):
        return None
    kv = {k: float(v) for k, v in _TAG.findall(c)}
    if c.startswith("AITE plano"):
        kv["tipo"] = "plano"
    elif c.startswith("AITE inclinada"):
        kv["tipo"] = "inclinada"
    else:
        return None
    return kv


def holgura_permitida(head: HeadProfile, rho: float) -> float:
    """Elevacion maxima del material sobre la punta a distancia horizontal ``rho``: la menor eta de las piezas con rho_i >= rho."""
    a = math.inf
    for p in head.primitives:
        if p.rho >= rho:
            a = min(a, p.eta - (head.thermal_margin_mm if p.hot else 0.0))
    return a


def _volumen_pedido(c: Constructor) -> float:
    a, p = c.arbol, c.p
    v = a.tronco ** 2 * a.altura
    for r in a.ramas:
        v += r.largo * r.alto * r.ancho
    lado_borde = a.tronco + 2 * p.borde * p.paso_plano
    v += (lado_borde ** 2 - a.tronco ** 2) * (c.t_primera if hasattr(c, "t_primera") else c.t_plano)
    return v


# ---------------------------------------------------------------------------------------------------------------------------
def verificar(texto: str, c: Constructor, head: Optional[HeadProfile] = None, con_seccion: bool = True, depurar: Optional[list] = None) -> Informe:
    """Verifica el G-code ``texto`` generado por el constructor ``c`` (se usan sus parametros y el arbol pedido)."""
    m, p, arbol = c.m, c.p, c.arbol
    head = head or load_head_profile()
    inf = Informe()
    err, d = inf.problemas, inf.datos
    cx, cy = c.cx, c.cy
    dz_seco = p.dz_seco if p.seco else 0.0
    x = y = z = None
    e = 0.0
    homed = caliente = False
    xy_visto = False
    tag: Optional[dict] = None
    altura: Dict[Tuple[int, int], float] = {}
    celdas: List[List[float]] = []                 # [cx, cy, ztop] de las celdas con material
    indice: Dict[Tuple[int, int], int] = {}
    seg_seccion: List[Tuple[Tuple[float, float, float], Tuple[float, float, float], dict]] = []
    e_ext = 0.0
    n_mov = 0
    vz_max = 0.0
    t_est = 0.0
    segs_t: List[Tuple[float, float, Tuple[float, float, float], float]] = []
    acc_p = 1000.0
    hold_min = (math.inf, 0.0, 0.0, 0.0, "")
    ultimas: List[str] = []
    n_lineas = 0
    cap_pl = cap_in = 0
    z_tope = 0.0

    def actualiza_celda(px: float, py: float, zt: float) -> None:
        k = (int(math.floor(px / CELDA)), int(math.floor(py / CELDA)))
        i = indice.get(k)
        if i is None:
            indice[k] = len(celdas)
            celdas.append([(k[0] + 0.5) * CELDA, (k[1] + 0.5) * CELDA, zt])
        elif zt > celdas[i][2]:
            celdas[i][2] = zt

    def holgura_en(px: float, py: float, pz: float) -> Tuple[float, float, float, str]:
        """Menor holgura (mm) entre la elevacion permitida y la real del material, para la boquilla en (px, py, pz)."""
        mejor = (math.inf, 0.0, 0.0, "")
        for cxx, cyy, zt in celdas:
            h = zt - pz
            if h < -2.0:                            # material bastante mas bajo que la boquilla: nunca choca
                continue
            rho = max(0.0, math.hypot(cxx - px, cyy - py) - 0.7072 * CELDA)
            a = holgura_permitida(head, rho)
            if a == math.inf:
                continue
            hol = a - h
            if hol < mejor[0]:
                mejor = (hol, cxx, cyy, "rho=%.1f" % rho)
        return mejor

    for n, raw in enumerate(texto.splitlines(), 1):
        n_lineas += 1
        if ";" in raw:
            cuerpo, com = raw.split(";", 1)
            et = _etiqueta(com)
            if et is not None:
                tag = et
                if et["tipo"] == "plano":
                    cap_pl += 1
                else:
                    cap_in += 1
        else:
            cuerpo = raw
        l = cuerpo.strip()
        if not l:
            continue
        ultimas.append(l)
        try:
            cmd, val = _palabras(l)
        except ValueError as ex:
            err.append("linea %d: %s" % (n, ex))
            continue
        if cmd not in PERMITIDAS:
            err.append("linea %d: orden no permitida %s" % (n, cmd))
            continue
        if cmd == "G28":
            homed = True
        elif cmd in ("M104", "M109", "M140", "M190"):
            s = val.get("S", val.get("R"))
            if s is None:
                err.append("linea %d: %s sin S ni R" % (n, cmd))
                continue
            if p.seco:
                err.append("linea %d: la version en seco no debe calentar (%s)" % (n, l))
            lim = m["cama_max"] if cmd in ("M140", "M190") else m["boquilla_max"]
            if not 0 <= s <= lim:
                err.append("linea %d: temperatura %s fuera de limites (0-%d)" % (n, l, lim))
            if cmd == "M109" and s >= 170:
                caliente = True
        elif cmd == "M106":
            if p.seco:
                err.append("linea %d: la version en seco no debe encender el ventilador" % n)
            if not 0 <= val.get("S", -1) <= 255:
                err.append("linea %d: ventilador fuera de 0-255" % n)
        elif cmd == "M204":
            for k in ("P", "T"):
                if k in val and not 200 <= val[k] <= 20000:
                    err.append("linea %d: aceleracion fuera de 200-20000" % n)
            if "P" in val:
                acc_p = val["P"]
        elif cmd == "G92":
            if "E" in val:
                e = val["E"]
        elif cmd == "G1":
            n_mov += 1
            nx, ny, nz, ne, fv = val.get("X"), val.get("Y"), val.get("Z"), val.get("E"), val.get("F")
            if not homed:
                err.append("linea %d: movimiento antes de G28" % n)
            if (nx is not None or ny is not None) and not xy_visto:
                xy_visto = True
                if (nz if nz is not None else (z if z is not None else 0.0)) < 1.0:
                    err.append("linea %d: primer desplazamiento XY con la boquilla a menos de 1 mm de la cama" % n)
            for nm, vv, lo, hi in (("X", nx, m["margen"], m["cama"][0] - m["margen"]), ("Y", ny, m["margen"], m["cama"][1] - m["margen"]), ("Z", nz, 0.0, m["z_max"])):
                if vv is not None and not (lo - 1e-6 <= vv <= hi + 1e-6):
                    err.append("linea %d: %s=%.3f fuera de [%.1f, %.1f]" % (n, nm, vv, lo, hi))
            if p.seco and ne is not None:
                err.append("linea %d: la version en seco no debe extruir" % n)
            x0, y0, z0 = x, y, z
            x1 = nx if nx is not None else x
            y1 = ny if ny is not None else y
            z1 = nz if nz is not None else z
            de = 0.0
            if ne is not None:
                de = ne - e
                e = ne
                if de < -3.0 - 1e-9:
                    err.append("linea %d: retraccion mayor de 3 mm" % n)
                if de > 0 and not caliente:
                    err.append("linea %d: extrusion antes de calentar" % n)
            mueve = (x0 is not None and y0 is not None and z0 is not None and x1 is not None and y1 is not None and z1 is not None
                     and (abs(x1 - x0) > 1e-9 or abs(y1 - y0) > 1e-9 or abs(z1 - z0) > 1e-9))
            if mueve:
                L = math.dist((x0, y0, z0), (x1, y1, z1))
                dxy = math.hypot(x1 - x0, y1 - y0)
                if fv is not None:
                    v = fv / 60.0
                    if dxy > 1e-9 and v > m["vmax_xy"] + 1e-6:
                        err.append("linea %d: velocidad XY %.0f mm/s > %.0f" % (n, v, m["vmax_xy"]))
                    vz = v * abs(z1 - z0) / L
                    vz_max = max(vz_max, vz)
                    if vz > m["vmax_z"] + 1e-6:
                        err.append("linea %d: velocidad de Z %.1f mm/s > %.0f" % (n, vz, m["vmax_z"]))
                    dzm = abs(z1 - z0)
                    a_ef = min(acc_p, m["amax_z"] * L / dzm) if dzm > 1e-9 else acc_p       # Z solo acelera a 150 mm/s2
                    v_ef = min(v, m["vmax_z"] * L / dzm) if dzm > 1e-9 else v
                    segs_t.append((L, max(v_ef, 1.0), ((x1 - x0) / L, (y1 - y0) / L, (z1 - z0) / L), a_ef))
                extruye = de > 1e-9
                if extruye:
                    e_ext += de
                    if de / L > 0.1:
                        err.append("linea %d: %.3f mm de filamento por mm de recorrido (excesivo)" % (n, de / L))
                # --- altura del material y holgura del cabezal
                nm = max(1, int(math.ceil(L / PASO_MUESTRA)))
                top_extra = 0.0
                if extruye and tag is not None:
                    top_extra = 0.15 if tag["tipo"] == "inclinada" else 0.0
                for i in range(1, nm + 1):
                    f = i / nm
                    px, py, pz = x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, z0 + (z1 - z0) * f
                    pzm = pz - dz_seco                                   # en coordenadas del arbol
                    if celdas and pzm < z_tope + 12.0:
                        hol, hx, hy, pieza = holgura_en(px, py, pzm)
                        if hol < hold_min[0]:
                            hold_min = (hol, px, py, pzm, pieza)
                    if not extruye and dxy > 1e-9 and celdas:
                        k = (int(math.floor(px / CELDA)), int(math.floor(py / CELDA)))
                        ii = indice.get(k)
                        if ii is not None and pzm < celdas[ii][2] + 0.2 - 1e-9:
                            err.append("linea %d: la boquilla se desplaza a z=%.2f por dentro del material (altura %.2f) en (%.1f, %.1f)" % (n, pzm, celdas[ii][2], px, py))
                if extruye:
                    for i in range(0, nm + 1):
                        f = i / nm
                        actualiza_celda(x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, z0 + (z1 - z0) * f - dz_seco + top_extra)
                    z_tope = max(z_tope, z0 - dz_seco + top_extra, z1 - dz_seco + top_extra)
                    if con_seccion and tag is not None:
                        seg_seccion.append(((x0, y0, z0 - dz_seco), (x1, y1, z1 - dz_seco), tag))
            x, y, z = x1, y1, z1
            if nx is not None and x0 is None:
                x = nx
            if ny is not None and y0 is None:
                y = ny
            if nz is not None and z0 is None:
                z = nz
    if not homed:
        err.append("falta G28")
    if p.seco:
        if any(u.startswith(("M104", "M109", "M140", "M190", "M106")) for u in ultimas):
            err.append("la version en seco contiene ordenes de calentar o ventilador")
    else:
        if not caliente:
            err.append("falta calentar la boquilla (M109)")
        if not any(u.startswith("M104 S0") for u in ultimas) or not any(u.startswith("M140 S0") for u in ultimas):
            err.append("falta apagar los calentadores al final")
    if not ultimas or ultimas[-1] != "M501":
        err.append("el archivo debe terminar con M501")

    d.update({"lineas": n_lineas, "movimientos": n_mov, "capas_planas": cap_pl, "capas_inclinadas": cap_in, "extruido_mm": e_ext, "vz_max": vz_max, "tiempo_min": estimar_tiempo(segs_t) / 60.0})
    if not p.seco:
        # el filamento del cebado inicial cuenta como extrusion: se descuenta (es la primera linea del archivo)
        pedido = _volumen_pedido(c)
        area_fil = math.pi * (p.filamento / 2.0) ** 2
        depositado = max(0.0, e_ext - c.cebado_mm) * area_fil / p.flujo
        d["volumen_ratio"] = depositado / pedido
        if not 0.95 <= d["volumen_ratio"] <= 1.06:
            err.append("el volumen depositado es %.1f %% del arbol pedido (fuera de 95-106 %%)" % (100 * d["volumen_ratio"]))
    if hold_min[0] < math.inf:
        d.update({"holgura_min": hold_min[0], "holgura_x": hold_min[1], "holgura_y": hold_min[2], "holgura_z": hold_min[3], "holgura_pieza": hold_min[4]})
        if hold_min[0] < 0:
            err.append("el cabezal CHOCA con lo ya impreso (holgura %.2f mm en x=%.1f y=%.1f z=%.2f)" % hold_min[:4])
        elif hold_min[0] < MARGEN_HOLGURA:
            inf.avisos.append("holgura del cabezal de solo %.2f mm (menos de %.1f)" % (hold_min[0], MARGEN_HOLGURA))
    if con_seccion and seg_seccion and not p.seco:
        _seccion(c, seg_seccion, inf, depurar)
    return inf


# ---------------------------------------------------------------------------------------------------------------------------
def _seccion(c: Constructor, segs, inf: Informe, depurar: Optional[list] = None) -> None:
    """Rasteriza el corte y = centro: cada cordon que lo cruza es un estadio (ancho w, espesor t) orientado segun su capa."""
    p, a = c.p, c.arbol
    cx, cy = c.cx, c.cy
    izq = max([r.largo for r in a.ramas if r.lado < 0] + [0.0]) + a.tronco / 2.0 + 1.0
    der = max([r.largo for r in a.ramas if r.lado > 0] + [0.0]) + a.tronco / 2.0 + 1.0
    x_min, x_max = cx - izq, cx + der
    z_min, z_max = -0.5, a.altura + 1.0
    nx, nz = int(round((x_max - x_min) / RES)), int(round((z_max - z_min) / RES))
    filas = [bytearray(nx) for _ in range(nz)]

    def capsula(ax, az, bx, bz, r):
        i0 = max(0, int((min(ax, bx) - r - x_min) / RES))
        i1 = min(nx - 1, int((max(ax, bx) + r - x_min) / RES) + 1)
        j0 = max(0, int((min(az, bz) - r - z_min) / RES))
        j1 = min(nz - 1, int((max(az, bz) + r - z_min) / RES) + 1)
        dx, dz = bx - ax, bz - az
        l2 = dx * dx + dz * dz
        r2 = r * r
        for j in range(j0, j1 + 1):
            zz = z_min + (j + 0.5) * RES
            fila = filas[j]
            for i in range(i0, i1 + 1):
                xx = x_min + (i + 0.5) * RES
                if l2 < 1e-18:
                    q = (xx - ax) ** 2 + (zz - az) ** 2
                else:
                    u = max(0.0, min(1.0, ((xx - ax) * dx + (zz - az) * dz) / l2))
                    q = (xx - ax - u * dx) ** 2 + (zz - az - u * dz) ** 2
                if q <= r2:
                    fila[i] = 1

    for (x0, y0, z0), (x1, y1, z1), tg in segs:
        t, w = tg["t"], tg["w"]
        inclinada = tg["tipo"] == "inclinada"
        if inclinada:
            al = math.radians(tg["alfa"])
            lado = tg["lado"]
            zc_off = (t / 2.0) / math.cos(al) + tg.get("dz", 0.0)
            ex, ez = lado * math.cos(al), -math.sin(al)
        else:
            zc_off = t / 2.0
            ex, ez = 1.0, 0.0
        r = t / 2.0 + RES / 2.0          # media celda mas: el raster no deja filas vacias en los contactos tangentes entre capas
        if abs(x1 - x0) < 1e-6 and abs(z1 - z0) < 1e-6 and abs(y1 - y0) > 1e-6 and min(y0, y1) - 1e-9 <= cy <= max(y0, y1) + 1e-9:
            h = (w - t) / 2.0
            capsula(x0 - h * ex, z0 - zc_off - h * ez, x0 + h * ex, z0 - zc_off + h * ez, r)
        elif abs(y1 - y0) < 1e-6 and abs(y0 - cy) < w / 2.0 and (abs(x1 - x0) > 1e-6 or abs(z1 - z0) > 1e-6):
            capsula(x0, z0 - zc_off, x1, z1 - zc_off, r)
    if depurar is not None:
        depurar.append((filas, x_min, z_min, nx, nz))
    # arbol pedido
    modelo: List[List[Tuple[float, float]]] = [[] for _ in range(nz)]
    for j in range(nz):
        zz = z_min + (j + 0.5) * RES
        if 0.0 <= zz <= a.altura:
            modelo[j].append((cx - a.tronco / 2.0, cx + a.tronco / 2.0))
        for r in c.stats["ramas"]:
            zb = r["z_piso"]
            if zb <= zz <= zb + r["alto"]:
                x0r = cx + r["lado"] * a.tronco / 2.0
                x1r = cx + r["lado"] * (a.tronco / 2.0 + r["largo"])
                modelo[j].append((min(x0r, x1r), max(x0r, x1r)))
    dentro = fuera = total = 0
    vacios = set()
    for j in range(nz):
        fila = filas[j]
        usados = 0
        for (xa, xb) in modelo[j]:
            ia, ib = max(0, int(round((xa - x_min) / RES))), min(nx, int(round((xb - x_min) / RES)))
            if ib > ia:
                total += ib - ia
                cub = fila.count(1, ia, ib)
                dentro += cub
                usados += cub
                if cub < ib - ia:
                    for i in range(ia, ib):
                        if not fila[i]:
                            vacios.add((j, i))
        fuera += fila.count(1) - usados
    # huecos: los pequenos (entre cordones redondos) los rellena el plastico al aplastarse; los grandes son faltas reales
    grande = 0
    mayor = 0
    visto = set()
    for ini in vacios:
        if ini in visto:
            continue
        pila = [ini]
        visto.add(ini)
        n = 0
        while pila:
            j, i = pila.pop()
            n += 1
            for q in ((j + 1, i), (j - 1, i), (j, i + 1), (j, i - 1)):
                if q in vacios and q not in visto:
                    visto.add(q)
                    pila.append(q)
        mayor = max(mayor, n)
        if n * RES * RES >= AREA_HUECO_GRANDE:
            grande += n
    inf.datos["falta_pct"] = 100.0 * (total - dentro) / total
    inf.datos["hueco_grande_pct"] = 100.0 * grande / total
    inf.datos["hueco_mayor_mm2"] = mayor * RES * RES
    inf.datos["sobra_pct"] = 100.0 * fuera / total
    if inf.datos["hueco_grande_pct"] > 0.5:
        inf.problemas.append("en la seccion central hay huecos grandes (>= %.2f mm2) que suman el %.2f %% del arbol (limite 0,5 %%); el mayor mide %.2f mm2" % (AREA_HUECO_GRANDE, inf.datos["hueco_grande_pct"], inf.datos["hueco_mayor_mm2"]))
    if inf.datos["sobra_pct"] > 3.0:
        inf.problemas.append("en la seccion central sobra el %.1f %% del arbol (limite 3 %%)" % inf.datos["sobra_pct"])
    # planitud del piso de cada rama: material mas bajo de cada columna dentro de la rama
    for idx, rr in enumerate(c.stats["ramas"]):
        zb = rr["z_piso"]

        class _R:                      # vista de la rama construida
            lado, largo, alto = rr["lado"], rr["largo"], rr["alto"]
        r = _R
        xs = [cx + r.lado * (a.tronco / 2.0 + dx) for dx in (1.0 + 0.5 * i for i in range(int((r.largo - 2.0) / 0.5) + 1))]
        mas_bajo: List[float] = []
        for xx in xs:
            i = int(round((xx - x_min) / RES))
            zlow = None
            for j in range(max(0, int((zb - 1.0 - z_min) / RES)), min(nz, int((zb + r.alto - z_min) / RES))):
                if filas[j][i]:
                    zlow = z_min + j * RES
                    break
            if zlow is not None:
                mas_bajo.append(zlow - zb)
        if mas_bajo:
            clave = "piso_%d_(lado %+d, z=%.2f)_desviacion_min/max_mm" % (idx, r.lado, zb)
            inf.datos[clave] = "piso rama lado %+d (z=%.2f): material mas bajo de la columna = piso %+.3f a %+.3f mm (%d columnas)" % (r.lado, zb, min(mas_bajo), max(mas_bajo), len(mas_bajo))
            if min(mas_bajo) < -0.10:
                inf.problemas.append("el piso de la rama lado %+d queda %.2f mm POR DEBAJO del plano pedido" % (r.lado, -min(mas_bajo)))
