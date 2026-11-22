# SPDX-License-Identifier: LGPL-3.0-or-later
"""Geometry Analyzer, parte 2: ocupacion del modelo como columnas verticales y como pila de capas con mapas de bits.

``ColumnGrid`` lanza un rayo vertical por el centro de cada celda XY y guarda los intervalos de z dentro del solido (paridad de cruces).
Es exacto para el volumen en z y admite voladizos (varios intervalos por columna), que es lo que un mapa de alturas no puede describir.
``LayerStack`` muestrea esos intervalos a la altura media de cada capa y guarda cada fila de celdas como un entero (mapa de bits), de modo
que dilatar, restar y contar son operaciones baratas en Python puro.

Limite conocido (Fase 1): resolucion uniforme en XY; las caras casi verticales se resuelven al tamano de celda.
"""
import math
from collections import deque
from typing import Dict, Iterable, Iterator, List, Optional, Set, Tuple

from .mesh import Mesh, MeshError

Cell = Tuple[int, int]
Rows = List[int]

_JITTER = (3.1e-4, 7.3e-4)   # fraccion de celda: evita que un rayo caiga exactamente sobre una arista


class ColumnGrid:
    def __init__(self, mesh: Mesh, cell: float = 0.25):
        if cell <= 0:
            raise ValueError("cell debe ser positivo")
        (mnx, mny, mnz), (mxx, mxy, mxz) = mesh.bbox()
        self.cell = cell
        self.x0 = math.floor(mnx / cell + 1e-9) * cell
        self.y0 = math.floor(mny / cell + 1e-9) * cell
        self.nx = max(1, int(math.ceil((mxx - self.x0) / cell - 1e-9)))
        self.ny = max(1, int(math.ceil((mxy - self.y0) / cell - 1e-9)))
        self.zmin, self.zmax = mnz, mxz
        self.intervals: Dict[Cell, List[Tuple[float, float]]] = {}
        self.bad_columns: List[Cell] = []
        self._cast(mesh)

    def _cast(self, mesh: Mesh) -> None:
        c, x0, y0, nx, ny = self.cell, self.x0, self.y0, self.nx, self.ny
        jx, jy = _JITTER[0] * c, _JITTER[1] * c
        hits: Dict[Cell, List[float]] = {}
        for t in mesh.triangles:
            (ax, ay, az), (bx, by, bz), (cx, cy, cz) = mesh.corners(t)
            d = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
            if abs(d) < 1e-14:
                continue
            i0 = max(0, int(math.floor((min(ax, bx, cx) - x0) / c - 0.5)))
            i1 = min(nx - 1, int(math.ceil((max(ax, bx, cx) - x0) / c - 0.5)))
            j0 = max(0, int(math.floor((min(ay, by, cy) - y0) / c - 0.5)))
            j1 = min(ny - 1, int(math.ceil((max(ay, by, cy) - y0) / c - 0.5)))
            inv = 1.0 / d
            for j in range(j0, j1 + 1):
                py = y0 + (j + 0.5) * c + jy
                for i in range(i0, i1 + 1):
                    px = x0 + (i + 0.5) * c + jx
                    w0 = ((bx - px) * (cy - py) - (by - py) * (cx - px)) * inv
                    if w0 < 0.0:
                        continue
                    w1 = ((cx - px) * (ay - py) - (cy - py) * (ax - px)) * inv
                    if w1 < 0.0:
                        continue
                    w2 = 1.0 - w0 - w1
                    if w2 < 0.0:
                        continue
                    hits.setdefault((i, j), []).append(w0 * az + w1 * bz + w2 * cz)
        for col, zs in hits.items():
            if len(zs) % 2:
                self.bad_columns.append(col)       # malla no estanca en esta columna: se excluye
                continue
            zs.sort()
            ivs = [(zs[k], zs[k + 1]) for k in range(0, len(zs), 2) if zs[k + 1] - zs[k] > 1e-9]
            if ivs:
                self.intervals[col] = ivs

    def center(self, i: int, j: int) -> Tuple[float, float]:
        return (self.x0 + (i + 0.5) * self.cell, self.y0 + (j + 0.5) * self.cell)

    def volume(self) -> float:
        a = self.cell * self.cell
        return sum((z1 - z0) * a for ivs in self.intervals.values() for z0, z1 in ivs)

    def top_heights(self) -> Dict[Cell, float]:
        return {col: ivs[-1][1] for col, ivs in self.intervals.items()}


# ---------------------------------------------------------------- mapas de bits por fila
def bit_cells(x: int) -> Iterator[int]:
    while x:
        low = x & -x
        yield low.bit_length() - 1
        x ^= low


def count_bits(rows: Rows) -> int:
    return sum(r.bit_count() for r in rows)


def smear(x: int, w: int) -> int:
    r = x
    for s in range(1, w + 1):
        r |= (x << s) | (x >> s)
    return r


def dilate_disk(rows: Rows, r: int, nx: int) -> Rows:
    """Dilatacion con un disco euclidiano de radio ``r`` celdas (r = 0 devuelve una copia)."""
    if r <= 0:
        return list(rows)
    mask = (1 << nx) - 1
    ny = len(rows)
    half = [int(math.sqrt(r * r - dy * dy) + 1e-9) for dy in range(r + 1)]
    smeared = {w: [smear(row, w) & mask for row in rows] for w in set(half)}
    out = [0] * ny
    for j in range(ny):
        acc = 0
        for dy in range(-r, r + 1):
            jj = j + dy
            if 0 <= jj < ny:
                acc |= smeared[half[abs(dy)]][jj]
        out[j] = acc
    return out


def and_not(a: Rows, b: Rows) -> Rows:
    return [x & ~y for x, y in zip(a, b)]


def and_(a: Rows, b: Rows) -> Rows:
    return [x & y for x, y in zip(a, b)]


def or_(a: Rows, b: Rows) -> Rows:
    return [x | y for x, y in zip(a, b)]


def rows_to_set(rows: Rows) -> Set[Cell]:
    return {(i, j) for j, r in enumerate(rows) for i in bit_cells(r)}


def set_to_rows(cells: Iterable[Cell], ny: int) -> Rows:
    rows = [0] * ny
    for i, j in cells:
        rows[j] |= 1 << i
    return rows


def components(cells: Set[Cell]) -> List[Set[Cell]]:
    """Componentes conexas con vecindad de 4."""
    left = set(cells)
    out: List[Set[Cell]] = []
    while left:
        seed = left.pop()
        comp = {seed}
        dq = deque([seed])
        while dq:
            i, j = dq.popleft()
            for n in ((i + 1, j), (i - 1, j), (i, j + 1), (i, j - 1)):
                if n in left:
                    left.discard(n)
                    comp.add(n)
                    dq.append(n)
        out.append(comp)
    return out


# ---------------------------------------------------------------- pila de capas
class LayerStack:
    """Capas de espesor ``h``. La capa k ocupa z en [z0 + k*h, z0 + (k+1)*h) y se muestrea en su altura media."""

    def __init__(self, nx: int, ny: int, cell: float, h: float, z0: float, rows: List[Rows], x0: float = 0.0, y0: float = 0.0):
        self.nx, self.ny, self.cell, self.h, self.z0, self.rows = nx, ny, cell, h, z0, rows
        self.x0, self.y0 = x0, y0

    @property
    def n(self) -> int:
        return len(self.rows)

    @classmethod
    def from_grid(cls, grid: ColumnGrid, h: float, z0: Optional[float] = None) -> "LayerStack":
        if h <= 0:
            raise ValueError("h debe ser positivo")
        z0 = grid.zmin if z0 is None else z0
        n = max(1, int(math.ceil((grid.zmax - z0) / h - 1e-9)))
        rows = [[0] * grid.ny for _ in range(n)]
        for (i, j), ivs in grid.intervals.items():
            bit = 1 << i
            for za, zb in ivs:
                k_lo = max(0, int(math.ceil((za - z0) / h - 0.5 - 1e-9)))
                k_hi = min(n - 1, int(math.ceil((zb - z0) / h - 0.5 - 1e-9)) - 1)
                for k in range(k_lo, k_hi + 1):
                    rows[k][j] |= bit
        return cls(grid.nx, grid.ny, grid.cell, h, z0, rows, grid.x0, grid.y0)

    def count(self, k: int) -> int:
        return count_bits(self.rows[k])

    def area_mm2(self, k: int) -> float:
        return self.count(k) * self.cell * self.cell

    def volume(self) -> float:
        return sum(self.count(k) for k in range(self.n)) * self.cell * self.cell * self.h

    def z_mid(self, k: int) -> float:
        return self.z0 + (k + 0.5) * self.h

    def center(self, i: int, j: int) -> Tuple[float, float]:
        return (self.x0 + (i + 0.5) * self.cell, self.y0 + (j + 0.5) * self.cell)
