"""Modelo de instancias de la escena y asignacion de IDs ARQUIS.

Una *hoja* (``SceneInstance``) es una malla de la escena de Cura (lo que CuraEngine etiqueta con ``;MESH:<nombre>``).
Un *objeto* ARQUIS (``ArquisObject``) es lo que el usuario cancela: una malla suelta o un grupo de Cura (varias hojas).
Las coordenadas estan SIEMPRE en el sistema del G-code (mm, origen = el de G1), no en el de la escena de Cura.
"""
import math
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple


@dataclass(frozen=True)
class SceneInstance:
    scene_index: int                 # posicion en el recorrido de la escena (orden estable mientras la escena no cambie)
    name: str                        # nombre del nodo = el que CuraEngine escribe en ;MESH:
    x0: float
    y0: float
    x1: float
    y1: float
    group: Optional[int] = None      # scene_index del grupo padre (None = malla suelta)
    group_name: Optional[str] = None
    shape: str = "R"                 # forma de la huella para el marcador: "C" circulo, "S" cuadrado, "R" rectangulo
    rot: int = 0                     # giro alrededor de la vertical, grados 0..359 (solo informativo)
    z1: float = 0.0                  # altura superior del objeto segun la escena (mm); 0 = desconocida

    def __post_init__(self):
        if not (self.x0 <= self.x1 and self.y0 <= self.y1):
            raise ValueError("caja invertida para %r: %r" % (self.name, (self.x0, self.y0, self.x1, self.y1)))

    @property
    def bbox(self) -> Tuple[float, float, float, float]:
        return (self.x0, self.y0, self.x1, self.y1)


_EXTENSIONS = ("stl", "obj", "3mf", "amf", "ply", "x3d", "gcode", "ufp", "off")


def split_name(name: str) -> Tuple[str, Optional[int]]:
    """Nombre de Cura -> (nombre base, sufijo de copia o None). 'pinon.stl(2)' -> ('pinon', 2); 'Soporte#3' -> ('Soporte', 3).

    Cura renombra las copias de forma que se distingan en su lista de objetos (``ObjectsModel._renameNodes``: ``nombre(n)``
    para mallas y ``nombre#n`` para grupos); el nombre BASE es lo que se muestra y se guarda una sola vez en el manifiesto.
    """
    base, suffix = name.strip(), None
    m = re.match(r"^(.*)\((\d+)\)$", base) or re.match(r"^(.*)#(\d+)$", base)
    if m:
        base, suffix = m.group(1), int(m.group(2))
    low = base.lower()
    for ext in _EXTENSIONS:
        if low.endswith("." + ext):
            base = base[:-(len(ext) + 1)]
            break
    return (base.strip() or name.strip() or "objeto"), suffix


def classify_shape(points: Sequence[Tuple[float, float]]) -> str:
    """Forma de referencia de un poligono convexo (puntos de la huella): 'C' circulo, 'S' cuadrado, 'R' rectangulo.

    Es una referencia VISUAL para el marcador, no una reproduccion de la pieza. Circulo: el contorno es casi equidistante del centro
    y la caja es casi cuadrada. Cuadrado: caja casi cuadrada (lados dentro del 12 %). Resto: rectangulo (incluye las alargadas).
    """
    pts = [(float(x), float(y)) for x, y in points]
    if len(pts) < 3:
        return "R"
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    w, d = max(xs) - min(xs), max(ys) - min(ys)
    if w <= 0 or d <= 0:
        return "R"
    square = abs(w - d) <= 0.12 * max(w, d)
    cx, cy = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2
    rs = [math.hypot(x - cx, y - cy) for x, y in pts]
    mean = sum(rs) / len(rs)
    var = math.sqrt(sum((r - mean) ** 2 for r in rs) / len(rs)) / mean if mean > 0 else 1.0
    # area / caja: circulo ~ 0,785, rectangulo ~ 1
    area = abs(sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))) / 2
    fill = area / (w * d)
    if square and var < 0.06 and 0.70 <= fill <= 0.86:
        return "C"
    return "S" if square else "R"


@dataclass
class ArquisObject:
    id: int                          # indice que se usara en M486 S<id> (o -1 si no es cancelable)
    name: str
    instance: int                    # 0..k entre los objetos con el mismo nombre (Llavero_01, Llavero_02...)
    leaves: Tuple[SceneInstance, ...]
    cancelable: bool = True          # False: fuera de la capacidad del firmware (id = -1)
    scene_order: int = 0             # id "natural" antes de aplicar el tope de capacidad

    @property
    def bbox(self) -> Tuple[float, float, float, float]:
        return (min(l.x0 for l in self.leaves), min(l.y0 for l in self.leaves),
                max(l.x1 for l in self.leaves), max(l.y1 for l in self.leaves))

    @property
    def base_name(self) -> str:
        return split_name(self.name)[0]

    @property
    def shape(self) -> str:
        """Un grupo se representa como rectangulo (su caja envolvente); una malla suelta usa la forma de su huella."""
        return self.leaves[0].shape if len(self.leaves) == 1 else "R"

    @property
    def rot(self) -> int:
        return self.leaves[0].rot if len(self.leaves) == 1 else 0


def build_objects(instances: Sequence[SceneInstance], capacity: int, id_order: str = "scene") -> List[ArquisObject]:
    """Agrupa hojas en objetos y asigna IDs 0..N-1 (contiguos y unicos). Los que superan ``capacity`` quedan con id -1.

    id_order: "scene" (orden de la escena; por defecto) o "position" (fila a fila desde el frente izquierdo).
    """
    if id_order not in ("scene", "position"):
        raise ValueError("id_order desconocido: %r" % (id_order,))
    seen_idx = set()
    for l in instances:
        if l.scene_index in seen_idx:
            raise ValueError("scene_index duplicado: %d" % l.scene_index)
        seen_idx.add(l.scene_index)

    groups: Dict[Tuple[str, int], List[SceneInstance]] = {}
    for l in instances:
        key = ("g", l.group) if l.group is not None else ("n", l.scene_index)
        groups.setdefault(key, []).append(l)

    ordered = sorted(groups.values(), key=lambda ls: min(l.scene_index for l in ls))
    if id_order == "position":
        def pos_key(ls):
            return (min(l.y0 for l in ls), min(l.x0 for l in ls), min(l.scene_index for l in ls))
        ordered = sorted(ordered, key=pos_key)

    objects: List[ArquisObject] = []
    per_name: Dict[str, int] = {}
    for n, ls in enumerate(ordered):
        ls_sorted = tuple(sorted(ls, key=lambda l: l.scene_index))
        name = ls_sorted[0].group_name if ls_sorted[0].group is not None and ls_sorted[0].group_name else ls_sorted[0].name
        base = split_name(name)[0]
        inst = per_name.get(base, 0)                 # 0..k entre los objetos de igual nombre base (Llavero, Llavero(1), ... -> 0, 1, ...)
        per_name[base] = inst + 1
        cancelable = n < capacity
        objects.append(ArquisObject(id=n if cancelable else -1, name=name, instance=inst, leaves=ls_sorted,
                                    cancelable=cancelable, scene_order=n))
    return objects
