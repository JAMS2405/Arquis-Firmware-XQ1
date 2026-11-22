# ARQUIS XQ1 — Manual de uso completo (pantallas y configuraciones)

Manual de la pantalla táctil **TS35** y de todo lo que se puede ajustar en la impresora, con el firmware de este repositorio (Marlin 2.1.2.10 + interfaz ARQUIS). Sirve también para el **Aquila D1** y otras máquinas con perfil (ver §9).

> **Qué está probado y qué no.** Las pantallas de la interfaz v1.0.0 (Dashboard, archivos, impresión, Mover, Temperatura, Home, etc.) las probaste en la impresora. **Lo nuevo no se ha probado en la impresora:** *Malla 5×5*, *PID*, *Rendimiento*, el orden Y−/Y+ de Mover, las torres de prueba y el perfil del Aquila D1. Están probados en PC (lógica, pantalla simulada con LVGL real, firmas contra Marlin) y por eso este manual los marca con 🆕. Los valores buenos de tu máquina solo salen de tus pruebas.

---

## 1. Árbol de pantallas

```
DASHBOARD (inicio)
│   estado: Reposo · Lista · Calentando · Temp. alcanzada · Enfriando · Imprimiendo · Pausada · Error
│   tarjetas: Hotend · Cama · Ventiladores · (avance de impresión)
│
├── IMPRIMIR ........................ explorador de la tarjeta SD
│     └── archivo ─ miniatura ─ [Imprimir] / [Cancelar]
│           └── SUPERVISIÓN DE LA IMPRESIÓN
│                 ├── [Pausar / Reanudar]   [Cancelar] (con confirmación)   [Inicio]
│                 ├── [Opciones]
│                 │     ├── Temperatura ........ (ver MOVER/TEMP más abajo)
│                 │     ├── Filamento .......... Pausar y cambiar
│                 │     ├── Ventiladores
│                 │     ├── Autoapagado ........ activar / desactivar
│                 │     ├── Velocidad .......... movimiento % y flujo %     (solo imprimiendo)
│                 │     ├── Babystep ........... ajuste fino de Z            (solo imprimiendo)
│                 │     ├── Extruir / Mover .... (solo en pausa o sin imprimir)
│                 │     └── Objetos ............ mapa y lista de objetos     (si el G-code los define)
│                 └── [Recuperar impresión]     (tras un corte de luz)
│
├── MOVER ........................... X, Y, Z (0,1 · 1 · 10 mm)
├── TEMP. ........................... hotend / cama (1 · 5 · 10 °C y preajustes)
│
├── HERRAM. (oculto al imprimir)
│     ├── Temperatura
│     ├── Extruir
│     ├── Mover
│     ├── Home ───────── Home todo · X · Y · Z · Motores off · Motores XY off
│     ├── Malla 5x5  🆕 ─ nivelación manual de 25 puntos con papel
│     ├── Nivelar cama ── los 4 tornillos (5 puntos)
│     ├── Filamento ───── Cargar / Descargar
│     └── Más ─────────── G-code (consola) · accesos personalizados (vacíos por defecto)
│
└── AJUSTES (deshabilitado al imprimir)
      ├── Parámetros ──┬─ Máquina ──── Aceleración · Jerk · Velocidad máxima
      │                ├─ Motor ────── Pasos/mm · Corriente TMC · Modo de paso TMC  (Sensibilidad de home: solo con homing sin sensores)
      │                ├─ Nivelación ─ Offset Z · Posiciones de nivelado · Asistente de Z offset
      │                └─ Avanzado ─── Filamento · Posición de pausa · Perilla · Wi-Fi
      ├── Memoria ──────── Guardar (M500) · Leer (M501) · Restaurar valores de fábrica
      ├── Ventiladores
      ├── Sistema ──────── datos de la máquina + diagnóstico en vivo (sensores, finales de carrera, SD, malla)
      ├── PID  🆕 ──────── autoajuste de hotend y de cama
      ├── Rendimiento  🆕 ─ aceleraciones · Linear Advance · Junction Deviation · Input Shaping
      ├── Wi-Fi
      └── Apagar (o "Motores off" si la placa no controla la alimentación)
```

Código de colores: **azul** navegación/selección · **verde** correcto/activo · **ámbar** calentando/advertencia · **rojo** error/cancelar/peligro · **gris** apagado o deshabilitado.

---

## 2. Dashboard
Es la pantalla de inicio. Arriba a la derecha, el estado de la máquina. Debajo, **Hotend** y **Cama** (temperatura actual grande, objetivo debajo o «Apagado»); tocar una tarjeta abre *Temp.*; tocar *Ventiladores* abre esa pantalla. Abajo los accesos: **Imprimir · Mover · Temp. · Herram. · Ajustes**.

Estados: *Reposo* (sin SD y todo apagado) · *Lista* (con SD, todo apagado) · *Calentando* (ámbar) · *Temp. alcanzada* (verde) · *Enfriando* (azul, calentador apagado y por encima de 50 °C) · *Imprimiendo* · *Pausada* · *Error* (rojo).

## 3. Imprimir un archivo
1. **Imprimir** abre el explorador de la SD (las carpetas llevan flecha; las flechas de la derecha cambian de página; *Atrás* sube un nivel).
2. Al tocar un archivo se ve su miniatura (si el G-code la trae). **Imprimir** inicia; **Cancelar** vuelve.
3. **Si el archivo tiene un manifiesto de objetos** (lo genera el plugin de Cura de este repositorio), la impresora lo lee y el mapa de objetos muestra sus nombres.

### Durante la impresión
La supervisión muestra progreso, tiempo transcurrido y restante (cuando hay datos para estimarlo), altura Z, temperaturas y ventiladores.

| Botón | Qué hace |
|---|---|
| **Pausar / Reanudar** | Refleja el estado real |
| **Cancelar** | Siempre pide confirmación; la pérdida de avance no se puede deshacer |
| **Inicio** | Vuelve al Dashboard sin detener nada (la tarjeta de avance regresa a la supervisión) |
| **Opciones** | Ver abajo |

**Bloqueado mientras imprime:** *Herram.* se oculta; *Mover* y *Ajustes* se deshabilitan (para no estropear la pieza). *Rendimiento*, *PID* y la *Malla 5x5* también se niegan a cambiar nada con una impresión en curso.

### Opciones (durante la impresión)
| Opción | Solo | Qué hace |
|---|---|---|
| Temperatura | siempre | Objetivo de hotend y cama |
| Filamento | siempre | **Pausar y cambiar**: pausa, lleva el cabezal a la posición de pausa y guía el cambio. Despeja la cama antes |
| Ventiladores | siempre | Porcentaje de cada ventilador |
| Autoapagado | si la placa controla la alimentación | Apaga la impresora al terminar (el interruptor se ve verde si está activo) |
| Velocidad | imprimiendo | **Movimiento %** y **flujo %** con pasos de 1, 5 y 10 (mínimo 10 %, máximo 999 %) |
| Babystep | imprimiendo | Ajuste fino de Z con pasos 0,01 / 0,05 / 0,1 mm; **se guarda al volver** |
| Extruir / Mover | pausada o sin imprimir | Como las pantallas normales |
| Objetos | si el G-code define objetos | Mapa y lista (ver §3.1) |

### 3.1 Objetos (cancelar uno sin parar el resto)
Requiere un G-code con `M486` (el plugin de Cura de este repositorio lo añade; PrusaSlicer/Orca: «Label objects» en formato Marlin). La pantalla muestra el **mapa de la cama** y la **lista** (hasta 30 objetos), con zoom **×1 / ×2 / ×4** y páginas. Tocar un objeto en el mapa o en la lista los sincroniza. Estados: *Pendiente · Imprimiendo · Completado · Cancelado*.

> ⚠️ Con manifiesto, el botón **Cancelar objeto** sale como **«Cancelación bloqueada»** a propósito (`ARQ_OBJ_VISUAL_CANCEL_VALIDATED 0`): la cancelación real no se ha probado en la impresora. Se habilitará solo después de probarla con un trabajo barato (ver `docs/slicer_suite/08_estado_proyecto.md` §3).

---

## 4. Control de la máquina

### Mover
X, Y, Z con pasos de **0,1 / 1 / 10 mm**. **Y− va arriba e Y+ abajo** 🆕 (en esta pantalla y en Babystep); la primera vez comprueba el sentido real con un paso de 0,1 mm y el cabezal lejos de los topes. Avisa si los ejes no están referenciados (haz *Home*).

### Extruir
Extruir o retraer con velocidad *Lenta / Normal / Rápida*. Con el hotend frío lo bloquea y lo explica («Hotend frío (mín. …)»).

### Temperatura
Objetivo del hotend o de la cama con pasos de 1, 5 y 10 °C y preajustes **PLA / ABS** (según `Configuration.h`).

### Ventiladores
Porcentaje de cada ventilador (y Aux, si existen).

---

## 5. Herramientas

### Home
**Home todo** (`G28`), **X**, **Y**, **Z**, **Motores off** (`M84`) y **Motores XY off** (`M84 XY`; se pierde la posición de los ejes apagados).

### Nivelar cama (tornillos)
Muestra los 5 puntos de la cama; cada uno sube, se desplaza y baja. **Despeja la cama antes.** Hace `G28 L0` para que la malla no interfiera al ajustar tornillos. **Hazlo antes de medir la malla.**

### Malla 5×5 🆕 (nivelación manual de 25 puntos, sin sonda)
Mide la cama con el **método del papel** y guarda una malla bilineal que Marlin aplica en cada movimiento (también al imprimir), sin tocar el G-code ni el slicer. Guía completa: `docs/arquis/MALLA_5x5.md`.

1. Ajusta los tornillos (*Nivelar cama*), limpia la boquilla y **retira objetos de la cama**.
2. **Herram. → Malla 5x5**. Elige la temperatura de calibración (−/+ 5 °C; 0 = no calentar) → **Iniciar medición** → confirmar.
3. Calienta, espera 30 s, hace `G28 L0` y va al punto 1.
4. Baja Z con ▼ hasta que el papel roce con ligera resistencia (pasos **1,00 / 0,10 / 0,05 / 0,01 mm**). **Confirmar** registra la altura real. Pasa solo al siguiente punto.
5. **Repetir**: toca un punto ya medido en el mapa y pulsa *Repetir*.
6. Con los 25 puntos: revisión (azul = más baja, ámbar = más alta; máx, mín, rango, desviación) → **Guardar** (valida, activa y ejecuta `M500`).
7. **Cancelar** (o ◄) descarta las mediciones y **conserva la última malla válida**.

Botones de la pantalla principal: **Iniciar medición · Activar/Desactivar · Guardar · Cargar**. Después de cada `G28` la malla se activa sola (`ENABLE_LEVELING_AFTER_G28`). Ajusta el **Z-offset en el centro de la cama**. Límite de seguridad: Z entre −2,00 y 10 mm; los pasos gruesos no bajan de Z = 0,5 mm.

### Filamento
**Cargar** y **Descargar** calientan el hotend a la temperatura de filamento, avisan cuando llega y piden confirmación antes de mover el filamento. *Cancelar* detiene el proceso.

### Más
**G-code**: escribe un comando y muestra el resultado. Los accesos personalizados (`MAIN_MENU_ITEM_n_GCODE`) están vacíos por defecto.

---

## 6. Ajustes

### Parámetros
Tocar una fila abre el **teclado numérico** (muestra el nombre del parámetro; *Cancelar* sale sin cambiar; *Aceptar* se deshabilita si no es un número). Los ajustes de Marlin viven en RAM hasta **Memoria → Guardar**. Excepción: la **posición de pausa**, el **filamento** (temperatura, longitudes y velocidades) y las **posiciones de nivelado** son ajustes de la pantalla: se guardan al aceptar el valor, en su propia memoria, y **no** dependen de `M500` ni de la EEPROM de Marlin.

| Grupo | Contenido |
|---|---|
| **Máquina** | **Aceleración** (impresión, retracción, vacío y por eje) · **Jerk** (con Junction Deviation el jerk clásico no se usa: ajusta JD en *Rendimiento*) · **Velocidad máxima** por eje |
| **Motor** | **Pasos por mm** · **Corriente TMC** · **Modo de paso TMC** (StealthChop / SpreadCycle por eje) · *Sensibilidad de home* (solo existe con homing sin sensores: aquí está desactivado y no aparece) |
| **Nivelación** | **Offset Z** · **Posiciones de nivelado** (los 5 puntos de *Nivelar cama*) · **Asistente de Z offset** |
| **Avanzado** | **Filamento** (temperatura, longitudes y velocidades de carga/descarga) · **Posición de pausa** (X, Y, Z) · **Perilla** (encoder) · **Wi-Fi** |

### Memoria
**Guardar** (`M500`), **Leer** (`M501`) y **Restaurar valores de fábrica** (botón rojo, pide confirmación). Solo afectan a los ajustes de Marlin (los de la pantalla, ver *Parámetros*, van aparte). Restaurar **borra tus ajustes de la EEPROM**: antes anota `M503`.

### Ventiladores
Porcentaje de cada ventilador.

### Sistema
Datos de la máquina: nombre, **Marlin**, **Interfaz ARQUIS**, placa, compilación, tiempo encendida, nº de impresiones y tiempo total. **Diagnóstico en vivo:** sensor de hotend y de cama (una lectura anómala sale en rojo como «Revisar»), finales de carrera X/Y/Z (*Activado/Abierto*), tarjeta SD, **malla de cama** (*Activa / Inactiva / Sin malla*) y estado de Marlin (*Normal / Detenido*).

### PID 🆕 (autoajuste de hotend y cama)
Guía completa: `docs/arquis/PID.md`.
1. Elige **Hotend** o **Cama**, la temperatura (−/+ 5 °C) y los ciclos (−/+ 1). Por defecto: hotend 210 °C / 8 ciclos; cama 60 °C / 6 ciclos.
2. **Iniciar autoajuste** → confirmar. **Se apagan todos los calentadores al empezar** y tarda varios minutos.
3. **Cancelar** detiene el ajuste sin cambiar nada.
4. Al terminar ves **Kp, Ki, Kd nuevos y (anteriores)**: **Aplicar y guardar** (`M301`/`M304` + `M500`) o **Descartar**.

Si tu EEPROM es de una versión anterior, la primera vez tras flashear esta **se restaura a fábrica** (el PID de cama y la malla 5×5 cambian su tamaño): anota `M503` antes.

### Rendimiento 🆕 (velocidad y calidad)
Guía completa: `docs/arquis/RENDIMIENTO.md`. Cada tarjeta es un valor; se toca, se cambia con − / + y **se aplica al instante**. Tocar el valor central cambia entre paso **fino y grueso**.

| Tarjeta | Orden | Rango | Para qué sirve |
|---|---|---|---|
| Acel. impresión | `M204 P` | 200–20000 mm/s² | Rapidez al cambiar de dirección imprimiendo |
| Acel. vacío | `M204 T` | 200–20000 mm/s² | Idem sin extruir |
| Acel. máx X / Y | `M201 X` / `M201 Y` | 500–20000 mm/s² | **Techo** de las dos anteriores |
| Linear Advance K | `M900 K` | 0,005–2,000 (OFF) | Presión en esquinas y costuras |
| Junction Deviation | `M205 J` | 0,01–0,3 mm | Cuánto frena en esquinas |
| Shaping X / Y | `M593 X F` / `M593 Y F` | **desde 25 Hz** hasta 200 (OFF) | Input Shaping (ringing) |

**Guardar** (`M500`) · **Restaurar** (vuelve a lo guardado). Los cambios sin guardar se pierden al apagar y se marcan con `*`. Solo se edita con la impresora quieta. Si pides más aceleración que el techo, Marlin aplica el techo (la línea de estado lo avisa).

### Wi-Fi
Estado (IP, red) y lista de redes; la conectada se ve en verde.

### Apagar
Corta la alimentación (o desactiva los motores si la placa no controla la alimentación), con confirmación.

> En esta placa no aparece la tarjeta **Idioma** (el idioma de la interfaz es español fijo). La rejilla de Ajustes admite 8 tarjetas y hoy hay exactamente 8.

---

## 7. Valores de fábrica del firmware (XQ1)
Son los de `Configuration.h` / `Configuration_adv.h`; **tu EEPROM puede tener otros** (los que guardaste con `M500`). Consulta los reales con `M503` o en *Parámetros*.

| Concepto | Valor |
|---|---|
| Placa / drivers | MKS Robin Nano V3.1 · TMC2209 · 32 microsteps |
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

---

## 8. Dónde se ajusta cada cosa

| Quiero… | Pantalla | Orden de Marlin | ¿Se guarda con Guardar/M500? |
|---|---|---|---|
| Nivelar la cama con la hoja | **Herram. → Malla 5x5** | (interna; `G28 L0`) | Sí (malla en la EEPROM) |
| Ajustar los tornillos | Herram. → Nivelar cama | — | — |
| Calibrar Z con el papel / Z-offset | Ajustes → Parámetros → Nivelación; **Babystep** (imprimiendo) | `M290`, offsets | Babystep sí, al volver |
| Autoajuste PID | Ajustes → PID | `M303` → `M301`/`M304` | Sí |
| Aceleración | Ajustes → **Rendimiento** o Parámetros → Máquina | `M204` / `M201` | Sí (con Guardar) |
| Linear Advance K | Ajustes → Rendimiento | `M900` | Sí |
| Input Shaping | Ajustes → Rendimiento | `M593` | Sí |
| Pasos/mm | Parámetros → Motor | `M92` | Sí |
| Corriente de motores / SpreadCycle | Parámetros → Motor | `M906` / `M569` | Sí |
| Velocidad o flujo durante la impresión | Opciones → Velocidad | `M220` / `M221` | No (solo ese trabajo) |
| Temperaturas | Temp. | `M104` / `M140` | No |
| Posición de pausa, longitudes/velocidades de filamento, posiciones de nivelado | Parámetros → Avanzado / Nivelación | (ajustes de la pantalla) | **No con M500:** se guardan al aceptar en la memoria de la propia pantalla |
| Ver los valores actuales | Consola (Más → G-code) | `M503` | — |
| Comprobar una torre de prueba | SD → `torre_*.gcode` | ver `RENDIMIENTO.md` | No (terminan con `M501`) |

---

## 9. Varias impresoras con un solo firmware (perfiles) 🆕
Los valores de cada impresora viven en `Marlin/machine/<nombre>.h` y se eligen con el entorno de PlatformIO:

```bash
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc   # Arquis XQ1
pio run -e aquila_d1                                 # Aquila D1 (cama 230×240, Z 240)
```

El nombre que ve el usuario (*Ajustes → Sistema → Máquina*) es el del perfil. El XQ1 no cambia. Para **actualizar sin tocar tus parámetros**: `git pull` y volver a compilar tu entorno; los datos de **tu EEPROM** son de tu placa y solo se pierden si cambia el tamaño de la EEPROM (anótalos con `M503`). Cómo añadir otra impresora: `docs/arquis/MAQUINAS.md`.

---

## 10. Primera puesta a punto (orden recomendado)
1. **Antes de flashear:** conecta por USB y guarda `M503`.
2. **Flashear** (ver §11) y comprobar *Ajustes → Sistema* (Marlin e Interfaz ARQUIS).
3. **Reaplicar tus ajustes** (steps, corrientes…) y **Memoria → Guardar**.
4. **Ejes y finales de carrera:** *Home todo* con la cama despejada; mira *Sistema → Diagnóstico*.
5. **Tornillos** (*Nivelar cama*) y luego **Malla 5x5**.
6. **PID** de hotend y de cama.
7. **Z-offset** en el centro con el papel / primera capa.
8. **Rendimiento:** torres de prueba en este orden: **caudal → frecuencia de shaping → K → aceleración**.
9. Imprimir un trabajo corto de prueba **con la mano cerca del paro**.

---

## 11. Actualizar el firmware (resumen)
```bash
git pull
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc
```
Copia `Robin_nano_v3.bin` y la carpeta `assets` (la de `sd_assets/` de este repositorio) a la **raíz** de la SD, con la impresora apagada y fría, e insértala al encender. La primera vez tarda más (copia los recursos); **no apagues**. Guía completa, problemas frecuentes, logo de arranque y alternativa por USB/pendrive: `docs/arquis/GUIA_INSTALACION.md`.

---

## 12. Seguridad
* Haz *Home*, *Nivelar cama*, *Malla 5x5* y *Mover* con la **cama despejada**.
* Si un sensor sale en **Revisar** (Sistema), revisa termistor y cable **antes de calentar**.
* Las protecciones térmicas de Marlin (`THERMAL_PROTECTION_*`, `WATCH_PID`) siguen activas; la interfaz no las cambia.
* **Rendimiento:** sube la aceleración de poco en poco; una aceleración demasiado alta pierde pasos o golpea el cabezal. La **máxima del eje es el techo**.
* Una torre de prueba o un cambio en pantalla **no se guardan** hasta que tú lo hagas.
* Ante cualquier comportamiento inesperado, **apaga la impresora**.

## 13. Problemas frecuentes
| Síntoma | Qué hacer |
|---|---|
| Sale el logo de Makerbase al arrancar | La carpeta `assets` no es la de `sd_assets/` de este repositorio (`GUIA_INSTALACION.md`) |
| `pio` no se reconoce | Instala Git y PlatformIO, o usa la terminal de PlatformIO en VS Code |
| `TMCStepper.h: No such file` al compilar | Borra `.pio/libdeps` y `~/.platformio/.cache`, o usa otro `PLATFORMIO_WORKSPACE_DIR` |
| Tras flashear, los ajustes vuelven a fábrica | Cambió el tamaño de la EEPROM: reaplica y `Guardar` (anota `M503` antes de flashear) |
| La malla no se aplica | *Sistema → Malla de cama*: ¿Activa? Se activa tras cada `G28`; si no, *Malla 5x5 → Activar* |
| «No se pudo cambiar» en Rendimiento | La impresora no está quieta (imprimiendo, moviéndose, PID o malla en curso) |
| Shaping no deja bajar de 25 Hz | Es el mínimo del búfer compilado; ver `RENDIMIENTO.md` §1 |
| «Cancelación bloqueada» en Objetos | Es intencionado hasta validar la cancelación real (§3.1) |

## 14. Documentación relacionada
`GUIA_USUARIO.md` (recorrido ilustrado de la interfaz v1.0.0) · `GUIA_INSTALACION.md` · `MALLA_5x5.md` · `PID.md` · `RENDIMIENTO.md` · `MAQUINAS.md` · `LISTA_VERIFICACION.md` (pruebas en la impresora) · `../slicer_suite/` (Cura: M486, gestor de objetos, no planar) · `../../tests/README.md` (pruebas automáticas).
