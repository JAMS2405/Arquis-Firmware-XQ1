#!/usr/bin/env python3
"""Tema Arquis para los iconos LVGL (MKS). Recolorea TODOS los bmp_*.bin de la carpeta 'assets':
  violeta/azul -> cian-azul (color del logo) ; rosa/magenta -> ambar ; fondo gris -> azul noche.
Uso:  python tema_arquis.py <assets_original> <assets_salida>
Ajusta la paleta en las constantes de abajo. El fondo (BG) debe coincidir con LV_COLOR_BACKGROUND en draw_ui.h."""
import sys, os, numpy as np
BG_OLD = (0x1A, 0x1A, 0x1A)
BG_NEW = (0x0B, 0x12, 0x20)          # azul noche  (draw_ui.h: LV_COLOR_BACKGROUND)
H_CYAN, H_SPREAD = 192.0, 0.40       # violeta/azul (200-300 grados) -> 192..232 (cian -> azul)
H_AMBER = 26.0                       # rosa/magenta (300-345) -> ambar
SKIP = {"bmp_logo.bin", "bmp_preview.bin"}

def rgb565_to_f(v):
    r = ((v >> 11) & 31) / 31.0; g = ((v >> 5) & 63) / 63.0; b = (v & 31) / 31.0
    return np.stack([r, g, b], -1)

def f_to_rgb565(a):
    a = np.clip(a, 0, 1)
    r = np.rint(a[..., 0] * 31).astype(np.uint16); g = np.rint(a[..., 1] * 63).astype(np.uint16); b = np.rint(a[..., 2] * 31).astype(np.uint16)
    return (r << 11) | (g << 5) | b

def rgb_to_hsv(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mx = a.max(-1); mn = a.min(-1); d = mx - mn
    h = np.zeros_like(mx)
    m = d > 1e-6
    rc = np.where(m & (mx == r), ((g - b) / np.where(m, d, 1)) % 6, 0)
    gc = np.where(m & (mx == g) & (mx != r), (b - r) / np.where(m, d, 1) + 2, 0)
    bc = np.where(m & (mx == b) & (mx != r) & (mx != g), (r - g) / np.where(m, d, 1) + 4, 0)
    h = (rc + gc + bc) * 60.0
    s = np.where(mx > 1e-6, d / np.where(mx > 1e-6, mx, 1), 0)
    return h % 360, s, mx

def hsv_to_rgb(h, s, v):
    c = v * s; hp = (h % 360) / 60.0; x = c * (1 - np.abs(hp % 2 - 1)); m = v - c
    z = np.zeros_like(h); conds = [(hp < 1), (hp < 2), (hp < 3), (hp < 4), (hp < 5), (hp >= 5)]
    rs = np.select(conds, [c, x, z, z, x, c]); gs = np.select(conds, [x, c, c, x, z, z]); bs = np.select(conds, [z, z, x, c, c, x])
    return np.stack([rs + m, gs + m, bs + m], -1)

def remap_hue(h, s):
    out = h.copy()
    a = (h >= 200) & (h < 300) & (s > 0.15)                 # violeta / azul
    out[a] = H_CYAN + (h[a] - 200) * H_SPREAD
    p = (h >= 300) & (h < 360) & (s > 0.15)                 # rosa / magenta
    out[p] = H_AMBER + (h[p] - 300) * 0.25
    return out

def theme(words):
    bg_old_w = f_to_rgb565(np.array([[c / 255 for c in BG_OLD]]))[0].astype(np.uint16)
    bg_new_w = f_to_rgb565(np.array([[c / 255 for c in BG_NEW]]))[0]
    a = rgb565_to_f(words)
    h, s, v = rgb_to_hsv(a)
    h2 = remap_hue(h, s)
    s2 = np.where((h != h2), np.minimum(1, s * 1.08), s)       # un poco mas de vida en el cian
    b = hsv_to_rgb(h2, s2, v)
    lum = a.mean(-1)
    delta = (np.array(BG_NEW) - np.array(BG_OLD)) / 255.0
    w = np.clip(1 - (lum - 0.10) / 0.15, 0, 1) * (s < 0.35)     # halos oscuros cerca del fondo
    b = b + w[..., None] * delta
    out = f_to_rgb565(b)
    out[words == bg_old_w] = bg_new_w                           # fondo exacto
    return out

def main(src, dst):
    os.makedirs(dst, exist_ok=True); n = 0
    for f in sorted(os.listdir(src)):
        p = os.path.join(src, f); d = open(p, "rb").read()
        if not f.lower().endswith(".bin") or not f.startswith("bmp_") or f in SKIP:
            open(os.path.join(dst, f), "wb").write(d); continue
        head, tail = (4, 1) if len(d) % 2 else (0, 0)          # iconos con tamano impar: 4 bytes de cabecera LVGL + 1 de cola
        w = np.frombuffer(d[head:len(d) - tail], dtype="<u2")
        open(os.path.join(dst, f), "wb").write(d[:head] + theme(w).astype("<u2").tobytes() + d[len(d) - tail:]); n += 1
    print("recoloreados:", n)

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
