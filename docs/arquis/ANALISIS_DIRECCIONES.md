# Análisis técnico: sentido de motores dinámico (un BIN, dos perfiles)

Estado: **PENDIENTE DE AUTORIZACIÓN.** No se ha modificado ningún componente protegido. Este documento es la presentación previa que exige el encargo.

## 1. Estado real del repositorio (HEAD `a98dd6a`)

| Tema | Hoy |
|---|---|
| Perfiles | **En tiempo de compilación**: `Marlin/machine/{xq1,aquila_d1}.h` + `ini/arquis.ini` (`-e aquila_d1`). Son **dos BIN**, no uno. |
| Sentidos XQ1 | `Configuration.h:1724-1744`: X true, Y true, Z false, E0 false. |
| Sentidos Aquila D1 | `aquila_d1.h`: X false, Y false, E0 true. **Z no está declarado** (hereda `false` del XQ1). |
| `shaft` TMC2209 | No se usa en ningún sitio del código (`grep shaft` en `module/`, `feature/`, `gcode/` = 0 resultados). |
| Detección de reinicio de driver | Ninguna. `MONITOR_DRIVER_STATUS` y `TMC_DEBUG` están desactivados; Marlin no relee `GSTAT`/`GCONF`. |
| UART TMC | Un hilo por driver (X=PD5, Y=PD7, Z=PD4, E0=PD9), serie por software. Un driver puede reiniciarse solo y los demás no. |
| Asistente inicial, Ajustes → Máquina, P1–P4 de Fase 5 | **No existen en este repositorio** (solo ramas `main` y `claude/wonderful-dijkstra-iuo3w9`, mismo historial). |

## 2. Por qué no se puede hacer dinámico sin tocar `stepper.cpp`

Marlin usa `INVERT_*_DIR` con `ENABLED()/TERN0()`, que pegan el valor como *token* (`_ENA_true`/`_ENA_false`). Hace falta un literal `true`/`false`; una variable en RAM **no compila**. Usos:

* `stepper.cpp:3001-3009` — dirección inicial tras `Stepper::init`.
* `stepper.cpp:3296` — `_INVERT_DIR()` del babystepping.
* `stepper.cpp:3441-3446` — dirección durante el babystep de Z (X/Y/Z).
* `stepper.cpp` (`set_directions` / `X_APPLY_DIR`…) — cada bloque nuevo.
* `stepper/indirection.h:499-546` — `NORM_E_DIR` / `REV_E_DIR` (extrusor, retracción, `stepper.cpp:617-630, 2248-2257`).

`indirection.h` pertenece al mismo componente que `stepper.cpp`; lo trato como protegido.

## 3. Alternativas

### A. `shaft` del TMC2209 (sin tocar componentes protegidos)
Se deja `INVERT_*_DIR` fijo en compilación y se invierte el motor con `GCONF.shaft` por UART según el perfil.

* Es equivalente físicamente, pero **la inversión vive en el driver, no en el microcontrolador**.
* **P5 queda abierto.** Un reinicio del TMC2209 (caída de VM, ruido, ESD, sobretemperatura) devuelve `GCONF` a su valor por defecto, `shaft=0`, y el motor gira al revés **sin que Marlin lo sepa**. En Z puede estrellar la boquilla contra la cama; en E extruye al revés.
* No se puede cerrar con sondeo: entre el reinicio y la siguiente consulta hay ventana de movimiento erróneo, y consultar por UART por software (57 600 baud, ~décimas de ms por byte, con interrupciones de temporizador) durante la impresión es justo lo que se pide evitar (microparadas).
* Necesita además un hilo de relectura fuera de los ficheros protegidos y una parada de emergencia ante discrepancia. Reduce la ventana, no la elimina.

### B. Máscara de inversión en el microcontrolador (recomendada; **requiere autorización**)
Se sustituye el valor de compilación por una máscara `arq_dir_inv` (1 byte: bits X, Y, Z, E0) que carga el perfil **antes** de `stepper.init()`. El orden de arranque ya lo permite: `settings.first_load()` (`MarlinCore.cpp:1336`) precede a `stepper.init()` (`MarlinCore.cpp:1370`).

Cambio mínimo y localizado:
* `stepper.cpp`: los 4 usos de arriba leen `arq_dir_inv` en lugar del literal.
* `indirection.h`: `NORM_E_DIR/REV_E_DIR` del extrusor 0 leen el bit E0.
* **No** se toca: generación de pasos, ISR, planner, Bresenham, temporización del pulso, input shaping, colas, endstops.
* Coste: una lectura de byte y un XOR **por cambio de sentido de bloque**, no por paso. Sin pausas.

Efectos:
* Sentido lógico de Marlin intacto → homing, finales de carrera, movimiento manual, extrusión e impresión no cambian de significado. Una sola inversión, en el pin DIR.
* `shaft` se deja siempre en 0 (valor de reinicio del driver) → **un reinicio del TMC ya no puede invertir el motor**. Es lo que cierra P5 en la parte de dirección.
* El sentido sigue en el perfil y se reinicia con él; cambiar de perfil exige reiniciar (se aplica antes de `stepper.init()`), nunca en caliente con motores activos.
* Hasta que el perfil esté cargado y validado, los motores permanecen sin energía y no se aceptan movimientos.

### C. Dos binarios
Descartado por el encargo.

## 4. Resto de P5 (independiente de A/B)
Un reinicio del TMC también pierde corriente, micropasos y `CHOPCONF`. Con B ya no invierte el sentido, pero sí puede dejar el motor sin corriente o con el micropaso erróneo (pérdida de pasos, no inversión). Eso se trata con una comprobación de `GSTAT.reset` fuera de la ruta de pasos (solo en reposo / pausas de G-code, nunca dentro de la ISR) y parada limpia si se detecta durante una impresión. Se diseñará tras la decisión de §3.

## 5. Datos que faltan (no se inventan)
* **Aquila D1: Z, corrientes, velocidades, aceleraciones X/Z, `*_CURRENT_HOME`, chopper.** El perfil actual solo declara lo que difiere del XQ1 y hereda el resto. Necesito su `Configuration.h`/`Configuration_adv.h` original (`Aquila_D1.zip`, no está en el repositorio) para confirmar `INVERT_Z_DIR` y el resto. Hasta entonces **no libero el perfil Aquila**.
* La rama/repositorio donde está la Fase 5 (P1–P4, asistente, Ajustes → Máquina), o confirmación de que hay que construirlo aquí.
* Compilación: este entorno no tiene PlatformIO ni `arm-none-eabi-gcc`; nada de lo posterior se declarará compilado hasta hacerlo.

## 6. Decisión que se solicita
1. ¿Autorizas el cambio mínimo B en `stepper.cpp` e `indirection.h`? (Recomendado.)
2. Si no: se hace A y P5 se documenta como **abierto** con riesgo residual, no como resuelto.
