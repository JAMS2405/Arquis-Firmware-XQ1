// Simulador PC de la pantalla "Autoajuste PID" (arquis_pid_view.cpp REAL sobre LVGL 6.1.1-MKS REAL). No ejecuta Marlin.
#include <stdio.h>
#include <string.h>
#include "arquis_pid_view.h"

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
static char msg[100];
static arq_pid_view_model_t base(uint8_t phase) {
  arq_pid_view_model_t m; memset(&m, 0, sizeof m);
  m.phase = phase; m.heater = ARQ_PID_HOTEND; m.has[0] = m.has[1] = true; m.temp_hot = 24; m.temp_bed = 23; m.target = 210; m.cycles = 8; m.temp_cur = 24;
  m.cur_pid[0][0] = 30.24f; m.cur_pid[0][1] = 2.80f; m.cur_pid[0][2] = 81.66f; m.cur_pid[1][0] = 10.0f; m.cur_pid[1][1] = 0.023f; m.cur_pid[1][2] = 305.4f;
  m.old_pid[0] = 30.24f; m.old_pid[1] = 2.80f; m.old_pid[2] = 81.66f; m.msg = msg;
  return m;
}
static void screen_with(const arq_pid_view_model_t &m) { lv_obj_t *s = new_screen(); arq_pid_view_build(s, cb); arq_pid_view_update(&m); }

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0); if (argc > 1) outdir = argv[1];
  lv_init(); lv_disp_buf_init(&disp_buf, buf1, nullptr, ARQ_W * 14);
  lv_disp_drv_t drv; lv_disp_drv_init(&drv); drv.flush_cb = flush_cb; drv.buffer = &disp_buf; lv_disp_drv_register(&drv);
  static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv); idrv.type = LV_INDEV_TYPE_POINTER;
  idrv.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) -> bool { d->point = pt; d->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; return false; };
  lv_indev_drv_register(&idrv);

  { arq_pid_view_model_t m = base(ARQ_PVP_SETUP); screen_with(m); dump("pid_1_elegir_hotend");
    m.heater = ARQ_PID_BED; m.target = 60; m.cycles = 6; arq_pid_view_update(&m); dump("pid_2_elegir_cama");
    arq_pid_view_model_t n = base(ARQ_PVP_RUNNING); n.temp_cur = 187; n.cycle = 3; n.elapsed_s = 135; arq_pid_view_update(&n); dump("pid_3_en_curso");
    arq_pid_view_model_t r = base(ARQ_PVP_RESULT); r.new_pid[0] = 22.2f; r.new_pid[1] = 1.083f; r.new_pid[2] = 113.9f; arq_pid_view_update(&r); dump("pid_4_resultado");
    arq_pid_view_model_t e = base(ARQ_PVP_MSG); snprintf(msg, sizeof msg, "Tiempo agotado: el calentador no sigue la temperatura. No se ha cambiado ning\xC3\xBAn valor."); e.msg_kind = ARQ_PV_MSG_ERR; arq_pid_view_update(&e); dump("pid_5_error"); }

  { arq_pid_view_model_t m = base(ARQ_PVP_SETUP); screen_with(m); run();
    CHECK(press(10 + 100, 52 + 30) == ARQ_PV_HOT && press(244 + 100, 52 + 30) == ARQ_PV_BED, "elegir: tarjetas Hotend y Cama");
    CHECK(press(10 + 40, 132 + 20) == ARQ_PV_TDEC && press(10 + 460 - 40, 132 + 20) == ARQ_PV_TINC, "temperatura - / +");
    CHECK(press(10 + 40, 180 + 20) == ARQ_PV_CDEC && press(10 + 460 - 40, 180 + 20) == ARQ_PV_CINC, "ciclos - / +");
    CHECK(press(240, 290) == ARQ_PV_START, "Iniciar autoajuste");
    CHECK(press(20, 22) == ARQ_PV_BACK, "Atras");
    arq_pid_view_model_t n = m; n.has[1] = false; arq_pid_view_update(&n); run();
    CHECK(press(244 + 100, 52 + 30) == -1, "cama sin PID: tarjeta deshabilitada");
    n.has[0] = false; arq_pid_view_update(&n); run(); CHECK(press(240, 290) == -1, "calentador sin PID: no se puede iniciar");
  }
  { arq_pid_view_model_t m = base(ARQ_PVP_RUNNING); m.cycle = 2; screen_with(m); run();
    CHECK(press(240, 290) == ARQ_PV_CANCEL, "en curso: Cancelar");
    CHECK(press(20, 22) == ARQ_PV_BACK, "en curso: Atras (la pantalla pide confirmacion)");
    arq_pid_view_model_t r = base(ARQ_PVP_RESULT); r.new_pid[0] = 22.2f; r.new_pid[1] = 1.08f; r.new_pid[2] = 114.0f; arq_pid_view_update(&r); run();
    CHECK(press(240, 52 + 108 + 30) == ARQ_PV_APPLY && press(240, 290) == ARQ_PV_DISCARD, "resultado: Aplicar y guardar / Descartar");
    arq_pid_view_model_t e = base(ARQ_PVP_MSG); snprintf(msg, sizeof msg, "Cancelado"); arq_pid_view_update(&e); run();
    CHECK(press(240, 310 - 12 - 24) == ARQ_PV_OK, "aviso: Aceptar");
  }
  { const uint32_t before = used(); uint32_t peak = 0;
    for (int i = 0; i < 300; ++i) {
      arq_pid_view_model_t m = base(ARQ_PVP_SETUP); lv_obj_t *s = new_screen(); arq_pid_view_build(s, cb); arq_pid_view_update(&m);
      for (int ph = ARQ_PVP_RUNNING; ph <= ARQ_PVP_MSG; ++ph) { m.phase = ph; m.cycle = i % 8; arq_pid_view_update(&m); if (used() > peak) peak = used(); }
      m.phase = ARQ_PVP_SETUP; arq_pid_view_update(&m); if (used() > peak) peak = used();
      new_screen(); arq_pid_view_forget();
    }
    run(5); new_screen();
    CHECK(used() <= before + 64, "300 ciclos de crear/cambiar de fase/destruir: heap %u B (antes %u B)", (unsigned)used(), (unsigned)before);
    CHECK(16384 - peak >= 6000, "pico %u B (en 64 bits); quedan %u B libres", (unsigned)peak, (unsigned)(16384 - peak));
  }
  printf("[pid_sim] %s\n", fails ? "HAY FALLOS" : "TODO OK"); return fails ? 1 : 0;
}
