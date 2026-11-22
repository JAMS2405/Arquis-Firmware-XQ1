// Pruebas de arquis_pid_logic.cpp REAL (limites, sesion, validacion del resultado, ordenes M303/M301/M304). Sin Marlin ni LVGL.
#include "arquis_pid_logic.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++fails; printf("[pid] FALLA: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static bool near(float a, float b, float t = 1e-4f) { return fabsf(a - b) <= t; }
static const float OLD[3] = { 30.24f, 2.80f, 81.66f };

int main() {
  // ---- Limites ----------------------------------------------------------------------------------------------------------------
  { const arq_pid_limits_t h = arq_pid_limits(ARQ_PID_HOTEND, 270), b = arq_pid_limits(ARQ_PID_BED, 105);
    CHECK(h.min == 150 && h.max == 270 && h.def == 210 && h.cyc_def == 8, "limites hotend");
    CHECK(b.min == 40 && b.max == 105 && b.def == 60 && b.cyc_def == 6, "limites cama");
    CHECK(arq_pid_limits(ARQ_PID_BED, 30).max == 40 && arq_pid_limits(ARQ_PID_BED, 30).def == 40, "max por debajo del minimo: se corrige");
    CHECK(arq_pid_clamp_target(ARQ_PID_HOTEND, 100, 270) == 150 && arq_pid_clamp_target(ARQ_PID_HOTEND, 400, 270) == 270 && arq_pid_clamp_target(ARQ_PID_BED, 60, 105) == 60, "clamp de objetivo");
    CHECK(arq_pid_clamp_cycles(1) == 3 && arq_pid_clamp_cycles(8) == 8 && arq_pid_clamp_cycles(99) == 20 && arq_pid_clamp_cycles(-5) == 3, "clamp de ciclos (M303 exige >= 3)");
  }
  // ---- Ordenes ---------------------------------------------------------------------------------------------------------------
  { char b[48];
    CHECK(arq_pid_fmt_start(b, sizeof b, ARQ_PID_HOTEND, 210, 8) && !strcmp(b, "M303 E0 S210 C8"), "M303 hotend: '%s'", b);
    CHECK(arq_pid_fmt_start(b, sizeof b, ARQ_PID_BED, 60, 6) && !strcmp(b, "M303 E-1 S60 C6"), "M303 cama: '%s'", b);
    CHECK(strstr(b, " U") == nullptr, "M303 no lleva U: no aplica nada por si solo");
    CHECK(arq_pid_fmt_start(b, 8, ARQ_PID_HOTEND, 210, 8) == 0, "no cabe: 0");
    CHECK(arq_pid_fmt_apply(b, sizeof b, ARQ_PID_HOTEND, 30.24f, 2.8f, 81.66f) && !strcmp(b, "M301 P30.2400 I2.8000 D81.6600"), "M301: '%s'", b);
    CHECK(arq_pid_fmt_apply(b, sizeof b, ARQ_PID_BED, 10.0f, 0.023f, 305.4f) && !strcmp(b, "M304 P10.0000 I0.0230 D305.4000"), "M304: '%s'", b);
    CHECK(arq_pid_fmt_apply(b, sizeof b, ARQ_PID_BED, -1, 1, 1) == 0 && arq_pid_fmt_apply(b, sizeof b, ARQ_PID_BED, NAN, 1, 1) == 0, "valores no validos: no se genera orden");
    CHECK(arq_pid_fmt_apply(b, 10, ARQ_PID_HOTEND, 30, 2, 80) == 0, "buffer corto");
    CHECK(strlen(b) < 64, "las ordenes caben en el bufer de inyeccion de Marlin (64 B)");
    arq_pid_fmt_apply(b, sizeof b, ARQ_PID_HOTEND, 999.9999f, 99.9f, 9999.0f); CHECK(strlen(b) < 64, "peor caso < 64 B: '%s'", b);
  }
  // ---- Formatos --------------------------------------------------------------------------------------------------------------
  { char b[16];
    arq_pid_fmt_num(b, sizeof b, 30.236f, 2); CHECK(!strcmp(b, "30.24"), "30.236 -> '%s'", b);
    arq_pid_fmt_num(b, sizeof b, 0.0234f, 3); CHECK(!strcmp(b, "0.023"), "0.0234 -> '%s'", b);
    arq_pid_fmt_num(b, sizeof b, 0.05f, 2); CHECK(!strcmp(b, "0.05"), "ceros a la izquierda de los decimales -> '%s'", b);
    arq_pid_fmt_num(b, sizeof b, -1.5f, 1); CHECK(!strcmp(b, "-1.5"), "negativo -> '%s'", b);
    arq_pid_fmt_num(b, sizeof b, 12.0f, 0); CHECK(!strcmp(b, "12"), "sin decimales -> '%s'", b);
    arq_pid_fmt_num(b, sizeof b, NAN, 2); CHECK(!strcmp(b, "--"), "NaN");
    arq_pid_fmt_num(b, sizeof b, 1e30f, 2); CHECK(!strcmp(b, "--"), "desbordamiento -> '%s'", b);
    arq_pid_fmt_time(b, sizeof b, 125); CHECK(!strcmp(b, "2:05"), "tiempo 125 s -> '%s'", b);
    arq_pid_fmt_time(b, sizeof b, 3725); CHECK(!strcmp(b, "62:05"), "tiempo > 1 h -> '%s'", b);
    for (uint8_t e = 0; e < ARQ_PID_E_COUNT + 2; ++e) CHECK(arq_pid_err_text(e) && arq_pid_err_text(e)[0], "texto de error %u", e);
  }
  // ---- Resultado plausible ---------------------------------------------------------------------------------------------------
  { CHECK(arq_pid_result_ok(22.2f, 1.08f, 114.0f), "valores tipicos de hotend");
    CHECK(arq_pid_result_ok(54.0f, 0.0187f, 1300.0f), "valores tipicos de cama");
    CHECK(!arq_pid_result_ok(0, 1, 1) && !arq_pid_result_ok(-1, 1, 1), "Kp <= 0");
    CHECK(!arq_pid_result_ok(10, 0, 1) && !arq_pid_result_ok(10, 0.00001f, 1), "Ki ~ 0 (Marlin dividiria por Ki)");
    CHECK(!arq_pid_result_ok(10, 1, -1), "Kd negativo");
    CHECK(!arq_pid_result_ok(NAN, 1, 1) && !arq_pid_result_ok(10, INFINITY, 1) && !arq_pid_result_ok(10, 1, NAN), "no finitos");
    CHECK(!arq_pid_result_ok(5000, 1, 1) && !arq_pid_result_ok(10, 500, 1) && !arq_pid_result_ok(10, 1, 50000), "fuera de rango");
  }
  // ---- Sesion ---------------------------------------------------------------------------------------------------------------
  { arq_pid_sess_t s; arq_pid_sess_reset(&s);
    CHECK(arq_pid_begin(&s, ARQ_PID_HOTEND, 210, 8, 270, OLD) == ARQ_PID_OK && s.st == ARQ_PID_RUNNING && s.cycles == 8 && near(s.old_pid[0], 30.24f), "begin hotend");
    CHECK(arq_pid_begin(&s, ARQ_PID_BED, 60, 6, 105, OLD) == ARQ_PID_E_STATE, "no se puede empezar otro con uno en curso");
    arq_pid_progress(&s, 3); CHECK(s.cycle == 3, "progreso"); arq_pid_progress(&s, 99); CHECK(s.cycle == 8, "progreso acotado a los ciclos");
    CHECK(arq_pid_finish_result(&s, ARQ_PID_R_DONE, 22.2f, 1.08f, 114.0f) == ARQ_PID_OK && s.st == ARQ_PID_RESULT && near(s.new_pid[0], 22.2f) && near(s.old_pid[0], 30.24f), "resultado correcto: nuevo y anterior se conservan");
    CHECK(arq_pid_finish_result(&s, ARQ_PID_R_DONE, 1, 1, 1) == ARQ_PID_E_STATE, "no se finaliza dos veces");
    arq_pid_discard(&s); CHECK(s.st == ARQ_PID_IDLE && s.target == 210, "descartar: vuelve a IDLE sin aplicar nada y recuerda lo elegido");
    // parametros invalidos
    arq_pid_sess_reset(&s);
    CHECK(arq_pid_begin(&s, 5, 210, 8, 270, OLD) == ARQ_PID_E_PARAM, "calentador invalido");
    CHECK(arq_pid_begin(&s, ARQ_PID_HOTEND, 149, 8, 270, OLD) == ARQ_PID_E_PARAM && arq_pid_begin(&s, ARQ_PID_HOTEND, 271, 8, 270, OLD) == ARQ_PID_E_PARAM, "objetivo fuera de limites");
    CHECK(arq_pid_begin(&s, ARQ_PID_HOTEND, 210, 2, 270, OLD) == ARQ_PID_E_PARAM && arq_pid_begin(&s, ARQ_PID_HOTEND, 210, 21, 270, OLD) == ARQ_PID_E_PARAM, "ciclos fuera de limites");
    CHECK(s.st == ARQ_PID_IDLE, "un begin rechazado no deja sesion");
    // fallos de Marlin: nunca hay valores que aplicar
    const uint8_t rs[] = { ARQ_PID_R_TOO_HIGH, ARQ_PID_R_TIMEOUT, ARQ_PID_R_CANCEL, ARQ_PID_R_NONE, ARQ_PID_R_RUNNING };
    const uint8_t es[] = { ARQ_PID_E_TOO_HIGH, ARQ_PID_E_TIMEOUT, ARQ_PID_E_CANCEL, ARQ_PID_E_NOT_RUN, ARQ_PID_E_NOT_RUN };
    for (int i = 0; i < 5; ++i) {
      arq_pid_sess_reset(&s); arq_pid_begin(&s, ARQ_PID_BED, 60, 6, 105, OLD);
      CHECK(arq_pid_finish_result(&s, rs[i], 22.2f, 1.08f, 114.0f) == es[i] && s.st == ARQ_PID_FAILED && s.err == es[i], "resultado de Marlin %d -> error %d", rs[i], es[i]);
      CHECK(s.new_pid[0] == 0 && s.new_pid[1] == 0 && s.new_pid[2] == 0, "en un fallo no se ofrecen valores (%d)", i);
      arq_pid_discard(&s); CHECK(s.st == ARQ_PID_IDLE, "descartar el fallo");
    }
    arq_pid_sess_reset(&s); arq_pid_begin(&s, ARQ_PID_HOTEND, 210, 8, 270, OLD);
    CHECK(arq_pid_finish_result(&s, ARQ_PID_R_DONE, NAN, 1, 1) == ARQ_PID_E_BAD_RESULT && s.st == ARQ_PID_FAILED, "DONE con valores no plausibles -> fallo, no se ofrecen");
    arq_pid_sess_reset(&s); arq_pid_begin(&s, ARQ_PID_HOTEND, 210, 8, 270, OLD); arq_pid_discard(&s);
    CHECK(s.st == ARQ_PID_RUNNING, "no se puede descartar un ajuste en marcha (hay que cancelarlo)");
    arq_pid_fail(&s, ARQ_PID_E_STOPPED); CHECK(s.st == ARQ_PID_FAILED && s.err == ARQ_PID_E_STOPPED, "fail");
  }
  printf("[pid] %d comprobaciones, %d fallos\n", checks, fails);
  return fails ? 1 : 0;
}
