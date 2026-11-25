# SPDX-License-Identifier: LGPL-3.0-or-later
"""Overhang Detector y Adaptive Angle Solver (version por region, Fase 1). Informe AITE, D.1 a D.3.

Dos detectores complementarios:
  * por facetas: beta_s = asin(max(0, -nz)) de cada triangulo (angulo local respecto a la vertical);
  * por capas: O_k = L_k \\ (L_{k-1} (+) B(s_c)), s_c = h*tan(beta_c): lo que de verdad queda sin apoyo a la resolucion del laminado.
Cada region de capas recibe el beta_s maximo de las facetas que la atraviesan y, con la ventana de D.3, la inclinacion minima de capa
necesaria y el deficit (grados) si supera la pendiente maxima del cabezal. Nunca se modifica el modelo.

Facetas apoyadas en la cama (todos sus vertices en z <= zmin) se ignoran: no son voladizos.
"""
import math
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from .columns import (ColumnGrid, LayerStack, and_, and_not, components, count_bits, dilate_disk, rows_to_set)
from .mesh import Mesh, MeshReport, analyze_mesh
from .physics import beta_of_normal_z, required_tilt, tilt_deficit


# ---------------------------------------------------------------- por facetas
@dataclass
class FacetAngle:
    index: int
    beta_deg: float
    area: float
    centroid: Tuple[float, float, float]


@dataclass
class FacetReport:
    total_area: float
    bed_area: float                 # facetas apoyadas en la cama, ignoradas
    overhang_area: float            # beta_s > beta_c
    infeasible_area: float          # beta_s > beta_c + alpha_max
    max_beta_deg: float
    max_deficit_deg: float
    normals_flipped: bool           # el volumen salio negativo: se analizo con las normales invertidas (el modelo no se toca)
    bins: List[Tuple[float, float, float]] = field(default_factory=list)   # (desde, hasta, area) en grados


def facet_angles(mesh: Mesh, ignore_bed: bool = True, bed_tol: float = 1e-6) -> Tuple[List[FacetAngle], float, bool]:
    """Devuelve (facetas con beta > 0, area apoyada en la cama, normales invertidas)."""
    flipped = mesh.signed_volume() < 0
    zmin = mesh.bbox()[0][2]
    out: List[FacetAngle] = []
    bed = 0.0
    for idx, t in enumerate(mesh.triangles):
        (nx, ny, nz), area = mesh.normal_area(t)
        if area == 0.0:
            continue
        a, b, c = mesh.corners(t)
        if ignore_bed and max(a[2], b[2], c[2]) <= zmin + bed_tol:
            bed += area
            continue
        beta = beta_of_normal_z(-nz if flipped else nz)
        if beta > 0.0:
            out.append(FacetAngle(idx, beta, area, ((a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3)))
    return out, bed, flipped


def facet_report(mesh: Mesh, beta_c: float, alpha_max: float, bin_deg: float = 10.0, ignore_bed: bool = True) -> FacetReport:
    facets, bed, flipped = facet_angles(mesh, ignore_bed)
    total = mesh.surface_area()
    nb = int(math.ceil(90.0 / bin_deg))
    bins = [0.0] * nb
    over = infeas = 0.0
    maxb = maxd = 0.0
    for f in facets:
        bins[min(nb - 1, int(f.beta_deg // bin_deg))] += f.area
        maxb = max(maxb, f.beta_deg)
        if f.beta_deg > beta_c:
            over += f.area
        d = tilt_deficit(f.beta_deg, beta_c, alpha_max)
        if d > 0.0:
            infeas += f.area
            maxd = max(maxd, d)
    return FacetReport(total, bed, over, infeas, maxb, maxd, flipped, [(i * bin_deg, min(90.0, (i + 1) * bin_deg), a) for i, a in enumerate(bins)])


# ---------------------------------------------------------------- por capas
@dataclass
class OverhangRegion:
    layer: int
    z_mm: float                     # base de la capa
    cells: int
    area_mm2: float
    bbox_mm: Tuple[float, float, float, float]      # x0, y0, x1, y1
    centroid_mm: Tuple[float, float]
    kind: str                       # 'voladizo' (anclado por un lado), 'puente' (por dos lados opuestos), 'isla' (sin anclaje)
    sides: str                      # lados anclados: subconjunto de 'EWNS' (E = +x)
    depth_mm: float                 # alejamiento maximo del anclaje (aprox., vecindad de 4)
    beta_s_deg: Optional[float] = None
    required_tilt_deg: Optional[float] = None
    deficit_deg: Optional[float] = None
    cell_set: set = field(default_factory=set, repr=False, compare=False)       # celdas (i, j) reales de la region

    @property
    def feasible(self) -> Optional[bool]:
        return None if self.deficit_deg is None else self.deficit_deg <= 0.0


def support_radius_cells(h: float, beta_c_deg: float, cell: float) -> int:
    """Radio (celdas) del disco con el que se dilata la capa inferior: s_c = h*tan(beta_c)."""
    if beta_c_deg >= 89.9:
        return 10 ** 6
    return int(round(h * math.tan(math.radians(beta_c_deg)) / cell))


def _depth(cells: set, sup_rows: List[int]) -> float:
    from collections import deque
    dist: Dict[Tuple[int, int], int] = {}
    dq = deque()
    for (i, j) in cells:
        if any(0 <= jj < len(sup_rows) and ii >= 0 and (sup_rows[jj] >> ii) & 1 for ii, jj in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1))):
            dist[(i, j)] = 1
            dq.append((i, j))
    best = 0
    while dq:
        c = dq.popleft()
        d = dist[c]
        best = max(best, d)
        i, j = c
        for n in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
            if n in cells and n not in dist:
                dist[n] = d + 1
                dq.append(n)
    return float(best)


def find_overhang_regions(stack: LayerStack, beta_c_deg: float, min_cells: int = 2) -> List[OverhangRegion]:
    r = support_radius_cells(stack.h, beta_c_deg, stack.cell)
    out: List[OverhangRegion] = []
    for k in range(1, stack.n):
        cur, below = stack.rows[k], stack.rows[k - 1]
        if r >= 10 ** 6:
            continue
        dil = dilate_disk(below, r, stack.nx)
        un = and_not(cur, dil)
        if not count_bits(un):
            continue
        sup = and_(cur, dil)
        for comp in components(rows_to_set(un)):
            if len(comp) < min_cells:
                continue
            sides = ""
            for ch, (di, dj) in (("E", (1, 0)), ("W", (-1, 0)), ("N", (0, 1)), ("S", (0, -1))):
                for (i, j) in comp:
                    ii, jj = i + di, j + dj
                    if 0 <= jj < stack.ny and ii >= 0 and (sup[jj] >> ii) & 1:
                        sides += ch
                        break
            if not sides:
                kind = "isla"
            elif ("E" in sides and "W" in sides) or ("N" in sides and "S" in sides):
                kind = "puente"
            else:
                kind = "voladizo"
            xs = [stack.center(i, j)[0] for i, j in comp]
            ys = [stack.center(i, j)[1] for i, j in comp]
            half = stack.cell / 2
            depth = _depth(comp, sup) * stack.cell if sides else 0.0
            out.append(OverhangRegion(k, stack.z0 + k * stack.h, len(comp), len(comp) * stack.cell ** 2,
                                      (min(xs) - half, min(ys) - half, max(xs) + half, max(ys) + half),
                                      (sum(xs) / len(xs), sum(ys) / len(ys)), kind, sides, depth, cell_set=comp))
    return out


def _samples(mesh: Mesh, f: FacetAngle, spacing: float):
    """Puntos de la faceta: vertices, centroide y una reticula baricentrica de paso <= ``spacing``, para que una faceta grande e inclinada
    toque todas las capas y celdas que atraviesa."""
    a, b, c = mesh.corners(mesh.triangles[f.index])
    longest = max(math.dist(a, b), math.dist(b, c), math.dist(c, a))
    n = max(1, int(math.ceil(longest / spacing)))
    pts = [f.centroid, a, b, c]
    if n >= 2:
        for i in range(n + 1):
            for j in range(n + 1 - i):
                wa, wb = i / n, j / n
                wc = 1.0 - wa - wb
                pts.append((wa * a[0] + wb * b[0] + wc * c[0], wa * a[1] + wb * b[1] + wc * c[1], wa * a[2] + wb * b[2] + wc * c[2]))
    return pts


def attach_facet_angles(regions: List[OverhangRegion], mesh: Mesh, stack: LayerStack, facets: List[FacetAngle]) -> None:
    """Asigna a cada region el beta_s maximo de las facetas que la tocan (celda +-1 y capa +-1 alrededor de 7 puntos de muestra)."""
    if not regions:
        return
    cell_sets = [reg.cell_set for reg in regions]
    by_layer: Dict[int, List[int]] = {}
    for idx, reg in enumerate(regions):
        by_layer.setdefault(reg.layer, []).append(idx)
    best: Dict[int, float] = {}
    for f in facets:
        for (x, y, z) in _samples(mesh, f, stack.cell):
            i = int(math.floor((x - stack.x0) / stack.cell))
            j = int(math.floor((y - stack.y0) / stack.cell))
            k = int(math.floor((z - stack.z0) / stack.h + 1e-9))
            for kk in (k - 1, k, k + 1):
                for idx in by_layer.get(kk, ()):
                    cs = cell_sets[idx]
                    if any((i + di, j + dj) in cs for di in (-1, 0, 1) for dj in (-1, 0, 1)):
                        best[idx] = max(best.get(idx, 0.0), f.beta_deg)
    for idx, beta in best.items():
        regions[idx].beta_s_deg = beta


def solve_region_angles(regions: List[OverhangRegion], beta_c: float, alpha_max: float, margin_deg: float = 0.0) -> None:
    """Adaptive Angle Solver por region (D.3): inclinacion necesaria = max(0, beta_s - beta_c + margen) y deficit si supera alpha_max."""
    for r in regions:
        if r.beta_s_deg is None:
            r.required_tilt_deg = r.deficit_deg = None
            continue
        r.required_tilt_deg = required_tilt(r.beta_s_deg, beta_c, margin_deg)
        r.deficit_deg = tilt_deficit(r.beta_s_deg, beta_c, alpha_max)


@dataclass
class OverhangAnalysis:
    mesh_report: MeshReport
    facets: FacetReport
    regions: List[OverhangRegion]
    layers: int
    columns: int
    bad_columns: int
    beta_c_deg: float
    alpha_max_deg: float
    margin_deg: float

    @property
    def infeasible_regions(self) -> List[OverhangRegion]:
        return [r for r in self.regions if r.deficit_deg is not None and r.deficit_deg > 0.0]

    @property
    def max_deficit_deg(self) -> float:
        return max([r.deficit_deg for r in self.regions if r.deficit_deg is not None] + [self.facets.max_deficit_deg, 0.0])


def analyze_overhangs(mesh: Mesh, beta_c_deg: float, alpha_max_deg: float, layer_h: float = 0.2, cell: float = 0.25,
                      margin_deg: float = 0.0, min_cells: int = 2) -> OverhangAnalysis:
    rep = analyze_mesh(mesh)
    grid = ColumnGrid(mesh, cell)
    stack = LayerStack.from_grid(grid, layer_h)
    regions = find_overhang_regions(stack, beta_c_deg, min_cells)
    facets, _, _ = facet_angles(mesh)
    attach_facet_angles(regions, mesh, stack, facets)
    solve_region_angles(regions, beta_c_deg, alpha_max_deg, margin_deg)
    return OverhangAnalysis(rep, facet_report(mesh, beta_c_deg, alpha_max_deg), regions, stack.n, len(grid.intervals), len(grid.bad_columns),
                            beta_c_deg, alpha_max_deg, margin_deg)
