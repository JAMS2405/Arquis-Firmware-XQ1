# Introducción

Este manual describe la pantalla táctil **TS35** de la **Arquis XQ1** y todo lo que se puede ajustar en la impresora con el firmware de este repositorio (Marlin 2.1.2.10 con la interfaz ARQUIS). Sirve también para el **Aquila D1** y otras máquinas con perfil (capítulo 9).

**Estado.** Las funciones de este manual fueron **probadas en la impresora por el propietario** (2026-11-22). Una excepción, que sigue como estaba: la **cancelación de objetos desde la pantalla** permanece **bloqueada a propósito** hasta validarla (apartado 3.1).

**Sobre las imágenes.** Son **capturas del simulador de la interfaz**: el mismo código de pantallas del firmware ejecutándose sobre LVGL en un PC, con datos de ejemplo. Los textos y la disposición son los reales; los valores (temperaturas, nombres de archivo…) son de ejemplo, y en la pantalla física los colores pueden verse algo distintos según la iluminación.

**Código de colores.** **Azul**: navegación, selección y acciones normales · **verde**: correcto, objetivo alcanzado, activo · **ámbar**: calentando, advertencia, pendiente · **rojo**: error, cancelar, acción peligrosa · **gris**: apagado, deshabilitado, secundario.

# 1. Árbol de pantallas

```
DASHBOARD (inicio)
│   estado: Reposo · Lista · Calentando · Temp. alcanzada · Enfriando · Imprimiendo · Pausada · Error
│   tarjetas: Hotend · Cama · Ventiladores · (avance de la impresión)
│
├── IMPRIMIR ....................... explorador de la tarjeta SD
│     └── archivo ── miniatura ── [Imprimir] / [Cancelar]
│           └── SUPERVISIÓN DE LA IMPRESIÓN
│                 ├── [Pausar / Reanudar]   [Cancelar] (con confirmación)   [Inicio]
│                 ├── [Opciones]
│                 │     ├── Temperatura
│                 │     ├── Filamento ......... pausar y cambiar
│                 │     ├── Ventiladores
│                 │     ├── Autoapagado ....... activar / desactivar
│                 │     ├── Velocidad ......... movimiento % y flujo %    (solo imprimiendo)
│                 │     ├── Babystep .......... ajuste fino de Z          (solo imprimiendo)
│                 │     ├── Extruir / Mover ... (solo en pausa)
│                 │     └── Objetos ........... mapa y lista de objetos   (si el G-code los define)
│                 └── [Recuperar impresión] ... tras un corte de luz
│
├── MOVER ......................... X, Y, Z (0,1 · 1 · 10 mm)
├── TEMP. ......................... hotend y cama
│
├── HERRAM.  (se oculta al imprimir)
│     ├── Temperatura · Extruir · Mover
│     ├── Home ──────── Home todo · X · Y · Z · Motores off · Motores XY
│     ├── Malla 5x5 (nuevo) ── nivelación manual de 25 puntos con papel
│     ├── Nivelar cama ─────── los tornillos (5 puntos)
│     ├── Filamento ────────── Cargar / Descargar
│     └── Más ──────────────── G-code (consola)
│
└── AJUSTES  (deshabilitado al imprimir)
      ├── Parámetros
      │     ├── Configuración de la máquina ── Aceleración · Jerk · Velocidades máximas
      │     ├── Motores ────────────────────── Pasos por mm · Corriente (TMC) · Modo silencioso
      │     ├── Nivelación ─────────────────── Comando de autonivelación · Offset boquilla-sonda
      │     │                                  Posiciones de nivelación manual · Asistente de offset Z
      │     └── Ajustes avanzados ──────────── Ajustes de filamento · Posición de pausa
      │                                        Perilla · Parámetros Wi-Fi
      ├── Memoria (EEPROM) ── Guardar · Leer · Restaurar valores de fábrica
      ├── Ventiladores
      ├── Sistema ─────────── datos de la máquina + diagnóstico en vivo
      ├── PID (nuevo) ─────── autoajuste de hotend y de cama
      ├── Rendimiento (nuevo) ─ aceleraciones · Linear Advance · Junction Deviation · Input Shaping
      ├── Wi-Fi
      └── Apagar ──────────── (o «Motores off» si la placa no controla la alimentación)
```

# 2. Dashboard e imprimir

## 2.1 Dashboard

![Dashboard en reposo](manual/img/dash_reposo.png)

Es la pantalla de inicio. Arriba a la derecha, el **estado de la máquina**. Debajo, **Hotend** y **Cama** (temperatura actual grande y, debajo, el objetivo o «Apagado»). Tocar una tarjeta abre *Temp.*; tocar *Ventiladores* abre esa pantalla. Abajo los accesos: **Imprimir · Mover · Temp. · Herram. · Ajustes**.

Estados: *Reposo* (sin SD y todo apagado) · *Lista* (con SD, todo apagado) · *Calentando* (ámbar) · *Temp. alcanzada* (verde) · *Enfriando* (azul: calentador apagado y por encima de 50 °C) · *Imprimiendo* · *Pausada* · *Error* (rojo).

![Dashboard calentando](manual/img/dash_calentando.png)

## 2.2 Imprimir un archivo

![Explorador de la SD](manual/img/files_lista.png)

1. **Imprimir** abre el explorador de la tarjeta SD. Las carpetas llevan una flecha; las flechas de la derecha cambian de página; *Atrás* sube un nivel.
2. Al tocar un archivo se ve su **miniatura** (si el G-code la trae) para confirmar qué vas a imprimir.

![Confirmar la impresión](manual/img/files_confirmar.png)

3. **Imprimir** inicia el trabajo; **Cancelar** vuelve a la lista sin hacer nada.

Si el archivo tiene un **manifiesto de objetos** (lo genera el plugin de Cura de este repositorio), la impresora lo lee al elegirlo y el mapa de objetos mostrará sus nombres.

## 2.3 Durante la impresión

![Supervisión](manual/img/print_imprimiendo.png)

Muestra el **progreso**, el tiempo transcurrido y restante (cuando hay datos suficientes para estimarlo), la altura Z, las temperaturas y los ventiladores.

| Botón | Qué hace |
|---|---|
| **Pausar / Reanudar** | El botón refleja el estado real de la impresión |
| **Cancelar** | Siempre pide confirmación; el avance perdido no se recupera |
| **Inicio** | Vuelve al Dashboard sin detener nada; desde allí la tarjeta de avance regresa a la supervisión |
| **Opciones** | Ver 2.4 |

![Impresión pausada](manual/img/print_pausada.png)

![Cancelar impresión: siempre con confirmación](manual/img/print_cancelar.png)

![Fin de impresión](manual/img/print_fin.png)

**Bloqueado mientras imprime:** *Herram.* se oculta y *Mover* y *Ajustes* se deshabilitan, para no estropear la pieza. *Rendimiento*, *PID* y la *Malla 5x5* también se niegan a cambiar nada si hay una impresión en curso.

## 2.4 Opciones durante la impresión

![Opciones imprimiendo](manual/img/opc_imprimiendo.png)

| Opción | Cuándo | Qué hace |
|---|---|---|
| Temperatura | siempre | Objetivo del hotend y de la cama |
| Filamento | siempre | **Pausar y cambiar**: pausa, lleva el cabezal a la posición de pausa y guía el cambio. Despeja antes la cama |
| Ventiladores | siempre | Porcentaje de cada ventilador |
| Autoapagado | si la placa controla la alimentación | Apaga la impresora al terminar (verde = activo) |
| Velocidad | imprimiendo | **Movimiento %** y **flujo %**, con pasos de 1, 5 y 10 (mínimo 10 %, máximo 999 %) |
| Babystep | imprimiendo | Ajuste fino de Z con pasos de 0,01 / 0,05 / 0,1 mm; **se guarda al volver** |
| Extruir / Mover | en pausa | Igual que las pantallas normales |
| Objetos | si el G-code define objetos | Mapa y lista (apartado 3.1) |

![Opciones con la impresión en pausa](manual/img/opc_pausa.png)

![Velocidad y flujo](manual/img/velocidad.png)

![Babystep](manual/img/babystep.png)

# 3. Objetos (cancelar uno sin parar el resto)

Requiere un G-code con `M486` (el plugin de Cura de este repositorio lo añade; en PrusaSlicer y Orca: «Label objects» en formato Marlin). La pantalla muestra el **mapa de la cama** y la **lista** (hasta 30 objetos), con **zoom ×1 / ×2 / ×4** y cambio de página. Tocar un objeto en el mapa o en la lista los sincroniza. Estados: *Pendiente · Imprimiendo · Completado · Cancelado*.

![Mapa y lista de objetos](manual/img/obj_mapa.png)

![Zoom sobre el mapa](manual/img/obj_zoom.png)

## 3.1 Cancelación bloqueada

![Cancelación bloqueada](manual/img/obj_bloqueada.png)

Con manifiesto, el botón sale como **«Cancelación bloqueada»** a propósito (`ARQ_OBJ_VISUAL_CANCEL_VALIDATED 0`): la cancelación real de un objeto **no se ha validado** en la impresora. Se habilitará solo después de probarla con un trabajo barato de varias piezas pequeñas (ver `docs/slicer_suite/08_estado_proyecto.md`, apartado 3). La visualización (mapa, lista, zoom, estados) sí funciona.

# 4. Control de la máquina

## 4.1 Mover

![Mover](manual/img/mover.png)

X, Y y Z con pasos de **0,1 / 1 / 10 mm**. **Y− va arriba e Y+ abajo** (también en Babystep). Si los ejes no están referenciados, la pantalla lo avisa: haz *Home* primero.

![Mover sin referenciar](manual/img/mover_sin_home.png)

## 4.2 Extruir

![Extruir](manual/img/extruir.png)

Extruye o retrae con velocidad *Lenta / Normal / Rápida*. Con el hotend frío lo bloquea y lo explica.

![Extruir con el hotend frío](manual/img/extruir_frio.png)

## 4.3 Temperatura y ventiladores

![Temperatura](manual/img/temp_hotend.png)

Cambia el objetivo del hotend o de la cama con pasos de 1, 5 y 10 °C y preajustes (PLA y ABS según `Configuration.h`).

![Ventiladores](manual/img/ventiladores.png)

# 5. Herramientas

![Herramientas](manual/img/herramientas.png)

## 5.1 Home

![Home](manual/img/home.png)

**Home todo** (`G28`), **X**, **Y**, **Z**, **Motores off** (`M84`) y **Motores XY** (`M84 XY`; los ejes apagados pierden su posición).

## 5.2 Nivelar cama (tornillos)

![Nivelar cama](manual/img/nivelar.png)

Muestra los 5 puntos; cada uno sube, se desplaza y baja. **Despeja la cama antes.** Hace `G28 L0` para que la malla no interfiera al ajustar tornillos. Hazlo **antes** de medir la malla.

## 5.3 Malla 5x5 (nivelación manual de 25 puntos, sin sonda)

Mide la cama con el **método del papel** y guarda una malla bilineal que Marlin aplica en cada movimiento, también al imprimir, sin tocar el G-code ni el slicer. Guía técnica completa: `docs/arquis/MALLA_5x5.md`.

![Malla guardada](manual/img/malla_guardada.png)

**Pantalla principal** (sin sesión): mapa con la malla guardada, estado (*Malla activa / inactiva / Sin malla*), temperatura de calibración y los botones **Iniciar medición · Activar/Desactivar · Guardar · Cargar**.

**Procedimiento**

1. Ajusta los tornillos (*Nivelar cama*), limpia la boquilla y **retira objetos de la cama**.
2. **Herram. → Malla 5x5**. Elige la temperatura de calibración (−/+ 5 °C; 0 = no calentar) → **Iniciar medición** → confirmar.
3. La máquina calienta la cama, espera 30 s, hace `G28 L0` y va al punto 1.

![Calentando](manual/img/malla_calentando.png)

4. Baja Z con ▼ hasta que el papel roce con ligera resistencia (pasos **1,00 / 0,10 / 0,05 / 0,01 mm**). **Confirmar** registra la altura real y pasa al siguiente punto.

![Ajustando un punto](manual/img/malla_ajustando.png)

5. **Repetir**: toca un punto ya medido en el mapa y pulsa *Repetir*.
6. Con los 25 puntos aparece la **revisión** (azul = más baja, ámbar = más alta; máximo, mínimo, rango y desviación). **Guardar** valida, activa la malla y ejecuta `M500`.

![Revisión](manual/img/malla_revision.png)

7. **Cancelar** (o ◄) descarta las mediciones y **conserva la última malla válida**.

Después de cada `G28` la malla se activa sola. Ajusta el **Z-offset en el centro de la cama**. Seguridad: Z entre −2,00 y 10 mm; los pasos gruesos no bajan de Z = 0,5 mm.

## 5.4 Filamento

![Cambio de filamento](manual/img/filamento.png)

**Cargar** y **Descargar** calientan el hotend a la temperatura de filamento, avisan cuando llega y piden confirmación antes de mover el filamento. *Cancelar* detiene el proceso.

## 5.5 Más: G-code

![Consola G-code](manual/img/gcode.png)

Escribe un comando y muestra la respuesta de Marlin. Sirve, por ejemplo, para `M503` (ver todos los ajustes) o `M119` (finales de carrera).

# 6. Ajustes

![Ajustes](manual/img/ajustes.png)

## 6.1 Parámetros

![Parámetros](manual/img/params_menu.png)

Cuatro grupos: **Configuración de la máquina** (Aceleración, Jerk, Velocidades máximas), **Motores** (Pasos por mm, Corriente de motores TMC, Modo silencioso StealthChop), **Nivelación** (Comando de autonivelación, Offset boquilla-sonda, Posiciones de nivelación manual, Asistente de offset Z) y **Ajustes avanzados** (Ajustes de filamento, Posición de pausa, Perilla, Parámetros Wi-Fi).

![Lista de aceleraciones](manual/img/params_accel.png)

Tocar una fila abre el **teclado numérico**: muestra el nombre del parámetro, *Cancelar* sale sin cambiar nada y *Aceptar* se deshabilita si el valor no es un número. Las filas con interruptor (p. ej. StealthChop por eje) se activan o desactivan con un toque.

![Teclado numérico](manual/img/teclado.png)

![Interruptores](manual/img/params_onoff.png)

Notas: con Junction Deviation el jerk clásico no se usa (ajusta JD en *Rendimiento*); *Sensibilidad de home* solo aparece con homing sin sensores, y aquí está desactivado.

**Dónde se guarda cada cosa.** Los ajustes de Marlin viven en RAM hasta *Memoria → Guardar*. Excepción: la **posición de pausa**, el **filamento** (temperatura, longitudes, velocidades) y las **posiciones de nivelación manual** son ajustes de la pantalla: se guardan al aceptar el valor, en su propia memoria, y **no** dependen de `M500`.

## 6.2 Memoria (EEPROM)

**Guardar** (`M500`), **Leer** (`M501`) y **Restaurar valores de fábrica**. Solo afectan a los ajustes de Marlin. Restaurar lleva el botón en rojo y pide confirmación; **borra tus ajustes de la EEPROM**, así que antes anota `M503`.

![Confirmación peligrosa](manual/img/peligro.png)

## 6.3 Sistema

![Sistema](manual/img/sistema.png)

A la izquierda, los datos de la máquina: nombre, Marlin, interfaz ARQUIS, placa, compilación, tiempo encendida, número de impresiones y tiempo total. A la derecha, el **diagnóstico en vivo**: sensor de hotend y de cama (una lectura anómala sale en rojo como «Revisar»), finales de carrera X/Y/Z (*Activado / Abierto*), tarjeta SD, **malla de cama** (*Activa / Inactiva / Sin malla*) y estado de Marlin (*Normal / Detenido*).

## 6.4 PID: autoajuste de hotend y cama

Guía técnica completa: `docs/arquis/PID.md`.

![PID: elegir](manual/img/pid_elegir.png)

1. Elige **Hotend** o **Cama**, la temperatura (−/+ 5 °C) y los ciclos (−/+ 1). Por defecto: hotend 210 °C y 8 ciclos; cama 60 °C y 6 ciclos.
2. **Iniciar autoajuste** → confirmar. **Se apagan todos los calentadores al empezar** y tarda varios minutos.

![PID en curso](manual/img/pid_en_curso.png)

3. **Cancelar** detiene el ajuste sin cambiar nada.
4. Al terminar ves **Kp, Ki y Kd nuevos y (anteriores)**: **Aplicar y guardar** (`M301`/`M304` + `M500`) o **Descartar**.

![PID: resultado](manual/img/pid_resultado.png)

Si tu EEPROM es de una versión anterior, la primera vez tras flashear esta versión **se restaura a fábrica** (el PID de cama y la malla 5×5 cambian su tamaño): anota `M503` antes.

## 6.5 Rendimiento: velocidad y calidad

Guía técnica completa: `docs/arquis/RENDIMIENTO.md`.

![Rendimiento](manual/img/perf_guardado.png)

Cada tarjeta es un valor. Se toca, se cambia con **−** / **+** y **se aplica al instante**. Tocar el valor central cambia entre paso **fino** y **grueso**.

| Tarjeta | Orden | Rango | Para qué sirve |
|---|---|---|---|
| Acel. impresión | `M204 P` | 200–20000 mm/s² | Rapidez al cambiar de dirección imprimiendo |
| Acel. vacío | `M204 T` | 200–20000 mm/s² | Igual, sin extruir |
| Acel. máx X / Y | `M201 X` / `M201 Y` | 500–20000 mm/s² | **Techo** de las dos anteriores |
| Linear Advance K | `M900 K` | 0,005–2,000 (OFF) | Presión del filamento en esquinas y costuras |
| Junction Deviation | `M205 J` | 0,01–0,3 mm | Cuánto frena en las esquinas |
| Shaping X / Y | `M593 X F` / `M593 Y F` | **desde 25 Hz** hasta 200 (OFF) | Input Shaping (reduce el ringing) |

**Guardar** ejecuta `M500`; **Restaurar** vuelve a lo guardado. Los cambios sin guardar se pierden al apagar y se marcan con `*`. Si pides más aceleración que el techo, Marlin aplica el techo y la línea de estado lo avisa:

![Limitada por el eje](manual/img/perf_limitada.png)

Solo se edita con la impresora quieta; con una impresión en curso (o moviéndose, o con la malla o el PID activos) todo se deshabilita y la línea de estado dice el motivo:

![Bloqueada al imprimir](manual/img/perf_bloqueada.png)

**Torres de prueba.** El generador `herramientas/torres/generar_torres.py` crea G-code que cambia un valor a medida que sube la pieza (ringing por frecuencia y por aceleración, Linear Advance K y caudal máximo). Se imprime, se mira en qué tramo queda mejor y ese valor se escribe aquí. Los archivos listos para el XQ1 (PLA 210/60 °C) están en `herramientas/torres/listos/`; con otro filamento, genera los tuyos. **Orden recomendado: caudal → frecuencia → K → aceleración.** Al terminar, cada torre ejecuta `M501` (vuelve a lo guardado). Detalle en `RENDIMIENTO.md`.

## 6.6 Wi-Fi y Apagar

![Wi-Fi](manual/img/wifi.png)

**Wi-Fi** muestra el estado (IP, red) y la lista de redes disponibles; la conectada se ve en verde. **Apagar** corta la alimentación (o desactiva los motores si la placa no la controla), siempre con confirmación.

En esta placa no aparece la tarjeta *Idioma*. La rejilla de Ajustes admite 8 tarjetas y hoy hay exactamente 8: añadir otra empujaría fuera la última.

# 7. Valores de fábrica del firmware (XQ1)

Son los de `Configuration.h` y `Configuration_adv.h`. **Tu EEPROM puede tener otros** (los que guardaste con `M500`); consulta los reales con `M503` o en *Parámetros*.

| Concepto | Valor |
|---|---|
| Placa y drivers | MKS Robin Nano V3.1 · TMC2209 · 32 microsteps |
| Cama | 300 × 300 mm · Z 450 mm |
| Pasos/mm (X, Y, Z, E) | 160 · 160 · 791,74 · 748,72 |
| Velocidad máxima (X, Y, Z, E) | 500 · 500 · 45 · 60 mm/s |
| Aceleración máxima (X, Y, Z, E) | 4500 · 4500 · 150 · 10000 mm/s² |
| Aceleración imprimiendo / retracción / vacío | 3000 / 2000 / 4000 mm/s² |
| Junction Deviation · curva S | 0,05 mm · activa |
| Linear Advance K | 0,013 (genérico: calíbralo con la torre de K) |
| Input Shaping X / Y | 35 Hz ζ 0,40 / 25 Hz ζ 0,70 (valores de referencia: mídelos con la torre de frecuencia) |
| Corriente TMC (X, Y, Z, E) | 650 · 650 · 650 · 700 mA RMS · StealthChop en XY |
| Temperatura máxima de objetivo | hotend 270 °C · cama 105 °C |
| Malla de cama | 5 × 5 manual, margen 15 mm, desvanecimiento 10 mm, se activa tras `G28` |
| Comunicación | 250000 baudios |

# 8. Dónde se ajusta cada cosa

| Quiero… | Pantalla | Orden de Marlin | ¿Se guarda? |
|---|---|---|---|
| Nivelar la cama con la hoja | Herram. → Malla 5x5 | (interna; `G28 L0`) | Sí (la malla va a la EEPROM) |
| Ajustar los tornillos | Herram. → Nivelar cama | — | — |
| Z-offset | Parámetros → Nivelación; Babystep (imprimiendo) | `M290`, offsets | Babystep sí, al volver |
| Autoajuste PID | Ajustes → PID | `M303` → `M301` / `M304` | Sí |
| Aceleración | Ajustes → Rendimiento, o Parámetros → Configuración de la máquina | `M204` / `M201` | Sí (con Guardar) |
| Linear Advance K | Ajustes → Rendimiento | `M900` | Sí |
| Input Shaping | Ajustes → Rendimiento | `M593` | Sí |
| Pasos por mm | Parámetros → Motores | `M92` | Sí |
| Corriente de motores, StealthChop | Parámetros → Motores | `M906` / `M569` | Sí |
| Velocidad o flujo durante la impresión | Opciones → Velocidad | `M220` / `M221` | No (solo ese trabajo) |
| Temperaturas | Temp. | `M104` / `M140` | No |
| Posición de pausa, filamento, nivelación manual | Parámetros → Ajustes avanzados / Nivelación | (ajustes de la pantalla) | Sí, en la memoria de la pantalla (no con `M500`) |
| Ver los valores actuales | Herram. → Más → G-code | `M503` | — |

# 9. Varias impresoras con un solo firmware

Los valores de cada impresora viven en `Marlin/machine/<nombre>.h` y se eligen con el entorno de PlatformIO:

```
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc   # Arquis XQ1
pio run -e aquila_d1                                 # Aquila D1 (cama 230×240, Z 240)
```

El nombre que ve el usuario (*Ajustes → Sistema → Máquina*) es el del perfil. El XQ1 no cambia. Para **actualizar sin tocar tus parámetros**: `git pull` y volver a compilar tu entorno; los datos de tu EEPROM son de tu placa y solo se pierden si cambia el tamaño de la EEPROM (anótalos con `M503` antes). Cómo añadir otra impresora: `docs/arquis/MAQUINAS.md`.

# 10. Primera puesta a punto (orden recomendado)

1. **Antes de flashear:** conecta por USB y guarda la salida de `M503`.
2. **Flashear** (capítulo 11) y comprobar en *Ajustes → Sistema* la versión de Marlin y de la interfaz.
3. **Reaplicar tus ajustes** (pasos/mm, corrientes…) y *Memoria → Guardar*.
4. **Ejes y finales de carrera:** *Home todo* con la cama despejada; mira *Sistema → Diagnóstico*.
5. **Tornillos** (*Nivelar cama*) y después **Malla 5x5**.
6. **PID** de hotend y de cama.
7. **Z-offset** en el centro con el papel, y primera capa.
8. **Rendimiento:** torres de prueba en este orden: **caudal → frecuencia de shaping → K → aceleración**.
9. Un trabajo corto de prueba **con la mano cerca del paro**.

# 11. Actualizar el firmware

```
git pull
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc
```

Copia `Robin_nano_v3.bin` y la carpeta `assets` (la de `sd_assets/` de este repositorio) a la **raíz** de la SD, con la impresora apagada y fría, e insértala al encender. La primera vez tarda más porque copia los recursos: **no apagues**. Guía completa, problemas frecuentes, logo de arranque y alternativa por USB: `docs/arquis/GUIA_INSTALACION.md`.

# 12. Seguridad

- Haz *Home*, *Nivelar cama*, *Malla 5x5* y *Mover* con la **cama despejada**.
- Si un sensor sale en **Revisar** (Sistema), revisa el termistor y su cable **antes de calentar**.
- Las protecciones térmicas de Marlin (`THERMAL_PROTECTION_*`, `WATCH_PID`) siguen activas; la interfaz no las cambia.
- **Rendimiento:** sube la aceleración poco a poco; una aceleración excesiva pierde pasos o golpea el cabezal. La aceleración máxima del eje es el techo.
- Una torre de prueba o un cambio en pantalla **no se guardan** hasta que tú lo hagas.
- Ante cualquier comportamiento inesperado, **apaga la impresora**.

# 13. Problemas frecuentes

| Síntoma | Qué hacer |
|---|---|
| Al arrancar sale el logo de Makerbase | La carpeta `assets` no es la de `sd_assets/` de este repositorio (ver `GUIA_INSTALACION.md`) |
| `pio` no se reconoce | Instala Git y PlatformIO, o usa la terminal de PlatformIO dentro de VS Code |
| `TMCStepper.h: No such file` al compilar | Borra `.pio/libdeps` y `~/.platformio/.cache`, o usa otro `PLATFORMIO_WORKSPACE_DIR` |
| Tras flashear, los ajustes vuelven a fábrica | Cambió el tamaño de la EEPROM: reaplica y pulsa Guardar (anota `M503` antes de flashear) |
| La malla no se aplica | Mira *Sistema → Malla de cama*: ¿Activa? Se activa tras cada `G28`; si no, *Malla 5x5 → Activar* |
| «No se pudo cambiar» en Rendimiento | La impresora no está quieta: imprimiendo, moviéndose, o con el PID o la malla en curso |
| Shaping no deja bajar de 25 Hz | Es el mínimo del búfer compilado (ver `RENDIMIENTO.md`) |
| «Cancelación bloqueada» en Objetos | Es intencionado hasta validar la cancelación real (apartado 3.1) |

# 14. Documentación relacionada

`GUIA_USUARIO.md` (recorrido ilustrado de la interfaz 1.0.0) · `GUIA_INSTALACION.md` · `MALLA_5x5.md` · `PID.md` · `RENDIMIENTO.md` · `MAQUINAS.md` · `LISTA_VERIFICACION.md` · `docs/slicer_suite/` (Cura: M486, gestor de objetos, no planar) · `tests/README.md` (pruebas automáticas).
