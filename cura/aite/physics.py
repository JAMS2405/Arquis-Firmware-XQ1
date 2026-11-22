# SPDX-License-Identifier: LGPL-3.0-or-later
"""Formulas del informe AITE (secciones D.3, D.5, D.8, D.10, D.11). Todas puras y con los dominios comprobados.

Convenciones del informe: beta = angulo de una superficie respecto a la VERTICAL (0 pared, 90 cara inferior plana);
alpha = inclinacion de las capas respecto a la horizontal; t = espesor normal de capa; w = ancho de cordon.
Los valores de c_min, beta_c y kappa NO son constantes: se miden (experimentos E2 y E5 del informe).
"""
import math


def _need(cond: bool, msg: str) -> None:
    if not cond:
        raise ValueError(msg)


def beta_of_normal_z(nz: float) -> float:
    """D.1: beta_s = asin(max(0, -nz)) en grados, para una normal EXTERIOR unitaria. 0 si la cara no mira hacia abajo."""
    _need(math.isfinite(nz), "nz no finito")
    return math.degrees(math.asin(min(1.0, max(0.0, -nz))))


def contact_fraction(beta_rel_deg: float, t: float, w: float) -> float:
    """D.5: c = 1 - (t/w)*tan(beta_rel). Fraccion del cordon apoyada en el de abajo si se desplaza t*tan(beta_rel)."""
    _need(t > 0 and w >= t, "se exige 0 < t <= w")
    _need(0.0 <= beta_rel_deg < 90.0, "beta_rel debe estar en [0, 90)")
    return 1.0 - (t / w) * math.tan(math.radians(beta_rel_deg))


def beta_c_deg(t: float, w: float, c_min: float) -> float:
    """D.5: angulo critico beta_c = atan((w/t)*(1-c_min)). c_min (apoyo minimo) se mide en E2."""
    _need(t > 0 and w >= t, "se exige 0 < t <= w")
    _need(0.0 <= c_min <= 1.0, "c_min debe estar en [0, 1]")
    return math.degrees(math.atan((w / t) * (1.0 - c_min)))


def required_c_min_for_overhang(overhang_deg: float, alpha_deg: float, t: float, w: float) -> float:
    """Apoyo minimo c_min que hace falta para imprimir un voladizo de ``overhang_deg`` con capas inclinadas ``alpha_deg``."""
    return contact_fraction(max(0.0, overhang_deg - alpha_deg), t, w)


def bead_area(w: float, t: float) -> float:
    """D.5 y D.8: seccion del cordon, rectangulo con extremos redondos A = (w - t)*t + pi*t^2/4 (mm^2)."""
    _need(t > 0 and w >= t, "se exige 0 < t <= w")
    return (w - t) * t + math.pi * t * t / 4.0


def filament_per_mm(w: float, t: float, filament_d: float = 1.75, flow: float = 1.0, kappa: float = 1.0) -> float:
    """D.8: mm de filamento por mm de trayecto 3D = A(w,t)*flow*kappa / (pi*d^2/4). kappa(alpha) se calibra en E5 (hoy 1)."""
    _need(filament_d > 0 and flow > 0 and kappa > 0, "filament_d, flow y kappa deben ser positivos")
    return bead_area(w, t) * flow * kappa / (math.pi * filament_d * filament_d / 4.0)


def tip_step_per_layer(t: float, alpha_deg: float) -> float:
    """D.2: avance horizontal de la punta de cada capa inclinada = t / sen(alpha)."""
    _need(t > 0 and 0.0 < alpha_deg <= 90.0, "se exige t > 0 y 0 < alpha <= 90")
    return t / math.sin(math.radians(alpha_deg))


def max_feasible_overhang(beta_c: float, alpha_max: float) -> float:
    """D.3: voladizo maximo sin soporte, beta_c + alpha_max (grados)."""
    return beta_c + alpha_max


def required_tilt(beta_s: float, beta_c: float, margin: float = 0.0) -> float:
    """D.3: inclinacion minima de las capas, max(0, beta_s - beta_c + margen)."""
    return max(0.0, beta_s - beta_c + margin)


def tilt_deficit(beta_s: float, beta_c: float, alpha_max: float) -> float:
    """D.3: deficit en grados cuando no hay ventana, max(0, beta_s - beta_c - alpha_max). 0 = factible."""
    return max(0.0, beta_s - beta_c - alpha_max)


def alpha_max_from_clearance(eta: float, rho: float) -> float:
    """D.10: pendiente maxima que tolera una pieza del cabezal a altura eta y distancia rho de la punta: atan(eta/rho)."""
    _need(eta > 0 and rho > 0, "eta y rho deben ser positivos")
    return math.degrees(math.atan(eta / rho))


def path_speed_limit(vz_max: float, alpha_deg: float) -> float:
    """D.11: velocidad de trayecto maxima por el eje Z, vz_max / sen(alpha). Infinita si alpha = 0."""
    _need(vz_max > 0 and 0.0 <= alpha_deg <= 90.0, "se exige vz_max > 0 y 0 <= alpha <= 90")
    s = math.sin(math.radians(alpha_deg))
    return math.inf if s == 0 else vz_max / s
