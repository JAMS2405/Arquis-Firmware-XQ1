// Pruebas de arquis_perf_logic.cpp REAL (limites, pasos, ordenes de Marlin, avisos). Sin Marlin ni LVGL.
#include "arquis_perf_logic.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++fails; printf("[perf] FALLA: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static const float SMIN = 25.0f;     // shaping_min_freq del XQ1 = min(35, 25)

static bool cmd_is(uint8_t id, float v, const char *want) { char b[40]; const size_t n = arq_pf_fmt_cmd(b, sizeof b, id, v, SMIN); return n && !strcmp(b, want); }

int main() {
  // ---- Tabla de parametros --------------------------------------------------------------------------------------------------
  for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) {
    const arq_pf_param_t p = arq_pf_param(i, SMIN);
    CHECK(p.label && p.unit && p.min > 0 && p.max >= p.min && p.fine > 0 && p.coarse >= p.fine && p.dec <= 3, "parametro %u bien definido", i);
    CHECK(strlen(p.label) < 24 && strlen(p.unit) < 8, "textos cortos del parametro %u", i);
  }
  CHECK(arq_pf_param(ARQ_PF_FX, 25).min == 25 && arq_pf_param(ARQ_PF_FY, 40).min == 40, "el minimo del shaping es el del buffer compilado (no se baja de el)");
  CHECK(arq_pf_param(ARQ_PF_K, SMIN).max <= 10.0f, "K dentro de lo que acepta M900 (0..10)");
  CHECK(arq_pf_param(ARQ_PF_JD, SMIN).min >= 0.01f && arq_pf_param(ARQ_PF_JD, SMIN).max <= 0.3f, "JD dentro de lo que acepta M205 J (0.01..0.3)");
  CHECK(arq_pf_param(99, SMIN).label == nullptr, "id invalido: sin tabla");

  // ---- Clamp ------------------------------------------------------------------------------------------------------------------
  CHECK(arq_pf_clamp(ARQ_PF_ACC_P, 100, SMIN) == 200 && arq_pf_clamp(ARQ_PF_ACC_P, 99999, SMIN) == 20000 && arq_pf_clamp(ARQ_PF_ACC_P, 3000, SMIN) == 3000, "aceleracion: limites");
  CHECK(arq_pf_clamp(ARQ_PF_ACC_P, NAN, SMIN) == 200 && arq_pf_clamp(ARQ_PF_ACC_P, INFINITY, SMIN) == 200, "NaN / infinito -> minimo");
  CHECK(arq_pf_clamp(ARQ_PF_K, NAN, SMIN) == 0, "K NaN -> 0 (apagado)");
  CHECK(arq_pf_clamp(ARQ_PF_K, 0.0f, SMIN) == 0 && arq_pf_clamp(ARQ_PF_K, -3, SMIN) == 0 && arq_pf_clamp(ARQ_PF_K, 0.0004f, SMIN) == 0, "K 0, negativa o que se redondea a 0 = apagado");
  CHECK(arq_pf_clamp(ARQ_PF_K, 0.001f, SMIN) == 0.005f, "K entre 0 y el minimo sube al minimo (como la frecuencia)");
  CHECK(fabsf(arq_pf_clamp(ARQ_PF_K, 0.013f, SMIN) - 0.013f) < 1e-6f && arq_pf_clamp(ARQ_PF_K, 50, SMIN) == 2.0f, "K 0.013 intacta; tope 2.0");
  CHECK(arq_pf_clamp(ARQ_PF_FX, 10, SMIN) == 25 && arq_pf_clamp(ARQ_PF_FX, 0, SMIN) == 0 && arq_pf_clamp(ARQ_PF_FX, 999, SMIN) == 200, "frecuencia: 0 = apagado, minimo 25, tope 200");
  CHECK(arq_pf_clamp(ARQ_PF_JD, 0.0f, SMIN) == arq_pf_param(ARQ_PF_JD, SMIN).min && fabsf(arq_pf_clamp(ARQ_PF_JD, 0.05f, SMIN) - 0.05f) < 1e-6f, "JD: minimo y valor normal");
  CHECK(arq_pf_clamp(99, 1, SMIN) == 0, "id invalido");

  // ---- Pasos ------------------------------------------------------------------------------------------------------------------
  CHECK(arq_pf_step(ARQ_PF_ACC_P, 3000, +1, false, SMIN) == 3100 && arq_pf_step(ARQ_PF_ACC_P, 3000, -1, false, SMIN) == 2900, "aceleracion: paso fino 100");
  CHECK(arq_pf_step(ARQ_PF_ACC_P, 3000, +1, true, SMIN) == 3500 && arq_pf_step(ARQ_PF_ACC_P, 3000, -1, true, SMIN) == 2500, "aceleracion: paso grueso 500");
  CHECK(arq_pf_step(ARQ_PF_ACC_P, 200, -1, false, SMIN) == 200 && arq_pf_step(ARQ_PF_ACC_P, 20000, +1, true, SMIN) == 20000, "los limites no se pasan");
  CHECK(arq_pf_step(ARQ_PF_AMAX_X, 4500, +1, false, SMIN) == 4750 && arq_pf_step(ARQ_PF_AMAX_Y, 4500, +1, true, SMIN) == 5500, "aceleracion maxima: 250 / 1000");
  { float k = 0.013f; for (int i = 0; i < 10; ++i) k = arq_pf_step(ARQ_PF_K, k, +1, false, SMIN);
    CHECK(fabsf(k - 0.063f) < 1e-5f, "K: 10 pasos finos desde 0.013 = 0.063 (sin deriva): %.6f", k);
    for (int i = 0; i < 10; ++i) k = arq_pf_step(ARQ_PF_K, k, -1, false, SMIN);
    CHECK(fabsf(k - 0.013f) < 1e-5f, "K: 10 pasos atras vuelve a 0.013: %.6f", k); }
  CHECK(arq_pf_step(ARQ_PF_K, 0.005f, -1, false, SMIN) == 0 && arq_pf_step(ARQ_PF_K, 0, +1, false, SMIN) == 0.005f && arq_pf_step(ARQ_PF_K, 0, -1, false, SMIN) == 0, "K: bajar del minimo la apaga; subir desde 0 da el minimo");
  CHECK(arq_pf_step(ARQ_PF_FX, 35, +1, false, SMIN) == 35.5f && arq_pf_step(ARQ_PF_FX, 35, -1, true, SMIN) == 30, "frecuencia: 0.5 / 5 Hz");
  CHECK(arq_pf_step(ARQ_PF_FX, 25, -1, false, SMIN) == 0 && arq_pf_step(ARQ_PF_FX, 27, -1, true, SMIN) == 0, "frecuencia: bajar del minimo del buffer la apaga (nunca por debajo: desbordaria)");
  CHECK(arq_pf_step(ARQ_PF_FY, 0, +1, false, SMIN) == 25 && arq_pf_step(ARQ_PF_FY, 0, -1, true, SMIN) == 0, "frecuencia: subir desde apagado da el minimo");
  CHECK(arq_pf_step(ARQ_PF_FX, 199.8f, +1, true, SMIN) == 200, "frecuencia: tope 200");
  CHECK(fabsf(arq_pf_step(ARQ_PF_JD, 0.05f, +1, false, SMIN) - 0.055f) < 1e-6f && arq_pf_step(ARQ_PF_JD, 0.01f, -1, false, SMIN) == 0.01f && arq_pf_step(ARQ_PF_JD, 0.29f, +1, true, SMIN) == 0.3f, "JD: paso y limites");
  CHECK(arq_pf_step(ARQ_PF_ACC_P, 3000, 0, false, SMIN) == 3000, "dir 0 = sin cambio");
  // el valor de partida fuera de limites se corrige al primer paso
  CHECK(arq_pf_step(ARQ_PF_ACC_P, 50, +1, false, SMIN) == 300, "partida fuera de limites: 50 -> 200 -> 300");

  // ---- Ordenes de Marlin ---------------------------------------------------------------------------------------------------
  CHECK(cmd_is(ARQ_PF_ACC_P, 3000, "M204 P3000") && cmd_is(ARQ_PF_ACC_T, 4000, "M204 T4000"), "M204 P / T");
  CHECK(cmd_is(ARQ_PF_AMAX_X, 4500, "M201 X4500") && cmd_is(ARQ_PF_AMAX_Y, 5250, "M201 Y5250"), "M201 X / Y (un solo eje cada vez: no pisa el otro)");
  CHECK(cmd_is(ARQ_PF_K, 0.013f, "M900 K0.013") && cmd_is(ARQ_PF_K, 0, "M900 K0.000"), "M900 K (0 = apagado)");
  CHECK(cmd_is(ARQ_PF_JD, 0.05f, "M205 J0.050"), "M205 J");
  CHECK(cmd_is(ARQ_PF_FX, 35, "M593 X F35.0") && cmd_is(ARQ_PF_FY, 27.5f, "M593 Y F27.5") && cmd_is(ARQ_PF_FX, 0, "M593 X F0.0"), "M593 X F / Y F (F0 = apagado)");
  { char b[40];
    CHECK(arq_pf_fmt_cmd(b, sizeof b, ARQ_PF_FX, 10, SMIN) == 0, "frecuencia por debajo del minimo: no se genera orden");
    CHECK(arq_pf_fmt_cmd(b, sizeof b, ARQ_PF_ACC_P, 50, SMIN) == 0 && arq_pf_fmt_cmd(b, sizeof b, ARQ_PF_ACC_P, 99999, SMIN) == 0, "aceleracion fuera de limites: sin orden");
    CHECK(arq_pf_fmt_cmd(b, sizeof b, ARQ_PF_JD, 0.0f, SMIN) == 0 && arq_pf_fmt_cmd(b, sizeof b, ARQ_PF_JD, 0.5f, SMIN) == 0, "JD fuera de 0.01..0.3: sin orden (Marlin la rechazaria)");
    CHECK(arq_pf_fmt_cmd(b, sizeof b, ARQ_PF_ACC_P, NAN, SMIN) == 0 && arq_pf_fmt_cmd(b, sizeof b, 99, 1, SMIN) == 0, "NaN / id invalido: sin orden");
    CHECK(arq_pf_fmt_cmd(b, 6, ARQ_PF_ACC_P, 3000, SMIN) == 0, "buffer corto: 0");
    size_t worst = 0;
    for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) { const arq_pf_param_t p = arq_pf_param(i, SMIN); const size_t n = arq_pf_fmt_cmd(b, sizeof b, i, p.max, SMIN); CHECK(n > 0, "orden del maximo de %u", i); if (n > worst) worst = n; }
    CHECK(worst < 32, "las ordenes caben en el bufer del controlador (32 B): peor caso %u", (unsigned)worst); }
  // toda orden generada para un valor que sale de step() es valida (recorre los dos sentidos de cada parametro)
  for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) {
    float v = arq_pf_param(i, SMIN).min; char b[40]; bool ok = true;
    for (int s = 0; s < 3000 && ok; ++s) { v = arq_pf_step(i, v, +1, s % 7 == 0, SMIN); ok = arq_pf_fmt_cmd(b, sizeof b, i, v, SMIN) > 0; }
    for (int s = 0; s < 3000 && ok; ++s) { v = arq_pf_step(i, v, -1, s % 5 == 0, SMIN); ok = arq_pf_fmt_cmd(b, sizeof b, i, v, SMIN) > 0; }
    CHECK(ok, "parametro %u: 6000 pasos arriba y abajo, todas las ordenes validas", i);
  }

  // ---- Formato de valores -------------------------------------------------------------------------------------------------
  { char b[16];
    arq_pf_fmt_value(b, sizeof b, ARQ_PF_ACC_P, 3000); CHECK(!strcmp(b, "3000"), "aceleracion '%s'", b);
    arq_pf_fmt_value(b, sizeof b, ARQ_PF_K, 0.013f); CHECK(!strcmp(b, "0.013"), "K '%s'", b);
    arq_pf_fmt_value(b, sizeof b, ARQ_PF_K, 0); CHECK(!strcmp(b, "OFF"), "K 0 -> OFF '%s'", b);
    arq_pf_fmt_value(b, sizeof b, ARQ_PF_FX, 35); CHECK(!strcmp(b, "35.0"), "frecuencia '%s'", b);
    arq_pf_fmt_value(b, sizeof b, ARQ_PF_FY, 0); CHECK(!strcmp(b, "OFF"), "frecuencia 0 -> OFF '%s'", b);
    arq_pf_fmt_value(b, sizeof b, ARQ_PF_JD, 0.05f); CHECK(!strcmp(b, "0.050"), "JD '%s'", b);
    arq_pf_fmt_value(b, sizeof b, ARQ_PF_ACC_P, NAN); CHECK(!strcmp(b, "--"), "NaN -> '--'");
    arq_pf_fmt_value(b, 3, ARQ_PF_ACC_P, 3000); CHECK(strlen(b) < 3, "buffer corto: sin desbordar");
  }
  CHECK(arq_pf_equal(ARQ_PF_K, 0.0130f, 0.01304f) && !arq_pf_equal(ARQ_PF_K, 0.013f, 0.014f) && arq_pf_equal(ARQ_PF_ACC_P, 3000, 3000.2f), "igualdad a la precision mostrada");

  // ---- Avisos -------------------------------------------------------------------------------------------------------------------
  CHECK(arq_pf_effective_acc(5000, 4500, 4500) == 4500 && arq_pf_effective_acc(3000, 4500, 3500) == 3000 && arq_pf_effective_acc(4000, 4500, 3500) == 3500, "aceleracion efectiva = minimo con la maxima de X e Y");
  CHECK(arq_pf_hint(ARQ_PF_ACC_P, 5000, 4500, 4500, 3000) == ARQ_PF_HINT_CAPPED, "aviso: limitada por la maxima del eje");
  CHECK(arq_pf_hint(ARQ_PF_ACC_P, 4400, 4500, 4500, 2000) == ARQ_PF_HINT_HIGH_ACC, "aviso: muy por encima de la guardada");
  CHECK(arq_pf_hint(ARQ_PF_ACC_P, 3000, 4500, 4500, 3000) == ARQ_PF_HINT_NONE, "sin aviso en el valor guardado");
  CHECK(arq_pf_hint(ARQ_PF_AMAX_X, 9000, 0, 0, 4500) == ARQ_PF_HINT_HIGH_ACC && arq_pf_hint(ARQ_PF_AMAX_X, 4750, 0, 0, 4500) == ARQ_PF_HINT_NONE, "aviso en la aceleracion maxima");
  CHECK(arq_pf_hint(ARQ_PF_K, 0, 0, 0, 0.013f) == ARQ_PF_HINT_K_ZERO && arq_pf_hint(ARQ_PF_FX, 0, 0, 0, 35) == ARQ_PF_HINT_NO_SHAPING && arq_pf_hint(ARQ_PF_JD, 0.05f, 0, 0, 0.05f) == ARQ_PF_HINT_NONE, "avisos de K / shaping apagados");
  CHECK(arq_pf_hint_text(ARQ_PF_HINT_NONE) == nullptr && arq_pf_hint_text(ARQ_PF_HINT_CAPPED) && strlen(arq_pf_hint_text(ARQ_PF_HINT_HIGH_ACC)) < 56, "textos de aviso (caben en la linea de estado)");
  for (uint8_t e = 0; e < ARQ_PF_E_COUNT; ++e) CHECK(arq_pf_err_text(e) && strlen(arq_pf_err_text(e)) < 40, "texto de error %u", e);
  CHECK(arq_pf_err_text(200) != nullptr, "error desconocido: texto por defecto");

  printf("[perf] %d comprobaciones, %d fallos\n", checks, fails);
  return fails ? 1 : 0;
}
