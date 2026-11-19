# SPDX-License-Identifier: LGPL-3.0-or-later
"""ARQUIS Non-Planar: lectura de una SUPERFICIE desde un modelo real (STL) y analisis. EXPERIMENTAL; solo analisis, no genera G-code.

Que hace: lee un STL (binario o ASCII), proyecta sus triangulos sobre una rejilla XY y obtiene, por celda, la altura de la cara SUPERIOR
(triangulos con la normal hacia +Z) y de la INFERIOR, mas una mascara de celdas con material (un anillo deja el agujero vacio).
Luego mide pendiente, relieve y espesor, y avisa de lo que no es una superficie 2,5D (columnas con varias caras superiores: voladizos,
piezas apiladas), que este enfoque NO puede imprimir.

Limitaciones: pieza en milimetros y en coordenadas de la maquina o de su propio origen (el STL se usa tal cual; no se coloca en la cama);
no lee 3MF ni la escena de Cura (eso requiere validar la API de Cura en el programa real); resolucion limitada por ``cell`` (mm).
"""
import math
import struct
from dataclasses import dataclass, field
from typing import List, Optional, Sequence, Tuple

Tri = Tuple[Tuple[float, float, float], Tuple[float, float, float], Tuple[float, float, float]]
MAX_TRIANGLES = 2_000_000


class StlError(ValueError):
    pass


def read_stl(data: bytes) -> List[Tri]:
    """STL binario o ASCII -> lista de triangulos. Lanza StlError ante un archivo truncado, vacio o con valores no finitos."""
    if len(data) >= 84:
        n = struct.unpack_from("<I", data, 80)[0]
        if 84 + 50 * n == len(data):                                   # binario exacto
            if n == 0:
                raise StlError("STL sin triangulos")
            if n > MAX_TRIANGLES:
                raise StlError("STL demasiado grande (%d triangulos)" % n)
            tris = []
            for i in range(n):
                v = struct.unpack_from("<12f", data, 84 + 50 * i)
                tris.append(((v[3], v[4], v[5]), (v[6], v[7], v[8]), (v[9], v[10], v[11])))
            _check_finite(tris)
            return tris
    text = data.decode("utf-8", errors="strict") if data[:5].lower() == b"solid" else None
    if text is None:
        raise StlError("STL binario truncado o formato desconocido")
    verts, tris = [], []
    for line in text.splitlines():
        p = line.split()
        if len(p) == 4 and p[0] == "vertex":
            try:
                verts.append((float(p[1]), float(p[2]), float(p[3])))
            except ValueError:
                raise StlError("vertice ilegible: %r" % line)
            if len(verts) == 3:
                tris.append(tuple(verts))
                verts = []
    if verts or not tris:
        raise StlError("STL ASCII incompleto o sin triangulos")
    if len(tris) > MAX_TRIANGLES:
        raise StlError("STL demasiado grande")
    _check_finite(tris)
    return tris


def _check_finite(tris) -> None:
    for t in tris:
        for v in t:
            for c in v:
                if not math.isfinite(c):
                    raise StlError("el STL contiene valores no finitos")


@dataclass
class Surface:
    x0: float
    y0: float
    cell: float
    nx: int
    ny: int
    top: List[List[Optional[float]]]      # altura de la cara superior por celda (None = sin material)
    bot: List[List[Optional[float]]]      # altura de la cara inferior
    stacked: int = 0                       # celdas con varias caras superiores distintas (no son 2,5D)
    triangles: int = 0

    def covered(self) -> List[Tuple[int, int]]:
        return [(i, j) for j in range(self.ny) for i in range(self.nx) if self.top[j][i] is not None]


def rasterize(tris: Sequence[Tri], cell: float = 1.0, stack_tol: float = 0.2) -> Surface:
    """Altura superior/inferior por celda en el CENTRO de la celda (interpolacion baricentrica). La cara superior es la de
    normal +Z; si una celda recibe caras superiores a alturas distintas (> stack_tol mm) se cuenta en ``stacked`` y se conserva la mas alta."""
    if cell <= 0:
        raise ValueError("cell debe ser > 0")
    xs = [v[0] for t in tris for v in t]
    ys = [v[1] for t in tris for v in t]
    x0, y0 = min(xs), min(ys)
    nx, ny = int(math.ceil((max(xs) - x0) / cell)) + 1, int(math.ceil((max(ys) - y0) / cell)) + 1
    if nx * ny > 4_000_000:
        raise ValueError("rejilla demasiado grande (%dx%d): sube cell" % (nx, ny))
    top = [[None] * nx for _ in range(ny)]
    bot = [[None] * nx for _ in range(ny)]
    ups: dict = {}
    for a, b, c in tris:
        nz = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])         # componente z de la normal (a->b x a->c)
        if abs(nz) < 1e-12:
            continue                                                               # triangulo vertical o degenerado
        det = nz
        i0 = max(0, int(math.floor((min(a[0], b[0], c[0]) - x0) / cell)))
        i1 = min(nx - 1, int(math.ceil((max(a[0], b[0], c[0]) - x0) / cell)))
        j0 = max(0, int(math.floor((min(a[1], b[1], c[1]) - y0) / cell)))
        j1 = min(ny - 1, int(math.ceil((max(a[1], b[1], c[1]) - y0) / cell)))
        for j in range(j0, j1 + 1):
            py = y0 + j * cell
            for i in range(i0, i1 + 1):
                px = x0 + i * cell
                w1 = ((px - a[0]) * (c[1] - a[1]) - (py - a[1]) * (c[0] - a[0])) / det
                w2 = ((b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0])) / det
                w0 = 1.0 - w1 - w2
                if w0 < -1e-9 or w1 < -1e-9 or w2 < -1e-9:
                    continue
                z = w0 * a[2] + w1 * b[2] + w2 * c[2]
                if nz > 0:
                    ups.setdefault((i, j), []).append(z)
                    if top[j][i] is None or z > top[j][i]:
                        top[j][i] = z
                else:
                    if bot[j][i] is None or z < bot[j][i]:
                        bot[j][i] = z
    stacked = 0
    for zs in ups.values():
        zs.sort()
        if any(zs[k + 1] - zs[k] > stack_tol for k in range(len(zs) - 1)):
            stacked += 1
    return Surface(x0, y0, cell, nx, ny, top, bot, stacked, len(tris))


@dataclass
class SurfaceReport:
    cells: int
    area_mm2: float
    z_min: float
    z_max: float
    relief: float                 # z_max - z_min de la cara superior
    slope_max_deg: float
    slope_limit_deg: float
    thick_min: float
    thick_max: float
    bad_cells: int                # celdas con pendiente > limite
    stacked: int
    holes: int                    # celdas vacias dentro de la caja de la pieza rodeadas de material (agujeros)
    findings: List[str] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.findings or all(not f.startswith("ERROR") for f in self.findings)


def analyze_surface(s: Surface, slope_limit_deg: float, max_relief: float = 2.0) -> SurfaceReport:
    """Pendiente por diferencias finitas entre celdas vecinas con material; espesor = superior - inferior (si hay inferior)."""
    cov = s.covered()
    if not cov:
        raise ValueError("la superficie no tiene celdas con material")
    zs = [s.top[j][i] for i, j in cov]
    slope_max, bad = 0.0, 0
    for i, j in cov:
        z = s.top[j][i]
        worst = 0.0
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ii, jj = i + di, j + dj
            if 0 <= ii < s.nx and 0 <= jj < s.ny and s.top[jj][ii] is not None:
                worst = max(worst, abs(s.top[jj][ii] - z) / s.cell)
        deg = math.degrees(math.atan(worst))
        slope_max = max(slope_max, deg)
        if deg > slope_limit_deg:
            bad += 1
    th = [s.top[j][i] - s.bot[j][i] for i, j in cov if s.bot[j][i] is not None]
    holes = 0
    for j in range(1, s.ny - 1):
        for i in range(1, s.nx - 1):
            if s.top[j][i] is None and _material_around(s, i, j):
                holes += 1
    rep = SurfaceReport(len(cov), len(cov) * s.cell * s.cell, min(zs), max(zs), max(zs) - min(zs), slope_max, slope_limit_deg,
                        min(th) if th else 0.0, max(th) if th else 0.0, bad, s.stacked, holes)
    if s.stacked:
        rep.findings.append("ERROR: %d celdas tienen varias caras superiores a distinta altura: la pieza no es una superficie 2,5D (voladizos o piezas apiladas)." % s.stacked)
    if bad:
        rep.findings.append("ERROR: %d celdas superan la pendiente maxima de %.1f deg (el cono de la boquilla chocaria)." % (bad, slope_limit_deg))
    if rep.relief > max_relief:
        rep.findings.append("ERROR: relieve %.2f mm > desviacion maxima admitida %.2f mm." % (rep.relief, max_relief))
    if s.cell > 1.0:
        rep.findings.append("WARN: celda de %.2f mm: la pendiente puede estar subestimada." % s.cell)
    if holes:
        rep.findings.append("INFO: %d celdas vacias rodeadas de material (agujeros): las trayectorias deberan interrumpirse y retraer ahi (no implementado aun)." % holes)
    return rep


def _material_around(s: Surface, i: int, j: int) -> bool:
    """Hay material a ambos lados en X y en Y (la celda vacia esta dentro de la pieza)."""
    def hit(di, dj):
        ii, jj = i, j
        while 0 <= ii < s.nx and 0 <= jj < s.ny:
            if s.top[jj][ii] is not None:
                return True
            ii += di; jj += dj
        return False
    return hit(1, 0) and hit(-1, 0) and hit(0, 1) and hit(0, -1)


def render_surface_preview(s: Surface, rep: SurfaceReport, name: str = "") -> str:
    """SVG: altura (azul-claro) y celdas con pendiente excesiva en rojo."""
    sc = max(2.0, min(10.0, 560.0 / max(s.nx, s.ny)))
    W, H = int(s.nx * sc) + 340, max(int(s.ny * sc) + 120, 360)
    z0, z1 = rep.z_min, max(rep.z_max, rep.z_min + 1e-6)
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" font-family="sans-serif" font-size="12">' % (W, H), '<rect width="100%" height="100%" fill="#0f1620"/>',
           '<text x="12" y="22" fill="#ffb020" font-size="15" font-weight="bold">ARQUIS Non-Planar EXPERIMENTAL - analisis de superficie (solo analisis; no es G-code)</text>',
           '<text x="12" y="42" fill="#9fb3c8">%s</text>' % _esc(name)]
    for j in range(s.ny):
        for i in range(s.nx):
            z = s.top[j][i]
            if z is None:
                continue
            t = (z - z0) / (z1 - z0)
            col = "#e74c3c" if _slope_at(s, i, j) > rep.slope_limit_deg else "#%02x%02x%02x" % (int(40 + 60 * t), int(90 + 110 * t), int(150 + 90 * t))
            out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"/>' % (12 + i * sc, 60 + (s.ny - 1 - j) * sc, sc + 0.3, sc + 0.3, col))
    x = int(s.nx * sc) + 30
    y = 70
    for ln in ["celdas %d, area %.0f mm2, celda %.2f mm" % (rep.cells, rep.area_mm2, s.cell), "Z superior %.2f..%.2f mm (relieve %.2f)" % (rep.z_min, rep.z_max, rep.relief),
               "pendiente max %.1f deg (limite %.1f)" % (rep.slope_max_deg, rep.slope_limit_deg), "espesor %.2f..%.2f mm" % (rep.thick_min, rep.thick_max),
               "rojo = pendiente sobre el limite (%d celdas)" % rep.bad_cells, "agujeros: %d celdas, apiladas: %d" % (rep.holes, rep.stacked)] + rep.findings:
        out.append('<text x="%d" y="%d" fill="%s">%s</text>' % (x, y, "#ff6b6b" if ln.startswith("ERROR") else "#cfd8e3", _esc(ln[:70])))
        y += 18
    out.append('</svg>')
    return "\n".join(out)


def _slope_at(s: Surface, i: int, j: int) -> float:
    z = s.top[j][i]
    worst = 0.0
    for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        ii, jj = i + di, j + dj
        if 0 <= ii < s.nx and 0 <= jj < s.ny and s.top[jj][ii] is not None:
            worst = max(worst, abs(s.top[jj][ii] - z) / s.cell)
    return math.degrees(math.atan(worst))


def _esc(t: str) -> str:
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
