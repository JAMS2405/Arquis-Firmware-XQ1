"""Modelo de instancias de la escena y asignacion de IDs ARQUIS.

Una *hoja* (``SceneInstance``) es una malla de la escena de Cura (lo que CuraEngine etiqueta con ``;MESH:<nombre>``).
Un *objeto* ARQUIS (``ArquisObject``) es lo que el usuario cancela: una malla suelta o un grupo de Cura (varias hojas).
Las coordenadas estan SIEMPRE en el sistema del G-code (mm, origen = el de G1), no en el de la escena de Cura.
"""
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

    def __post_init__(self):
        if not (self.x0 <= self.x1 and self.y0 <= self.y1):
            raise ValueError("caja invertida para %r: %r" % (self.name, (self.x0, self.y0, self.x1, self.y1)))

    @property
    def bbox(self) -> Tuple[float, float, float, float]:
        return (self.x0, self.y0, self.x1, self.y1)


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
        inst = per_name.get(name, 0)
        per_name[name] = inst + 1
        cancelable = n < capacity
        objects.append(ArquisObject(id=n if cancelable else -1, name=name, instance=inst, leaves=ls_sorted,
                                    cancelable=cancelable, scene_order=n))
    return objects
