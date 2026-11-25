# SPDX-License-Identifier: LGPL-3.0-or-later
"""Comparador de volumen modelo / material depositado (informe AITE, D.9). Base de la validacion de la primera prueba (F).

    V_falta = |M \\ D|        V_exceso = |D \\ M|

con una banda de tolerancia epsilon alrededor del borde (el redondeo del cordon es inevitable): una celda de M cuenta como faltante solo
si no hay material depositado a menos de epsilon, y una celda de D como exceso solo si no hay modelo a menos de epsilon.
En la Fase 1 ``D`` es otra ``LayerStack`` (p. ej. otra malla); en la Fase 2 la rellenara el simulador de deposito.
"""
from dataclasses import dataclass
from typing import List

from .columns import LayerStack, Rows, and_not, count_bits, dilate_disk, or_


@dataclass
class VolumeComparison:
    model_mm3: float
    deposited_mm3: float
    missing_mm3: float
    excess_mm3: float
    eps_xy_cells: int
    eps_z_layers: int

    @property
    def missing_pct(self) -> float:
        return 100.0 * self.missing_mm3 / self.model_mm3 if self.model_mm3 else 0.0

    @property
    def excess_pct(self) -> float:
        return 100.0 * self.excess_mm3 / self.model_mm3 if self.model_mm3 else 0.0


def _compatible(a: LayerStack, b: LayerStack) -> None:
    if (a.nx, a.ny) != (b.nx, b.ny) or abs(a.cell - b.cell) > 1e-12 or abs(a.h - b.h) > 1e-12 or abs(a.z0 - b.z0) > 1e-9 \
            or abs(a.x0 - b.x0) > 1e-9 or abs(a.y0 - b.y0) > 1e-9:
        raise ValueError("las dos pilas deben compartir rejilla, espesor de capa y origen")


def _padded(s: LayerStack, n: int) -> List[Rows]:
    return s.rows + [[0] * s.ny for _ in range(n - s.n)]


def _grown(rows: List[Rows], nx: int, eps_xy: int, eps_z: int) -> List[Rows]:
    d = [dilate_disk(r, eps_xy, nx) for r in rows]
    if eps_z <= 0:
        return d
    out = []
    for k in range(len(d)):
        acc = d[k]
        for dk in range(1, eps_z + 1):
            for kk in (k - dk, k + dk):
                if 0 <= kk < len(d):
                    acc = or_(acc, d[kk])
        out.append(acc)
    return out


def compare_stacks(model: LayerStack, deposited: LayerStack, eps_xy_cells: int = 0, eps_z_layers: int = 0) -> VolumeComparison:
    _compatible(model, deposited)
    n = max(model.n, deposited.n)
    m, d = _padded(model, n), _padded(deposited, n)
    d_grown = _grown(d, model.nx, eps_xy_cells, eps_z_layers)
    m_grown = _grown(m, model.nx, eps_xy_cells, eps_z_layers)
    cell_vol = model.cell * model.cell * model.h
    missing = sum(count_bits(and_not(m[k], d_grown[k])) for k in range(n))
    excess = sum(count_bits(and_not(d[k], m_grown[k])) for k in range(n))
    return VolumeComparison(sum(count_bits(r) for r in m) * cell_vol, sum(count_bits(r) for r in d) * cell_vol,
                            missing * cell_vol, excess * cell_vol, eps_xy_cells, eps_z_layers)
