#!/usr/bin/env python3
"""Variantes del LOGOTIPO OFICIAL ARQUIS a partir de herramientas/logo_arquis.png (sin redibujarlo ni deformarlo).
  1) Splash 480x320  -> sd_assets/bmp_logo.bin (RGB565 little-endian, fondo grafito #151D26) + vista previa
  2) Marca reducida (simbolo "A" con circuito) 32x32 y 20x20 -> Marlin/.../arquis_logo.c/.h
     La marca es bicolor (azul/verde): se guarda como DOS mascaras de 4 bits (azul y verde) que el firmware tine
     con #30ACE3 y #4CAF53 -> 2 x 512 B (32 px) + 2 x 200 B (20 px) de Flash, sin RAM.
Uso: python arquis_logo.py        Requiere: pillow numpy"""
import os, numpy as np
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SRC = os.path.join(ROOT, 'herramientas', 'logo_arquis.png')
MKS = os.path.join(ROOT, 'Marlin', 'src', 'lcd', 'extui', 'mks_ui')
BG = (0x15, 0x1D, 0x26)
logo = Image.open(SRC).convert('RGBA')

# ---- 1) Splash ------------------------------------------------------------------------------------------
W, H = 480, 320
target_h = 222                                              # alto del logo en el splash (proporcion original)
lw = round(logo.width * target_h / logo.height)
lg = logo.resize((lw, target_h), Image.LANCZOS)
canvas = Image.new('RGBA', (W, H), BG + (255,))
canvas.alpha_composite(lg, ((W - lw) // 2, 22))
d = ImageDraw.Draw(canvas)
font = ImageFont.truetype('/usr/share/fonts/opentype/inter/Inter-Medium.otf', 15)
txt = 'ARQUIS 3D'
tw = d.textlength(txt, font=font)
d.text(((W - tw) / 2, 276), txt, font=font, fill=(0x9B, 0xAA, 0xB8))
d.rectangle([W // 2 - 28, 302, W // 2 + 28, 304], fill=(0x30, 0xAC, 0xE3))      # acento discreto
splash = canvas.convert('RGB')
a = np.asarray(splash, dtype=np.uint16)
v = ((a[..., 0] >> 3) << 11) | ((a[..., 1] >> 2) << 5) | (a[..., 2] >> 3)
open(os.path.join(ROOT, 'sd_assets', 'bmp_logo.bin'), 'wb').write(v.astype('<u2').tobytes())
splash.save(os.path.join(HERE, 'splash_preview.png'))

# ---- 2) Marca reducida (solo el simbolo, sin la palabra ARQUIS) -----------------------------------------------
arr = np.asarray(logo, dtype=np.float32)
rgb, alpha = arr[..., :3], arr[..., 3] / 255.0
sat = (rgb.max(-1) - rgb.min(-1)) / 255.0
mark = (alpha > 0.05) & (sat > 0.35)
mark[215:, :] = False                                       # por debajo esta la palabra (y el acento de la Q)
ys, xs = np.where(mark)
x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
crop_rgb, crop_a = rgb[y0:y1, x0:x1], alpha[y0:y1, x0:x1] * mark[y0:y1, x0:x1]
BLUE_B, GREEN_B = 226.0, 80.0                               # canal B de #25ABE2 y #4CB050: da la mezcla azul<->verde
t = np.clip((crop_rgb[..., 2] - GREEN_B) / (BLUE_B - GREEN_B), 0, 1)
a_blue, a_green = crop_a * t, crop_a * (1 - t)

def mask(a, n):
    h, w = a.shape; s = n / max(h, w)
    im = Image.fromarray((a * 255).astype(np.uint8), 'L').resize((max(1, round(w * s)), max(1, round(h * s))), Image.LANCZOS)
    can = Image.new('L', (n, n), 0); can.paste(im, ((n - im.width) // 2, (n - im.height) // 2))
    # refuerzo de contraste en reduccion: los trazos finos del circuito no se pierden a 32/20 px
    f = np.asarray(can, dtype=np.float32) / 255.0
    q = np.rint(np.clip(f ** 0.55, 0, 1) * 15).astype(np.uint8)
    rows = []
    for r in q:
        r = list(r) + ([0] if len(r) % 2 else [])
        rows.append(bytes((r[i] << 4) | r[i + 1] for i in range(0, len(r), 2)))
    return b''.join(rows), q

c = ['/* ARQUIS UI - marca reducida del logotipo oficial. ARCHIVO GENERADO por herramientas/arquis_ui/arquis_logo.py: no editar. */',
     '#include "arquis_logo.h"', '']
prev = Image.new('RGB', (4 * 70 + 10, 90), BG)
for n in (32, 20):
    for name, aa in (('blue', a_blue), ('green', a_green)):
        data, q = mask(aa, n)
        c.append(f'static const uint8_t arq_logo{n}_{name}_map[] = {{')
        st = (n + 1) // 2
        for i in range(0, len(data), st):
            c.append('  ' + ','.join(f'0x{x:02x}' for x in data[i:i + st]) + ',')
        c.append('};')
        c.append(f'const lv_img_dsc_t arq_logo{n}_{name} = {{ .header = {{ .cf = LV_IMG_CF_ALPHA_4BIT, .always_zero = 0, .reserved = 0, .w = {n}, .h = {n} }}, .data_size = {len(data)}, .data = arq_logo{n}_{name}_map }};')
        c.append('')
open(os.path.join(MKS, 'arquis_logo.c'), 'w').write('\n'.join(c))
open(os.path.join(MKS, 'arquis_logo.h'), 'w').write('''/* ARQUIS UI - marca reducida del logotipo oficial (generado por herramientas/arquis_ui/arquis_logo.py) */
#pragma once
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
// El simbolo es bicolor: dibujar la mascara azul (#30ACE3) y la verde (#4CAF53) en la misma posicion.
extern const lv_img_dsc_t arq_logo32_blue, arq_logo32_green;   // 32x32 (encabezado)
extern const lv_img_dsc_t arq_logo20_blue, arq_logo20_green;   // 20x20 (icono reducido)
#ifdef __cplusplus
}
#endif
''')
# vista previa de la marca
for k, n in enumerate((32, 20)):
    for name, col in (('blue', (0x30, 0xAC, 0xE3)), ('green', (0x4C, 0xAF, 0x53))):
        _, q = mask(a_blue if name == 'blue' else a_green, n)
        m = Image.fromarray((q * 17).astype(np.uint8), 'L')
        prev.paste(Image.new('RGB', (n, n), col), (10 + k * 120, 10), m)
prev.resize((prev.width * 3, prev.height * 3), Image.NEAREST).save(os.path.join(HERE, 'marca_preview.png'))
print('OK splash 480x320 y marca (bbox %dx%d)' % (x1 - x0, y1 - y0))
