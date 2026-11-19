# SPDX-License-Identifier: LGPL-3.0-or-later
"""Backend Qt del Gestor de objetos: expone a QML el modelo de vista (lista + mapa sincronizados). No conoce Cura: recibe instancias y cama."""
from typing import Optional, Sequence, Tuple

from PyQt6.QtCore import QObject, pyqtProperty, pyqtSignal, pyqtSlot

import arquis_engines.object_manager_model as omm
from arquis_engines import bed_map_engine as bm
from arquis_engines import protocol_limits as limits
from arquis_engines.object_engine import SceneInstance, build_objects


class ObjectManagerBackend(QObject):
    changed = pyqtSignal()

    def __init__(self, parent: Optional[QObject] = None) -> None:
        super().__init__(parent)
        self._instances: Sequence[SceneInstance] = ()
        self._bed: Tuple[float, float, float, float] = (0.0, 0.0, 300.0, 300.0)
        self._selected = -1
        self._states = None
        self._vm = omm.build_view_model([], self._bed)

    # --- datos -------------------------------------------------------------------------------------------------------------------
    def setData(self, instances: Sequence[SceneInstance], bed: Tuple[float, float, float, float], states=None) -> None:
        self._instances, self._bed, self._states = tuple(instances), tuple(bed), states
        self._rebuild()

    def _rebuild(self) -> None:
        objs = build_objects(self._instances, limits.DESIGN_CAPACITY)
        keys = {(o.id if o.cancelable else -(o.scene_order + 1)) for o in objs}
        if self._selected not in keys:
            self._selected = -1
        self._vm = omm.build_view_model(objs, self._bed, self._selected, states=self._states)
        self.changed.emit()

    @pyqtSlot()
    def refreshFromCura(self) -> None:
        """Lee la escena activa de Cura (solo dentro de Cura)."""
        import arquis_adapters.cura_scene_adapter as scene
        self.setData(scene.capture_scene(), scene.capture_bed())

    # --- seleccion (lista y mapa comparten UN indice) ---------------------------------------------------------------------------------
    @pyqtSlot(int)
    def select(self, key: int) -> None:
        if key != self._selected:
            self._selected = key
            self._rebuild()

    @pyqtSlot(int, int)
    def pickAt(self, px: int, py: int) -> None:
        k = omm.pick_id(self._vm, px, py)
        self.select(k if k is not None else -1)

    # --- propiedades para QML ---------------------------------------------------------------------------------------------------------
    @pyqtProperty("QVariantList", notify=changed)
    def markers(self):
        return self._vm["markers"]

    @pyqtProperty("QVariantList", notify=changed)
    def rows(self):
        return self._vm["rows"]

    @pyqtProperty("QVariantList", notify=changed)
    def grid(self):
        return self._vm["grid"]

    @pyqtProperty("QVariantMap", notify=changed)
    def bed(self):
        return self._vm["bed"]

    @pyqtProperty("QStringList", notify=changed)
    def warnings(self):
        return self._vm["warnings"]

    @pyqtProperty(bool, notify=changed)
    def mapValid(self):
        return self._vm["mapValid"]

    @pyqtProperty(str, notify=changed)
    def bedMismatch(self):
        return self._vm["bedMismatch"]

    @pyqtProperty(str, notify=changed)
    def summary(self):
        return self._vm["summary"]

    @pyqtProperty(int, notify=changed)
    def selectedKey(self):
        return self._selected

    @pyqtProperty(str, notify=changed)
    def selectedInfo(self):
        for r in self._vm["rows"]:
            if r["id"] == self._selected:
                return "%s  %s  -  %s  -  %s" % (r["label"], r["name"], ("sin posicion valida" if not self._vm["mapValid"] else "X %.1f  Y %.1f" % (r["x"], r["y"])), r["stateText"])
        return "Toca un objeto del mapa o de la lista"
