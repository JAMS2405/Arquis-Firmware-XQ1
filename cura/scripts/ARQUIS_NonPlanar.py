# SPDX-License-Identifier: LGPL-3.0-or-later
# ARQUIS Ondulado no planar - script de post-procesado para Cura 5.11 (EXPERIMENTAL, apagado por defecto, sin pruebas fisicas).
# Se copia a la carpeta "scripts" de Cura junto con la carpeta plugins/ARQUISPlugin. Toda la logica vive en el plugin
# (engines/nonplanar_warp.py); este archivo es solo el adaptador, como ARQUIS_M486.py.
#
# Barreras (las mismas que la linea de comandos): 1) apagado por defecto; 2) la PRIMERA exportacion NO aplica nada: escribe la vista previa
# (~/arquis_nonplanar_vista.svg) y muestra un CODIGO; 3) solo cuando escribes ese codigo en el ajuste y exportas de nuevo se aplica el ondulado;
# 4) no hay errores y el cabezal esta confirmado. Cualquier fallo devuelve el G-code ORIGINAL sin tocar.
# Orden en Cura: ponlo DESPUES de "ARQUIS M486" (el manifiesto y las marcas ya estan puestos).
import os
import sys

from ..Script import Script


def _load_engine():
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
    arquis_bootstrap.load_engines()
    import arquis_engines.nonplanar_warp as nw
    return nw, path


def _notify(text: str, level: str) -> None:
    try:
        from UM.Message import Message
        mt = {"error": Message.MessageType.ERROR, "warning": Message.MessageType.WARNING}.get(level, Message.MessageType.POSITIVE)
        Message(text, title="ARQUIS Ondulado no planar", message_type=mt, lifetime=0).show()
    except Exception:
        pass


class ARQUIS_NonPlanar(Script):
    def getSettingDataString(self):
        return """{
            "name": "ARQUIS Ondulado no planar (EXPERIMENTAL)",
            "key": "ARQUIS_NonPlanar",
            "metadata": {},
            "version": 2,
            "settings": {
                "warp_on": {
                    "label": "Activar el ondulado (experimental)",
                    "description": "Apagado por defecto. Deforma las capas con una onda para crear volumen con capas no planas. EXPERIMENTAL: sin pruebas fisicas. La primera exportacion NO lo aplica: genera una vista previa y un codigo que debes escribir abajo.",
                    "type": "bool",
                    "default_value": false
                },
                "amplitude": {
                    "label": "Amplitud pico a pico (mm)",
                    "description": "Altura total de la onda (de cresta a valle). Empieza con 1 mm. El maximo duro es 4 mm.",
                    "type": "float",
                    "unit": "mm",
                    "default_value": 1.0,
                    "minimum_value": "0.1",
                    "maximum_value": "4",
                    "enabled": "warp_on"
                },
                "wavelength": {
                    "label": "Longitud de onda (mm)",
                    "description": "Distancia entre crestas en X e Y. Mas corta = mas pendiente (el motor rechaza pendientes que el cono del cabezal no tolera).",
                    "type": "float",
                    "unit": "mm",
                    "default_value": 20.0,
                    "minimum_value": "4",
                    "enabled": "warp_on"
                },
                "function": {
                    "label": "Funcion de onda",
                    "description": "wave (dos senos en X e Y), radial (anillos) o una expresion propia de x, y, r, A, L (por ejemplo A/2*sin(2*pi*x/L)). Solo se admiten numeros, esas variables y funciones matematicas.",
                    "type": "str",
                    "default_value": "wave",
                    "enabled": "warp_on"
                },
                "z_in": {
                    "label": "Altura donde empieza la onda (mm)",
                    "description": "Por debajo de esta altura las capas quedan planas (adherencia a la cama intacta). Debe estar por encima de la primera capa.",
                    "type": "float",
                    "unit": "mm",
                    "default_value": 0.8,
                    "minimum_value": "0.3",
                    "enabled": "warp_on"
                },
                "ramp": {
                    "label": "Rampa de transicion (mm)",
                    "description": "Altura en la que la onda crece de cero a su amplitud completa. Una rampa corta hace las capas muy desiguales: si el motor la rechaza, sube este valor o baja la amplitud.",
                    "type": "float",
                    "unit": "mm",
                    "default_value": 5.0,
                    "minimum_value": "0.5",
                    "enabled": "warp_on"
                },
                "flat_top": {
                    "label": "Cara superior plana",
                    "description": "Si esta activado, la onda vuelve a cero en las ultimas capas y la cara superior queda plana. Si no, la cara superior tambien ondula.",
                    "type": "bool",
                    "default_value": true,
                    "enabled": "warp_on"
                },
                "accept_code": {
                    "label": "Codigo de vista previa aceptado",
                    "description": "Pega aqui el codigo de 12 caracteres que muestra Cura tras la primera exportacion, DESPUES de revisar la vista previa (~/arquis_nonplanar_vista.svg). Sin el codigo correcto no se aplica nada. Cambia si cambia la pieza o cualquier ajuste.",
                    "type": "str",
                    "default_value": "",
                    "enabled": "warp_on"
                }
            }
        }"""

    def execute(self, data):
        try:
            if not bool(self.getSettingValueByKey("warp_on")):
                return data                                   # apagado: no se toca nada, ni siquiera se carga el motor
            nw, plugin_dir = _load_engine()
            settings = {k: self.getSettingValueByKey(k) for k in ("warp_on", "amplitude", "wavelength", "function", "z_in", "ramp", "flat_top", "accept_code")}
            out = nw.apply_for_cura("".join(data), settings, os.path.join(plugin_dir, "nonplanar_head.json"), os.path.expanduser("~"))
        except Exception as e:                                # nunca romper la exportacion: el G-code original queda intacto
            _notify("Ondulado NO aplicado (el G-code se exporta sin cambios): %s" % e, "error")
            return data
        if out.message:
            _notify(out.message, out.level)
        return [out.text] if out.applied else data
