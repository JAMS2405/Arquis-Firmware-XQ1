// Simulador PC de la pantalla "Rendimiento" (arquis_perf_view.cpp REAL sobre LVGL 6.1.1-MKS REAL). No ejecuta Marlin.
#include <stdio.h>
#include <string.h>
#include "arquis_perf_view.h"

extern "C" { extern lv_font_t arq_font_text16; lv_font_t lv_font_roboto_22 = arq_font_text16; }
extern "C" { uint32_t lv_open_gcode_file(char *) { return 0; } void lv_gcode_file_read(uint8_t *) {} void lv_close_gcode_file() {} void lv_log_register_print_cb(void *) {} }

static uint16_t fb[ARQ_W * ARQ_H]; static lv_disp_buf_t disp_buf; static lv_color_t buf1[ARQ_W * 14]; static const char *outdir = ".";
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *c) { for (int y = a->y1; y <= a->y2; ++y) for (int x = a->x1; x <= a->x2; ++x) fb[y * ARQ_W + x] = (c++)->full; lv_disp_flush_ready(drv); }
static void run(int n = 20) { for (int i = 0; i < n; ++i) { lv_tick_inc(10); lv_task_handler(); } }
static void dump(const char *name) {
  run(); char p[256]; snprintf(p, sizeof(p), "%s/%s.ppm", outdir, name);
  FILE *f = fopen(p, "wb"); fprintf(f, "P6\n%d %d\n255\n", ARQ_W, ARQ_H);
  for (int i = 0; i < ARQ_W * ARQ_H; ++i) { const uint16_t v = fb[i]; const uint8_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31; fputc((r << 3) | (r >> 2), f); fputc((g << 2) | (g >> 4), f); fputc((b << 3) | (b >> 2), f); }
  fclose(f);
}
static uint32_t used() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.total_size - m.free_size; }
static int fails = 0, ev_count = 0, ev_last = -1;
#define CHECK(c, ...) do { if (c) { printf("[test] OK    "); } else { printf("[test] FALLA "); ++fails; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void cb(lv_obj_t *o, lv_event_t ev) { if (ev == LV_EVENT_RELEASED) { ++ev_count; ev_last = o->mks_obj_id; } }
static lv_point_t pt; static bool down;
static int press(int x, int y) { ev_count = 0; ev_last = -1; pt = { (lv_coord_t)x, (lv_coord_t)y }; down = true; run(4); down = false; run(4); return ev_last; }
static lv_obj_t * new_screen() { lv_obj_t *old = lv_scr_act(), *scr = lv_obj_create(nullptr, nullptr); arq_screen_apply(scr); lv_scr_load(scr); if (old && old != scr) lv_obj_del(old); return scr; }

static arq_perf_view_model_t base() {
  arq_perf_view_model_t m; memset(&m, 0, sizeof m);
  const float v[ARQ_PF_COUNT] = { 3000, 4000, 4500, 4500, 0.013f, 0.05f, 35.0f, 25.0f };
  for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) { m.has[i] = true; m.val[i] = m.ref[i] = v[i]; }
  m.shaping_min = 25; m.sel = ARQ_PF_ACC_P;
  return m;
}
static void screen_with(const arq_perf_view_model_t &m) { lv_obj_t *s = new_screen(); arq_perf_view_build(s, cb); arq_perf_view_update(&m); }
// centros de los controles (coordenadas de la vista)
static int pick_x(int i) { return 10 + (i % 2) * 234 + 113; }
static int pick_y(int i) { return 52 + (i / 2) * 35 + 16; }
enum { MINUS_X = 50, MID_X = 240, PLUS_X = 430, STEP_Y = 213, BTN_Y = 289, RESTORE_X = 123, SAVE_X = 357 };

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0); if (argc > 1) outdir = argv[1];
  lv_init(); lv_disp_buf_init(&disp_buf, buf1, nullptr, ARQ_W * 14);
  lv_disp_drv_t drv; lv_disp_drv_init(&drv); drv.flush_cb = flush_cb; drv.buffer = &disp_buf; lv_disp_drv_register(&drv);
  static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv); idrv.type = LV_INDEV_TYPE_POINTER;
  idrv.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) -> bool { d->point = pt; d->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; return false; };
  lv_indev_drv_register(&idrv);

  // ---- Capturas --------------------------------------------------------------------------------------------------------------
  { arq_perf_view_model_t m = base(); screen_with(m); dump("perf_1_guardado");
    m.val[ARQ_PF_ACC_P] = 3500; m.dirty = true; m.sel = ARQ_PF_ACC_P; arq_perf_view_update(&m); dump("perf_2_cambios_sin_guardar");
    m.val[ARQ_PF_ACC_P] = 5000; m.val[ARQ_PF_AMAX_X] = 4500; arq_perf_view_update(&m); dump("perf_3_limitada_por_eje");
    m = base(); m.sel = ARQ_PF_K; m.coarse = true; m.val[ARQ_PF_K] = 0.035f; m.dirty = true; arq_perf_view_update(&m); dump("perf_4_k_paso_grueso");
    m = base(); m.sel = ARQ_PF_FY; m.val[ARQ_PF_FY] = 0; m.dirty = true; arq_perf_view_update(&m); dump("perf_5_shaping_apagado");
    m = base(); m.why = ARQ_PF_E_PRINTING; arq_perf_view_update(&m); dump("perf_6_imprimiendo_bloqueada");
    m = base(); m.has[ARQ_PF_K] = false; m.sel = ARQ_PF_FX; arq_perf_view_update(&m); dump("perf_7_sin_linear_advance"); }

  // ---- Toques ----------------------------------------------------------------------------------------------------------------
  { arq_perf_view_model_t m = base(); screen_with(m); run();
    bool all = true; for (int i = 0; i < ARQ_PF_COUNT; ++i) all = all && press(pick_x(i), pick_y(i)) == ARQ_PFV_PICK0 + i;
    CHECK(all, "las 8 tarjetas devuelven su id (PICK0 + id), en las dos columnas");
    CHECK(press(MINUS_X, STEP_Y) == ARQ_PFV_MINUS && press(PLUS_X, STEP_Y) == ARQ_PFV_PLUS, "- y +");
    CHECK(press(MID_X, STEP_Y) == ARQ_PFV_STEP, "tocar el valor cambia el paso");
    CHECK(press(SAVE_X, BTN_Y) == ARQ_PFV_SAVE, "Guardar");
    CHECK(press(20, 22) == ARQ_PFV_BACK, "Atras");
    CHECK(press(RESTORE_X, BTN_Y) == -1, "Restaurar deshabilitado sin cambios");
    m.dirty = true; m.val[ARQ_PF_K] = 0.02f; arq_perf_view_update(&m); run();
    CHECK(press(RESTORE_X, BTN_Y) == ARQ_PFV_RESTORE, "Restaurar con cambios");
    m.why = ARQ_PF_E_PRINTING; arq_perf_view_update(&m); run();
    CHECK(press(MINUS_X, STEP_Y) == -1 && press(PLUS_X, STEP_Y) == -1 && press(SAVE_X, BTN_Y) == -1 && press(RESTORE_X, BTN_Y) == -1, "imprimiendo: -, +, Guardar y Restaurar deshabilitados");
    CHECK(press(pick_x(3), pick_y(3)) == ARQ_PFV_PICK0 + 3 && press(20, 22) == ARQ_PFV_BACK, "imprimiendo: se puede mirar (elegir) y salir");
    m = base(); m.has[ARQ_PF_K] = false; arq_perf_view_update(&m); run();
    CHECK(press(pick_x(ARQ_PF_K), pick_y(ARQ_PF_K)) == -1 && press(pick_x(ARQ_PF_FX), pick_y(ARQ_PF_FX)) == ARQ_PFV_PICK0 + ARQ_PF_FX, "valor no disponible: tarjeta deshabilitada; las demas siguen");
  }
  // ---- Memoria ---------------------------------------------------------------------------------------------------------------
  { const uint32_t before = used(); uint32_t peak = 0;
    for (int i = 0; i < 300; ++i) {
      arq_perf_view_model_t m = base(); lv_obj_t *s = new_screen(); arq_perf_view_build(s, cb); arq_perf_view_update(&m); if (used() > peak) peak = used();
      for (int k = 0; k < 8; ++k) { m.sel = k; m.val[k] += 1; m.dirty = (k & 1); m.why = (k == 5) ? ARQ_PF_E_BUSY : 0; arq_perf_view_update(&m); }
      if (used() > peak) peak = used();
      new_screen(); arq_perf_view_forget();
    }
    run(5); new_screen();
    CHECK(used() <= before + 64, "300 ciclos de crear/actualizar/destruir: heap %u B (antes %u B)", (unsigned)used(), (unsigned)before);
    CHECK(16384 - peak >= 6000, "pico %u B (en 64 bits); quedan %u B libres", (unsigned)peak, (unsigned)(16384 - peak));
  }
  printf("[perf_sim] %s\n", fails ? "HAY FALLOS" : "TODO OK"); return fails ? 1 : 0;
}
