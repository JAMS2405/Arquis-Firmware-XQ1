"""Extension de Cura. Etapa 1: solo expone los motores y la carpeta del plugin a los scripts de post-procesado
(``scripts/ARQUIS_M486.py`` y ``scripts/ARQUIS_Validation.py``). La interfaz (gestor de objetos, vista de cama) llega en la Etapa 2."""
from UM.Extension import Extension
from UM.Logger import Logger

from . import arquis_bootstrap


class ARQUISPlugin(Extension):
    def __init__(self) -> None:
        super().__init__()
        arquis_bootstrap.load_adapters()          # registra arquis_engines / arquis_adapters para los scripts
        Logger.log("i", "ARQUIS Slicer Suite 0.1.0 cargado")
