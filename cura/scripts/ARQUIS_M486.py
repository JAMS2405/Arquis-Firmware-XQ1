# ARQUIS M486 - script de post-procesado para Cura 5.11 (se copia a la carpeta "scripts" de Cura; ver docs/slicer_suite/installation.md).
# Toda la logica vive en el plugin ARQUISPlugin (motores sin dependencia de Cura); este archivo solo es el adaptador.
import os
import sys

from ..Script import Script


def _load_arquis():
    """Devuelve (m486_engine, scene_adapter). Lanza RuntimeError con un mensaje claro si el plugin no esta instalado."""
    path = os.environ.get("ARQUIS_PLUGIN_PATH")             # solo para pruebas
    if not path:
        try:
            from UM.PluginRegistry import PluginRegistry
            path = PluginRegistry.getInstance().getPluginPath("ARQUISPlugin")
        except Exception:
            path = None
    if not path or not os.path.isdir(path):
        raise RuntimeError("El plugin ARQUISPlugin no esta instalado: copia plugins/ARQUISPlugin a la carpeta de plugins de Cura y reinicia Cura.")
    if path not in sys.path:
        sys.path.insert(0, path)
    import arquis_bootstrap
    arquis_bootstrap.load_adapters()
    import arquis_engines.m486_engine as eng
    import arquis_adapters.cura_scene_adapter as scene
    return eng, scene


def _notify(text: str, error: bool) -> None:
    try:
        from UM.Message import Message
        Message(text, title="ARQUIS M486", message_type=Message.MessageType.ERROR if error else Message.MessageType.WARNING, lifetime=0).show()
    except Exception:
        pass


class ARQUIS_M486(Script):
    def getSettingDataString(self):
        return """{
            "name": "ARQUIS M486 - Cancelar objeto",
            "key": "ARQUIS_M486",
            "metadata": {},
            "version": 2,
            "settings": {
                "capacity": {
                    "label": "Capacidad de cancelacion del firmware",
                    "description": "Numero maximo de objetos cancelables del firmware (indices M486 S0..S<n-1>). ARQUIS Firmware con la ampliacion: 64. Marlin sin modificar: 32. Los objetos que sobren se imprimen pero no se podran cancelar.",
                    "type": "int",
                    "default_value": 64,
                    "minimum_value": 1,
                    "maximum_value": 128
                },
                "tolerance": {
                    "label": "Tolerancia de posicion (mm)",
                    "description": "Holgura al comprobar que cada tramo del G-code cae dentro de su objeto de la escena. Subela solo si el script informa de tramos fuera de su objeto y has verificado que la escena es correcta.",
                    "type": "float",
                    "unit": "mm",
                    "default_value": 1.0,
                    "minimum_value": "0",
                    "maximum_value_warning": "3"
                },
                "id_order": {
                    "label": "Numeracion de objetos",
                    "description": "Escena: el orden de la lista de objetos de Cura. Posicion: de la esquina frontal izquierda hacia atras, fila a fila.",
                    "type": "enum",
                    "options": {"scene": "Orden de la escena", "position": "Por posicion en la cama"},
                    "default_value": "scene"
                }
            }
        }"""

    def execute(self, data):
        try:
            eng, scene = _load_arquis()
            instances = scene.capture_scene()
            opt = eng.M486Options(capacity=int(self.getSettingValueByKey("capacity")),
                                  tolerance_mm=float(self.getSettingValueByKey("tolerance")),
                                  id_order=str(self.getSettingValueByKey("id_order")))
            res = eng.process_gcode(data, instances, opt)
        except Exception as e:                               # nunca romper la exportacion: el G-code original queda intacto
            _notify("No se pudo preparar la cancelacion de objetos (el G-code se exporta sin cambios): %s" % e, True)
            return data
        if res.status == "blocked":
            _notify("Cancelacion de objetos BLOQUEADA (el G-code se exporta sin M486):\n" +
                    "\n".join(str(p) for p in res.problems if p.severity == "error"), True)
        elif res.status == "ok":
            warns = [str(p) for p in res.problems if p.severity == "warning"]
            if warns:
                _notify("Cancelacion de objetos lista con avisos:\n" + "\n".join(warns), False)
        return res.chunks
