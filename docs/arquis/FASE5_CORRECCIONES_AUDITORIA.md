# ARQUIS — Fase 5: correcciones P1–P5 de la auditoría del firmware único

**Estado:** compilado y probado **solo por software** (pruebas nativas y simuladores). **No está probado en ninguna impresora.** No es una versión definitiva.

## 1. Rama y commits
| | |
|---|---|
| Rama de corrección | `claude/pensive-cray-ib96kb` |
| Base | `fdff0cd` de `feature/arquis-firmware-unico-perfiles` (código = `758fe50`) |
| Código, pruebas y herramientas | `0bae5fc` |
| Documentación y BIN de pruebas | commit siguiente (`firmware_pruebas/fase5`) |
| `main` | sin cambios (`a98dd6a`) |

**Archivos de firmware modificados:** solo cinco, todos del firmware único.
- `Marlin/src/lcd/extui/mks_ui/arquis_machine_logic.h` / `.cpp`
- `Marlin/src/lcd/extui/mks_ui/arquis_machine.h` / `.cpp`
- `Marlin/src/lcd/extui/mks_ui/arquis_machine_ui.cpp`

**Sin cambios** respecto a `fdff0cd`:
- `stepper.cpp/.h`, `endstops.cpp/.h`, `planner.cpp/.h` y `stepper/trinamic.cpp`;
- `settings.cpp`, `gcode.cpp` y `temperature.cpp`;
- `M593.cpp` (Input Shaping), `feature/bedlevel` y `wifi_module.cpp`;
- `tft_lvgl_configuration.*` (`bmp_public_buf` de 14 336 B), `draw_ui.cpp` (Roboto 22);
- `Configuration*.h`, `ini/`;
- los menús ARQUIS (`arquis_tools.cpp`, `arquis_dashboard*`) y los módulos de malla, PID y rendimiento.

**Herramientas y pruebas modificadas o nuevas:**
- `tests/machines/run.sh`, `tests/machines/machine_logic_test.cpp`, `tests/machines/glue_test.cpp` (nuevo);
- `herramientas/arquis_ui/sim/machine_sim.cpp`, `build_machine_sim.sh`, `check_marlin_api.sh`;
- `herramientas/arquis_ui/stub_machine/**`, incluida la máquina simulada `arq_fake_hw.*` (nueva).

## 2. Correcciones

### P1 — Selección de modelo sin mezclar parámetros mecánicos
**Antes:** en el asistente, la mecánica se podía conservar en dos casos:
- si la detección fallaba (pasos calibrados);
- si el usuario elegía «Conservar» después de un aviso.

Así, un Aquila podía quedar con los pasos, aceleraciones y corrientes del XQ1.

**Ahora**, en la lógica pura (`arquis_machine_logic.cpp`):
- **`arq_mc_keep_check(elegido, EEPROM válida, modelo del registro, detectado)`** decide si se puede conservar la mecánica.
  - **Sí**, solo si:
    - los pasos Z/E coinciden con los de fábrica del modelo elegido; o
    - no coinciden con ninguno (están calibrados) y el registro ARQUIS confirma que se guardaron con ese modelo.
  - **No**, si los pasos son los de fábrica del otro modelo, aunque el registro diga lo contrario.
  - **No** en los demás casos: sin registro y sin detección, o EEPROM de Marlin no válida.
- **`arq_mc_choose(...)`** construye la copia de trabajo:
  - **Con «Conservar» confirmado:** la mecánica guardada.
  - **En cualquier otro caso:** *toda* la mecánica y la geometría de fábrica del modelo elegido (pasos, velocidades, aceleraciones, JD, corrientes, Input Shaping, medidas, finales y sentidos).
  - **M206:** solo se conserva si los datos están confirmados para ese modelo y el valor está dentro de rango; si no, 0.

**En la pantalla** (`arquis_machine_ui.cpp`):
- La opción se llama «Ajustes mecánicos».
- Si no se puede conservar, queda en «De fábrica». Al pulsarla aparece «No se puede conservar» con el motivo: datos de la otra máquina, datos sin confirmar o EEPROM no válida.
- El asistente parte de lo **guardado** (`arq_mc_read_saved`: geometría del registro y mecánica de Marlin), no de lo que unas pruebas hayan dejado en RAM. Antes, volver al paso 1 tras probar el otro modelo podía arrastrar sus medidas y sentidos.
- El aviso de «Cambiar de modelo» dice que se cargan los valores de fábrica del modelo nuevo y que no se mezclan.

**Se conservan siempre, por ser independientes de la mecánica del modelo:** PID, Linear Advance y el resto de la EEPROM de Marlin. Al cambiar de modelo conviene repetir el autoajuste PID (riesgo en la sección 5).

### P2 — Mensajes y advertencias
- **«Ajuste Z» desaparece:**
  - el ajuste fino en vivo se llama **Babystep (no se guarda)**;
  - los offsets se llaman **offsets M206**.
- **Malla:**
  - Las pruebas del asistente o del menú ya **no borran la malla**: con otras medidas solo se suspende la nivelación en RAM, y se reanuda al descartar.
  - La malla solo se borra al **guardar** medidas distintas de aquellas para las que se midió, y siempre con aviso previo: «Se borrará la malla 5x5 guardada: era de otras medidas de cama».
  - El aviso aparece al guardar en el asistente, en «Guardar cambios», en el diálogo de salida y en «Restaurar mecánica».
  - El resultado de «Restaurar mecánica» dice la verdad: «…la malla se borró (otras medidas)» o «Se conservan PID, malla y offsets M206».
- **«Restaurar todo»:** «Se pierden malla, PID, offsets M206, Linear Advance y Rendimiento».

### P3 — Ajustes → Máquina ↔ Otros parámetros (MKS)
- **Ida a «Otros parámetros (MKS)»:**
  - con cambios sin guardar, pide **Guardar o Descartar** antes de abrir MKS;
  - las pruebas aplicadas en RAM se deshacen siempre.
- **Descartar** usa `arq_mc_revert_ram()`: vuelve a medidas, finales, sentidos (bit `shaft`) y comprobaciones del registro.
  - **No hace `M501`**, así que no toca otros ajustes de Marlin que estuvieran en RAM (comportamiento original de Marlin).
  - Antes, descartar hacía `settings.load()`.
- **Vuelta de MKS:** se relee la copia de trabajo (`arq_machine_ui_draw`). Guardar después ya no sobrescribe lo que se cambió en MKS.
- **Salida de la pantalla por cualquier camino** (`arq_machine_ui_clear`): se desactiva el modo prueba y se deshacen las pruebas (si no hay movimiento ni órdenes en cola).
- **Entrada al menú:** también vuelve a lo guardado.
- **Garantía en el pegamento:** con medidas, finales o sentidos en RAM distintos del registro **no se mueve** (`ARQ_BLK_UNSAVED`). Lo mismo si falla la escritura del registro al guardar.

### P4 — Herramientas de prueba
- **`machine_sim.cpp`:**
  - ahora enlaza sin archivos externos: define `lv_font_roboto_22`, como los demás simuladores;
  - usa el **pegamento real** (`arquis_machine.cpp`) sobre una máquina simulada (`stub_machine/arq_fake_hw.*`: EEPROM, TMC2209, M500/M501/M502, G-code), en lugar de un pegamento falso;
  - comprueba los textos **realmente dibujados** (envuelve `lv_draw_label`);
  - las pantallas que crea el propio simulador ya no se acumulan en el heap.
- **`check_marlin_api.sh`:**
  - **falla** si falta el compilador o LVGL;
  - cuenta los archivos: deben compilar 32 de 32;
  - incluye `arquis_machine_logic.cpp`, `arquis_machine.cpp` y `arquis_machine_ui.cpp`;
  - antes devolvía 0 aunque no se ejecutara nada.
- **Nuevo `tests/machines/glue_test.cpp`** (ASan+UBSan): el pegamento real con fallos inyectados, dentro de `tests/machines/run.sh`.
- **Defecto anterior detectado, no corregido** (fuera del alcance): `build_sim.sh` (ARM) y los simuladores nativos de malla, PID y rendimiento comparten `sim/build/`. Si se ejecutan en ese orden, los nativos fallan al enlazar. Basta con borrar `sim/build/` entre ellos.

### P5 — Sin ventana de movimiento con dirección incorrecta en el movimiento manual
**Cómo detecta TMCStepper 0.8.10 la falta de respuesta** (`TMC2208Stepper::read`, `src/source/TMC2208Stepper.cpp`):
- **Intentos:** hace `max_retries = 2` intentos.
- **Cada intento:**
  - `delay(3)`;
  - envía 4 bytes;
  - `replyDelay = 2` ms;
  - busca el sincronismo y lee el cuerpo, con un plazo de `abort_window = 8` ms cada uno.
- **Si se agota un plazo,** `_sendDatagram()` devuelve 0.
- **Con la respuesta** se calcula el CRC: `if ((crc != (uint8_t)out) || crc == 0) { CRCerror = true; out = 0; }`.
- Sin respuesta, todo es 0 y su CRC es 0, así que también activa `CRCerror`.
- **Conclusión:** `CRCerror = true` significa **«sin respuesta válida»**. Puede ser:
  - un driver desconectado o sin alimentación;
  - ruido;
  - una trama corrupta;
  - una respuesta válida cuyo CRC sea 0 (1 de cada 256).
- **No significa necesariamente «desconectado»,** y el firmware lo trata así: «Sin respuesta válida de un driver TMC».
- **Duración:** con respuesta, ~13 ms; sin ella, ~34 ms.
- **Decisión:** el firmware ya no repite la lectura (antes hacía hasta 3 llamadas, unos 100 ms), salvo una segunda lectura para descartar un fallo puntual antes de mover.

**Cambios:**
- **Clasificación de `GCONF` en lógica pura** (`arq_mc_tmc_classify`), con los bits de `TMC2208_bitfields.h`: `shaft` = bit 3, `pdn_disable` = 6, `mstep_reg_select` = 7.
  - correcto;
  - sin respuesta válida;
  - reiniciado (valores de arranque, p. ej. 0x101);
  - `shaft` distinto.
- **Movimiento manual** (pantalla, USB o Wi-Fi; sin trabajo en curso y con el planificador vacío):
  - Antes de aceptar la orden, se lee `GCONF` **de los drivers que mueve la orden y cuyo sentido depende del bit `shaft`**:
    - `G0`/`G1`: los ejes que aparecen;
    - arcos: X e Y;
    - el resto: todos.
  - **Si no se confirma** (tras una segunda lectura):
    1. no se mueve;
    2. se recupera la configuración con `restore_trinamic_drivers()`;
    3. se verifican los 4 drivers;
    4. los ejes quedan sin origen;
    5. se pide **hacer home y repetir**, con un mensaje claro en pantalla y por serie.
  - Si el estado era «sin confirmar» (p. ej. una lectura anterior falló), el siguiente movimiento manual vuelve a leer y recuperar en ese momento. Antes había que esperar a la lectura periódica, hasta unos 4 s.
- **Homing (`G28`), `M24` y `M1000`:** verificación de los 4 drivers con recuperación, como antes.
- **Inicio de trabajos:** la **primera orden de movimiento de cada trabajo** también se verifica entera, aunque no haya `M24`: impresión iniciada por Wi-Fi o por un programa por USB.
- **Babystep (`M290`):**
  - sin imprimir, se verifica como cualquier movimiento manual;
  - imprimiendo, no espera: Z no está invertido en ningún modelo de fábrica.
- **Durante la impresión:**
  - Con drivers invertidos (Aquila) solo se vigilan esos (X, Y, E0), uno por segundo.
  - Si un driver aparece reiniciado o con `shaft` distinto (confirmado con una segunda lectura), parada de seguridad (`kill`). Un corte de los 24 V reinicia todos los drivers a la vez, así que se detecta en ≤ 1 s.
  - Un fallo de lectura **no** corta la impresión.
- **XQ1:** no tiene drivers invertidos. No se hace ninguna lectura antes de mover y no hay bloqueos nuevos.
- **Tras cada carga del registro** (arranque y `M501`) se verifican los 4 drivers en el siguiente ciclo. Antes, solo en el primer arranque.
- **Coste:** una lectura (~13 ms) por cada eje invertido de la orden, solo cuando la máquina está parada.

**No se ha tocado** `stepper.cpp`, `stepper.h`, `endstops.cpp`, el planificador ni la generación de pasos: no ha hecho falta.

## 3. Pruebas automáticas (simulaciones, no pruebas físicas)
| Suite | Antes (`fdff0cd`) | Después (`0bae5fc`) |
|---|---|---|
| `tests/machines` `test_profiles.py` | 51/51 | 51/51 |
| `tests/machines` lógica (ASan+UBSan) | 56/56 | **90/90** (P1: tabla de decisión, XQ1↔Aquila, E calibrado, Z modificado, detectado / no detectado, «Conservar», barrido de 32 combinaciones sin mezcla; P5: clasificación de `GCONF`) |
| `tests/machines` pegamento real (ASan+UBSan) | no existía | **69/69** (P5 con fallos inyectados, P3, P2, EEPROM, `M502`, `M9050 R`) |
| Simulador del asistente y del menú (LVGL real) | **no enlazaba** (`lv_font_roboto_22`) | **82/82**, 49 capturas, pico de heap 9 576 B / 16 384 (64 bits) |
| `check_marlin_api.sh` (ARM) | 29 archivos | **32/32** (incluye `arquis_machine*`) |
| Simulador de la interfaz (ARM + qemu) | 236 OK / 0 fallos | 236 OK / 0 fallos |
| Lógica de la interfaz (ASan) | 83/83 | 83/83 |
| Malla (`tests/mesh`) | 50 + 367 + 654 | 50 + 367 + 654 |
| PID (`tests/pid`) | 31 + 74 | 31 + 74 |
| Rendimiento (`tests/perf`) | 32 + 95 | 32 + 95 |
| Torres | 16 | 16 |
| Capacidad (`tests/capacity`) | sin errores | sin errores |
| Simuladores de malla, PID y rendimiento | OK | OK |

## 4. Compilación (PlatformIO, entorno `mks_robin_nano_v3_1_usb_flash_drive_msc`)
| | Flash | RAM | BIN |
|---|---:|---:|---:|
| Base `fdff0cd` (recompilada) | 421 204 B (88,6 %) | 77 752 B (59,3 %) | 421 736 B |
| **Corrección `0bae5fc`** | **423 828 B (89,2 %)** | **78 108 B (59,6 %)** | **424 360 B** |
| Diferencia | +2 624 B | +356 B | |

- **Margen:** quedan 51 308 B de Flash libres (por debajo del 90 %).
- **Avisos:** solo los dos que ya había en Marlin: `Warnings.cpp:738` (DIAG) y `wifi_module.cpp:829` (`diveDir`).
- **Dependencias exactas, reconstruidas archivo por archivo** (los ZIP de GitHub no son accesibles desde la máquina de compilación):
  - framework `rhapsodyv/Arduino_Core_STM32@39f37d6d6a`: lista de archivos y tamaños de Software Heritage, contenido de `raw.githubusercontent.com`;
  - LVGL-6.1.1-MKS `a3ebe98bc6`;
  - QRCode `261c5a696a`;
  - TMCStepper `v0.8.10`.
  - Los recursos de la microSD de MKS no entran en el BIN.
- **Validación de la reconstrucción:** recompilar `fdff0cd` da el mismo tamaño que el BIN de la Fase 4. Comparados byte a byte, **solo difieren 225 bytes**, que corresponden a:
  - 8 rutas de archivo del framework (identificador de la sesión de compilación);
  - la fecha y hora de compilación.

  Con esto queda verificado también que el BIN de `firmware_pruebas/fase4` corresponde al código de `758fe50`.
- **Los dos perfiles lógicos** (XQ1 y Aquila D1) están en el mismo BIN. Se han ejercitado con el pegamento real en `glue_test` y en el simulador.
- **BIN de pruebas:**
  - archivo: `firmware_pruebas/fase5/Robin_nano_v3.bin`;
  - SHA-256 `8995f6e43650a1dd01b76780adb2a243055fb5a8e1d7c6a6a8f068434623123e`;
  - compilado desde el `git archive` de `0bae5fc`.

## 5. Riesgos que siguen pendientes
1. **Nada está probado en una impresora.** UART real, I²C real, movimientos, finales, temperaturas, Wi-Fi, microSD y USB siguen pendientes.
2. **El Aquila depende de leer los TMC por UART.** Sin respuesta válida no se mueve: es el comportamiento buscado, pero hay que confirmar en la placa que la lectura funciona (prueba 20).
3. **Ventana residual durante la impresión del Aquila:**
   - si se reinician todos los drivers (corte de 24 V), ≤ 1 s más la lectura de confirmación antes del `kill`;
   - si se reinicia uno solo de X, Y o E0, hasta ~3 s.

   Eliminarla del todo exigiría leer `GCONF` continuamente o cambiar la línea DIR (`stepper.cpp`). En el movimiento manual la ventana queda reducida al tiempo entre la lectura y el movimiento (milisegundos).
4. **Nuevas lecturas UART** (~13 ms una vez por segundo durante la impresión del Aquila, y ~13 ms por eje invertido antes de cada movimiento manual) con `SoftwareSerial` a 19 200 baudios: confirmar en la placa que no producen defectos de impresión.
5. **El PID por defecto es común a las dos máquinas.** Tras instalar o cambiar de modelo, hacer el autoajuste PID.
6. **Instalación nueva con pasos calibrados y sin registro ARQUIS:** el modelo no se puede confirmar, así que se cargan los valores mecánicos de fábrica. Es lo que se pidió, pero el usuario tendrá que volver a meter sus pasos calibrados. PID y malla no se tocan.
7. Siguen vigentes los puntos de la Fase 4:
   - `M92`/`M203`/`M906` y las pantallas MKS no aplican los rangos de Máquina;
   - `G29 H` usa el centro compilado;
   - la Z del XQ1 (5 mm/s, 100 mm/s²) es provisional;
   - una impresión por Wi-Fi sin perfil calienta antes de abortarse.

## 6. Nuevo informe de auditoría (después de las correcciones)
| Área | Estado | Evidencia concreta | Riesgo | Acción necesaria |
|---|---|---|---|---|
| P1 — Sin mezcla de parámetros al cambiar de modelo | VERIFICADO | `arq_mc_keep_check` / `arq_mc_choose`. Lógica: 90/90, con un barrido de 32 combinaciones y 0 mezclas. Simulador con el pegamento real: XQ1→Aquila (E calibrado), Aquila→XQ1 (Z modificado), detectado, mismo modelo y primera instalación sin confirmar | Bajo | Prueba física 23 |
| P2 — Mensajes, malla y M206 | VERIFICADO | El texto dibujado se comprueba en el simulador; sin «Ajuste Z» en el BIN. La malla solo se borra al guardar medidas nuevas y con aviso (pegamento y simulador) | Bajo | Pruebas físicas 24 y 31 |
| P3 — Ida y vuelta con Otros parámetros (MKS) | VERIFICADO | Simulador: 5 secuencias (descartar, guardar, prueba aplicada, cambio de MKS sin M500, salida por fuera). Pegamento: `ARQ_BLK_UNSAVED` y descartar sin `M500`/`M501` | Bajo | Prueba física 32 |
| P4 — Herramientas sin falsos positivos | VERIFICADO | El simulador enlaza solo. `check_marlin_api.sh` da rc=2 sin compilador o LVGL, rc=1 si algo no compila, y 32/32 con el ARM de PlatformIO | Bajo | Ninguna |
| P5 — Lectura de `GCONF` antes de mover (manual) y recuperación | VERIFICADO (simulado) | `glue_test`: reinicio, `shaft` distinto, sin respuesta, fallo puntual, 24 V apagados y encendidos, Wi-Fi sin M24, babystep, XQ1 sin lecturas | Medio hasta probarlo en la placa | Pruebas físicas 20, 29 y 30 |
| P5 — Durante la impresión | PENDIENTE DE PRUEBA FÍSICA | `kill` en ≤ 3 s con driver invertido reiniciado (simulado). Un fallo de lectura no corta | Medio (ventana residual) | Prueba física 30 (en vacío) |
| Semántica de `CRCerror` en TMCStepper 0.8.10 | VERIFICADO | Código real de `TMC2208Stepper::read`/`_sendDatagram` (sección 2, P5) | Bajo | Ninguna |
| Integridad del BIN de la Fase 4 frente a `758fe50` | VERIFICADO | Recompilación byte a byte: solo 225 B distintos (rutas y fecha) | Ninguno | Ninguna |
| BIN de la Fase 5 frente a `0bae5fc` | VERIFICADO | Compilado desde el `git archive` de `0bae5fc` con las mismas dependencias; SHA-256 en `SHA256SUMS.txt` | Bajo | Ninguna |
| Archivos protegidos | VERIFICADO | `git diff fdff0cd 0bae5fc`: solo cambian 5 archivos `arquis_machine*` | Ninguno | Ninguna |
| Menús ARQUIS | VERIFICADO | `arquis_tools.cpp`, `arquis_dashboard*`, `draw_ui.cpp` sin cambios | Ninguno | Ninguna |
| Memoria | VERIFICADO | Flash 89,2 %, RAM 59,6 %, 2 avisos ya existentes; heap LVGL del asistente 9 576 B (64 bits) | Bajo | Ninguna |
| Motores, finales, temperaturas, EEPROM I²C, Wi-Fi, microSD, USB | PENDIENTE DE PRUEBA FÍSICA | Solo inspección y simulación | Alto hasta probarlos | `PRUEBAS_FISICAS_ARQUIS.md` |

**Veredicto:** versión corregida **lista para auditar**. Si la auditoría la aprueba, apta para **iniciar** las pruebas físicas en el orden de `PRUEBAS_FISICAS_ARQUIS.md`. **No** es apta para producción.
