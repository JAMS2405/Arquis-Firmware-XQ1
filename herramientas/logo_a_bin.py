#!/usr/bin/env python3
"""Convierte un PNG/JPG a bmp_logo.bin (480x320, RGB565 little-endian) para la pantalla MKS TS35 en modo LVGL.
Uso:  python logo_a_bin.py mi_logo.png            -> genera bmp_logo.bin y vista previa
Requiere:  pip install pillow numpy
Copia bmp_logo.bin dentro de la carpeta 'assets' de la SD (reemplaza el original) y reinicia la impresora con la SD puesta."""
import sys
from PIL import Image
import numpy as np
src = sys.argv[1] if len(sys.argv) > 1 else "logo_arquis.png"
img = Image.open(src).convert("RGB")
# ajusta a 480x320 manteniendo proporcion, fondo negro
W, H = 480, 320
img.thumbnail((W, H), Image.LANCZOS)
canvas = Image.new("RGB", (W, H), (0, 0, 0))
canvas.paste(img, ((W - img.width) // 2, (H - img.height) // 2))
a = np.asarray(canvas, dtype=np.uint16)
v = ((a[..., 0] >> 3) << 11) | ((a[..., 1] >> 2) << 5) | (a[..., 2] >> 3)
open("bmp_logo.bin", "wb").write(v.astype("<u2").tobytes())
canvas.save("bmp_logo_preview.png")
print("OK: bmp_logo.bin (%d bytes)" % (W * H * 2))
