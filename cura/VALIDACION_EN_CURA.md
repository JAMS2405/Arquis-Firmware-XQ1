# Validación del plugin ARQUIS dentro de Cura 5.11 real

**Estado (2026-11-19): validado en Cura 5.11 real para captura de escena, duplicados, posiciones y generación del manifiesto con una escena de 6 copias y cama 300 × 300** (ver el registro al final). Quedan sin ejecutar en Cura real el perfil de 450 × 450, rotaciones/grupos/modelos pequeños y la orientación física. Las pruebas automáticas de este repositorio usan **nodos falsos** (`SimpleNamespace`) o G-code procesado fuera de Cura, y por eso **no** pueden dar por válida la API. Esta guía y las herramientas `verify_cura_capture.py` / `check_cura_api_source.py` sirven para cerrar ese hueco con evidencia generada por Cura.

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

## Registro (resultados del propietario, 2026-11-19, Cura 5.11.0 real, Windows)

Evidencia: `cura/tests/data/real_cura_5_11_captura_6_pinones.json` (volcado hecho **dentro de Cura**, `source: "cura"`) y el G-code exportado de esa escena (`;ARQUIS_M486 V=1 N=6 CAP=30 STATUS=OK`, manifiesto de 6 objetos, cama 300 x 300). `verify_cura_capture.py <captura> <gcode>` y `arquis_check.py` dan **VALIDADO EN CURA REAL** y `M486 y sincronizacion con el G-code: CORRECTO` (ejecutados en el entorno de desarrollo sobre esos archivos).

| Comprobación | Resultado en Cura real |
|---|---|
| Instalación y carga del plugin | **OK**: el menú generó la captura con `arquis_plugin 0.2.0`. El script `ARQUIS_M486` carga y exporta (tras corregir la falta de `description` en `bed_x`/`bed_y`; ver `07_etapa3_firmware.md`) |
| Llamadas a la API sin errores | **OK** (`errors: []` en la captura y en los 20 nodos) |
| Captura de instancias de la escena | **OK**: 20 nodos, 6 mallas imprimibles, 6 instancias |
| Duplicados idénticos | **OK con 6 copias** (`pinon rusvel 66 dientes.stl`, `(1)`…`(5)`): cada una con su caja y su ID |
| Posiciones y dimensiones | **OK**: `x = X + ancho/2`, `y = −Z + fondo/2` coinciden con los números de Cura (±0,1 mm) |
| Mapa y lista sincronizados | **Parcial**: la ventana del gestor abre y muestra mapa y lista de 6 (captura de pantalla). El propietario no ha informado de pulsar marcador↔fila **dentro de Cura**; la comprobación automática se hizo sobre los datos capturados |
| Generación y validación del manifiesto | **OK**: manifiesto válido (CRC, 6 objetos, cama 300 × 300), igual a la escena capturada; el G-code real cae dentro de la caja de cada objeto |
| Perfil 450 × 450 bloquea mapa y manifiesto | **Sin ejecutar en Cura real** (solo con los G-code de muestra procesados fuera de Cura) |
| Orientación física (ORG=FL) | **Confirmada por el propietario (2026-11-19)** por observación de la impresión de 6 piezas; no se imprimió la prueba formal de 5 piezas |

Nota: la validación cubre la API y los datos que usa el plugin con una escena de 6 copias y un perfil `creality_cr10max` renombrado «Arquis XQ1» con cama 300 × 300. No cubre rotaciones, grupos, modelos muy pequeños ni otras versiones de Cura.
