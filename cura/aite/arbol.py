# SPDX-License-Identifier: LGPL-3.0-or-later
"""AITE - arbol de prueba con ramas en voladizo de 90 grados (primera prueba fisica, ensayo E2).

Genera el G-code de un tronco cuadrado con ramas horizontales de cara inferior PLANA (90 grados), sin soportes:

* El tronco se imprime con capas planas normales.
* Cada rama se imprime con CAPAS INCLINADAS finas que BAJAN hacia afuera desde la pared del tronco (el eje Z se mueve dentro de
  la capa). Las puntas de todas las capas caen sobre el plano del piso de la rama, que queda plano. Cada capa sobresale de la
  anterior t / sen(alfa) en horizontal: con alfa = 18,5 grados y t = 0,10 mm son 0,32 mm, un 55 % del cordon apoyado. Con capas planas
  de 0,3 mm el apoyo seria negativo: por eso el voladizo plano de 90 grados solo es posible con capa fina + inclinacion.
* El tronco sube INTERCALADO con la rama (la pared debe existir hasta la altura donde cada capa inclinada la toca).
* Antes de cada rama el cabezal se aparta, baja la temperatura (PETG 230 -> 220) y sube el ventilador al 100 %; al terminar la rama
  se aparta otra vez y vuelve a la temperatura normal.

Nada de esto esta probado en la maquina real. El G-code se verifica leyendo SU TEXTO (aite.gcode_check) contra los limites del
firmware, la geometria del cabezal y el volumen del arbol; la fisica (apoyo minimo c_min, combado, rozamiento de la punta) la
decide la prueba. Solo biblioteca estandar de Python.
"""
import math
from dataclasses import dataclass, field, replace
from typing import List, Optional, Sequence, Tuple

from .head import load_head_profile
from .physics import beta_c_deg, filament_per_mm

VERSION = 1
EPS = 1e-9

# Mismos limites que herramientas/torres/generar_torres.py (XQ1) y Marlin/Configuration.h; una prueba lo comprueba.
MAQUINA_XQ1 = {"nombre": "Arquis XQ1", "cama": (300.0, 300.0), "z_max": 450.0, "vmax_xy": 500.0, "vmax_z": 45.0, "amax_z": 150.0,
               "boquilla_max": 270, "cama_max": 105, "margen": 10.0}

LINK_MAX = 1.6          # mm: una union mas corta que esto se EXTRUYE (no se retrae ni se levanta)
LINEA_MIN = 0.1         # mm: una linea mas corta que esto no se imprime (cierra el hueco central de las espirales)
HOLGURA_ALFA = 1.0      # grados que debe quedar la inclinacion por debajo del cono del cabezal


# ---------------------------------------------------------------------------------------------------------------------------
@dataclass
class Rama:
    lado: int                 # +1 = sale hacia +X, -1 = hacia -X
    z_piso: float             # altura (mm) de la cara inferior plana
    largo: float = 20.0       # desde la pared del tronco
    alto: float = 8.0
    ancho: float = 10.0       # en Y


@dataclass
class Arbol:
    tronco: float = 10.0      # lado del tronco cuadrado
    altura: float = 50.0
    ramas: List[Rama] = field(default_factory=lambda: [Rama(+1, 18.0), Rama(-1, 34.0)])

    def ancho_total(self) -> float:
        izq = max([r.largo for r in self.ramas if r.lado < 0] + [0.0])
        der = max([r.largo for r in self.ramas if r.lado > 0] + [0.0])
        return self.tronco + izq + der


@dataclass
class Proceso:
    material: str = "PETG"
    boquilla: float = 0.6
    ancho: float = 0.66
    capa_plana: float = 0.3
    capa_zona: float = 0.15         # espesor de las capas planas del tronco mientras se imprime una rama (la pared no pasa de la punta)
    capa_incl: float = 0.10
    alfa: float = 18.5              # inclinacion de las capas de la rama, grados desde la horizontal
    c_min: float = 0.5              # apoyo minimo SUPUESTO (se mide en el ensayo E2)
    filamento: float = 1.75
    flujo: float = 0.95
    t_boq: int = 230
    t_voladizo: int = 220
    t_cama: int = 70
    ventilador: int = 100           # 0-255, capas planas desde la capa 3 (igual que la torre PETG del repositorio)
    ventilador_voladizo: int = 255
    v_primera: float = 20.0
    v_plana: float = 40.0
    v_incl: float = 20.0
    v_viaje: float = 150.0
    min_capa_s: float = 8.0
    v_min: float = 15.0
    acc_plana: int = 1000
    acc_incl: int = 500             # la aceleracion de Z es 150 mm/s2: 150 / sen(18,5) = 473 mm/s2 de trayecto
    retr: float = 1.0
    v_retr: float = 40.0
    salto_z: float = 0.6
    traslape_pared: float = 0.08    # mm que el primer cordon de cada capa inclinada se mete en la pared del tronco
    elevar_z: float = 0.0           # mm extra de Z sobre las capas inclinadas (se ajusta si la punta roza la capa de abajo)
    borde: int = 4                  # anillos de borde (brim) en la primera capa
    aparcar: bool = True
    purga: float = 2.0              # mm de filamento que se purgan en el aparcamiento
    espera_s: float = 0.0           # pausa extra de enfriamiento antes de volver
    seco: bool = False              # sin extruir, sin calentar y 10 mm mas arriba: solo para ver el movimiento
    dz_seco: float = 10.0

    def paso(self, t: float) -> float:
        """Separacion entre lineas contiguas para un espesor ``t``: ancho - t*(1 - pi/4)."""
        return self.ancho - t * (1.0 - math.pi / 4.0)

    @property
    def paso_plano(self) -> float:
        """Separacion entre lineas contiguas de una capa plana: ancho - t*(1 - pi/4), asi la seccion (w-t)*t + pi*t^2/4 llena justo."""
        return self.ancho - self.capa_plana * (1.0 - math.pi / 4.0)

    @property
    def paso_incl(self) -> float:
        return self.ancho - self.capa_incl * (1.0 - math.pi / 4.0)


# ---------------------------------------------------------------------------------------------------------------------------
def alfa_minima(p: Proceso) -> float:
    """Inclinacion minima para que el piso de 90 grados quede apoyado al menos c_min: 90 - beta_c."""
    return 90.0 - beta_c_deg(p.capa_incl, p.ancho, p.c_min)


def contacto_de_la_punta(p: Proceso) -> float:
    """Fraccion del cordon de la punta apoyada en la capa de abajo: c = 1 - (t/w) tan(90 - alfa)."""
    return 1.0 - (p.capa_incl / p.ancho) * math.tan(math.radians(90.0 - p.alfa))


def validar(arbol: Arbol, p: Proceso, cono_deg: Optional[float] = None, maquina: Optional[dict] = None) -> List[str]:
    """Lanza ValueError si algun parametro es invalido; devuelve la lista de AVISOS (cosas que se cumplen pero con poco margen)."""
    m = maquina or MAQUINA_XQ1
    avisos: List[str] = []
    if cono_deg is None:
        cono_deg = load_head_profile().cone_alpha_deg()
    if not 0.8 * p.boquilla <= p.ancho <= 1.4 * p.boquilla:
        raise ValueError("el ancho de linea debe estar entre 0,8 y 1,4 veces la boquilla")
    if not 0.05 <= p.capa_incl <= p.capa_plana <= p.ancho:
        raise ValueError("se exige 0,05 <= capa inclinada <= capa plana <= ancho de linea")
    if not 0.05 <= p.capa_zona <= p.capa_plana:
        raise ValueError("la capa del tronco dentro de la rama debe estar entre 0,05 y la capa plana")
    if p.capa_plana > 0.75 * p.boquilla:
        raise ValueError("la capa plana no debe pasar del 75 % de la boquilla")
    if not 0.0 < p.c_min < 1.0:
        raise ValueError("c_min debe estar entre 0 y 1")
    if not 150 <= p.t_boq <= m["boquilla_max"] or not 150 <= p.t_voladizo <= p.t_boq:
        raise ValueError("temperaturas: 150 <= voladizo <= boquilla <= %d" % m["boquilla_max"])
    if not 0 <= p.t_cama <= m["cama_max"]:
        raise ValueError("la cama debe estar entre 0 y %d" % m["cama_max"])
    for nombre, v in (("ventilador", p.ventilador), ("ventilador_voladizo", p.ventilador_voladizo)):
        if not 0 <= v <= 255:
            raise ValueError("%s entre 0 y 255" % nombre)
    if not 0 < p.flujo <= 1.2:
        raise ValueError("flujo entre 0 y 1,2")
    if p.v_incl <= 0 or p.v_incl * math.sin(math.radians(p.alfa)) > m["vmax_z"]:
        raise ValueError("la velocidad inclinada pide mas velocidad de Z que el maximo del firmware (%.0f mm/s)" % m["vmax_z"])
    a_min, a_top = alfa_minima(p), cono_deg - HOLGURA_ALFA
    if a_min > a_top:
        raise ValueError("no hay ventana de inclinacion para un voladizo de 90 grados: con capa %.2f y ancho %.2f hace falta alfa >= %.1f grados "
                         "(apoyo minimo %.2f) y el cabezal solo permite %.1f (cono %.1f menos %.1f de margen). Baja --capa-incl o sube --ancho"
                         % (p.capa_incl, p.ancho, a_min, p.c_min, a_top, cono_deg, HOLGURA_ALFA))
    if not a_min - 1e-9 <= p.alfa <= a_top + 1e-9:
        raise ValueError("--alfa %.1f fuera de la ventana [%.1f, %.1f] grados (apoyo minimo %.2f abajo; cono del cabezal arriba)" % (p.alfa, a_min, a_top, p.c_min))
    if a_top - a_min < 1.5:
        avisos.append("la ventana de inclinacion es de solo %.1f grados (%.1f a %.1f): cualquier error de c_min o del cono la cierra" % (a_top - a_min, a_min, a_top))
    if p.capa_incl < 0.25 * p.boquilla:
        avisos.append("la capa inclinada (%.2f mm) es el %.0f %% de la boquilla (se recomienda >= 25 %%): es la que da el angulo, pero puede dar poco caudal o roce" % (p.capa_incl, 100 * p.capa_incl / p.boquilla))
    if not arbol.ramas:
        raise ValueError("el arbol necesita al menos una rama")
    if arbol.ancho_total() > 50.0 + 1e-9:
        avisos.append("el ancho total del arbol es %.1f mm (> 50 mm)" % arbol.ancho_total())
    cx, cy = m["cama"][0] / 2.0, m["cama"][1] / 2.0
    for r in arbol.ramas:
        if r.lado not in (-1, 1):
            raise ValueError("lado de rama debe ser +1 o -1")
        if min(r.largo, r.alto, r.ancho) <= 0 or r.ancho > arbol.tronco + 1e-9:
            raise ValueError("rama con cotas invalidas (el ancho en Y no puede pasar del tronco)")
        if r.largo < 2.0 or r.alto < 2.0:
            raise ValueError("la rama debe medir al menos 2 mm de largo y de alto")
        if r.z_piso < 2.0 * p.capa_plana + 3.0:
            raise ValueError("el piso de la rama debe estar al menos %.1f mm sobre la cama" % (2.0 * p.capa_plana + 3.0))
        if r.z_piso + r.alto + 1.0 > arbol.altura:
            raise ValueError("la rama (piso %.1f + alto %.1f) no cabe en la altura del tronco (%.1f)" % (r.z_piso, r.alto, arbol.altura))
        if r.z_piso + r.alto + 0.5 > arbol.altura:
            raise ValueError("la rama (piso %.1f + alto %.1f) no cabe en la altura del tronco (%.1f)" % (r.z_piso, r.alto, arbol.altura))
        if cx - arbol.tronco / 2 - r.largo < m["margen"] or cx + arbol.tronco / 2 + r.largo > m["cama"][0] - m["margen"]:
            raise ValueError("la rama se sale de la cama")
    pisos = sorted((r.z_piso, r.alto) for r in arbol.ramas)
    for (z0, h0), (z1, _) in zip(pisos, pisos[1:]):
        if z1 < z0 + h0 + 1.0:
            raise ValueError("dos ramas se pisan: separa los pisos al menos alto + 1 mm")
    if arbol.altura + (p.dz_seco if p.seco else 0.0) + 10.0 > m["z_max"]:
        raise ValueError("el arbol no cabe en Z")
    return avisos


# ---------------------------------------------------------------------------------------------------------------------------
def espiral(a: float, b: float, s: float, w: float) -> List[List[Tuple[float, float]]]:
    """Polilineas (p, q) que rellenan [0,a] x [0,b] (lugar de los CENTROS de cordon): anillos rectangulares concentricos cada ``s``, de
    fuera hacia dentro. Cada anillo es cerrado y empieza por la linea q (el lado p = 0); un enlace diagonal lleva al siguiente.
    Cuando ya no cabe otro anillo, el canal que queda entre las lineas opuestas se cierra con lineas paralelas al eje largo,
    repartidas a ~``s``; si el rectangulo es una tira (< s), una sola linea por su eje largo."""
    out: List[List[Tuple[float, float]]] = []
    lo = 0.0
    while True:
        ep, eq = a - 2 * lo, b - 2 * lo
        if ep < -EPS or eq < -EPS:
            break
        e_min = min(ep, eq)
        if e_min < s:
            if max(ep, eq) >= LINEA_MIN or not out:
                if ep >= eq:
                    out.append([(lo, lo + eq / 2), (lo + ep, lo + eq / 2)])
                else:
                    out.append([(lo + ep / 2, lo), (lo + ep / 2, lo + eq)])
            break
        ph, qh = a - lo, b - lo
        out.append([(lo, lo), (lo, qh), (ph, qh), (ph, lo), (lo, lo)])
        if e_min - 2 * s >= s:
            lo += s                         # cabe otro anillo completo
            continue
        n = int(round(e_min / s)) - 1       # lineas que reparten el canal que queda entre las dos lineas opuestas del anillo
        if n > 0 and e_min - w > 0.25 * w:
            for k in range(1, n + 1):
                f = e_min * k / (n + 1)
                if ep >= eq and ep - w > 0.05:
                    linea = [(lo + w / 2.0, lo + f), (ph - w / 2.0, lo + f)]
                elif eq > ep and eq - w > 0.05:
                    linea = [(lo + f, lo + w / 2.0), (lo + f, qh - w / 2.0)]
                else:
                    break
                out.append(linea if k % 2 else list(reversed(linea)))
        break
    return out


def largo_polilineas(polis: Sequence[Sequence[Tuple[float, float, float]]]) -> float:
    total = 0.0
    for poli in polis:
        for u, v in zip(poli, poli[1:]):
            total += math.dist(u, v)
    return total


# ---------------------------------------------------------------------------------------------------------------------------
class GeometriaRama:
    """Capas inclinadas de una rama en su marco local: x' hacia afuera desde la pared del tronco (x' = 0), z' sobre el piso (z' = 0).

    Plano de la capa k (centro): x' sen(alfa) + z' cos(alfa) = (k + 1/2) t.  Un cordon es un estadio de ancho w y espesor t a lo largo del
    plano; sus extremos redondos sobresalen del centro ext en x' y dz en z'. Los CENTROS deben quedar: z' >= dz (piso), z' <= alto - dz
    (techo), x' >= -traslape + ext (pared) y x' <= largo - ext (extremo de la rama).
    """

    def __init__(self, rama: Rama, p: Proceso):
        self.r, self.p = rama, p
        self.a = math.radians(p.alfa)
        self.sa, self.ca = math.sin(self.a), math.cos(self.a)
        t, w = p.capa_incl, p.ancho
        self.ext = ((w - t) / 2.0) * self.ca + t / 2.0
        self.dz = ((w - t) / 2.0) * self.sa + t / 2.0
        self.V = rama.ancho - w                                   # lugar de los centros en Y

    def z_en(self, k: int, x: float) -> float:
        return ((k + 0.5) * self.p.capa_incl - x * self.sa) / self.ca

    def capa(self, k: int) -> Optional[dict]:
        r, p = self.r, self.p
        phi = (k + 0.5) * p.capa_incl
        x_hi_piso = (phi - self.dz * self.ca) / self.sa
        x_lo_techo = (phi - (r.alto - self.dz) * self.ca) / self.sa
        x_lo = max(-p.traslape_pared + self.ext, x_lo_techo)
        x_hi = min(r.largo - self.ext, x_hi_piso)
        if x_hi < x_lo - EPS:
            return None
        return {"k": k, "x_lo": x_lo, "x_hi": x_hi, "U": (x_hi - x_lo) / self.ca, "z_lo": self.z_en(k, x_lo), "z_hi": self.z_en(k, x_hi)}

    def capas(self) -> List[dict]:
        kmax = int(math.ceil((self.r.largo * self.sa + self.r.alto * self.ca) / self.p.capa_incl)) + 2
        out = [c for c in (self.capa(k) for k in range(kmax)) if c is not None]
        return out

    def punto(self, c: dict, pq: Tuple[float, float]) -> Tuple[float, float, float]:
        """(p, q) de la capa -> (x', y relativo al tronco, z') del CENTRO del cordon."""
        pp, qq = pq
        return (c["x_hi"] - pp * self.ca, qq - self.V / 2.0, c["z_hi"] + pp * self.sa)


# ---------------------------------------------------------------------------------------------------------------------------
class Escritor:
    """Escribe el G-code con E absoluto (M82) y lleva la posicion, el filamento y la altura maxima impresa."""

    def __init__(self, p: Proceso, maquina: dict):
        self.p, self.m = p, maquina
        self.lineas: List[str] = []
        self.x = self.y = self.z = None
        self.e = 0.0
        self.retraido = False
        self.area_fil = math.pi * (p.filamento / 2.0) ** 2
        self.z_tope = 0.0
        self.dz = p.dz_seco if p.seco else 0.0
        self.e_total = 0.0                 # filamento realmente extruido (sin contar retracciones)
        self.long_extruida = 0.0
        self.tiempo = 0.0                  # estimacion tosca: longitud / velocidad
        self.acc_actual = None
        self.ventilador_actual = None

    # --- bajo nivel
    def raw(self, s: str) -> None:
        self.lineas.append(s)

    def c(self, s: str) -> None:
        self.lineas.append(";" + s)

    def _g1(self, x=None, y=None, z=None, e=None, f=None) -> None:
        w = ["G1"]
        if x is not None:
            w.append("X%.3f" % x)
        if y is not None:
            w.append("Y%.3f" % y)
        if z is not None:
            w.append("Z%.3f" % (z + self.dz))
        if e is not None and not self.p.seco:
            w.append("E%.5f" % e)
        if f is not None:
            w.append("F%d" % round(f * 60))
        self.lineas.append(" ".join(w))
        if x is not None:
            self.x = x
        if y is not None:
            self.y = y
        if z is not None:
            self.z = z

    # --- ordenes de maquina que en seco no se escriben
    def calentar(self, t: int, esperar: bool = False, enfriar: bool = False) -> None:
        if self.p.seco:
            self.c("(seco) M104 S%d" % t)
            return
        self.raw("M104 S%d" % t)
        if esperar:
            self.raw(("M109 R%d" if enfriar else "M109 S%d") % t)

    def ventilador(self, s: int) -> None:
        if self.ventilador_actual == s:
            return
        self.ventilador_actual = s
        self.raw(("; (seco) M106 S%d" if self.p.seco else "M106 S%d") % s)

    def aceleracion(self, a: int) -> None:
        if self.acc_actual != a:
            self.acc_actual = a
            self.raw("M204 P%d T%d" % (a, a))

    # --- movimientos
    def retraer(self) -> None:
        if not self.retraido:
            self.retraido = True
            if not self.p.seco:
                self.e -= self.p.retr
                self._g1(e=self.e, f=self.p.v_retr)

    def cebar(self) -> None:
        if self.retraido:
            self.retraido = False
            if not self.p.seco:
                self.e += self.p.retr
                self._g1(e=self.e, f=self.p.v_retr)

    def purgar(self, mm: float, v: float = 4.0) -> None:
        """Extrusion lenta en el sitio (mm de FILAMENTO a v mm/s): limpia lo que supuro la boquilla al cambiar de temperatura."""
        if self.p.seco or mm <= 0:
            return
        self.e += mm
        self._g1(e=self.e, f=v)
        self.tiempo += mm / v

    def mover(self, pt, f: float) -> None:
        d = math.dist((self.x, self.y, self.z), pt)
        self.tiempo += d / f
        self._g1(pt[0], pt[1], pt[2], f=f)

    def extruir(self, pt, v: float, t: float, w: float) -> None:
        longitud = math.dist((self.x, self.y, self.z), pt)
        de = longitud * filament_per_mm(w, t, self.p.filamento, self.p.flujo)
        self.e += de
        self.e_total += de
        self.long_extruida += longitud
        self.tiempo += longitud / v
        self._g1(pt[0], pt[1], pt[2], e=self.e, f=v)
        self.z_tope = max(self.z_tope, pt[2] + 0.15)

    def viaje(self, pt, z_extra: float = 0.0) -> None:
        """Retrae, sube por encima de TODO lo impreso, cruza y baja encima del destino."""
        self.retraer()
        zs = max(self.z_tope + self.p.salto_z + z_extra, pt[2] + self.p.salto_z, self.z if self.z is not None else 0.0)
        if self.z is None or self.z < zs - 1e-9:
            self._g1(z=zs, f=10.0)
            self.tiempo += abs(zs - (self.z or 0.0)) / 10.0
        self.mover((pt[0], pt[1], zs), self.p.v_viaje)
        self._g1(z=pt[2], f=10.0)
        self.tiempo += (zs - pt[2]) / 10.0
        self.cebar()

    def ir_a(self, pt, v: float, t: float, w: float) -> None:
        """Une con el punto actual: enlace corto extruido, o viaje con retraccion."""
        if self.x is None:
            self._g1(pt[0], pt[1], None, f=self.p.v_viaje)
            self._g1(z=pt[2], f=10.0)
            return
        if not self.retraido and math.hypot(pt[0] - self.x, pt[1] - self.y) < 0.05 and abs(pt[2] - self.z) <= 1.0:
            self.tiempo += abs(pt[2] - self.z) / 10.0
            self._g1(z=pt[2], f=10.0)               # cambio de capa en el sitio: solo Z, sin extruir
        elif math.dist((self.x, self.y, self.z), pt) <= LINK_MAX and not self.retraido:
            self.extruir(pt, v, t, w)
        else:
            self.viaje(pt)

    def imprimir(self, polis: Sequence[Sequence[Tuple[float, float, float]]], v: float, t: float, w: float) -> None:
        for poli in polis:
            self.ir_a(poli[0], v, t, w)
            for pt in poli[1:]:
                self.extruir(pt, v, t, w)

    def aparcar(self, destino_t: int, ventilador: int, enfriar: bool, motivo: str) -> None:
        """Aparta el cabezal del modelo, cambia temperatura y ventilador, espera y purga un poco."""
        p = self.p
        self.c("--- APARCAR: %s" % motivo)
        px = self.m["cama"][0] / 2.0 + 70.0
        py = self.m["cama"][1] / 2.0
        self.retraer()
        zs = self.z_tope + 3.0
        self._g1(z=zs, f=10.0)
        self.mover((px, py, zs), p.v_viaje)
        self.ventilador(ventilador)
        self.calentar(destino_t, esperar=True, enfriar=enfriar)
        if p.espera_s > 0 and enfriar and not p.seco:
            self.raw("G4 S%d" % round(p.espera_s))
        if p.purga > 0 and not p.seco:
            self.cebar()
            self.purgar(p.purga)
            self.retraer()
        self.c("--- fin del aparcamiento")


# ---------------------------------------------------------------------------------------------------------------------------
class Constructor:
    def __init__(self, arbol: Arbol, p: Proceso, maquina: Optional[dict] = None, cono_deg: Optional[float] = None):
        self.m = maquina or MAQUINA_XQ1
        self.arbol, self.p = arbol, p
        self.cono = load_head_profile().cone_alpha_deg() if cono_deg is None else cono_deg
        self.avisos = validar(arbol, p, self.cono, self.m)
        self.cx, self.cy = self.m["cama"][0] / 2.0, self.m["cama"][1] / 2.0
        self.t_plano = p.capa_plana                        # espesor nominal de las capas planas (la ultima se ajusta a la altura)
        self.w = Escritor(p, self.m)
        self.T = 0.0                                       # altura de la ultima capa plana impresa
        self.n_plana = 0
        self.stats = {"capas_planas": 0, "capas_inclinadas": 0, "aparcamientos": 0, "ramas": []}
        self.zona_activa = False
        self.cebado_mm = 0.0

    # --- capas planas del tronco
    def capa_plana(self, brim: int = 0, t: Optional[float] = None) -> None:
        """Una capa plana del tronco. ``t`` = espesor; sin el, el nominal ajustado para que las capas que faltan lleguen justo a la altura."""
        p, w = self.p, self.w
        if t is None:
            m_cap = max(1, math.ceil((self.arbol.altura - self.T) / self.t_plano - 1e-9))
            t = (self.arbol.altura - self.T) / m_cap
        t = min(t, self.arbol.altura - self.T)
        self.n_plana += 1
        i = self.n_plana
        self.T += t
        z = self.T
        s = p.paso(t)
        lado = self.arbol.tronco - p.ancho + 2 * brim * s
        polis2 = espiral(lado, lado, s, p.ancho)
        # el anillo exterior empieza en una esquina: las capas pares van de dentro afuera y acaban donde empieza la siguiente
        if i % 2 == 0:
            polis2 = [list(reversed(pl)) for pl in reversed(polis2)]
        x0, y0 = self.cx - lado / 2.0, self.cy - lado / 2.0
        polis = [[(x0 + pp, y0 + qq, z) for pp, qq in pl] for pl in polis2]
        if i == 1:
            v = p.v_primera
        else:
            v = max(p.v_min, min(p.v_plana, largo_polilineas(polis) / p.min_capa_s))
        w.c("CAPA PLANA %d z=%.3f" % (i, z))
        w.c("AITE plano t=%.4f w=%.4f" % (t, p.ancho))
        w.aceleracion(p.acc_plana)
        if i == 3 and not self.zona_activa:
            w.ventilador(p.ventilador)
        w.imprimir(polis, v, t, p.ancho)
        self.stats["capas_planas"] += 1

    def subir_tronco_hasta(self, altura: float, t: Optional[float] = None) -> None:
        while self.T < altura - 1e-6 and self.T < self.arbol.altura - 1e-6:
            self.capa_plana(t=t)

    def piso_ajustado(self, r: Rama) -> float:
        """El piso se ajusta a una altura que el tronco alcanza con capas completas, para que quede a ras de una capa."""
        n = max(0, round((r.z_piso - self.T) / self.t_plano))
        return self.T + n * self.t_plano

    # --- ramas
    def rama(self, r: Rama) -> None:
        p, w = self.p, self.w
        g = GeometriaRama(r, p)
        z_b = self.piso_ajustado(r)
        capas = g.capas()
        lado = r.lado
        pared = self.arbol.tronco / 2.0
        for _ in range(int(round((z_b - self.T) / self.t_plano))):
            self.capa_plana(t=self.t_plano)
        if p.aparcar:
            w.aparcar(p.t_voladizo, p.ventilador_voladizo, True, "baja a %d C y ventilador al maximo antes de la rama (piso z=%.2f)" % (p.t_voladizo, z_b))
            self.stats["aparcamientos"] += 1
        else:
            w.calentar(p.t_voladizo, esperar=True, enfriar=True)
            w.ventilador(p.ventilador_voladizo)
        self.zona_activa = True
        w.c("=== RAMA lado %+d: piso z=%.3f, largo %.1f, alto %.1f, ancho %.1f, alfa %.2f, capas inclinadas %d ===" % (lado, z_b, r.largo, r.alto, r.ancho, p.alfa, len(capas)))
        t, s = p.capa_incl, p.paso_incl
        for n, c in enumerate(capas):
            z_pared = z_b + c["z_lo"]                   # la pared del tronco debe llegar al menos al centro del cordon que la toca
            while self.T < z_pared - 1e-9 and self.T < self.arbol.altura - 1e-6:
                self.capa_plana(t=p.capa_zona)          # dentro de la rama las capas del tronco son finas: la pared no pasa de la punta
            polis2 = espiral(c["U"], g.V, s, p.ancho)
            # sentido: se elige el que arranca mas cerca de donde esta la boquilla

            def a3(pq, c=c):
                xl, yl, zl = g.punto(c, pq)
                return (self.cx + lado * (pared + xl), self.cy + yl, z_b + zl + t / 2.0 / g.ca + p.elevar_z)
            fuera_dentro = [[a3(q) for q in pl] for pl in polis2]
            dentro_fuera = [list(reversed(pl)) for pl in reversed(fuera_dentro)]
            cur = (w.x, w.y, w.z)
            if math.dist(cur, dentro_fuera[0][0]) < math.dist(cur, fuera_dentro[0][0]):
                fuera_dentro = dentro_fuera
            if n % 20 == 0:
                w.c("capa inclinada %d/%d (punta en x'=%.2f)" % (n + 1, len(capas), c["x_hi"]))
            w.c("AITE inclinada alfa=%.3f lado=%d t=%.4f w=%.4f dz=%.4f" % (p.alfa, lado, t, p.ancho, p.elevar_z))
            w.aceleracion(p.acc_incl)
            w.imprimir(fuera_dentro, p.v_incl, t, p.ancho)
            self.stats["capas_inclinadas"] += 1
        self.zona_activa = False
        self.stats["ramas"].append({"lado": lado, "z_piso": z_b, "capas": len(capas), "largo": r.largo, "alto": r.alto, "ancho": r.ancho})
        if p.aparcar:
            w.aparcar(p.t_boq, p.ventilador, False, "fin de la rama: vuelve a %d C y al ventilador normal" % p.t_boq)
            self.stats["aparcamientos"] += 1
        else:
            w.calentar(p.t_boq, esperar=True)
            w.ventilador(p.ventilador)

    # --- programa completo
    def construir(self) -> str:
        p, w, m = self.p, self.w, self.m
        ramas = sorted(self.arbol.ramas, key=lambda r: r.z_piso)
        self._cabecera(ramas)
        self._inicio()
        # primera capa con borde
        self.capa_plana(brim=p.borde)
        for r in ramas:
            self.rama(r)
        self.subir_tronco_hasta(self.arbol.altura)
        self._fin()
        return "\n".join(w.lineas) + "\n"

    def _cabecera(self, ramas: List[Rama]) -> None:
        p, a, w = self.p, self.arbol, self.w
        c = w.c
        c("ARQUIS AITE v%d - arbol de prueba con ramas en voladizo de 90 grados%s" % (VERSION, " (EN SECO: sin extruir, sin calentar, +%.0f mm de altura)" % p.dz_seco if p.seco else ""))
        c("Maquina: %s (cama %dx%d). Material: %s. Boquilla %.2f, ancho de linea %.2f, capa plana %.2f (%.2f dentro de las ramas), capa inclinada %.2f." % (self.m["nombre"], self.m["cama"][0], self.m["cama"][1], p.material, p.boquilla, p.ancho, self.t_plano, p.capa_zona, p.capa_incl))
        c("Temperaturas: boquilla %d C (en las ramas %d C), cama %d C. Ventilador %d (en las ramas %d), de 0 a 255." % (p.t_boq, p.t_voladizo, p.t_cama, p.ventilador, p.ventilador_voladizo))
        c("Tronco %.1f x %.1f x %.1f mm; %d rama(s) de %s mm; ancho total %.1f mm." % (a.tronco, a.tronco, a.altura, len(ramas), "/".join("%.0f" % r.largo for r in ramas), a.ancho_total()))
        c("Inclinacion de las capas de rama: %.1f grados (ventana %.1f a %.1f; cono del cabezal %.1f)." % (p.alfa, alfa_minima(p), self.cono - HOLGURA_ALFA, self.cono))
        c("Apoyo teorico de la punta sobre la capa de abajo: %.0f %% (SUPUESTO c_min = %.2f; se mide con este mismo ensayo)." % (100 * contacto_de_la_punta(p), p.c_min))
        for av in self.avisos:
            c("AVISO: " + av)
        c("NO PROBADO EN LA MAQUINA. Vigila las primeras capas de la primera rama y ten a mano el boton de parada.")
        if p.seco:
            c("Version EN SECO: el cabezal hace todo el recorrido sin extruir, 10 mm mas arriba. Sirve para ver el movimiento y el tiempo.")

    def _inicio(self) -> None:
        p, w, m = self.p, self.w, self.m
        w.raw("G21")
        w.raw("G90")
        w.raw("M82")
        w.raw("G92 E0")
        if p.seco:
            w.c("(seco) sin M140/M104/M190/M109")
        else:
            w.raw("M140 S%d" % p.t_cama)
            w.raw("M104 S%d" % max(150, p.t_boq - 50))
            w.raw("M190 S%d" % p.t_cama)
        w.raw("G28")
        w.raw("G1 Z5.000 F600")
        if not p.seco:
            w.raw("M109 S%d" % p.t_boq)
        w.raw("M220 S100")
        w.raw("M221 S100")
        w.raw("M107")
        w.ventilador_actual = 0
        w.aceleracion(p.acc_plana)
        # linea de cebado en el borde delantero, lejos del arbol
        y = self.cy - self.arbol.tronco / 2.0 - p.borde * p.paso_plano - 15.0
        w.c("linea de cebado")
        w.raw("G1 X%.3f Y%.3f F%d" % (self.cx - 40.0, y, round(p.v_viaje * 60)))
        w.x, w.y, w.z = self.cx - 40.0, y, 5.0
        w._g1(z=0.3, f=10.0)
        if not p.seco:
            w.extruir((self.cx + 40.0, y, 0.3), 30.0, 0.3, p.ancho * 1.2)
            self.cebado_mm = w.e_total              # filamento de la linea de cebado (no es del arbol)
            w.e_total = 0.0
            w.long_extruida = 0.0
        else:
            w._g1(x=self.cx + 40.0, y=y, f=30.0)
        w.retraer()
        w._g1(z=2.0, f=10.0)
        w.z_tope = 0.0

    def _fin(self) -> None:
        p, w, m = self.p, self.w, self.m
        w.c("FIN")
        w.retraer()
        w._g1(z=min(w.z_tope + 10.0, m["z_max"] - 1.0 - w.dz), f=10.0)
        w._g1(x=m["margen"] + 5.0, y=m["cama"][1] - m["margen"] - 5.0, f=150.0)
        if not p.seco:
            w.raw("M104 S0")
            w.raw("M140 S0")
            w.raw("M107")
        w.raw("M84 X Y E")
        w.raw("M501")


# ---------------------------------------------------------------------------------------------------------------------------
def generar(arbol: Optional[Arbol] = None, proceso: Optional[Proceso] = None, maquina: Optional[dict] = None, cono_deg: Optional[float] = None) -> Tuple[str, Constructor]:
    """Devuelve (texto del G-code, constructor con estadisticas). Lanza ValueError con un mensaje claro si algo no es valido."""
    c = Constructor(arbol or Arbol(), proceso or Proceso(), maquina, cono_deg)
    return c.construir(), c


def probeta_rama(proceso: Optional[Proceso] = None, largo: float = 20.0, alto: float = 8.0, piso: float = 6.0) -> Arbol:
    """Probeta corta de una sola rama (se imprime en menos tiempo que el arbol completo): es la que conviene probar primero."""
    return Arbol(tronco=10.0, altura=piso + alto + 1.5, ramas=[Rama(+1, piso, largo, alto)])


def con(proceso: Proceso, **kw) -> Proceso:
    return replace(proceso, **kw)
