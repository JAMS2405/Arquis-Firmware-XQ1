# SPDX-License-Identifier: LGPL-3.0-or-later
# ARQUIS Validation - comprueba el G-code ya procesado (ultimo paso del post-procesado). No modifica el G-code.
import os
import sys

from ..Script import Script


def _load_engines():
    path = os.environ.get("ARQUIS_PLUGIN_PATH")
    if not path:
        try:
            from UM.PluginRegistry import PluginRegistry
            path = PluginRegistry.getInstance().getPluginPath("ARQUISPlugin")
        except Exception:
            path = None
    if not path or not os.path.isdir(path):
        raise RuntimeError("El plugin ARQUISPlugin no esta instalado")
    if path not in sys.path:
        sys.path.insert(0, path)
    import arquis_bootstrap
    arquis_bootstrap.load_engines()
    import arquis_engines.validation_engine as val
    return val


class ARQUIS_Validation(Script):
    def getSettingDataString(self):
        return """{
            "name": "ARQUIS Validacion del G-code",
            "key": "ARQUIS_Validation",
            "metadata": {},
            "version": 2,
            "settings": {
                "capacity": {
                    "label": "Capacidad de cancelacion del firmware",
                    "description": "Debe coincidir con la del script ARQUIS M486.",
                    "type": "int", "default_value": 30, "minimum_value": 1, "maximum_value": 32
                }
            }
        }"""

    def execute(self, data):
        try:
            val = _load_engines()
            problems = val.validate_m486(data, capacity=int(self.getSettingValueByKey("capacity")))
        except Exception as e:
            problems = None
            err = str(e)
        try:
            from UM.Message import Message
            if problems is None:
                Message("No se pudo validar el G-code: %s" % err, title="ARQUIS Validacion", message_type=Message.MessageType.WARNING, lifetime=0).show()
            elif any(p.severity == "error" for p in problems):
                Message("El G-code NO supera la validacion de cancelacion de objetos:\n" + "\n".join(str(p) for p in problems if p.severity == "error"),
                        title="ARQUIS Validacion", message_type=Message.MessageType.ERROR, lifetime=0).show()
        except Exception:
            pass
        return data
