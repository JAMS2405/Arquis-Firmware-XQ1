# SPDX-License-Identifier: LGPL-3.0-or-later
"""Perfil del cabezal (informe AITE, D.10) y cono unico de inclinacion maxima.

Cada pieza del cabezal es un par (eta, rho): altura de su punto mas bajo sobre la punta y distancia horizontal al eje. Una superficie plana
que sube hacia ese lado con pendiente alfa toca la pieza cuando tan(alfa) > eta/rho. Con ``single_cone`` (decision del propietario) la
pendiente maxima es la MENOR de todas las piezas y direcciones, igual en todas.

Fase 1: modelo de pendiente maxima (suficiente para el solucionador de angulo). El barrido continuo contra lo ya impreso es de la Fase 2.
"""
import json
import os
from dataclasses import dataclass, field
from typing import List, Optional

from .physics import alpha_max_from_clearance

DEFAULT_PROFILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "head_arquis_xq1.json")


@dataclass
class Primitive:
    name: str
    eta: float
    rho: float
    hot: bool = False
    basis: str = ""


@dataclass
class HeadProfile:
    name: str
    primitives: List[Primitive] = field(default_factory=list)
    single_cone: bool = True
    thermal_margin_mm: float = 0.0
    source: str = ""

    def alpha_of(self, p: Primitive) -> float:
        """Pendiente maxima (grados) que tolera la pieza; las calientes restan el margen termico de su altura."""
        eta = p.eta - (self.thermal_margin_mm if p.hot else 0.0)
        if eta <= 0:
            return 0.0
        return alpha_max_from_clearance(eta, p.rho)

    def limiting(self) -> Primitive:
        if not self.primitives:
            raise ValueError("el perfil del cabezal no tiene piezas")
        return min(self.primitives, key=self.alpha_of)

    def cone_alpha_deg(self) -> float:
        """Pendiente maxima unica: la menor de todas las piezas."""
        return self.alpha_of(self.limiting())

    def table(self) -> List[tuple]:
        return [(p.name, p.eta, p.rho, self.alpha_of(p)) for p in self.primitives]


class HeadProfileError(ValueError):
    pass


def load_head_profile(path: Optional[str] = None) -> HeadProfile:
    path = path or DEFAULT_PROFILE
    try:
        with open(path, "r", encoding="utf-8") as f:
            d = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        raise HeadProfileError("no se pudo leer el perfil del cabezal %s: %s" % (path, e)) from e
    try:
        prims = [Primitive(str(p["name"]), float(p["eta"]), float(p["rho"]), bool(p.get("hot", False)), str(p.get("basis", "")))
                 for p in d["primitives"]]
    except (KeyError, TypeError, ValueError) as e:
        raise HeadProfileError("perfil del cabezal mal formado: %s" % e) from e
    for p in prims:
        if not (p.eta > 0 and p.rho > 0):
            raise HeadProfileError("la pieza %r necesita eta y rho positivos" % p.name)
    if not prims:
        raise HeadProfileError("el perfil no tiene piezas")
    margin = float(d.get("thermal_margin_mm", 0.0))
    if margin < 0:
        raise HeadProfileError("thermal_margin_mm no puede ser negativo")
    return HeadProfile(str(d.get("name", "cabezal")), prims, bool(d.get("single_cone", True)), margin, str(d.get("source", "")))
