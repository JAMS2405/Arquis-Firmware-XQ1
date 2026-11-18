#!/usr/bin/env python3
"""Genera el resto de iconos Arquis (estilo linea azul/verde) y los escribe como bmp_*.bin en sd_assets.
Complementa a iconos_a_bin.py (que usa los SVG de iconos_svg).  Requiere playwright+chromium, pillow, numpy.
Cada .bin conserva su tamano original: 117x140 (grandes), 150x80 (botones), 45x45 (estado), 117x92 (atras/paginas)."""
import os, math, numpy as np
from PIL import Image
from playwright.sync_api import sync_playwright

HERE = os.path.dirname(os.path.abspath(__file__)); ASSETS = os.path.join(HERE, '..', 'sd_assets')
BLUE, GREEN, RED, DIM, BG = '#25ABE2', '#4CB050', '#EF5350', '#46607C', '#0B1220'

def S(c=BLUE, w=5, f='none'): return f'fill="{f}" stroke="{c}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"'
def node(x, y, c=GREEN, r=3.6): return f'<circle cx="{x}" cy="{y}" r="{r}" fill="{BG}" stroke="{c}" stroke-width="2.4"/>'
def txt(s, x, y, size=30, c=GREEN, w=700): return f'<text x="{x}" y="{y}" font-family="DejaVu Sans, Noto Sans CJK SC, sans-serif" font-weight="{w}" font-size="{size}" text-anchor="middle" fill="{c}">{s}</text>'
def arrow(x1, y1, x2, y2, c=BLUE, w=5, h=11):
    a = math.atan2(y2 - y1, x2 - x1)
    p1 = (x2 - h * math.cos(a - .6), y2 - h * math.sin(a - .6)); p2 = (x2 - h * math.cos(a + .6), y2 - h * math.sin(a + .6))
    return f'<path d="M{x1} {y1}L{x2} {y2}M{p1[0]:.1f} {p1[1]:.1f}L{x2} {y2}L{p2[0]:.1f} {p2[1]:.1f}" {S(c, w)}/>'
def ring(c=BLUE, w=4, r=38): return f'<circle cx="50" cy="50" r="{r}" {S(c, w)}/>'
def blade(rot, c=BLUE, w=5): return f'<path d="M50 50C47 24 66 12 78 22C86 31 70 48 50 50Z" {S(c, w)} transform="rotate({rot} 50 50)"/>'
def fan(c=BLUE, hub=GREEN): return blade(0, c) + blade(120, c) + blade(240, c) + node(50, 50, hub, 6)
def power(c=BLUE, a=GREEN): return f'<path d="M30 28A31 31 0 1 0 70 28" {S(c, 5.5)}/><path d="M50 12V48" {S(a, 6.5)}/>'
def gauge(pos, c=BLUE, a=GREEN):
    ang = math.radians(180 + pos * 180); x, y = 50 + 28 * math.cos(ang), 70 + 28 * math.sin(ang)
    return f'<path d="M12 70A38 38 0 0 1 88 70" {S(c)}/><path d="M50 70L{x:.1f} {y:.1f}" {S(a, 5)}/><circle cx="50" cy="70" r="6" fill="{BG}" {S(a, 4)[11:]}/><path d="M14 84H86" {S(c, 3.4)}/>'
def house(letter=None, c=BLUE, a=GREEN):
    g = f'<path d="M12 52L50 18L88 52" {S(c)}/><path d="M24 44V84H76V44" {S(c)}/>'
    return g + (txt(letter, 50, 74, 30, a) if letter else f'<path d="M42 84V62H58V84" {S(a, 4.5)}/>')
def chip(text, sub=None, c=BLUE, a=GREEN):
    g = f'<rect x="10" y="26" width="80" height="48" rx="11" {S(c, 4)}/>'
    if sub: return g + txt(text, 50, 55, 27, a) + txt(sub, 50, 69, 13, c, 600)
    return g + txt(text, 50, 60, 30, a)
def nozzle(num=None, c=BLUE, a=GREEN):
    g = f'<rect x="28" y="12" width="44" height="32" rx="7" {S(c)}/><path d="M34 44L50 80L66 44" {S(c)}/><path d="M50 82V92" {S(a, 5)}/>'
    return g + (txt(num, 50, 36, 24, a) if num else '')
def bedsq(dot=None, c=BLUE, a=GREEN):
    pos = {1: (26, 70), 2: (74, 70), 3: (74, 30), 4: (26, 30), 5: (50, 50)}
    g = f'<rect x="12" y="16" width="76" height="68" rx="9" {S(c)}/>'
    for k, (x, y) in pos.items(): g += (f'<circle cx="{x}" cy="{y}" r="8" fill="{a}"/>' if k == dot else node(x, y, c, 4))
    return g
def doc(c=BLUE, a=GREEN): return f'<path d="M26 10H58L76 28V90H26Z" {S(c)}/><path d="M58 10V28H76" {S(c)}/><path d="M36 48H66M36 60H66M36 72H56" {S(a, 4)}/>'
def folder(c=BLUE, a=GREEN): return f'<path d="M10 28Q10 22 16 22H38L46 32H84Q90 32 90 38V76Q90 82 84 82H16Q10 82 10 76Z" {S(c)}/><path d="M10 44H90" {S(a, 3.4)}/>'
def sdcard(c=BLUE, a=GREEN): return f'<path d="M26 10H64L78 24V90H26Z" {S(c)}/><path d="M36 10V26M46 10V26M56 10V26" {S(a, 4)}/>' + txt('SD', 52, 68, 24, a)
def usb(c=BLUE, a=GREEN): return f'<rect x="32" y="42" width="36" height="48" rx="7" {S(c)}/><rect x="38" y="12" width="24" height="30" {S(c, 4)}/><path d="M45 20V30M55 20V30" {S(a, 4)}/>' + node(50, 66, a, 5)
def cloud(c=BLUE, a=GREEN): return f'<path d="M28 72H72A16 16 0 0 0 72 40A24 24 0 0 0 28 46A13 13 0 0 0 28 72Z" {S(c)}/>' + node(50, 58, a, 5)
def pencil(c=BLUE, a=GREEN): return f'<path d="M20 80L28 54L66 16L84 34L46 72Z" {S(c)}/><path d="M28 54L46 72" {S(a, 4)}/>'
def bulb(on, c=BLUE, a=GREEN):
    g = f'<path d="M50 12A24 24 0 0 0 36 56V66H64V56A24 24 0 0 0 50 12Z" {S(a if on else c)}/><path d="M40 76H60M44 86H56" {S(c, 4)}/>'
    return g + (f'<path d="M50 2V6M14 24L18 27M86 24L82 27" {S(a, 3.4)}/>' if on else '')
def hand(c=BLUE, a=GREEN): return f'<path d="M30 54V26M42 48V16M54 48V16M66 52V24M30 54L24 46M30 54Q30 82 52 86Q74 84 76 58V44" {S(c)}/>' + node(52, 70, a, 4)
def pages(c=BLUE, a=GREEN): return f'<rect x="14" y="22" width="40" height="56" rx="5" {S(c)}/><rect x="30" y="14" width="40" height="56" rx="5" {S(c)}/><rect x="46" y="30" width="40" height="56" rx="5" {S(a)}/>'
def bedheat(c=BLUE, a=GREEN): return f'<path d="M10 78H90" {S(c)}/><path d="M24 62Q30 54 24 46Q18 38 24 30M50 62Q56 54 50 46Q44 38 50 30M76 62Q82 54 76 46Q70 38 76 30" {S(a, 5)}/>'
def clock(c=BLUE, a=GREEN): return ring(c, 5, 38) + f'<path d="M50 28V50L66 60" {S(a, 6)}/>'
def chart(c=BLUE, a=GREEN): return f'<path d="M16 12V84H90" {S(c)}/><path d="M26 66L44 42L58 56L84 26" {S(a)}/>'

def glyph(kind, **k): return k

def arrow_dir(label, d, c=BLUE, a=GREEN):
    dx, dy = d; cx, cy = 50, 40
    x1, y1, x2, y2 = cx - 32 * dx, cy - 30 * dy, cx + 32 * dx, cy + 30 * dy
    return arrow(x1, y1, x2, y2, c, 6.5, 15) + txt(label, 50, 96, 24, a)

# ---------------------------------------------------------------- definiciones
BIG = {}   # 117x140
BIG['bmp_Add'] = ring() + f'<path d="M50 28V72M28 50H72" {S(GREEN, 7)}/>'
BIG['bmp_Dec'] = ring() + f'<path d="M28 50H72" {S(GREEN, 7)}/>'
BIG['bmp_in'] = f'<path d="M50 8V60" {S(GREEN, 6.5)}/><path d="M32 44L50 62L68 44" {S(GREEN, 6.5)}/><path d="M16 74V88H84V74" {S(BLUE)}/>'
BIG['bmp_out'] = f'<path d="M50 62V10" {S(GREEN, 6.5)}/><path d="M32 28L50 10L68 28" {S(GREEN, 6.5)}/><path d="M16 74V88H84V74" {S(BLUE)}/>'
BIG['bmp_extru1'] = nozzle('1'); BIG['bmp_extru2'] = nozzle('2')
BIG['bmp_extruct_sel'] = nozzle(None, GREEN, BLUE)
BIG['bmp_speed_extruct'] = gauge(.55) + f'<path d="M50 6V28" {S(GREEN, 4.5)}/>'
BIG['bmp_speed0'] = fan(DIM, DIM); BIG['bmp_speed127'] = fan(BLUE, GREEN).replace(blade(240, BLUE), blade(240, DIM)); BIG['bmp_speed255'] = fan(GREEN, BLUE)
BIG['bmp_speed_slow'] = gauge(.18); BIG['bmp_speed_normal'] = gauge(.5); BIG['bmp_speed_high'] = gauge(.84, BLUE, RED)
for m in ('0_01', '0_05', '0_1', '1', '10'):
    t = m.replace('_', '.'); BIG['bmp_step_move' + m] = chip(t, 'mm')
for m in ('0_01', '0_05', '0_1'):
    t = m.replace('_', '.'); BIG['bmp_baby_move' + m] = chip(t, 'mm')
for n in (1, 5, 10):
    BIG[f'bmp_step{n}_mm'] = chip(str(n), 'mm'); BIG[f'bmp_step{n}_degree'] = chip(f'{n}°'); BIG[f'bmp_step{n}_percent'] = chip(f'{n}%')
for ax, d in (('x', 'X'), ('y', 'Y'), ('z', 'Z')):
    pass
BIG['bmp_xAdd'] = arrow_dir('X+', (1, 0)); BIG['bmp_xDec'] = arrow_dir('X-', (-1, 0))
BIG['bmp_yAdd'] = arrow_dir('Y+', (0, -1)); BIG['bmp_yDec'] = arrow_dir('Y-', (0, 1))
BIG['bmp_zAdd'] = arrow_dir('Z+', (0, -1)); BIG['bmp_zDec'] = arrow_dir('Z-', (0, 1))
BIG['bmp_zeroAll'] = house(); BIG['bmp_zeroX'] = house('X'); BIG['bmp_zeroY'] = house('Y'); BIG['bmp_zeroZ'] = house('Z')
BIG['bmp_manual_off'] = power(); BIG['bmp_auto_off'] = power() + txt('A', 50, 72, 26, GREEN); BIG['bmp_Power'] = power(RED, RED)
BIG['bmp_Mamual'] = hand(); BIG['bmp_bed'] = bedheat(); BIG['bmp_Position'] = ring(BLUE, 4, 34) + f'<path d="M50 6V94M6 50H94" {S(BLUE, 3.4)}/>' + node(50, 50, GREEN, 7)
BIG['bmp_Print_ui'] = (open(os.path.join(HERE, 'iconos_svg', 'Imprimir.svg')).read().split('>', 1)[1].rsplit('</svg>', 1)[0])
BIG['bmp_cloud'] = cloud(); BIG['bmp_file'] = doc(); BIG['bmp_dir'] = folder()
BIG['bmp_sd'] = sdcard(); BIG['bmp_sd_sel'] = sdcard(GREEN, BLUE); BIG['bmp_lcd_sd'] = sdcard(); BIG['bmp_lcd_sd_sel'] = sdcard(GREEN, BLUE)
BIG['bmp_usb_disk'] = usb(); BIG['bmp_usb_disk_sel'] = usb(GREEN, BLUE)
for i in range(1, 6): BIG[f'bmp_leveling{i}'] = bedsq(i)
for i in range(1, 8): BIG[f'bmp_custom{i}'] = f'<rect x="14" y="14" width="72" height="72" rx="14" {S()}/>' + txt(str(i), 50, 66, 44, GREEN) + node(14, 14) + node(86, 86)
BIG['bmp_function1'] = f'<path d="M14 26H86M14 50H86M14 74H86" {S(BLUE, 4)}/>' + ''.join(f'<circle cx="{x}" cy="{y}" r="8" fill="{BG}" {S(GREEN, 4.5)[11:]}/>' for x, y in ((62, 26), (34, 50), (70, 74)))
BIG['bmp_mov_changeSpeed'] = gauge(.5) + arrow(30, 30, 30, 12, GREEN, 4.5, 8) + arrow(70, 12, 70, 30, GREEN, 4.5, 8)
BIG['bmp_mov_sel'] = arrow(50, 50, 50, 10, GREEN) + arrow(50, 50, 50, 90, GREEN) + arrow(50, 50, 10, 50, GREEN) + arrow(50, 50, 90, 50, GREEN) + node(50, 50, BLUE, 6)
BIG['bmp_Laser_ui'] = f'<path d="M50 8V56" {S(GREEN, 6)}/><path d="M30 56H70L62 76H38Z" {S(BLUE)}/><path d="M24 90H76" {S(BLUE, 4)}/>'
BIG['bmp_engraving'] = pencil(); BIG['bmp_Light_open'] = bulb(True); BIG['bmp_Light_close'] = bulb(False)
BIG['bmp_Cut_times'] = f'<circle cx="28" cy="74" r="12" {S()}/><circle cx="28" cy="26" r="12" {S()}/><path d="M38 66L84 20M38 34L84 80" {S(GREEN, 5)}/>'
BIG['bmp_twoInOneSet'] = f'<rect x="10" y="30" width="38" height="40" rx="8" {S()}/><rect x="52" y="30" width="38" height="40" rx="8" {S(GREEN)}/>' + node(29, 50, GREEN, 4) + node(71, 50, BLUE, 4)
for code, t in (('english', 'EN'), ('french', 'FR'), ('italy', 'IT'), ('russian', 'RU'), ('spanish', 'ES'), ('simplified_cn', '简'), ('traditional_cn', '繁')):
    BIG['bmp_' + code] = ring() + txt(t, 50, 61, 30, GREEN)
    BIG['bmp_' + code + '_sel'] = ring(GREEN, 5) + txt(t, 50, 61, 30, BLUE)

BACK = {}  # 117x92
BACK['bmp_back'] = ring(BLUE, 4, 36) + f'<path d="M58 32L40 50L58 68" {S(GREEN, 7)}/>'
BACK['bmp_pageUp'] = ring(BLUE, 4, 36) + f'<path d="M32 58L50 40L68 58" {S(GREEN, 7)}/>'
BACK['bmp_pageDown'] = ring(BLUE, 4, 36) + f'<path d="M32 42L50 60L68 42" {S(GREEN, 7)}/>'

BTN = {}   # 150x80, icono pequeno a la izquierda
BTN['bmp_pause'] = f'<path d="M36 22V78M64 22V78" {S(GREEN, 12)}/>'
BTN['bmp_stop'] = f'<rect x="22" y="22" width="56" height="56" rx="11" fill="{RED}" stroke="{RED}" stroke-width="4" stroke-linejoin="round"/>'
BTN['bmp_resume'] = f'<path d="M28 18L80 50L28 82Z" fill="{GREEN}" stroke="{GREEN}" stroke-width="5" stroke-linejoin="round"/>'
BTN['bmp_operate'] = (open(os.path.join(HERE, 'iconos_svg', 'Opciones.svg')).read().split('>', 1)[1].rsplit('</svg>', 1)[0])

STATE = {}  # 45x45
STATE['bmp_ext1_state'] = nozzle('1'); STATE['bmp_ext2_state'] = nozzle('2'); STATE['bmp_bed_state'] = bedheat(); STATE['bmp_fan_state'] = fan()
STATE['bmp_time_state'] = clock(); STATE['bmp_zpos_state'] = ring() + txt('Z', 50, 63, 36, GREEN); STATE['bmp_speed_state'] = gauge(.55)
STATE['bmp_power_state'] = chart(); STATE['bmp_init_state'] = power(); STATE['bmp_times_state'] = pages()

# ---------------------------------------------------------------- render
def to565(img):
    a = np.asarray(img, dtype=np.uint16); return (((a[..., 0] >> 3) << 11) | ((a[..., 1] >> 2) << 5) | (a[..., 2] >> 3)).astype('<u2').tobytes()

def render(page, inner, box):
    page.set_content(f'<body style="margin:0;background:{BG}"><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="{box*4}" height="{box*4}">{inner}</svg>')
    page.screenshot(path='/tmp/_e.png', clip={'x': 0, 'y': 0, 'width': box * 4, 'height': box * 4})
    return Image.open('/tmp/_e.png').convert('RGB').resize((box, box), Image.LANCZOS)

BGQ = (0x0B, 0x12, 0x20)

def compose(g, W, H, kind):
    c = Image.new('RGB', (W, H), BGQ)
    if kind == 'big': c.paste(g, ((W - 92) // 2, 8))
    elif kind == 'back': c.paste(g, ((W - 76) // 2, (H - 76) // 2))
    elif kind == 'state': c.paste(g, (0, 0))
    elif kind == 'btn':
        from PIL import ImageDraw
        d = ImageDraw.Draw(c); d.rounded_rectangle((6, 6, W - 7, H - 7), radius=9, outline=(0x25, 0xAB, 0xE2), width=2)
        c.paste(g, (14, 22), None)
    return c

def write(name, img, W, H):
    f = os.path.join(ASSETS, name + '.bin'); old = open(f, 'rb').read()
    odd = len(old) % 2 == 1; assert len(old) == (4 if odd else 0) + W * H * 2 + (1 if odd else 0), (name, len(old))
    open(f, 'wb').write((old[:4] if odd else b'') + to565(img) + (old[-1:] if odd else b''))

if __name__ == '__main__':
    n = 0
    with sync_playwright() as pw:
        b = pw.chromium.launch(); p = b.new_page(viewport={'width': 400, 'height': 400})
        for table, (W, H), kind, box in ((BIG, (117, 140), 'big', 92), (BACK, (117, 92), 'back', 76), (BTN, (150, 80), 'btn', 36), (STATE, (45, 45), 'state', 45)):
            for name, svg in table.items():
                write(name, compose(render(p, svg, box), W, H, kind), W, H); n += 1
        b.close()
    print('OK', n, 'iconos')
