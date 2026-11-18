# ARQUIS UI — Guía de compilación e instalación

Versión de este documento: ARQUIS UI **1.0.0**. Pensada para quien mantiene el firmware (PC con Windows; sirve igual en Linux/macOS).

## 1. Requisitos
- **PlatformIO** (VS Code + extensión, o la línea de comandos `pio`) y **Python 3**.
- Conexión a Internet **la primera vez** (PlatformIO descarga el núcleo STM32, LVGL, QRCode y TMCStepper).
- Una tarjeta SD de 8–32 GB en FAT32.
- Este repositorio, en la rama `claude/nice-gauss-y90u12` (o en `main` cuando se fusione).

## 2. Compilar
Desde la carpeta del repositorio (PowerShell):
```powershell
git pull
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc 2>&1 | Tee-Object -FilePath build.log
```
- Debe terminar en **SUCCESS** con **Flash ≈ 85,5 %** y **RAM ≈ 61,6 %**. El único aviso es el de Marlin sobre los jumpers DIAG (`SENSORLESS_HOMING`), que no impide nada (ver `ENTREGA_v1.0.0.md` §7).
- El firmware queda en `.pio\build\mks_robin_nano_v3_1_usb_flash_drive_msc\Robin_nano_v3.bin` (el `.elf` se llama `Robin_nano_v3.elf`).
- **Si PlatformIO falla con `TMCStepper.h: No such file or directory`** (pasa con algunas unidades externas, p. ej. `F:`), compila con el espacio de trabajo en otra unidad. En la **misma** ventana:
```powershell
$env:PLATFORMIO_WORKSPACE_DIR = "C:\pio_arquis"
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc 2>&1 | Tee-Object -FilePath build.log
```
  El firmware queda entonces en `C:\pio_arquis\build\mks_robin_nano_v3_1_usb_flash_drive_msc\Robin_nano_v3.bin`. Si `pio` no se reconoce, usa la ruta completa: `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e ...`.
- Medir Flash y RAM (opcional): `python herramientas\arquis_ui\auditar_flash.py RUTA\Robin_nano_v3.elf`.

## 3. Armar el paquete para la SD
```powershell
python herramientas\arquis_ui\empaquetar_entrega.py RUTA\Robin_nano_v3.bin --salida entrega
```
Crea `entrega\Arquis-XQ1_UI_v1.0.0\` (con `Robin_nano_v3.bin`, `assets\`, `LEEME.txt` y `SHA256SUMS.txt`) y el `.zip` equivalente. El script **comprueba** que el `.bin` exista, no esté vacío y quepa en la Flash, que `sd_assets` tenga el logo y la fuente, y que no falte ningún recurso que el firmware pide (avisa si falta alguno).

> **Importante — carpeta `assets`:** usa siempre la de este repositorio (`sd_assets\`), **no** la que descarga PlatformIO en `.pio\build\...\assets` (esa trae el logo y los iconos originales de Makerbase). Si al arrancar ves el logo de Makerbase, la carpeta no es la correcta.

## 4. Instalar en la impresora
1. **Guarda una copia del firmware actual** (`.bin`) por si quieres volver atrás.
2. Con la impresora **apagada y fría**, copia a la **raíz** de la tarjeta: `Robin_nano_v3.bin` y la carpeta `assets` (con ese nombre exacto).
3. Inserta la tarjeta en la ranura donde siempre pones la SD para actualizar el firmware y **enciende**.
4. La placa actualiza sola y reinicia. La **primera vez tarda más**: copia los recursos de `assets` a su memoria interna. **No apagues durante ese proceso.**
5. Verás el **logo de ARQUIS** y el **Dashboard**. En *Ajustes → Sistema* debe decir **Interfaz ARQUIS 1.0.0**.
6. Apaga, retira la tarjeta (el firmware queda como `Robin_nano_v3.CUR` en la SD; se puede borrar).
7. Recorre [`LISTA_VERIFICACION.md`](LISTA_VERIFICACION.md) **con la impresora fría y sin filamento**.

Los ajustes guardados en la memoria (EEPROM: pasos, aceleración, PID, offsets…) **se conservan** al actualizar entre versiones de ARQUIS UI, porque no cambian el formato. No hace falta `M502`/`M500`. Si en *Ajustes → Memoria* aparece un problema de versión, no restaures valores de fábrica a ciegas: perderías tus ajustes (anota antes los que te importen; *Ajustes → Parámetros* los muestra).

## 5. Volver atrás
- **A un firmware anterior:** copia su `Robin_nano_v3.bin` a la SD y arranca como en el paso 3 del apartado 4.
- **A la interfaz original de MKS:** comenta `#define ARQUIS_UI` en `Marlin/Configuration.h`, recompila y flashea. (Con `ARQUIS_UI` desactivado compila el código original de MKS con todos sus idiomas.)

## 6. Problemas frecuentes
| Síntoma | Causa y solución |
|---|---|
| Al arrancar sale el **logo de Makerbase** | La carpeta `assets` de la SD no es la de este repositorio. Usa `sd_assets\` renombrada a `assets` |
| **Colores rotos** (fondo verde/oliva, bordes arcoíris) | Con la placa y la librería actuales `ARQUIS_TFT_BYTE_SWAP` debe estar **desactivado** (comentado) en `Configuration.h`; actívalo solo si sin él ocurre eso |
| `TMCStepper.h: No such file…` al compilar | Problema de PlatformIO con la unidad externa: usa `PLATFORMIO_WORKSPACE_DIR` en otra unidad (apartado 2) |
| `pio` no se reconoce en PowerShell | Usa la ruta completa de `pio.exe` (apartado 2) |
| Aviso `Motherboard DIAG jumpers must be removed…` | Es de Marlin y no impide compilar: con finales de carrera físicos los jumpers DIAG de los drivers deben estar quitados. Silenciar solo con `#define DIAG_JUMPERS_REMOVED` una vez comprobado |
| El diagnóstico muestra un sensor en "Revisar" | Esa lectura es ≤ 0 °C o cercana al máximo configurado: revisa el termistor y su cable antes de calentar |
| La cama marca ≈ 45 °C apagada | Pendiente conocido (ver `ENTREGA_v1.0.0.md` §7): comprobar el termistor |
