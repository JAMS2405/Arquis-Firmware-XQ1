#!/usr/bin/env python3
"""Arma el paquete de entrega de ARQUIS UI para copiarlo a la tarjeta SD de la impresora.

Uso (desde la raiz del repositorio):
    python herramientas/arquis_ui/empaquetar_entrega.py RUTA/Robin_nano_v3.bin [--salida entrega]

Crea  <salida>/Arquis-XQ1_UI_v<version>/
          Robin_nano_v3.bin     el firmware que compilaste
          assets/               copia de sd_assets/ (recursos que la placa carga en su memoria la primera vez)
          LEEME.txt             pasos de instalacion
          SHA256SUMS.txt        sumas de verificacion de todo el contenido
      y  <salida>/Arquis-XQ1_UI_v<version>.zip   (el mismo contenido, comprimido)

Antes de empaquetar COMPRUEBA:
  - que el .bin exista, no este vacio y quepa en la Flash (475 136 B; la placa reserva 48 KB para el cargador),
  - que sd_assets tenga el logo (bmp_logo.bin) y la fuente (FontUNIGBK.bin),
  - que esten todos los recursos que el firmware pide en pic_manager.cpp (avisa de los que falten; el unico que falta
    desde el origen, en los recursos de Makerbase tambien, es bmp_mov_changespeed.bin y no se usa con ARQUIS UI).
No necesita nada fuera de la libreria estandar de Python.
"""
import argparse
import hashlib
import os
import re
import shutil
import sys
import zipfile

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
FLASH_MAX = 475136            # 512 KB - 48 KB del cargador (Robin Nano V3.1)
OBLIGATORIOS = ['bmp_logo.bin', 'FontUNIGBK.bin']
CONOCIDOS_AUSENTES = {'bmp_mov_changespeed.bin'}   # tampoco esta en los recursos originales de Makerbase


def version():
    """Lee ARQUIS_UI_VERSION de arquis_theme.h (unica fuente de la version)."""
    ruta = os.path.join(RAIZ, 'Marlin', 'src', 'lcd', 'extui', 'mks_ui', 'arquis_theme.h')
    m = re.search(r'#define\s+ARQUIS_UI_VERSION\s+"([^"]+)"', open(ruta, encoding='utf-8').read())
    if not m:
        sys.exit('No encuentro ARQUIS_UI_VERSION en arquis_theme.h')
    return m.group(1)


def sha256(ruta):
    h = hashlib.sha256()
    with open(ruta, 'rb') as f:
        for bloque in iter(lambda: f.read(1 << 16), b''):
            h.update(bloque)
    return h.hexdigest()


def recursos_pedidos():
    """Nombres de recursos que pic_manager.cpp espera encontrar en la carpeta assets."""
    src = open(os.path.join(RAIZ, 'Marlin', 'src', 'lcd', 'extui', 'mks_ui', 'pic_manager.cpp'), encoding='utf-8').read()
    return set(re.findall(r'"([A-Za-z0-9_]+\.bin)"', src))


LEEME = """ARQUIS UI {ver} - Arquis XQ1 (Marlin 2.1.2.10, MKS Robin Nano V3.1, pantalla MKS TS35)

CONTENIDO
  Robin_nano_v3.bin   firmware
  assets/             recursos graficos (logo de arranque, iconos, fuente)
  SHA256SUMS.txt      sumas de verificacion

INSTALAR (primera vez o actualizacion)
 1. Guarda una copia del firmware anterior (por si quieres volver atras).
 2. Formatea la tarjeta SD en FAT32 o usa una vacia de 8-32 GB.
 3. Copia a la RAIZ de la tarjeta:  Robin_nano_v3.bin  y la carpeta  assets  (con ese nombre exacto).
 4. Impresora APAGADA y fria. Inserta la tarjeta en la ranura donde siempre pones la SD para actualizar el firmware y enciende.
 5. La pantalla actualiza sola y reinicia. La PRIMERA vez tarda mas: carga los recursos de la carpeta assets
    en la memoria de la placa. No apagues durante ese proceso.
 6. Cuando veas el logo de ARQUIS y el Dashboard, apaga, retira la tarjeta y borra Robin_nano_v3.CUR si quieres.
    Si ves el logo de Makerbase, la carpeta assets no es la de este paquete: repite con esta.

DESPUES DE INSTALAR
  - Ajustes > Sistema: debe decir "Interfaz ARQUIS {ver}".
  - Si los valores guardados (pasos, aceleracion...) no cargan, Ajustes > Memoria muestra la causa; no restaures
    valores de fabrica a ciegas: se perderian tus ajustes.
  - Recorre la lista de verificacion (docs/arquis/LISTA_VERIFICACION.md) con la impresora fria y sin filamento.

VOLVER ATRAS
  Copia a la tarjeta el firmware anterior (Robin_nano_v3.bin) y arranca como en el paso 4.
"""


def main():
    ap = argparse.ArgumentParser(description='Empaqueta la entrega de ARQUIS UI')
    ap.add_argument('bin', help='ruta de Robin_nano_v3.bin (compilado con PlatformIO)')
    ap.add_argument('--salida', default='entrega', help='carpeta de salida (por defecto: entrega)')
    a = ap.parse_args()

    # ---- comprobaciones ----
    if not os.path.isfile(a.bin):
        sys.exit(f'No existe {a.bin}. Compila primero con: pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc')
    tam = os.path.getsize(a.bin)
    if tam == 0:
        sys.exit('El .bin esta vacio')
    if tam > FLASH_MAX:
        sys.exit(f'El .bin ocupa {tam} B y la Flash util es {FLASH_MAX} B: no cabe')
    if not a.bin.lower().endswith('.bin'):
        sys.exit('El firmware debe ser un archivo .bin (no el .elf)')
    assets = os.path.join(RAIZ, 'sd_assets')
    if not os.path.isdir(assets):
        sys.exit(f'No encuentro {assets}')
    presentes = set(os.listdir(assets))
    faltan_obl = [n for n in OBLIGATORIOS if n not in presentes]
    if faltan_obl:
        sys.exit(f'Faltan recursos obligatorios en sd_assets: {faltan_obl}')
    faltan = sorted(recursos_pedidos() - presentes - CONOCIDOS_AUSENTES)
    if faltan:
        print(f'AVISO: el firmware pide {len(faltan)} recursos que no estan en sd_assets: {faltan}')
    ver = version()

    # ---- armado ----
    nombre = f'Arquis-XQ1_UI_v{ver}'
    carpeta = os.path.join(a.salida, nombre)
    if os.path.exists(carpeta):
        shutil.rmtree(carpeta)
    os.makedirs(carpeta)
    shutil.copy2(a.bin, os.path.join(carpeta, 'Robin_nano_v3.bin'))
    shutil.copytree(assets, os.path.join(carpeta, 'assets'))
    with open(os.path.join(carpeta, 'LEEME.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(LEEME.format(ver=ver))

    # ---- sumas de verificacion ----
    lineas = []
    for base, _, archivos in os.walk(carpeta):
        for n in sorted(archivos):
            ruta = os.path.join(base, n)
            lineas.append(f'{sha256(ruta)}  {os.path.relpath(ruta, carpeta).replace(os.sep, "/")}')
    lineas.sort(key=lambda l: l.split('  ', 1)[1])
    with open(os.path.join(carpeta, 'SHA256SUMS.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lineas) + '\n')

    # ---- zip ----
    ruta_zip = os.path.join(a.salida, nombre + '.zip')
    with zipfile.ZipFile(ruta_zip, 'w', zipfile.ZIP_DEFLATED) as z:
        for base, _, archivos in os.walk(carpeta):
            for n in sorted(archivos):
                ruta = os.path.join(base, n)
                z.write(ruta, os.path.join(nombre, os.path.relpath(ruta, carpeta)))

    print(f'Version      : ARQUIS UI {ver}')
    print(f'Firmware     : {tam} B ({100 * tam / FLASH_MAX:.1f} % de la Flash util)  sha256 {sha256(a.bin)[:16]}...')
    print(f'Recursos     : {len(presentes)} archivos en assets/')
    print(f'Carpeta      : {carpeta}')
    print(f'Zip          : {ruta_zip} ({os.path.getsize(ruta_zip) // 1024} KB)')


if __name__ == '__main__':
    main()
