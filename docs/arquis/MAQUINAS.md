# Varias impresoras con el mismo firmware (perfiles de máquina)

**Problema:** el firmware (pantallas, nivelación 5×5, PID…) lo mantienes tú, pero cada impresora tiene sus medidas, pasos/mm, sentidos de motor, finales de carrera…, y cada `git pull` pisaba esos valores.
**Solución:** esos valores viven en un **perfil por máquina** (`Marlin/machine/<nombre>.h`). El resto del firmware es común. Para actualizar, tu primo hace `git pull` y compila **su entorno**; sus parámetros no se tocan porque nunca están en `Configuration.h`.

## Cómo está montado
| Pieza | Para qué |
|---|---|
| `Marlin/machine/xq1.h` | Arquis XQ1: solo overrides de Z (velocidad 5 mm/s, aceleración 100), prueba por Z que no subía al imprimir; el resto vale lo de `Configuration.h`. |
| `Marlin/machine/aquila_d1.h` | Aquila D1: solo lo que difiere del XQ1 (cama 230×240, Z 240, pasos/mm, sentidos X/Y/E, finales de carrera X/Y, aceleración Y). Sacado de su `Configuration.h`. |
| `Marlin/machine/machine.h` | Selector: elige el perfil según el entorno. |
| `ini/arquis.ini` | Entornos de PlatformIO: `aquila_d1` = el entorno normal + `-DARQUIS_MACHINE_AQUILA_D1`. |
| `Configuration.h` / `_adv.h` | Incluyen el selector **antes** de sus valores; los valores "de máquina" están protegidos con `#ifndef`, así que **gana lo que defina el perfil**. |
Lista de valores que un perfil puede definir: ver `Marlin/machine/_plantilla.h` (medidas, pasos/mm, velocidades y aceleraciones, sentidos, finales de carrera, sensores y temperaturas máximas, PID de hotend y cama, input shaping, chopper, corrientes y micropasos de los TMC).

## Qué hace cada uno
* **Tú (XQ1):** nada cambia. `pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc`. Se comprobó que **las 1266 macros de configuración quedan idénticas** a antes de los perfiles (`tests/machines/test_profiles.py`).
* **Tu primo (Aquila D1):**
  ```bash
  git pull
  pio run -e aquila_d1
  ```
  Y flashea el `.bin` como siempre. Cada actualización nueva = esos dos comandos. Si algo suyo no está en su perfil (p. ej. corrientes de motor o PID distintos), se **añade a `aquila_d1.h`** (no a `Configuration.h`). Todo lo que no aparece en su perfil lo hereda del firmware común.

## Importante: lo que hay dentro de la impresora (EEPROM)
Hay dos capas de "parámetros":
1. **Valores por defecto** (en el firmware): los del perfil. Se mantienen en cada actualización.
2. **Valores guardados en la EEPROM de la placa** (`M500`: Z-offset/home offset, PID afinado, pasos/mm ajustados, babystep…). Un flasheo normal **los conserva**… salvo cuando una actualización **cambia el tamaño de la EEPROM**. Esta actualización lo hace (malla 3×3→5×5 y PID de cama): **la primera vez Marlin descarta la EEPROM y arranca con los valores del perfil**.
Por eso, **antes de flashear esta versión**: conectar y ejecutar `M503`, copiar/guardar la salida (sobre todo `M92`, `M206`/`M851`, `M301`, `M304`, `M204`, `M205`); **después**, reintroducir lo que difiera del perfil y `M500`. Los ajustes propios de la pantalla (idioma, posiciones de tornillos, etc.) están en otra memoria y se conservan. Cuando una versión futura no toque la EEPROM, se avisará en `CAMBIOS.md`.

## Qué se adapta solo a su cama (230×240)
Nivelación 5×5: área de 15 a 215 × 15 a 225 mm (pasos de 50 y 52,5 mm); el mapa de cancelar objeto y el esquema de tornillos usan `X_BED_SIZE`/`Y_BED_SIZE`. Las posiciones de los 5 tornillos se configuran en la pantalla (Ajustes → Parámetros → Nivelación). **PID:** los valores por defecto son los del XQ1; debe hacer el autoajuste de hotend y cama (Ajustes → PID) y guardarlo.

## Añadir otra impresora
1. Copia `Marlin/machine/_plantilla.h` como `Marlin/machine/<nombre>.h` y deja solo las líneas que difieran.
2. En `machine.h`: `#elif defined(ARQUIS_MACHINE_<NOMBRE>)` → `#include "<nombre>.h"`.
3. En `ini/arquis.ini`: copia `[env:aquila_d1]`, cambia nombre y `-D`.
4. `python3 tests/machines/test_profiles.py` comprueba que cada valor está protegido y que el XQ1 no cambia.
**Regla de oro:** lo de la máquina, en su perfil; lo común (funciones nuevas, pantallas), en el firmware.

## Limitaciones
* Cambios futuros que toquen a la vez una línea "de máquina" en `Configuration.h` darían conflicto de `git`; por eso están envueltas en `#ifndef` y los cambios de valores de máquina deben hacerse en el perfil.
* No compilado aquí: tras `git pull`, comprueba en el log de `pio run -e aquila_d1 -v` que aparece `-DARQUIS_MACHINE_AQUILA_D1` y que el nombre de máquina en la pantalla (Sistema) es "Aquila D1".
