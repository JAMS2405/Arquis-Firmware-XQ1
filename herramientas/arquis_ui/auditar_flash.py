#!/usr/bin/env python3
"""Auditoria de Flash/RAM del firmware compilado. Ejecutar DESPUES de `pio run` en tu PC.
Uso:  python herramientas/arquis_ui/auditar_flash.py [ruta/a/firmware.elf]
      (por defecto busca .pio/build/mks_robin_nano_v3_1_usb_flash_drive_msc/firmware.elf)
Muestra: tamano por seccion, margen libre, los 40 simbolos mas grandes, y el reparto por modulo
(LVGL, ARQUIS UI, idiomas MKS, WiFi, fuentes, resto de Marlin). Los grupos se deducen del NOMBRE del simbolo.
Necesita arm-none-eabi-nm/size (vienen con PlatformIO en ~/.platformio/packages/toolchain-gccarmnoneeabi/bin)."""
import os, subprocess, sys, shutil, glob, re
elf = sys.argv[1] if len(sys.argv) > 1 else '.pio/build/mks_robin_nano_v3_1_usb_flash_drive_msc/firmware.elf'
if not os.path.exists(elf):
    sys.exit(f'No existe {elf}. Compila primero con: pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc')
def tool(n):
    p = shutil.which(n)
    if p: return p
    for c in glob.glob(os.path.expanduser('~/.platformio/packages/toolchain-gccarmnoneeabi*/bin/' + n + '*')) + glob.glob(os.path.expandvars('%USERPROFILE%/.platformio/packages/toolchain-gccarmnoneeabi*/bin/' + n + '*')):
        return c
    sys.exit(f'No encuentro {n}. Anade la carpeta bin de toolchain-gccarmnoneeabi al PATH.')
NM, SIZE = tool('arm-none-eabi-nm'), tool('arm-none-eabi-size')
FLASH_TOTAL = 475136; RAM_TOTAL = 131072   # los mismos limites que informa PlatformIO para esta placa (512 KB - 48 KB de offset; 128 KB RAM)

print('== Secciones (arm-none-eabi-size) ==')
out = subprocess.check_output([SIZE, '-A', elf]).decode()
print(out)
t, d, b = [int(x) for x in subprocess.check_output([SIZE, elf]).decode().splitlines()[1].split()[:3]]
flash, ram = t + d, d + b                         # igual que PlatformIO: Flash = text+data ; RAM = data+bss
print(f'Flash usada = {flash} B de {FLASH_TOTAL} B ({100.0*flash/FLASH_TOTAL:.1f} %)  -> margen libre {FLASH_TOTAL-flash} B')
print(f'RAM  usada  = {ram} B de {RAM_TOTAL} B ({100.0*ram/RAM_TOTAL:.1f} %)  (data {d} + bss {b})')

syms = []
for l in subprocess.check_output([NM, '-S', '--size-sort', '-C', elf]).decode().splitlines():
    p = l.split(None, 3)
    if len(p) == 4 and p[2] in 'tTrRdDbBwW':
        syms.append((int(p[1], 16), p[2], p[3]))
syms.sort(reverse=True)
print('\n== 40 simbolos mas grandes ==')
for s, t, n in syms[:40]: print(f'{s:8d}  {t}  {n[:100]}')

GROUPS = [('ARQUIS UI', r'^(arq_|arquis_)'), ('LVGL (nucleo y widgets)', r'^(lv_|_lv_)'), ('Idiomas MKS (tft_Language)', r'(language|Language|_menu_def|disp_language)'),
          ('Fuente china SPI (gb2312)', r'gb2312'), ('WiFi MKS', r'(wifi|WIFI|esp|ESP)'), ('Pantallas MKS (lv_draw_/draw_)', r'(lv_draw_|lv_clear_|draw_|disp_)'),
          ('Planificador/movimiento', r'(Planner|planner|Stepper|stepper|block_)'), ('Temperatura', r'(Temperature|thermal|PID)'), ('G-code', r'(GcodeSuite|gcode|M[0-9]{2,3}\b)'),
          ('HAL/USB/FatFs/HAL STM32', r'(HAL_|USB|usb|FATFS|f_|SPI_|spi)')]
tot = {g: 0 for g, _ in GROUPS}; other = 0
for s, t, n in syms:
    for g, rx in GROUPS:
        if re.search(rx, n): tot[g] += s; break
    else: other += s
print('\n== Reparto aproximado por modulo (segun nombre del simbolo; las funciones sin simbolo no se cuentan) ==')
for g, v in sorted(tot.items(), key=lambda kv: -kv[1]): print(f'{v:8d} B  {g}')
print(f'{other:8d} B  resto')
