#!/usr/bin/env bash
# Genera las fuentes ARQUIS (Inter, licencia SIL OFL) como arrays LVGL compilados en Flash.
# Requisitos: node + npm i lv_font_conv ; fuentes Inter instaladas (https://rsms.me/inter).
# Uso: ./arquis_fuentes.sh [carpeta_fuentes_Inter]   (por defecto /usr/share/fonts/opentype/inter)
set -e
I=${1:-/usr/share/fonts/opentype/inter}
OUT="$(cd "$(dirname "$0")/../.." && pwd)/Marlin/src/lcd/extui/mks_ui"
C=${LV_FONT_CONV:-lv_font_conv}
gen() { n=$1; f=$2; s=$3; b=$4; shift 4
  $C --font "$I/$f.otf" --size "$s" --bpp "$b" --format lvgl --no-compress --no-kerning --lv-include lvgl.h --lv-font-name "$n" "$@" -o "$OUT/$n.c"
  # LVGL 6.1 no tiene el campo user_data salvo con LV_USE_USER_DATA
  sed -i 's|^    \.user_data = NULL,|#if defined(LV_USE_USER_DATA) \&\& LV_USE_USER_DATA\n    .user_data = NULL,\n#endif|' "$OUT/$n.c"
}
# Texto (4 bpp): ASCII + juego del espanol. ARQUIS UI 2.0 (F3-04): se quitan los Latin-1 que la interfaz no usa
# (las 78 pantallas del simulador salen identicas; los nombres de archivo con Latin-1 ya no se ven con UTF_FILENAME_SUPPORT apagado)
gen arq_font_text16 Inter-Medium            16 4 -r 0x20-0x7E,0xA0 --symbols "¡¿°·ºª²³±×µ«»áéíóúüñÁÉÍÓÚÜÑ"
# Cifras grandes (2 bpp, F3-03): temperaturas, porcentaje, tiempos; indistinguible de 4 bpp a 36 px
gen arq_font_num36  InterDisplay-SemiBold   36 2 --symbols "0123456789.-+/:%°C "
