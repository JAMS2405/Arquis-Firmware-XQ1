# Un solo firmware para Arquis XQ1 y Aquila D1

Desde la Fase 4 hay **un único `Robin_nano_v3.bin`** para las dos impresoras. El modelo **no se elige al compilar**: se elige en la pantalla.
- **Primera instalación:** lo elige el asistente del primer arranque.
- **Después:** se cambia en **Ajustes → Máquina**.

La elección se guarda en la EEPROM. Instalación paso a paso: [`INSTALACION_FIRMWARE_UNICO.md`](INSTALACION_FIRMWARE_UNICO.md). Detalles técnicos: [`FASE4_IMPLEMENTACION_FINAL.md`](FASE4_IMPLEMENTACION_FINAL.md).

## Qué cambia entre las dos máquinas y dónde vive
| Dato | XQ1 | Aquila D1 | Dónde se guarda |
|---|---|---|---|
| Modelo | Arquis XQ1 | Aquila D1 | Registro ARQUIS (EEPROM) |
| Cama X × Y, altura Z (mm) | 300 × 300, 450 | 230 × 240, 240 | Registro ARQUIS |
| Lógica de los finales X / Y / Z | invertida / invertida / invertida | normal / normal / invertida | Registro ARQUIS |
| Sentido de los motores X / Y / Z / E0 | invertido / invertido / normal / normal | normal / normal / normal / invertido | Registro ARQUIS. Se aplica con el bit `GCONF.shaft` del TMC2209 |
| Pasos/mm Z / E | 791,74 / 748,72 | 803,41 / 781,08 | EEPROM de Marlin (`M92`) |
| Velocidad máx. Z, aceleración máx. Y / Z | 5 mm/s*, 4500 / 100* | 45 mm/s, 3500 / 150 | EEPROM de Marlin (`M203` / `M201`) |

\* XQ1: Z a 5 mm/s y 100 mm/s² es la **prueba pendiente de validar** («Z no subía al imprimir»). En la pantalla aparece con un asterisco.

**Es común a las dos máquinas (va fijo en el BIN y no se edita desde la pantalla):**
- placa MKS Robin Nano V3.1, pantalla TS35 y perilla;
- TMC2209 por UART, 32 micropasos, *chopper* de 24 V;
- termistores, temperaturas máximas (285 / 115 °C) y protecciones térmicas;
- homing al mínimo con origen 0;
- USB CDC/COM, Wi-Fi, microSD.

Los valores de fábrica de cada modelo están en la tabla `profiles` de `Marlin/src/lcd/extui/mks_ui/arquis_machine_logic.cpp`. `Marlin/machine/xq1.h` y `aquila_d1.h` se conservan como **referencia** (el firmware ya no los incluye). `tests/machines/test_profiles.py` comprueba que la tabla coincide con ellos y con `Configuration*.h`.

## Cómo se compila
```bash
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc
```
Ese único entorno produce el BIN de las dos máquinas. El entorno `aquila_d1` **ya no existe**.

El nombre del entorno se conserva por compatibilidad, aunque el BIN ya no lleva USB MSC ni USB host (F3-08).

## EEPROM
- **Registro ARQUIS:** 32 bytes con 2 copias, en las direcciones 0x0F00 y 0x0F80 de la EEPROM I²C de 4 KB, aparte de los datos de Marlin (100–788). Lleva identificador `ARQM`, versión, longitud, número de secuencia y CRC16.
- **EEPROM de Marlin:** no cambia de estructura ni de versión, así que **una actualización no borra los ajustes**.
- **`M502`:** restaura los valores de fábrica **del modelo elegido**, no siempre los del XQ1.

## Añadir otra impresora
Ver `Marlin/machine/_plantilla.h`. Se añade una fila a la tabla de perfiles, no un entorno de compilación.
