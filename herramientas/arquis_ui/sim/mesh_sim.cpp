// Simulador PC de la pantalla "NIVELACION DE CAMA" (arquis_mesh_view.cpp REAL sobre LVGL 6.1.1-MKS REAL) con un controlador FALSO
// construido sobre la logica pura REAL (arquis_mesh_logic.cpp). NO ejecuta Marlin: prueba el dibujo, los toques, el progreso y el heap.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "arquis_mesh_view.h"

extern "C" { extern lv_font_t arq_font_text16; lv_font_t lv_font_roboto_22 = arq_font_text16; }
extern "C" {   // ganchos que el firmware real define fuera de LVGL (miniatura de G-code, log): no se usan aqui
  uint32_t lv_open_gcode_file(char *) { return 0; }
  void lv_gcode_file_read(uint8_t *) {}
  void lv_close_gcode_file() {}
  void lv_log_register_print_cb(void *) {}
}

static uint16_t fb[ARQ_W * ARQ_H];
static lv_disp_buf_t disp_buf;
static lv_color_t buf1[ARQ_W * 14];
static const char *outdir = ".";
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *c) {
  for (int y = a->y1; y <= a->y2; ++y) for (int x = a->x1; x <= a->x2; ++x) fb[y * ARQ_W + x] = (c++)->full;
  lv_disp_flush_ready(drv);
}
static void run(int n = 20) { for (int i = 0; i < n; ++i) { lv_tick_inc(10); lv_task_handler(); } }
static void dump(const char *name) {
  run();
  char p[256]; snprintf(p, sizeof(p), "%s/%s.ppm", outdir, name);
  FILE *f = fopen(p, "wb"); fprintf(f, "P6\n%d %d\n255\n", ARQ_W, ARQ_H);
  for (int i = 0; i < ARQ_W * ARQ_H; ++i) { const uint16_t v = fb[i]; const uint8_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31; fputc((r << 3) | (r >> 2), f); fputc((g << 2) | (g >> 4), f); fputc((b << 3) | (b >> 2), f); }
  fclose(f);
}
static void rgb(int x, int y, int *r, int *g, int *b) { const uint16_t v = fb[y * ARQ_W + x]; *r = ((v >> 11) & 31) << 3; *g = ((v >> 5) & 63) << 2; *b = (v & 31) << 3; }
static uint32_t used() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.total_size - m.free_size; }
static uint32_t biggest() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.free_biggest_size; }
static int fails = 0, ev_count = 0, ev_last = -1, hit_last = -2;
#define CHECK(c, ...) do { if (c) { printf("[test] OK    "); } else { printf("[test] FALLA "); ++fails; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void cb(lv_obj_t *o, lv_event_t ev) {
  if (ev != LV_EVENT_RELEASED) return;
  ++ev_count; ev_last = o->mks_obj_id;
  if (o->mks_obj_id == ARQ_MV_MAP) hit_last = arq_mesh_view_hit();
}
static lv_point_t pt; static bool down;
static void tap(int x, int y) { pt = { (lv_coord_t)x, (lv_coord_t)y }; down = true; run(4); down = false; run(4); }
static lv_obj_t * new_screen() {
  lv_obj_t *old = lv_scr_act(), *scr = lv_obj_create(nullptr, nullptr);
  arq_screen_apply(scr); lv_scr_load(scr);
  if (old && old != scr) lv_obj_del(old);
  return scr;
}

// ---- Controlador falso: sesion pura + modelo de la vista (lo que hace arquis_mesh_ui.cpp con el controlador real) ----------------
static arq_mesh_sess_t S;
static char msg[96];
static arq_mesh_view_model_t model(uint8_t phase, bool stored_rel) {
  arq_mesh_view_model_t m; memset(&m, 0, sizeof(m));
  m.phase = phase; m.cur = S.cur; m.sel = 0xFF; m.count = arq_mesh_count(&S); m.percent = arq_mesh_percent(&S); m.step_idx = S.step_idx;
  m.mask = S.done; m.z_now = S.z_now; m.bed_cur = 58; m.bed_set = 60; m.mesh_ok = stored_rel; m.leveling_on = stored_rel; m.rel = stored_rel; m.fade = 10;
  m.can_repeat = m.count > 0; m.msg = msg; m.msg_kind = ARQ_MV_MSG_INFO;
  memcpy(m.val, S.z, sizeof(m.val));
  return m;
}
static float terrain(uint8_t p) { const float x = arq_mesh_ix(p) - 2, y = arq_mesh_iy(p) - 2; return 0.06f * x - 0.04f * y + 0.05f * (x * x - y * y) / 4 * 0 + ((p == 18) ? 0.22f : 0) + ((p == 6) ? -0.18f : 0); }

static lv_obj_t *scr_;
static void screen_with(const arq_mesh_view_model_t &m) { scr_ = new_screen(); arq_mesh_view_build(scr_, cb); arq_mesh_view_update(&m); }

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc > 1) outdir = argv[1];
  lv_init();
  lv_disp_buf_init(&disp_buf, buf1, nullptr, ARQ_W * 14);
  lv_disp_drv_t drv; lv_disp_drv_init(&drv); drv.flush_cb = flush_cb; drv.buffer = &disp_buf; lv_disp_drv_register(&drv);
  static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv); idrv.type = LV_INDEV_TYPE_POINTER;
  idrv.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) -> bool { d->point = pt; d->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; return false; };
  lv_indev_drv_register(&idrv);
  const uint32_t base = used();
  printf("[mem] tras lv_init: %u B usados de 16384 B (OJO: medido en 64 bits, la placa usa ~la mitad en punteros)\n", (unsigned)base);

  // ---- 1) Pantallas por fase (capturas) -----------------------------------------------------------------------------------------
  arq_mesh_sess_reset(&S);
  { arq_mesh_view_model_t m = model(ARQ_MVP_IDLE, false); m.mesh_ok = false; m.bed_set = 60; screen_with(m); dump("mesh_1_sin_malla"); }
  { arq_mesh_sess_reset(&S); for (uint8_t p = 0; p < 25; ++p) { S.z[p] = terrain(p); S.done |= 1UL << p; }
    float c[25]; arq_mesh_corrections(&S, c); memcpy(S.z, c, sizeof c);       // desviaciones relativas al centro
    arq_mesh_view_model_t m = model(ARQ_MVP_IDLE, true); m.bed_set = 70; arq_mesh_view_update(&m); dump("mesh_2_malla_guardada"); }
  { arq_mesh_sess_reset(&S); arq_mesh_view_model_t m = model(ARQ_MVP_PREP, false); m.prep = 1; m.bed_cur = 42; m.bed_set = 60; arq_mesh_view_update(&m); dump("mesh_3_calentando");
    m.prep = 2; m.soak_s = 24; arq_mesh_view_update(&m); dump("mesh_3b_estabilizando");
    m.prep = 3; arq_mesh_view_update(&m); dump("mesh_3c_homing"); }
  { arq_mesh_sess_reset(&S); arq_mesh_begin(&S); arq_mesh_prep_done(&S);
    arq_mesh_view_model_t m = model(ARQ_MVP_MOVING, false); arq_mesh_view_update(&m); dump("mesh_4_moviendo"); }
  { arq_mesh_sess_reset(&S); arq_mesh_begin(&S); arq_mesh_prep_done(&S); arq_mesh_arrived(&S, 1.0f);
    const float meas[] = { 0.12f, 0.10f, 0.08f, 0.07f, 0.05f, 0.09f, -0.06f, 0.11f, 0.13f, 0.15f, 0.14f };
    for (float z : meas) { arq_mesh_confirm(&S, z); arq_mesh_arrived(&S, z + 0.6f); }
    S.z_now = 0.18f;
    arq_mesh_view_model_t m = model(ARQ_MVP_ADJUST, false); arq_mesh_view_update(&m); dump("mesh_5_ajustando"); }
  { arq_mesh_sess_reset(&S); arq_mesh_begin(&S); arq_mesh_prep_done(&S); arq_mesh_arrived(&S, 1.0f);
    for (int i = 0; i < 25; ++i) { arq_mesh_confirm(&S, 0.20f + terrain(S.cur) - 0.30f); if (S.st == ARQ_MESH_MOVING) arq_mesh_arrived(&S, 1.0f); }
    float c[25]; const uint8_t e = arq_mesh_corrections(&S, c); CHECK(e == ARQ_MESH_OK && S.st == ARQ_MESH_REVIEW, "25 puntos confirmados -> REVIEW con correcciones validas");
    arq_mesh_view_model_t m = model(ARQ_MVP_REVIEW, true); memcpy(m.val, c, sizeof c); m.mask = 0x1FFFFFFu; arq_mesh_view_update(&m); dump("mesh_6_revision");
    // error: desviacion excesiva -> se ven las Z absolutas y el aviso
    arq_mesh_view_model_t w = model(ARQ_MVP_REVIEW, false); snprintf(msg, sizeof(msg), "Desviaci\xC3\xB3n excesiva (> 2 mm). Repite el punto sospechoso."); w.msg_kind = ARQ_MV_MSG_WARN; w.mask = 0x1FFFFFFu; arq_mesh_view_update(&w); dump("mesh_7_revision_error"); }
  { arq_mesh_view_model_t m = model(ARQ_MVP_RESULT, true); snprintf(msg, sizeof(msg), "Malla 5x5 aplicada, activa y guardada en la EEPROM."); m.msg_kind = ARQ_MV_MSG_OK; arq_mesh_view_update(&m); dump("mesh_8_resultado_ok");
    snprintf(msg, sizeof(msg), "Hay una impresi\xC3\xB3n en curso. Se conserva la malla anterior."); m.msg_kind = ARQ_MV_MSG_ERR; arq_mesh_view_update(&m); dump("mesh_9_resultado_error"); }

  // ---- 2) Colores del mapa: azul = mas baja, ambar = mas alta, panel = referencia; pendiente distinto de medido -----------------
  { arq_mesh_sess_reset(&S); arq_mesh_view_model_t m = model(ARQ_MVP_IDLE, true); m.mask = 0x1FFFFFFu;
    for (int p = 0; p < 25; ++p) m.val[p] = 0;
    m.val[3] = 0.30f; m.val[21] = -0.30f;
    arq_mesh_view_update(&m); run();
    auto cell_rgb = [&](int p, int *r, int *g, int *b) { const int cx = 6 + 6 + arq_mesh_ix(p) * 50 + 4, cy = 52 + 6 + (4 - arq_mesh_iy(p)) * 50 + 4; rgb(cx, cy, r, g, b); };
    int r, g, b; cell_rgb(3, &r, &g, &b); CHECK(r > b + 40 && r >= g, "celda +0,30 (punto 4): ambar (R %d G %d B %d)", r, g, b);
    cell_rgb(21, &r, &g, &b); CHECK(b > r + 40, "celda -0,30 (punto 22): azul (R %d G %d B %d)", r, g, b);
    cell_rgb(12, &r, &g, &b); CHECK(abs(r - 0x2D) < 8 && abs(g - 0x3B) < 8 && abs(b - 0x49) < 9, "celda de referencia: color de panel (R %d G %d B %d)", r, g, b);
    m.mask = 0; arq_mesh_view_update(&m); run(); cell_rgb(3, &r, &g, &b); CHECK(abs(r - 0x15) < 12 && abs(b - 0x26) < 14, "celda pendiente: fondo oscuro, sin valor (R %d G %d B %d)", r, g, b);
    m.mask = 1UL << 3; m.rel = false; m.val[3] = 0.1f; m.phase = ARQ_MVP_ADJUST; arq_mesh_view_update(&m); run(); cell_rgb(3, &r, &g, &b); CHECK(g > r + 20 && g > b + 10, "celda medida (Z absoluta): verde (R %d G %d B %d)", r, g, b);
    m.cur = 7; m.phase = ARQ_MVP_ADJUST; arq_mesh_view_update(&m); run(); cell_rgb(7, &r, &g, &b); CHECK(b > r + 40, "celda en curso: azul (R %d G %d B %d)", r, g, b);
  }

  // ---- 3) Toques: cada una de las 25 celdas devuelve su indice; Y crece hacia atras (fila delantera abajo) ---------------------------
  { arq_mesh_sess_reset(&S); arq_mesh_view_model_t m = model(ARQ_MVP_REVIEW, true); m.mask = 0x1FFFFFFu; screen_with(m); run();
    bool ok = true;
    for (int p = 0; p < 25; ++p) {
      const int cx = 6 + 6 + arq_mesh_ix(p) * 50 + 23, cy = 52 + 6 + (4 - arq_mesh_iy(p)) * 50 + 23;
      ev_count = 0; hit_last = -2; tap(cx, cy);
      if (ev_count != 1 || ev_last != ARQ_MV_MAP || hit_last != p) { ok = false; printf("   toque en celda %d -> evento %d, indice %d\n", p, ev_last, hit_last); }
    }
    CHECK(ok, "los 25 toques del mapa devuelven el indice correcto");
    ev_count = 0; hit_last = -2; tap(6 + 3, 52 + 3); CHECK(hit_last == 20 || hit_last == -1, "toque en el margen del mapa no se sale de rango (%d)", hit_last);
  }

  // ---- 4) Botones por fase: posicion, id y estado deshabilitado ---------------------------------------------------------------
  auto press = [&](int x, int y) { ev_count = 0; ev_last = -1; tap(x, y); return ev_last; };
  const int PXc = 272 + 99;
  { arq_mesh_sess_reset(&S); arq_mesh_view_model_t m = model(ARQ_MVP_IDLE, true); m.mask = 0x1FFFFFFu; screen_with(m); run();
    CHECK(press(PXc, 52 + 128 + 46 + 22) == ARQ_MV_START, "IDLE: Iniciar medicion");
    CHECK(press(272 + 20, 52 + 128 + 20) == ARQ_MV_TEMP_DEC, "IDLE: temperatura -");
    CHECK(press(272 + 198 - 20, 52 + 128 + 20) == ARQ_MV_TEMP_INC, "IDLE: temperatura +");
    CHECK(press(272 + 30, 290) == ARQ_MV_TOGGLE && press(272 + 99, 290) == ARQ_MV_SAVE && press(272 + 170, 290) == ARQ_MV_LOAD, "IDLE: Activar / Guardar / Cargar");
    CHECK(press(20, 22) == ARQ_MV_BACK, "IDLE: Atras");
    arq_mesh_view_model_t n = m; n.mesh_ok = false; n.mask = 0; arq_mesh_view_update(&n); run();
    CHECK(press(272 + 30, 290) == -1 && press(272 + 99, 290) == -1, "sin malla valida: Activar y Guardar deshabilitados");
    CHECK(press(PXc, 52 + 128 + 46 + 22) == ARQ_MV_START, "sin malla valida: se puede iniciar");
  }
  { arq_mesh_sess_reset(&S); arq_mesh_begin(&S); arq_mesh_prep_done(&S); arq_mesh_arrived(&S, 1.0f);
    arq_mesh_view_model_t m = model(ARQ_MVP_ADJUST, false); m.count = 0; m.can_repeat = false; screen_with(m); run();
    const int sy = 52 + 80 + 20;
    bool steps = true; for (int i = 0; i < 4; ++i) if (press(272 + i * 49 + 23, sy) != ARQ_MV_STEP0 + i) steps = false;
    CHECK(steps, "ADJUST: los 4 pasos (1,00 / 0,10 / 0,05 / 0,01)");
    const int uy = 52 + 80 + 44 + 23;
    CHECK(press(272 + 40, uy) == ARQ_MV_UP && press(272 + 150, uy) == ARQ_MV_DOWN, "ADJUST: subir / bajar");
    CHECK(press(PXc, uy + 47) == ARQ_MV_CONFIRM, "ADJUST: Confirmar");
    const int by = uy + 91;
    CHECK(press(272 + 40, by) == -1, "ADJUST: Repetir deshabilitado con 0 puntos medidos");
    CHECK(press(272 + 150, by) == ARQ_MV_CANCEL, "ADJUST: Cancelar");
    arq_mesh_view_model_t b = m; b.busy = true; b.can_repeat = true; arq_mesh_view_update(&b); run();
    CHECK(press(272 + 40, uy) == -1 && press(272 + 150, uy) == -1 && press(PXc, uy + 47) == -1, "ADJUST: con la maquina ocupada no se acepta subir/bajar/confirmar");
    CHECK(press(272 + 150, by) == ARQ_MV_CANCEL, "ADJUST: Cancelar sigue disponible con la maquina ocupada");
    arq_mesh_view_model_t c = m; c.can_repeat = true; c.count = 3; arq_mesh_view_update(&c); run();
    CHECK(press(272 + 40, by) == ARQ_MV_REPEAT, "ADJUST: Repetir con puntos medidos");
  }
  { arq_mesh_sess_reset(&S); arq_mesh_view_model_t m = model(ARQ_MVP_REVIEW, true); m.mask = 0x1FFFFFFu; screen_with(m); run();
    CHECK(press(PXc, 52 + 128 + 20) == ARQ_MV_SAVE, "REVIEW: Guardar");
    CHECK(press(PXc, 52 + 128 + 48 + 15) == ARQ_MV_REPEAT, "REVIEW: Repetir punto");
    CHECK(press(PXc, 52 + 128 + 90 + 20) == ARQ_MV_CANCEL, "REVIEW: Cancelar");
    arq_mesh_view_model_t w = m; w.rel = false; snprintf(msg, sizeof(msg), "Desviaci\xC3\xB3n excesiva"); arq_mesh_view_update(&w); run();
    CHECK(press(PXc, 52 + 128 + 20) == -1, "REVIEW con error de rango: Guardar deshabilitado");
    CHECK(press(PXc, 52 + 128 + 48 + 15) == ARQ_MV_REPEAT, "REVIEW con error de rango: se puede repetir un punto");
  }
  { arq_mesh_view_model_t m = model(ARQ_MVP_PREP, false); m.prep = 1; screen_with(m); run();
    CHECK(press(PXc, 290) == ARQ_MV_CANCEL, "PREP: Cancelar");
    arq_mesh_view_model_t n = model(ARQ_MVP_RESULT, true); snprintf(msg, sizeof(msg), "Calibraci\xC3\xB3n cancelada."); arq_mesh_view_update(&n); run();
    CHECK(press(PXc, 310 - 12 - 22) == ARQ_MV_OK, "RESULT: Aceptar");
  }

  // ---- 5) Sesion completa de 25 puntos con el modelo de la vista: el mapa se actualiza punto a punto -------------------------------
  { arq_mesh_sess_reset(&S); arq_mesh_begin(&S); arq_mesh_prep_done(&S); arq_mesh_arrived(&S, 1.0f);
    arq_mesh_view_model_t m = model(ARQ_MVP_ADJUST, false); screen_with(m); run();
    bool ok = true;
    for (int i = 0; i < 25; ++i) {
      const uint8_t p = S.cur;
      arq_mesh_confirm(&S, 0.10f + terrain(p));
      const bool review = S.st == ARQ_MESH_REVIEW;
      if (S.st == ARQ_MESH_MOVING) arq_mesh_arrived(&S, 0.7f);
      arq_mesh_view_model_t n = model(review ? ARQ_MVP_REVIEW : ARQ_MVP_ADJUST, false);
      float c[25]; if (review && arq_mesh_corrections(&S, c) == ARQ_MESH_OK) { memcpy(n.val, c, sizeof c); n.rel = true; }
      arq_mesh_view_update(&n); run();
      int r, g, b; const int cx = 6 + 6 + arq_mesh_ix(p) * 50 + 4, cy = 52 + 6 + (4 - arq_mesh_iy(p)) * 50 + 4; rgb(cx, cy, &r, &g, &b);
      if (n.count != i + 1 || n.percent != (i + 1) * 100 / 25) ok = false;
      if (!review && !(g > r + 20 && g > b + 10)) { ok = false; printf("   punto %d no aparece como medido\n", i); }
    }
    CHECK(ok && S.st == ARQ_MESH_REVIEW, "los 25 puntos se van marcando como medidos y el avance llega al 100 %%");
    dump("mesh_10_final");
  }

  // ---- 6) Memoria: pico por fase y ausencia de fugas ---------------------------------------------------------------------------------------
  { uint32_t peak = 0;
    for (int ph = ARQ_MVP_IDLE; ph <= ARQ_MVP_RESULT; ++ph) {
      arq_mesh_sess_reset(&S); arq_mesh_view_model_t m = model(ph, ph == ARQ_MVP_IDLE || ph == ARQ_MVP_REVIEW || ph == ARQ_MVP_RESULT); m.mask = m.rel ? 0x1FFFFFFu : 0; m.prep = 1;
      screen_with(m); run();
      printf("[mem] fase %d: %u B usados, mayor bloque libre %u B\n", ph, (unsigned)used(), (unsigned)biggest());
      if (used() > peak) peak = used();
    }
    CHECK(16384 - peak >= 6000, "peor fase: %u B usados; quedan %u B libres (en 64 bits; en ARM de 32 bits es menor)", (unsigned)peak, (unsigned)(16384 - peak));
    const uint32_t before = used();
    for (int i = 0; i < 300; ++i) {
      arq_mesh_sess_reset(&S);
      arq_mesh_view_model_t m = model(ARQ_MVP_IDLE, true); m.mask = 0x1FFFFFFu;
      screen_with(m);
      for (int ph = ARQ_MVP_PREP; ph <= ARQ_MVP_RESULT; ++ph) { m.phase = ph; m.rel = ph == ARQ_MVP_REVIEW || ph == ARQ_MVP_RESULT; m.prep = 1 + (i % 3); arq_mesh_view_update(&m); }
      m.phase = ARQ_MVP_IDLE; arq_mesh_view_update(&m);
      lv_obj_t *t = new_screen(); (void)t; arq_mesh_view_forget();      // como arq_mesh_clear(): se libera la pantalla
    }
    run(5); lv_obj_t *e = new_screen(); (void)e;
    const uint32_t after = used();
    CHECK(after <= before + 64, "300 ciclos de crear/cambiar de fase/destruir: heap %u B (antes %u B)", (unsigned)after, (unsigned)before);
  }
  printf("[mesh_sim] %s\n", fails ? "HAY FALLOS" : "TODO OK");
  return fails ? 1 : 0;
}
