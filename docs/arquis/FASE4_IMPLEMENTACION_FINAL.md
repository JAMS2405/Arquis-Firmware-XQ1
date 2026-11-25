# ARQUIS — Fase 4: firmware único XQ1 + Aquila D1 (implementación final)

Rama: `feature/arquis-firmware-unico-perfiles`. Base: `a98dd6a` (= `main`, sin cambios).

**Estado:** compilado y probado por software. **No está validado físicamente**: ver [`PRUEBAS_FISICAS_ARQUIS.md`](PRUEBAS_FISICAS_ARQUIS.md).

> **Fase 5:** varias partes de este documento se corrigieron tras la auditoría: elección de modelo, «Conservar», textos de la malla y de «ajuste Z», descartar sin `M501`, verificación de los TMC antes de mover, pruebas y herramientas. Ver [`FASE5_CORRECCIONES_AUDITORIA.md`](FASE5_CORRECCIONES_AUDITORIA.md).

## 1. Arquitectura
| Capa | Contenido | Dónde |
|---|---|---|
| Firmware común | Interfaz ARQUIS, Wi-Fi, USB CDC, microSD, malla 5×5, PID, velocidad real, M221, Input Shaping, Rendimiento, herramientas | Código existente, sin cambios funcionales |
| Electrónica protegida | Placa, pines, TMC2209 por UART, 32 micropasos, *chopper*, Rsense, termistores, temperaturas máximas, protecciones térmicas, ventiladores | `Configuration*.h`. **No se edita desde la pantalla** |
| Perfil físico | Modelo, medidas, lógica de finales, sentido de los motores, *offsets*; pasos, velocidades, aceleraciones, corrientes, Input Shaping | Registro ARQUIS (lo que Marlin no guarda) + EEPROM de Marlin (lo que ya guarda). Valores de fábrica en una **tabla en Flash** |

**Archivos nuevos (`Marlin/src/lcd/extui/mks_ui/`):**
- `arquis_machine_logic.*`: lógica pura (tabla de perfiles, rangos, registro, CRC, dos copias, migración). Se prueba en el PC.
- `arquis_machine.*`: conexión con Marlin (EEPROM, aplicar el perfil, TMC, bloqueo de G-code).
- `arquis_machine_ui.*`: el asistente y Ajustes → Máquina. Son **las mismas páginas** en los dos modos.

## 2. Perfiles de fábrica
Extraídos con el preprocesador de la configuración real (`Configuration*.h` + `machine/xq1.h` o `aquila_d1.h`). `tests/machines/test_profiles.py` comprueba que la tabla coincide valor por valor.

| | Arquis XQ1 | Aquila D1 |
|---|---|---|
| Cama X × Y / altura Z | 300 × 300 / 450 | 230 × 240 / 240 |
| Pasos X / Y / Z / E | 160 / 160 / 791,74 / 748,72 | 160 / 160 / 803,41 / 781,08 |
| Velocidad máx. X / Y / Z / E | 500 / 500 / **5\*** / 60 | 500 / 500 / 45 / 60 |
| Aceleración máx. X / Y / Z / E | 4500 / 4500 / **100\*** / 10000 | 4500 / 3500 / 150 / 10000 |
| Aceleración impresión / retracción / viaje, JD | 3000 / 2000 / 4000, 0,05 | igual |
| Corrientes X / Y / Z / E (mA) | 650 / 650 / 650 / 700 | igual (motores del mismo modelo) |
| Input Shaping X / Y | 35 / 25 Hz | igual |
| Lógica de finales X / Y / Z | invertida / invertida / invertida | normal / normal / invertida |
| Sentido de motores X / Y / Z / E0 | invertido / invertido / normal / normal | normal / normal / normal / invertido |

\* **Provisionales:** son la prueba «Z no subía» de `xq1.h`, pendiente de validación física. Están marcados en la tabla (`pending`) y con `*` en la pantalla.

## 3. Funcionamiento
**Arranque:**
- `MarlinSettings::load()` lee primero el registro ARQUIS, antes de que Marlin inicialice los drivers (`tmc_init`).
- Si no hay registro válido, `lv_draw_ready_print()` abre el asistente en lugar de Inicio.

**Asistente (8 pasos):**
1. **Modelo:** detección por los pasos Z/E guardados; opción Conservar / De fábrica.
2. **Medidas:** el máximo es el de fábrica + 10 mm.
3. **Finales:** comprobación en vivo; cada final tiene que verse abierto ≥ 1 s, luego activado y luego abierto.
4. **Motores:** movimiento de +10 mm (Z, 5 mm) a velocidad lenta con pregunta «¿se alejó de su final?»; si la respuesta es «No», se invierte. El extrusor es opcional, a 200 °C.
5. **Home** de X, Y y Z, con confirmación.
6. **Mecánica**, con rangos.
7. **Revisión** de todos los valores.
8. **Guardar.**

Mientras dura el asistente, todo está en una copia en RAM. Las pruebas usan un «modo prueba» que solo permite `G0`, `G1` y `G28`.

**Guardar:**
1. Validar.
2. Aplicar en RAM.
3. Recalcular los tornillos de nivelación (campo existente de `gCfgItems`).
4. `settings.save()` (M500).
5. **Por último**, el registro ARQUIS: es el punto de confirmación.

No hace falta reiniciar.

**Menú Ajustes → Máquina:** sustituye a la ficha «Parámetros», con el mismo icono.
- **Contenido:**
  - modelo (cambiar = asistente con valores de fábrica);
  - medidas, finales, motores, home y *offsets*;
  - pasos, velocidades y aceleraciones, corrientes, Input Shaping;
  - restaurar la mecánica de fábrica (conserva PID, malla y ajuste Z);
  - restaurar todo (`M502` del modelo, con aviso de lo que se pierde);
  - repetir el asistente;
  - «Otros parámetros (MKS)», con el árbol anterior completo.
- **Durante una impresión:** no se abre.
- **Cambios sin guardar:** al salir pregunta Guardar / Descartar. Descartar ejecuta `M501`.

**Sin reaparición:** el asistente solo aparece si el registro no es válido o no tiene la marca «completado». Actualizar el BIN no toca el registro.

## 4. EEPROM
- **Ubicación:** EEPROM I²C de 4 KB.
  - Marlin: dirección 100 + 689 B (hasta la 788). **Sin cambios de estructura ni de versión** (`V88`).
  - Registro ARQUIS: **2 copias de 32 B en 0x0F00 y 0x0F80**.
  - `settings.cpp` comprueba al compilar que los datos de Marlin no llegan a 0x0F00 (`static_assert`).
  - En un chip de 2 KB, esas direcciones se reflejarían en 0x700 / 0x780, que tampoco usa Marlin.
- **Formato del registro:** `magic "ARQM"` · formato (1) · longitud · secuencia · modelo · flags (asistente, finales, motores, extrusor) · lógica de finales · sentido de motores · cama X/Y y altura Z (décimas de mm) · reservado · **CRC16**.
- **Escritura:** en la copia no vigente, con secuencia + 1. Después se **relee y se verifica**. Un corte a mitad deja intacta la otra copia (probado en los 32 puntos de corte posibles).
- **Lectura:** se usa la copia válida más reciente, con vuelta del contador.
- **Rechazo:** magic, formato futuro, longitud, CRC, modelo, flags o medidas fuera de rango.
- **Migración:** un formato posterior solo puede añadir campos al final (se comprueba con `len`); los campos conocidos se siguen leyendo.
- **Valores seguros sin registro:** la geometría más pequeña (230 × 240 × 240), la lógica y los sentidos compilados, y **movimiento bloqueado**. Para finales y sentidos no existe un valor seguro común, porque son opuestos entre las dos máquinas.
- **Recuperación:** `M9050 R` (por USB, Wi-Fi o un archivo de la microSD) vuelve al asistente conservando el modelo. `M9050` informa del estado.

## 5. Direcciones de motor sin tocar `stepper.cpp` (bit `shaft` del TMC2209)
- **Base:** las direcciones compiladas son las del XQ1. Para otro sentido, `tmc_init` pone `GCONF.shaft`.
  - XQ1: ningún driver invertido. Se comporta como antes aunque un driver se reinicie.
  - Aquila: X, Y y E0 invertidos.
- **Verificación:**
  1. Se lee `GCONF` por UART de X, Y, Z y E0.
  2. Se distingue «sin comunicación» (CRC; 3 intentos) de «driver reiniciado» (`pdn_disable` o `mstep_reg_select` a 0) y de «`shaft` distinto».
  3. Se lee **justo antes** de `G28`, `M24`, `M1000` y de iniciar una impresión desde la pantalla, y **cada segundo** un driver (unos 10 ms).
- **Respuesta:**
  - **Sin imprimir:** si un driver se reinició, se restauran todos con `restore_trinamic_drivers()` (reenvía corriente, micropasos, *chopper* y `GCONF` con `shaft`), los ejes quedan sin origen y aparece un aviso.
  - **Imprimiendo:** si la dirección deja de ser fiable (`shaft` distinto, o reinicio de un driver que debía estar invertido, confirmado con una segunda lectura), se ejecuta `kill` (parada de seguridad). Un fallo de comunicación puntual **no** corta la impresión.
- **Bloqueos:** con drivers invertidos (Aquila) y sin lectura confirmada, `G28` y el inicio de impresión quedan bloqueados.
- ***Babystep* y extrusión:** son correctos, porque la inversión ocurre dentro del driver para todos los pasos.

## 6. Archivos modificados de Marlin (justificación)
| Archivo | Cambio | Motivo |
|---|---|---|
| `module/settings.cpp` | Carga del registro al principio de `load()`; mecánica del modelo en `reset()`; `static_assert` | El perfil tiene que estar antes de los drivers; `M502` según el modelo; no solapar la EEPROM |
| `module/stepper/trinamic.cpp` | 1 línea: `gconf.shaft` en `tmc_init` (TMC2209) | Sentido del motor por modelo sin tocar `stepper.cpp` |
| `gcode/gcode.cpp` | Gancho antes del despacho, como `PASSWORD_FEATURE` | Bloqueo de movimiento y `M9050` |
| `gcode/host/M115.cpp` | Nombre del modelo en tiempo de ejecución | Los hosts ven la máquina correcta |
| `machine/machine.h` | `X/Y/Z_MIN_ENDSTOP_INVERTING` = bit de `arq_mc_es_inv` | Lógica de finales por modelo; **`endstops.cpp` sin cambios** |
| `module/motion.h` | `base_max_pos()` lee el modelo | El `static const` de `XYZ_DEFS` se habría quedado con el primer valor |
| `module/motion.cpp` | Alcance y elevación de Z con los límites del modelo | Límites reales |
| `module/probe.h` | `_max_x()` / `_max_y()` ya no son `constexpr` | El área de sondeo y la **malla 5×5** siguen a la cama; **el módulo de malla no se toca** |
| `libs/nozzle.cpp` | El punto de aparcado se limita a la cama y la altura del modelo | `NOZZLE_PARK_POINT` se compila con 300 × 300: Y = 290 quedaría fuera del Aquila |
| `feature/powerloss.cpp` | Elevación de Z tras un corte limitada a la altura del modelo | Ídem |
| Interfaz | Inicio, impresión, Sistema, mapa de objetos, tornillos: nombre y cama del modelo; confirmación de impresión bloqueada sin perfil; nuevo estado `ARQ_MACHINE_UI` | |
| `ini/arquis.ini` | Sin entorno `aquila_d1` | Un solo BIN |

**Sin cambios** (comprobado por `test_profiles.py` contra `a98dd6a`):
- `stepper.cpp/.h`, `planner.cpp/.h`, `endstops.cpp/.h`, `stepper/indirection.h`;
- `M593.cpp` (Input Shaping) y `feature/bedlevel`;
- `arquis_mesh*`, `arquis_pid`, `arquis_perf`;
- `wifi_module.cpp`, `temperature.cpp`.

Tampoco cambian `bmp_public_buf` (14 336 B), el heap de LVGL (16 KB) ni la fuente Roboto 22.

## 7. Funciones conservadas y eliminaciones
- **Se conservan:** malla 5×5, velocidad real, M221, ajuste Z, Input Shaping y sus búferes, PID, Wi-Fi, microSD, USB CDC/COM, interfaz ARQUIS, perilla, herramientas, protecciones térmicas y recuperación tras corte.
- **Retirados, autorizados:** USB MSC y USB host (F3-08). En el BIN hay 0 símbolos de MSC y 0 de host; el CDC está presente.
- **Atención:** esta rama incluye **F3-06 (sin Nube MKS) y F3-07 (solo español)**, aprobados en las Fases 2 y 3. La instrucción de la Fase 4 dice «no eliminar Nube MKS ni idiomas». Si eso significa que hay que recuperarlos, se revierten con `git revert 6827052 725e942`: son commits independientes. Coste medido: unos 15,7 KB de Flash y 2,8 KB de RAM.

## 8. Compilación (medida, PlatformIO, mismas dependencias fijadas)
| Versión | Flash | RAM |
|---|---:|---:|
| Fase 3 sin F3-08 (referencia indicada) | 407 880 (85,8 %) | 86 416 (65,9 %) |
| F3-08 | 400 472 (84,3 %) | 76 668 (58,5 %) |
| **Fase 4 (BIN de la rama)** | **421 204 (88,6 %)** | **77 752 (59,3 %)** |
| Original `a98dd6a` | 441 764 (93,0 %) | 89 120 (68,0 %) |

- **Coste de la Fase 4 frente a F3-08:** +20 732 B de Flash y +1 084 B de RAM. La estimación de la Fase 4.2 era de 8–11 KB: el código de las pantallas resultó mayor.
- **Reparto medido por símbolos:**
  - interfaz: unos 9 KB;
  - conexión con Marlin: unos 3,5 KB;
  - lógica: unos 1,5 KB;
  - ganchos: unos 0,3 KB;
  - textos: unos 5 KB.
- **Margen:** quedan **53 932 B libres**, 6,4 KB por debajo del 90 %, y 20,5 KB menos que el firmware original.
- **Avisos:** solo los dos que ya estaban en Marlin (`Warnings.cpp:738` DIAG y `wifi_module.cpp:829` `diveDir`).
- **Entorno:** `mks_robin_nano_v3_1_usb_flash_drive_msc`, el único. Su nombre se conserva, aunque ya no lleva MSC.
- **BIN:** `firmware_pruebas/fase4/Robin_nano_v3.bin`, 421 736 B, compilado desde el `git archive` del commit `758fe50`. SHA-256 en `SHA256SUMS.txt`, registro en `compilacion.log`.
- **Comprobado en el binario:**
  - CDC presente;
  - MSC, host, Nube y QRCode ausentes;
  - Wi-Fi, microSD, asistente, menú, registro y verificación TMC presentes;
  - los dos perfiles dentro;
  - `Endstops::update()` y `M119` leen la lógica de finales del perfil.

## 9. Pruebas automáticas (todas ejecutadas, 0 fallos)
| Prueba | Resultado |
|---|---|
| `tests/machines/test_profiles.py`: tabla = configuración real de cada máquina; configuración común; restricciones (archivos sin tocar, un entorno, USB) | 51 / 51 |
| `tests/machines/machine_logic_test.cpp` (ASan+UBSan): CRC, dos copias, secuencia, cortes de luz en los 32 bytes, migración, rangos, detección, `shaft` | 56 / 56 |
| Simulador del asistente y del menú (`build_machine_sim.sh`; código real + LVGL real, toques simulados): flujo completo, rechazos, bloqueo tras cambiar un final, guardado y tornillos recalculados | 23 / 23, 49 capturas |
| Pico de heap de LVGL en el asistente (compilado en 64 bits: en la placa es menor) | 10 376 B de 16 384 |
| Malla (`tests/mesh`) | 50 + 367 + 654 |
| PID (`tests/pid`) | 31 + 74 |
| Rendimiento (`tests/perf`) | 32 + 95 |
| Torres | 16 |
| Lógica de la interfaz | 83 / 83 |
| `check_marlin_api.sh` (29 archivos contra las firmas de Marlin; se corrigió el sustituto, al que le faltaba `FLOW_UI` desde `a98dd6a`) | 29 / 29 |

**No ejecutadas (necesitan la placa):**
- lectura real de `GCONF` por UART;
- comportamiento del I²C con el registro;
- movimientos, finales y home reales;
- impresión por microSD, Wi-Fi y USB.

## 10. Limitaciones y riesgos para las pruebas físicas
1. **El Aquila depende de leer los TMC por UART.** Si en la placa real no hay lectura, su perfil no podrá moverse: queda bloqueado por seguridad. El XQ1 no depende de esto. Prueba 20 de la lista.
2. **Un driver reiniciado durante una impresión del Aquila detiene la máquina (`kill`).** Antes se habría impreso con el sentido equivocado.
3. **Rangos solo en Ajustes → Máquina.** Las pantallas MKS de «Otros parámetros» y los G-code `M92`, `M203` y `M906` no tienen esos límites (como antes).
4. **`G29 H<tamaño>`** centra en 150 mm aunque recorta al área real del modelo. La malla ARQUIS no lo usa.
5. **La posición de pausa propia de MKS** (`gCfgItems.pausePos*`, por defecto sin uso) solo la limitan los finales por software tras el home.
6. **Mínimos y sentido de homing fijos:** origen 0 y home al mínimo, iguales en las dos máquinas.
7. **Velocidad y aceleración de Z del XQ1:** siguen pendientes de validar (prueba 18).
8. **Iniciar desde el Wi-Fi sin perfil válido:** no se comprueba antes de empezar (`wifi_module.cpp` no se toca). La impresión se cancela en el primer movimiento rechazado.
