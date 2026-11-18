#!/usr/bin/env python3
"""Convierte los SVG de herramientas/iconos_svg a bmp_*.bin (117x140 RGB565 LE) dentro de sd_assets.
Uso: python iconos_a_bin.py   (requiere playwright+chromium, pillow, numpy)
Conserva la cabecera de 4 bytes y el byte final de cada .bin original."""
import os, sys, numpy as np
from PIL import Image
from playwright.sync_api import sync_playwright
HERE=os.path.dirname(os.path.abspath(__file__)); ASSETS=os.path.join(HERE,'..','sd_assets'); SVG=os.path.join(HERE,'iconos_svg')
BG=(0x0B,0x12,0x20); W,H=117,140; S=92
MAP={'bmp_tool':'Herramientas','bmp_set':'Ajustes','bmp_printing':'Imprimir','bmp_preHeat':'Precalentar','bmp_temp':'Precalentar',
 'bmp_extruct':'Extruir','bmp_extrude_opr':'Extruir','bmp_mov':'Mover','bmp_move_opr':'Mover','bmp_zero':'Origen','bmp_leveling':'Nivelar',
 'bmp_filamentchange':'Filamento','bmp_more':'Mas','bmp_fan':'Ventilador','bmp_speed':'Velocidad','bmp_wifi':'WiFi','bmp_language':'Idioma',
 'bmp_about':'Acerca_de','bmp_machine_para':'Parametros','bmp_eeprom_settings':'Parametros','bmp_return':'Atras'}
def render(name,p):
    svg=open(os.path.join(SVG,name+'.svg')).read()
    p.set_content(f'<body style="margin:0;background:#0B1220"><div style="width:400px;height:400px">{svg.replace("width=\"100\" height=\"100\"","width=\"400\" height=\"400\"")}</div>')
    p.screenshot(path='/tmp/_i.png',clip={'x':0,'y':0,'width':400,'height':400})
    im=Image.open('/tmp/_i.png').convert('RGB').resize((S,S),Image.LANCZOS)
    c=Image.new('RGB',(W,H),BG); c.paste(im,((W-S)//2,8)); return c
def to565(img):
    a=np.asarray(img,dtype=np.uint16); return (((a[...,0]>>3)<<11)|((a[...,1]>>2)<<5)|(a[...,2]>>3)).astype('<u2').tobytes()
with sync_playwright() as pw:
    b=pw.chromium.launch(); p=b.new_page(viewport={'width':400,'height':400}); cache={}
    for bmp,n in MAP.items():
        f=os.path.join(ASSETS,bmp+'.bin'); old=open(f,'rb').read(); assert len(old)==4+W*H*2+1,bmp
        cache.setdefault(n,render(n,p)); open(f,'wb').write(old[:4]+to565(cache[n])+old[-1:]); print('OK',bmp,'<-',n)
    b.close()
