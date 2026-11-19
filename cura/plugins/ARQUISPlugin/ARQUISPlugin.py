# SPDX-License-Identifier: LGPL-3.0-or-later
"""Extension de Cura: menu ARQUIS > Gestor de objetos (lista + mapa de la cama, sincronizados) y carga de los motores para los scripts de
post-procesado (``scripts/ARQUIS_M486.py``, ``scripts/ARQUIS_Validation.py``). Toda la logica esta en ``engines/`` y ``adapters/``."""
import os

from PyQt6.QtCore import QObject

from UM.Extension import Extension
from UM.Logger import Logger
from UM.PluginRegistry import PluginRegistry

from . import arquis_bootstrap


class ARQUISPlugin(QObject, Extension):
    def __init__(self) -> None:
        super().__init__()
        arquis_bootstrap.load_adapters()          # registra arquis_engines / arquis_adapters (tambien para los scripts)
        from .ui.backend import ObjectManagerBackend
        self._backend = ObjectManagerBackend()
        self._view = None
        self.setMenuName("ARQUIS")
        self.addMenuItem("Gestor de objetos...", self.showManager)
        self.addMenuItem("Diagn\u00f3stico: guardar captura de la escena...", self.saveDiagnostics)
        Logger.log("i", "ARQUIS Slicer Suite 0.2.0 cargado")

    def showManager(self) -> None:
        from cura.CuraApplication import CuraApplication
        if self._view is None:
            path = os.path.join(str(PluginRegistry.getInstance().getPluginPath("ARQUISPlugin")), "ui", "ObjectManager.qml")
            self._view = CuraApplication.getInstance().createQmlComponent(path, {"manager": self._backend})
        if self._view is None:
            Logger.log("e", "No se pudo crear la ventana del Gestor de objetos de ARQUIS")
            return
        self._backend.refreshFromCura()
        self._view.show()

    def saveDiagnostics(self) -> None:
        """Guarda ~/arquis_cura_captura.json con lo que el plugin ve en la escena REAL de Cura (para cura/tools/verify_cura_capture.py)."""
        from UM.Message import Message
        try:
            import arquis_adapters.cura_diagnostics as diag
            dump = diag.capture_dump()
            path = diag.save_dump(dump)
            Message("Captura guardada en:\n%s\n\nNodos: %d - instancias imprimibles: %d - errores de la API de Cura: %d.\nVerifica el archivo con cura/tools/verify_cura_capture.py"
                    % (path, len(dump["nodes"]), len(dump["instances"]), len(dump["errors"]) + sum(len(n["errors"]) for n in dump["nodes"])),
                    title="ARQUIS - diagnostico", lifetime=0).show()
        except Exception as e:                                           # noqa: BLE001
            Logger.logException("e", "ARQUIS: diagnostico fallido")
            Message("No se pudo guardar la captura: %s" % e, title="ARQUIS - diagnostico", message_type=Message.MessageType.ERROR, lifetime=0).show()
