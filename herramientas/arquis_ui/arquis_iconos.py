#!/usr/bin/env python3
"""Genera los iconos ARQUIS como mascaras de transparencia de 4 bits (LV_IMG_CF_ALPHA_4BIT, 40x40, 800 bytes c/u).
Se compilan dentro del firmware (const -> Flash, 0 bytes de RAM) y se tiñen con el color del tema en tiempo de ejecucion,
asi un mismo icono sirve en azul, blanco, verde, rojo o gris sin archivos extra en la SD.
Uso:  python arquis_iconos.py   -> escribe ../../Marlin/src/lcd/extui/mks_ui/arquis_icons.c y arquis_icons.h
Requiere: pip install cairosvg pillow numpy"""
import os, math, io
import numpy as np
from PIL import Image
import cairosvg
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, '..', '..', 'Marlin', 'src', 'lcd', 'extui', 'mks_ui'))
N = 40   # px finales
S = 'fill="none" stroke="#fff" stroke-width="3.4" stroke-linecap="round" stroke-linejoin="round"'
F = 'fill="#fff" stroke="none"'

def gear():
    # Engranaje real: 8 dientes trapezoidales sobre un cuerpo circular + agujero central
    pts = []
    for i in range(8):
        t = i * math.pi / 4
        for dr, r in ((-0.23, 14.5), (-0.14, 19.5), (0.14, 19.5), (0.23, 14.5)):
            pts.append((24 + r * math.cos(t + dr), 24 + r * math.sin(t + dr)))
    d = 'M' + ' L'.join(f'{x:.2f} {y:.2f}' for x, y in pts) + ' Z'
    return f'<g {S}><path d="{d}"/><circle cx="24" cy="24" r="6"/></g>'

def fan():
    b = ''.join(f'<path transform="rotate({r} 24 24)" d="M24 20.5c-1-7 3-13 10-12 2 6-1 11-10 12z"/>' for r in (0, 120, 240))
    return f'<g {S}><circle cx="24" cy="24" r="19.5" stroke-width="2.6"/><circle cx="24" cy="24" r="2.6" {F}/>{b}</g>'

ICONS = {
 'folder':  f'<g {S}><path d="M6 14.5a3 3 0 0 1 3-3h9l4.5 5H39a3 3 0 0 1 3 3V37a3 3 0 0 1-3 3H9a3 3 0 0 1-3-3z"/></g>',
 'move':    f'<g {S}><path d="M24 6v36M6 24h36M18 12l6-6 6 6M18 36l6 6 6-6M12 18l-6 6 6 6M36 18l6 6-6 6"/></g>',
 'temp':    f'<g {S}><path d="M19.5 25.5V10a4.5 4.5 0 0 1 9 0v15.5a9 9 0 1 1-9 0z"/><path d="M24 16v17"/><circle cx="24" cy="35" r="3.4" {F}/></g>',
 'gear':    gear(),
 'print':   f'<g {S}><rect x="7" y="5.5" width="34" height="37" rx="3"/><path d="M7 15.5h34M7 35h34"/><path d="M20 15.5h8l-2.2 8h-3.6z" fill="#fff"/></g>',
 'nozzle':  f'<g {S}><path d="M15 5.5h18v9H15zM17.5 14.5h13l-3.5 9h-6z"/><path d="M24 29c4.5 5.5 6 8 6 10.5a6 6 0 0 1-12 0c0-2.5 1.5-5 6-10.5z"/></g>',
 'bed':     f'<g {S}><path d="M5 31h38l-4.5 9h-29z"/><path d="M15 25c-2.5-3.5 2.5-6 0-10M24 25c-2.5-3.5 2.5-6 0-10M33 25c-2.5-3.5 2.5-6 0-10"/></g>',
 'fan':     fan(),
 'play':    f'<g {S}><path d="M14 9.5l24 14.5-24 14.5z"/></g>',
 'pause':   f'<g {S}><rect x="12.5" y="10" width="7" height="28" rx="1.5"/><rect x="28.5" y="10" width="7" height="28" rx="1.5"/></g>',
 'stop':    f'<g {S}><rect x="11" y="11" width="26" height="26" rx="3.5"/></g>',
 'back':    f'<g {S}><path d="M30 9L15 24l15 15"/></g>',
 'check':   f'<g {S}><path d="M9 25l10 10 20-22"/></g>',
 'close':   f'<g {S}><path d="M12 12l24 24M36 12L12 36"/></g>',
 'warn':    f'<g {S}><path d="M24 6.5L43 39.5H5z"/><path d="M24 18.5v10"/><circle cx="24" cy="34" r="1.9" {F}/></g>',
 'level':   f'<g {S}><circle cx="24" cy="24" r="10"/><circle cx="24" cy="24" r="2.6" {F}/><path d="M24 4v8M24 36v8M4 24h8M36 24h8"/></g>',
 'tools':   f'<g {S}><path d="M34 6a10 10 0 0 0-9.5 13L7 36.5A4.2 4.2 0 0 0 12.500 42L30 24.500A10 10 0 0 0 42 14l-6.500 5-5-1-1-5z"/></g>',
 'wifi':    f'<g {S}><path d="M5 18.5a27 27 0 0 1 38 0M11.500 25.500a18 18 0 0 1 25 0M18 32.500a9 9 0 0 1 12 0"/><circle cx="24" cy="39" r="2.4" {F}/></g>',
 'home':    f'<g {S}><path d="M6 24L24 8l18 16M11 21v19h26V21"/><path d="M20 40V30h8v10"/></g>',
 'file':    f'<g {S}><path d="M11 5.500h18l9 9V42H11z"/><path d="M29 5.500v9h9"/><path d="M17 26h14M17 33h10"/></g>',
 'speed':   f'<g {S}><path d="M7 33a17 17 0 1 1 34 0"/><path d="M24 33l8-11"/><circle cx="24" cy="33" r="2.6" {F}/></g>',
}

S2 = 'fill="none" stroke="#fff" stroke-width="4.6" stroke-linecap="round" stroke-linejoin="round"'
SMALL = {   # 20x20: ventiladores (trazo mas grueso para que se lea a tamano pequeno)
 'fan':      f'<g {S2}><circle cx="24" cy="24" r="19"/><path transform="rotate(0 24 24)" d="M24 21c0-8 6-13 12-9-1 7-6 9-12 9z"/><path transform="rotate(120 24 24)" d="M24 21c0-8 6-13 12-9-1 7-6 9-12 9z"/><path transform="rotate(240 24 24)" d="M24 21c0-8 6-13 12-9-1 7-6 9-12 9z"/></g>',
 'heatsink': f'<g {S2}><path d="M6 41h36M12 41V9M21 41V9M30 41V9M39 41V9"/></g>',
 'aux':      f'<g {S2}><rect x="6" y="6" width="36" height="36" rx="6"/><circle cx="24" cy="24" r="9"/><circle cx="24" cy="24" r="2.5" fill="#fff" stroke="none"/></g>',
}
N2 = 20

def render(svg, N=N):
    xml = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48" width="{N*8}" height="{N*8}">{svg}</svg>'
    png = cairosvg.svg2png(bytestring=xml.encode())
    a = Image.open(io.BytesIO(png)).convert('RGBA').split()[3]
    return np.asarray(a.resize((N, N), Image.LANCZOS), dtype=np.uint8)

def pack4(a):  # filas alineadas a byte, nibble alto primero (formato LV_IMG_CF_ALPHA_4BIT)
    q = np.rint(a.astype(np.float32) / 17.0).astype(np.uint8)      # 0..15
    rows = []
    for r in q:
        row = list(r) + ([0] if len(r) % 2 else [])
        rows.append(bytes((row[i] << 4) | row[i+1] for i in range(0, len(row), 2)))
    return b''.join(rows)

previews = {k: render(v) for k, v in ICONS.items()}
data = {k: pack4(a) for k, a in previews.items()}
prev_s = {k: render(v, N2) for k, v in SMALL.items()}
data_s = {k: pack4(a) for k, a in prev_s.items()}
names = list(ICONS)
stride = (N + 1) // 2
c = ['/* ARQUIS UI - iconos 40x40, mascara de transparencia de 4 bits. ARCHIVO GENERADO por herramientas/arquis_ui/arquis_iconos.py: no editar a mano. */',
     '#include "arquis_icons.h"', '']
for k in names:
    d = data[k]
    c.append(f'static const uint8_t arq_icon_{k}_map[] = {{')
    for i in range(0, len(d), stride):
        c.append('  ' + ','.join(f'0x{x:02x}' for x in d[i:i+stride]) + ',')
    c.append('};')
    c.append(f'const lv_img_dsc_t arq_icon_{k} = {{ .header = {{ .cf = LV_IMG_CF_ALPHA_4BIT, .always_zero = 0, .reserved = 0, .w = {N}, .h = {N} }}, .data_size = {len(d)}, .data = arq_icon_{k}_map }};')
    c.append('')
stride2 = (N2 + 1) // 2
for k in SMALL:
    d = data_s[k]
    c.append(f'static const uint8_t arq_icon_s_{k}_map[] = {{')
    for i in range(0, len(d), stride2):
        c.append('  ' + ','.join(f'0x{x:02x}' for x in d[i:i+stride2]) + ',')
    c.append('};')
    c.append(f'const lv_img_dsc_t arq_icon_s_{k} = {{ .header = {{ .cf = LV_IMG_CF_ALPHA_4BIT, .always_zero = 0, .reserved = 0, .w = {N2}, .h = {N2} }}, .data_size = {len(d)}, .data = arq_icon_s_{k}_map }};')
    c.append('')
open(os.path.join(OUT, 'arquis_icons.c'), 'w').write('\n'.join(c))
h = ['/* ARQUIS UI - iconos (generado por herramientas/arquis_ui/arquis_iconos.py) */', '#pragma once', '#include "lvgl.h"', '#ifdef __cplusplus', 'extern "C" {', '#endif', '',
     f'#define ARQ_ICON_SIZE {N}', '']
h += [f'extern const lv_img_dsc_t arq_icon_{k};' for k in names]
h += ['', f'#define ARQ_ICON_SMALL {N2}', '// Iconos pequenos (20 px): ventiladores'] + [f'extern const lv_img_dsc_t arq_icon_s_{k};' for k in SMALL]
h += ['', '#ifdef __cplusplus', '}', '#endif', '']
open(os.path.join(OUT, 'arquis_icons.h'), 'w').write('\n'.join(h))
# hoja de contacto
cols = (len(names) + 1) // 2
sheet = Image.new('RGB', (cols * 56 + 16, 2 * 56 + 16), (0x15, 0x1D, 0x26))
for i, k in enumerate(names):
    a = Image.fromarray(previews[k], 'L')
    col = Image.new('RGB', (N, N), (0x30, 0xAC, 0xE3))
    sheet.paste(col, (8 + (i % cols) * 56, 8 + (i // cols) * 56), a)
sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(os.path.join(HERE, 'iconos_hoja.png'))
print('OK', len(names), 'iconos 40 px +', len(SMALL), 'de 20 px,', len(names) * N * stride + len(SMALL) * N2 * stride2, 'bytes de Flash')
