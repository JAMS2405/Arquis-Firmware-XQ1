# G-code REAL de CuraEngine 5.11.0 (aportado por el propietario)

Origen: dos archivos exportados por Cura 5.11.0 (`Creality CR-10 Max`, brim, soporte en arbol, capa 0,24 mm, 25 capas), uno con **3 modelos distintos** y otro con **4 copias del mismo STL**.
Los `*_reducido.gcode.gz` conservan, **sin modificar ninguna linea**, la cabecera, el G-code de inicio, las capas 0, 1, 2 y la ultima (24) y el G-code final; se quitaron las capas intermedias para que el repositorio no pese. Los archivos completos (2 MB cada uno) se usan, si estan disponibles, con la variable `ARQUIS_REAL_GCODE_DIR` (ver `test_real_gcode.py`).

Lo que se aprendio de ellos (y esta cubierto por pruebas):
- Cura 5.11 renombra las copias en la escena: `pinon.stl`, `pinon.stl(1)`, `(2)`, `(3)` (`ObjectsModel._renameNodes`). La verificacion geometrica sigue siendo necesaria (se prueba renombrando las copias a un mismo nombre).
- Cada capa empieza con contexto implicito NONMESH (brim/soporte) y `;MESH:NONMESH` solo al final de la capa (viaje + Z a la siguiente capa).
- Las retracciones van **dentro** del tramo del objeto, y como `G0 X.. Y.. E<retraido>` (viaje con filamento). Sin partirlas, cancelar un objeto dejaba 0,8 mm de filamento de mas por retraccion.
- El G-code final del usuario (`G1 Z5`, `G1 X0 Y310`, `M84`) va tras el ultimo `;TIME_ELAPSED:` sin marcador.

# Captura REAL de la escena en Cura 5.11.0 (aportada por el propietario, 2026-11-19)

`real_cura_5_11_captura_6_pinones.json`: volcado del menu *Extensiones -> ARQUIS -> Diagnostico* ejecutado **dentro de Cura 5.11.0** (`source: "cura"`, plugin 0.2.0, Python 3.12.2, Windows) con 6 copias del mismo STL en una cama de 300 x 300 mm. No contiene nodos falsos. Lo usa `test_cura_validation_tools.py::test_real_cura_capture_*`.
El G-code exportado de esa escena (7,9 MB, manifiesto de 6 objetos) no se incluye por tamano; con `ARQUIS_REAL_GCODE_DIR` apuntando a el, `cura/tools/verify_cura_capture.py <captura> <gcode>` da `VALIDADO EN CURA REAL` (la orientacion fisica es independiente).

