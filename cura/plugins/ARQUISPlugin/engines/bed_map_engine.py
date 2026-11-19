"""Mapa proporcional de la cama: coordenadas del manifiesto -> pixeles, tamano y forma de los marcadores, seleccion y estados.

Aritmetica ENTERA (unidades de 0,1 mm y pixeles) para que el firmware (C++) pueda reproducir cada resultado bit a bit: los
vectores de ``shared/object_protocol/test_vectors/bed_map_v1.json`` los generan estas funciones y los ejecutaran tambien las
pruebas del firmware (Etapa 3). Este mismo codigo dibuja la vista previa del gestor de objetos en Cura (``ui/``).

Orientacion (regla unica, no se invierte ningun eje por intuicion): X crece hacia la DERECHA; Y crece hacia ATRAS, es decir hacia la
PARTE SUPERIOR de la pantalla; el frente de la cama queda abajo. Es la vista de quien esta de pie delante de la impresora SI el
origen del G-code (X0 Y0) es la esquina frontal izquierda (``ORG=FL`` del manifiesto). Que eso sea cierto en la maquina real lo
decide la prueba de orientacion fisica (docs/slicer_suite/05_etapa2_manifiesto.md); el firmware puede reflejar ejes cambiando
``ORG`` sin cambiar el protocolo.
"""
from dataclasses import dataclass
from typing import List, Optional, Sequence, Tuple

MIN_MARKER_PX = 22            # tamano minimo de un marcador (legible y tocable)
VIEW_PAD_PX = 8               # margen del mapa

# Estados (valores del protocolo de interfaz; el firmware usa los mismos)
PENDING, PRINTING, DONE, CANCELED, UNAVAILABLE = 0, 1, 2, 3, 4
STATE_TEXT = {PENDING: "Pendiente", PRINTING: "Imprimiendo", DONE: "Completado", CANCELED: "Cancelado", UNAVAILABLE: "No disponible"}
# Insignia ASCII dentro/junto al marcador: el estado nunca depende solo del color
STATE_BADGE = {PENDING: "", PRINTING: ">", DONE: "+", CANCELED: "x", UNAVAILABLE: "?"}


def div_round(a: int, b: int) -> int:
    """Division entera redondeada al mas cercano (mitades hacia +inf) con b > 0: floor((2a + b) / (2b)). Igual en C++ con division por suelo."""
    assert b > 0
    return (2 * a + b) // (2 * b)


@dataclass(frozen=True)
class Bed:
    min_x: int       # 0,1 mm (coordenadas de G-code)
    min_y: int
    width: int
    depth: int


@dataclass(frozen=True)
class View:
    x: int           # rectangulo del mapa en pantalla (px), origen arriba-izquierda
    y: int
    w: int
    h: int
    zoom: int = 1                                  # 1, 2 o 4
    center_x: Optional[int] = None                 # centro de la vista en coordenadas de cama (0,1 mm); None = centro de la cama
    center_y: Optional[int] = None


@dataclass(frozen=True)
class Marker:
    id: int
    x0: int          # rectangulo en pantalla, incluyendo ambos extremos
    y0: int
    x1: int
    y1: int
    shape: str       # "C" | "S" | "R"
    cx: int          # centro en pixeles
    cy: int

    @property
    def w(self) -> int:
        return self.x1 - self.x0 + 1

    @property
    def h(self) -> int:
        return self.y1 - self.y0 + 1

    def contains(self, px: int, py: int) -> bool:
        return self.x0 <= px <= self.x1 and self.y0 <= py <= self.y1


def _scale(bed: Bed, view: View) -> Tuple[int, int]:
    usable = min(view.w, view.h) - 2 * VIEW_PAD_PX
    assert usable > 0 and view.zoom in (1, 2, 4)
    return usable * view.zoom, max(bed.width, bed.depth)          # (numerador, denominador): px por 0,1 mm = num/den


def _center(bed: Bed, view: View) -> Tuple[int, int]:
    cx = view.center_x if view.center_x is not None else bed.min_x + bed.width // 2
    cy = view.center_y if view.center_y is not None else bed.min_y + bed.depth // 2
    return cx, cy


def to_px(x: int, y: int, bed: Bed, view: View) -> Tuple[int, int]:
    """Punto de la cama (0,1 mm) -> pixel de pantalla. X a la derecha, Y hacia ARRIBA en pantalla (atras)."""
    num, den = _scale(bed, view)
    cx, cy = _center(bed, view)
    return (view.x + view.w // 2 + div_round(num * (x - cx), den),
            view.y + view.h // 2 - div_round(num * (y - cy), den))


def to_bed(px: int, py: int, bed: Bed, view: View) -> Tuple[int, int]:
    """Inversa aproximada de ``to_px`` (para localizar un toque en la cama)."""
    num, den = _scale(bed, view)
    cx, cy = _center(bed, view)
    return (cx + div_round(den * (px - view.x - view.w // 2), num), cy - div_round(den * (py - view.y - view.h // 2), num))


def marker(obj_id: int, cx: int, cy: int, w: int, d: int, shape: str, bed: Bed, view: View) -> Marker:
    """Marcador de un objeto: su CENTRO real (nunca se desplaza) y un tamano proporcional con minimo legible.

    Los objetos pequenos se agrandan hasta ``MIN_MARKER_PX`` alrededor de su centro; los alargados son rectangulos; el circulo usa
    el lado mayor para que su caja sea cuadrada.
    """
    num, den = _scale(bed, view)
    pcx, pcy = to_px(cx, cy, bed, view)
    pw = max(MIN_MARKER_PX, div_round(num * w, den))
    ph = max(MIN_MARKER_PX, div_round(num * d, den))
    if shape == "C":
        pw = ph = max(pw, ph)
    x0, y0 = pcx - pw // 2, pcy - ph // 2
    return Marker(obj_id, x0, y0, x0 + pw - 1, y0 + ph - 1, shape, pcx, pcy)


def pick(markers: Sequence[Marker], px: int, py: int) -> int:
    """Marcador bajo un toque: gana el de MENOR area (permite elegir una pieza pequena que queda dentro de otra mayor); empate: id mayor.
    -1 si no hay ninguno. Los marcadores solapados se pueden seleccionar siempre desde la lista."""
    best, best_key = -1, None
    for m in markers:
        if m.contains(px, py):
            key = (m.w * m.h, -m.id)
            if best_key is None or key < best_key:
                best, best_key = m.id, key
    return best


def overlaps(markers: Sequence[Marker]) -> List[Tuple[int, int]]:
    """Pares de marcadores que se tocan en pantalla (la interfaz avisa de que conviene seleccionar desde la lista o ampliar)."""
    out = []
    for i, a in enumerate(markers):
        for b in markers[i + 1:]:
            if a.x0 <= b.x1 and b.x0 <= a.x1 and a.y0 <= b.y1 and b.y0 <= a.y1:
                out.append((a.id, b.id))
    return out


def display_id(obj_id: int) -> int:
    """Numero que se ve en el marcador y en la lista: id + 1 (el indice de M486 es el id; la pantalla empieza en 1)."""
    return obj_id + 1


def object_state(*, available: bool, trusted: bool, canceled: bool, seq: bool, z0: int, ztop: int, order: int,
                 z_print: int, active_order: int) -> int:
    """Estado visual de un objeto.

    available: el id es cancelable (< capacidad) · trusted: el manifiesto esta verificado · canceled: esta en la mascara de cancelados ·
    seq: impresion de un objeto cada vez · z_print: Z (0,1 mm) de la ULTIMA EXTRUSION (no la del salto en Z) · active_order: orden de
    impresion del objeto activo (-1 si aun no hay).
    """
    if not available or not trusted:
        return UNAVAILABLE
    if canceled:
        return CANCELED
    if seq:
        if active_order < 0 or order > active_order:
            return PENDING
        return PRINTING if order == active_order else DONE
    if z_print < z0:
        return PENDING
    return DONE if z_print > ztop else PRINTING
