# Validación del plugin ARQUIS dentro de Cura 5.11 real

**Estado: NO VALIDADO en Cura real.** El plugin y los scripts nunca se han ejecutado dentro de Cura. Todas las pruebas automáticas de este repositorio usan **nodos falsos** (`SimpleNamespace`) o G-code procesado fuera de Cura, y por eso **no** pueden dar por válida la API. Esta guía y las herramientas `verify_cura_capture.py` / `check_cura_api_source.py` sirven para cerrar ese hueco con evidencia generada por Cura.

## Qué está hecho y qué no

| Evidencia | Qué demuestra | Qué NO demuestra |
|---|---|---|
| `tools/check_cura_api_source.py` (lectura del código fuente 5.11.0 de Cura y Uranium) | Los **33 símbolos** de la API que usa el plugin existen con ese nombre (`SceneNode.getBoundingBox`, `AxisAlignedBox.front/back`, `callDecoration`, `ConvexHullDecorator.getConvexHull`, `QtApplication.createQmlComponent`, `Script.execute`…) | Que se comporten como el plugin espera, ni que el plugin cargue |
| Pruebas con nodos falsos (`test_scene_adapter.py`, `test_cura_validation_tools.py`) | La lógica del adaptador y de la herramienta de verificación | **Nada** sobre Cura |
| QML cargado con Qt 6.12 real (`test_ui_qml.py`) | `ObjectManagerView.qml` carga sin avisos | `ObjectManager.qml` (necesita `UM.Dialog`), el menú, la extensión |
| **Esta guía, ejecutada por ti en Cura real** | La carga, la captura, los duplicados, las posiciones, la sincronización, el manifiesto | — |

Una cosa **no** la valida ni esta guía: que `x = X + ancho/2`, `y = −Z + fondo/2` corresponda a la orientación física de tu cama. Eso lo confirma **solo** la prueba de orientación impresa (paso 6). `ORG=FL` sigue siendo **provisional**.

## Pasos (≈ 20 min)

### 0. Preparación
1. En Cura, instala/selecciona una impresora con **cama 300 × 300 mm** (la del firmware). Si solo tienes el perfil CR-10 Max (450 × 450), el plugin lo detecta, **no genera manifiesto** y el gestor muestra un banner rojo: eso también es parte de la prueba (paso 5).
2. Copia la carpeta `cura/plugins/ARQUISPlugin` a la carpeta de plugins de Cura (Ayuda → Mostrar carpeta de configuración → `plugins`) y los scripts `cura/scripts/ARQUIS_M486.py` y `ARQUIS_Validation.py` a `scripts` (misma carpeta de configuración). Reinicia Cura.
3. Cura → Extensiones → Post-procesado → Añadir script → **ARQUIS M486**.

### 1. Instalación y carga del plugin
Debe aparecer el menú **Extensiones → ARQUIS** con «Gestor de objetos…» y «Diagnóstico: guardar captura de la escena…». Si no aparece: Ayuda → Mostrar carpeta de configuración → `cura.log`, y busca «ARQUIS». Guárdalo.

### 2. Escena de prueba (carga estos modelos)
a. 3 modelos distintos. b. **4 copias de un mismo STL** (Ctrl+C, Ctrl+V; Cura las renombra `nombre(1)`, `(2)`…). c. 1 modelo **girado** 30° con la herramienta de rotación. d. Dos modelos **agrupados** (Ctrl+G). e. Uno muy pequeño (≈ 6 mm).

### 3. Captura y diagnóstico
Extensiones → ARQUIS → **Diagnóstico: guardar captura de la escena…**. Cura muestra un mensaje con la ruta de `~/arquis_cura_captura.json` y cuántos errores de API hubo. Luego, en una terminal:
```bash
python3 cura/tools/verify_cura_capture.py ~/arquis_cura_captura.json
```
Cada línea sale como `OK`, `FALLA`, `NO EJERCITADO` o `PENDIENTE`. Con `source: cura` y sin FALLA, las comprobaciones de captura, duplicados, geometría y sincronización cuentan como validadas en Cura real.

### 4. Gestor de objetos (mapa y lista)
Extensiones → ARQUIS → **Gestor de objetos…**. Comprueba a ojo: (i) la ventana abre; (ii) hay un marcador y una fila por modelo, también para las 4 copias («pinon 1…4»); (iii) pulsar una fila resalta su marcador y pulsar un marcador resalta su fila; (iv) el marcador pequeño es tocable; (v) cada estado muestra texto e insignia. Haz una captura de pantalla.

### 5. Generación del manifiesto con el G-code exportado
Rebanar → Guardar en archivo (con el script ARQUIS M486 activo). Después:
```bash
python3 cura/tools/verify_cura_capture.py ~/arquis_cura_captura.json RUTA/exportado.gcode
python3 cura/tools/arquis_check.py RUTA/exportado.gcode
```
Debe indicar `STATUS=OK`, manifiesto válido con la cama de 300 × 300 y «G-code real vs manifiesto» sin errores. **Repite con el perfil de 450 × 450**: el script debe informar de que omite el manifiesto, `verify_cura_capture.py` debe dar `FALLA` en «Perfil de Cura = máquina ARQUIS» y la ventana del gestor debe mostrar el banner rojo y **ningún** marcador.

### 6. Orientación física (independiente de Cura)
```bash
python3 cura/tools/make_orientation_gcode.py --bed 300x300
```
Imprime `prueba_orientacion.gcode` (5 piezas) y anota en qué esquina de la cama quedó la pieza más pequeña (20 mm). Para `ORG=FL` debe quedar en la **esquina frontal izquierda** (origen X0 Y0 de la máquina). Dime el resultado: hasta entonces no se habilita la cancelación visual.

## Qué enviarme
`~/arquis_cura_captura.json`, la salida completa de `verify_cura_capture.py` (con y sin G-code, con el perfil de 300 y el de 450), el G-code exportado, la captura del gestor y las líneas «ARQUIS» de `cura.log`. Con eso se rellena la tabla siguiente.

## Registro (se rellena con tus resultados; hoy todo está sin ejecutar)

| Comprobación | Resultado en Cura real |
|---|---|
| Instalación y carga del plugin | **sin ejecutar** |
| Llamadas a la API sin errores | **sin ejecutar** |
| Captura de instancias de la escena | **sin ejecutar** |
| Duplicados idénticos (4 copias) | **sin ejecutar** |
| Posiciones y dimensiones | **sin ejecutar** |
| Mapa y lista sincronizados | **sin ejecutar** |
| Generación y validación del manifiesto | **sin ejecutar** |
| Perfil 450 × 450 bloquea mapa y manifiesto | **sin ejecutar** |
| Orientación física (ORG=FL) | **sin ejecutar** |
