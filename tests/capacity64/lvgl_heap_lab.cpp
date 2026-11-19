// Laboratorio: LVGL 6.1.1-MKS REAL. Mide el heap de LVGL al dibujar N marcadores (rect + etiqueta) en UN solo objeto con design_cb,
// que es la tecnica de arquis_objects_view.cpp, con N = 16, 40, 64 y 128. Host de 64 bits: los tamanos de LVGL son MAYORES que en la placa (32 bits).
#include <stdio.h>
#include <string.h>
#include "arquis_widgets.h"
extern "C" { uint32_t lv_open_gcode_file(char *) { return 0; } void lv_gcode_file_read(uint8_t *) {} void lv_close_gcode_file() {} void lv_log_register_print_cb(void *) {} }
extern "C" { extern lv_font_t arq_font_text16; lv_font_t lv_font_roboto_22 = arq_font_text16; }   // el simulador no trae la fuente por defecto del fabricante
static uint16_t fb[ARQ_W * ARQ_H]; static lv_disp_buf_t db; static lv_color_t buf1[ARQ_W * 14];
static void flush_cb(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *c) { for (int y = a->y1; y <= a->y2; ++y) for (int x = a->x1; x <= a->x2; ++x) fb[y * ARQ_W + x] = (c++)->full; lv_disp_flush_ready(d); }
static uint32_t used() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.total_size - m.free_size; }
static uint32_t freeb() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.free_size; }
static int N = 16; static long rects = 0, labels = 0; static lv_style_t st;
static bool design(lv_obj_t *o, const lv_area_t *mask, lv_design_mode_t mode) {
  if (mode != LV_DESIGN_DRAW_MAIN) return mode == LV_DESIGN_COVER_CHK ? false : true;
  lv_area_t a; lv_obj_get_coords(o, &a);
  for (int i = 0; i < N; ++i) {
    const int cx = 8 + (i % 8) * 31, cy = 8 + (i / 8) * 31;
    lv_area_t r = { (lv_coord_t)(a.x1 + cx), (lv_coord_t)(a.y1 + cy), (lv_coord_t)(a.x1 + cx + 24), (lv_coord_t)(a.y1 + cy + 24) };
    lv_draw_rect(&r, mask, &st, LV_OPA_COVER); ++rects;
    char t[6]; snprintf(t, sizeof t, "%d", i + 1);
    lv_draw_label(&r, mask, arq_style_text(ARQ_TEXT_BODY), LV_OPA_COVER, t, LV_TXT_FLAG_CENTER, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR); ++labels;
  }
  return true;
}
int main() {
  lv_init(); lv_disp_buf_init(&db, buf1, nullptr, ARQ_W * 14);
  lv_disp_drv_t drv; lv_disp_drv_init(&drv); drv.flush_cb = flush_cb; drv.buffer = &db; lv_disp_drv_register(&drv);
  arq_theme_init(); lv_style_copy(&st, &lv_style_plain); st.body.radius = 12; st.body.main_color = st.body.grad_color = LV_COLOR_RED;
  lv_obj_t *scr = lv_obj_create(nullptr, nullptr); lv_scr_load(scr);
  printf("heap tras lv_init: %u B usados (de 16384)\n", (unsigned)used());
  int ns[] = { 0, 16, 40, 64, 128 };
  for (int n : ns) {
    N = n; rects = labels = 0;
    lv_obj_t *o = lv_obj_create(scr, nullptr); lv_obj_set_pos(o, 6, 40); lv_obj_set_size(o, 266, 266); lv_obj_set_design_cb(o, design);
    const uint32_t before = used(); uint32_t peak = before, minfree = freeb();
    for (int k = 0; k < 40; ++k) { lv_obj_invalidate(o); lv_tick_inc(10); lv_task_handler(); if (used() > peak) peak = used(); if (freeb() < minfree) minfree = freeb(); }
    printf("N=%3d: heap antes=%u, pico durante 40 redibujados=%u (delta pico=%d B), rects dibujados=%ld, etiquetas=%ld\n", n, (unsigned)before, (unsigned)peak, (int)peak - (int)before, rects, labels);
    lv_obj_del(o);
  }
  printf("heap final: %u B\n", (unsigned)used());
  return 0;
}
