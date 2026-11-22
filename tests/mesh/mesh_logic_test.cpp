// Pruebas de arquis_mesh_logic.cpp REAL (logica pura de la nivelacion manual 5x5). Sin Marlin ni LVGL.
#include "arquis_mesh_logic.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++fails; printf("[mesh] FALLA: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static bool near(float a, float b, float t = 1e-4f) { return fabsf(a - b) <= t; }

static const arq_mesh_area_t AREA = { 15, 285, 15, 285 };

// Sesion hasta ADJUST en el primer punto
static void start(arq_mesh_sess_t &s) {
  arq_mesh_sess_reset(&s);
  CHECK(arq_mesh_begin(&s) && s.st == ARQ_MESH_PREP, "begin");
  CHECK(arq_mesh_prep_done(&s) && s.st == ARQ_MESH_MOVING, "prep_done");
  CHECK(arq_mesh_arrived(&s, 1.0f) && s.st == ARQ_MESH_ADJUST, "arrived");
}
// Confirma el punto en curso con z y avanza (deja la sesion en ADJUST del siguiente si lo hay)
static uint8_t take(arq_mesh_sess_t &s, float z) {
  const uint8_t r = arq_mesh_confirm(&s, z);
  if (r == ARQ_MESH_OK && s.st == ARQ_MESH_MOVING) arq_mesh_arrived(&s, 1.0f);
  return r;
}
static void fill(arq_mesh_sess_t &s, float (*zf)(uint8_t p)) {
  start(s);
  for (int i = 0; i < ARQ_MESH_PTS; ++i) { const uint8_t p = s.cur; CHECK(take(s, zf(p)) == ARQ_MESH_OK, "confirmar punto %d", i); }
}

int main() {
  // ---- Orden y coordenadas -----------------------------------------------------------------------------------------------------
  { bool seen[25] = {}; bool ok = true;
    for (uint8_t q = 0; q < 25; ++q) { const uint8_t p = arq_mesh_seq_to_p(q); if (p >= 25 || seen[p]) ok = false; else seen[p] = true; if (arq_mesh_p_to_seq(p) != q) ok = false; }
    CHECK(ok, "el orden de medicion cubre los 25 puntos una vez y es invertible");
    // Mismo orden que G29 manual de Marlin (abl/G29.cpp): exterior = Y, interior = X, serpentina en las filas impares
    bool same = true;
    for (uint8_t idx = 0; idx < 25; ++idx) { uint8_t outer = idx / 5, inner = idx - outer * 5; if (outer & 1) inner = 4 - inner; if (arq_mesh_seq_to_p(idx) != outer * 5 + inner) same = false; }
    CHECK(same, "orden igual al de G29 manual de Marlin");
    CHECK(arq_mesh_seq_to_p(0) == 0 && arq_mesh_seq_to_p(4) == 4 && arq_mesh_seq_to_p(5) == 9 && arq_mesh_seq_to_p(9) == 5 && arq_mesh_seq_to_p(24) == 24, "serpentina");
    CHECK(arq_mesh_seq_to_p(25) == 0xFF && arq_mesh_p_to_seq(30) == 0xFF, "fuera de rango");
    CHECK(arq_mesh_p_to_seq(ARQ_MESH_CENTER) == 12 && arq_mesh_ix(ARQ_MESH_CENTER) == 2 && arq_mesh_iy(ARQ_MESH_CENTER) == 2, "el centro es el indice 12 (2,2)");
    float x, y; arq_mesh_point_xy(&AREA, 0, &x, &y); CHECK(near(x, 15) && near(y, 15), "punto 0 = (15,15)");
    arq_mesh_point_xy(&AREA, 24, &x, &y); CHECK(near(x, 285) && near(y, 285), "punto 24 = (285,285)");
    arq_mesh_point_xy(&AREA, 12, &x, &y); CHECK(near(x, 150) && near(y, 150), "punto 12 = (150,150)");
    arq_mesh_point_xy(&AREA, 3, &x, &y); CHECK(near(x, 217.5f) && near(y, 15), "punto 3 = (217,5; 15)");
    CHECK(near(arq_mesh_spacing_x(&AREA), 67.5f) && near(arq_mesh_spacing_y(&AREA), 67.5f), "paso 67,5 mm");
    CHECK(arq_mesh_area_ok(&AREA), "area valida");
    arq_mesh_area_t bad = { 285, 15, 15, 285 }; CHECK(!arq_mesh_area_ok(&bad), "area invertida");
    bad = { 15, NAN, 15, 285 }; CHECK(!arq_mesh_area_ok(&bad), "area no finita");
    bad = { 15, 16, 15, 285 }; CHECK(!arq_mesh_area_ok(&bad), "area demasiado pequena");
    CHECK(!arq_mesh_area_ok(nullptr), "area nula");
  }
  // ---- Z con proteccion --------------------------------------------------------------------------------------------------------
  { CHECK(near(arq_mesh_step_z(1.0f, -1, 0.10f), 0.90f), "bajar 0,10");
    CHECK(near(arq_mesh_step_z(1.0f, +1, 0.05f), 1.05f), "subir 0,05");
    CHECK(near(arq_mesh_step_z(1.0f, -1, 1.0f), 0.50f), "paso grueso hacia abajo: se detiene en el suelo (0,50), no baja a 0,00");
    CHECK(near(arq_mesh_step_z(0.50f, -1, 1.0f), 0.50f), "paso grueso en el suelo: no se mueve");
    CHECK(near(arq_mesh_step_z(0.30f, -1, 1.0f), 0.30f), "paso grueso por debajo del suelo: no se mueve (no hay descenso grueso cerca del papel)");
    CHECK(near(arq_mesh_step_z(0.30f, +1, 1.0f), 1.30f), "paso grueso hacia arriba siempre permitido");
    CHECK(near(arq_mesh_step_z(0.30f, -1, 0.10f), 0.20f), "paso fino cerca del papel");
    CHECK(near(arq_mesh_step_z(-1.99f, -1, 0.10f), ARQ_MESH_Z_MIN), "limite inferior absoluto -2,00");
    CHECK(near(arq_mesh_step_z(ARQ_MESH_Z_MIN, -1, 0.01f), ARQ_MESH_Z_MIN), "no baja de -2,00");
    CHECK(near(arq_mesh_step_z(9.5f, +1, 1.0f), ARQ_MESH_Z_MAX), "limite superior 10,00");
    CHECK(arq_mesh_step_z(NAN, 1, 0.1f) != arq_mesh_step_z(NAN, 1, 0.1f), "Z NaN se devuelve sin tocar (NaN)");
    CHECK(near(arq_mesh_step_z(1.0f, 0, 0.1f), 1.0f) && near(arq_mesh_step_z(1.0f, 1, 0.0f), 1.0f) && near(arq_mesh_step_z(1.0f, 1, -0.1f), 1.0f), "dir/paso invalidos");
    float z = 1.0f; for (int i = 0; i < 1000; ++i) z = arq_mesh_step_z(z, i & 1 ? 1 : -1, 0.01f);
    CHECK(near(z, 1.0f, 1e-6f), "1000 pulsos de 0,01 mm no acumulan error (%.9f)", z);
    z = 0.5f; for (int i = 0; i < 49; ++i) z = arq_mesh_step_z(z, -1, 0.01f);
    CHECK(near(z, 0.01f, 1e-6f), "49 pulsos finos bajan exactamente 0,49 mm (%.9f)", z);
    CHECK(near(arq_mesh_start_z(false, 0, 1.0f), 1.0f), "primer punto: altura inicial");
    CHECK(near(arq_mesh_start_z(true, 0.00f, 1.0f), 0.60f), "siguiente: ultima + 0,6");
    CHECK(near(arq_mesh_start_z(true, 0.80f, 1.0f), 1.0f), "nunca mas alto que la altura inicial");
    CHECK(near(arq_mesh_start_z(true, -1.0f, 1.0f), -0.40f), "cama baja: parte de la ultima + 0,6 (negativo)");
    CHECK(near(arq_mesh_start_z(true, -5.0f, 1.0f), ARQ_MESH_Z_MIN), "limitada por Z_MIN");
    CHECK(near(arq_mesh_start_z(true, NAN, 1.0f), 1.0f), "ultima no finita: altura inicial");
  }
  // ---- Sesion completa: 25 puntos validos --------------------------------------------------------------------------------------
  { arq_mesh_sess_t s;
    fill(s, [](uint8_t p) -> float { return 0.10f + 0.01f * p; });      // todos distintos, positivos
    CHECK(s.st == ARQ_MESH_REVIEW && arq_mesh_count(&s) == 25 && arq_mesh_percent(&s) == 100, "25 puntos -> REVIEW al 100 %%");
    float c[25]; CHECK(arq_mesh_corrections(&s, c) == ARQ_MESH_OK, "correcciones validas");
    CHECK(near(c[ARQ_MESH_CENTER], 0.0f), "referencia = centro = 0");
    bool rel = true; for (int p = 0; p < 25; ++p) if (!near(c[p], 0.01f * (p - 12))) rel = false;
    CHECK(rel, "correcciones relativas al centro");
    CHECK(arq_mesh_finish(&s) && s.st == ARQ_MESH_DONE, "finish");
    CHECK(!arq_mesh_finish(&s), "finish dos veces");
  }
  // Valores positivos y negativos mezclados; el signo se conserva y se normaliza al centro aunque el centro sea negativo
  { arq_mesh_sess_t s;
    fill(s, [](uint8_t p) -> float { return p == 12 ? -0.30f : p == 0 ? 0.20f : p == 24 ? -0.80f : -0.30f; });
    float c[25]; CHECK(arq_mesh_corrections(&s, c) == ARQ_MESH_OK, "mezcla");
    CHECK(near(c[0], 0.50f) && near(c[24], -0.50f) && near(c[12], 0.0f) && near(c[1], 0.0f), "centro negativo: la referencia se resta (0 - (-0,30) = +0,30 ...) c0=%.3f c24=%.3f", c[0], c[24]);
  }
  // Valores iguales (cama perfectamente plana)
  { arq_mesh_sess_t s; fill(s, [](uint8_t) -> float { return 0.0f; });
    float c[25]; CHECK(arq_mesh_corrections(&s, c) == ARQ_MESH_OK, "plana");
    arq_mesh_stats_t st; arq_mesh_stats(c, 0x1FFFFFFu, &st);
    CHECK(near(st.range, 0) && near(st.sigma, 0) && st.n == 25, "estadisticas de cama plana");
  }
  // Valores extremos dentro de limites: Z_MIN y Z_MAX exactos son aceptables como medicion; la malla se rechaza si la desviacion > 2 mm
  { arq_mesh_sess_t s;
    fill(s, [](uint8_t p) -> float { return p == 12 ? 0.0f : p == 0 ? 2.0f : p == 24 ? -2.0f : 0.0f; });
    float c[25]; CHECK(arq_mesh_corrections(&s, c) == ARQ_MESH_OK, "desviacion de exactamente 2,00 mm: se acepta");
    arq_mesh_sess_t t; fill(t, [](uint8_t p) -> float { return p == 12 ? 0.0f : p == 7 ? 2.01f : 0.0f; });
    float d[25]; memset(d, 0x55, sizeof d);
    CHECK(arq_mesh_corrections(&t, d) == ARQ_MESH_E_RANGE, "desviacion 2,01 mm: rechazada");
    uint8_t raw[sizeof d]; memset(raw, 0x55, sizeof raw); CHECK(!memcmp(d, raw, sizeof d), "una malla rechazada no escribe la salida");
    arq_mesh_sess_t u; start(u);
    CHECK(arq_mesh_confirm(&u, ARQ_MESH_Z_MIN) == ARQ_MESH_OK, "Z = Z_MIN se acepta como medicion");
    arq_mesh_sess_t v; start(v);
    CHECK(arq_mesh_confirm(&v, ARQ_MESH_Z_MIN - 0.5f) == ARQ_MESH_E_Z_RANGE && v.st == ARQ_MESH_ADJUST && arq_mesh_count(&v) == 0, "Z bajo Z_MIN: rechazada y no se registra");
    CHECK(arq_mesh_confirm(&v, ARQ_MESH_Z_MAX + 0.5f) == ARQ_MESH_E_Z_RANGE, "Z sobre Z_MAX rechazada");
    CHECK(arq_mesh_confirm(&v, NAN) == ARQ_MESH_E_NAN, "NaN rechazado");
    CHECK(arq_mesh_confirm(&v, INFINITY) == ARQ_MESH_E_NAN, "infinito rechazado");
    CHECK(arq_mesh_count(&v) == 0 && v.done == 0, "ninguna medicion falsa registrada");
  }
  // ---- No se simula una medicion sin confirmar ---------------------------------------------------------------------------------
  { arq_mesh_sess_t s; arq_mesh_sess_reset(&s);
    CHECK(arq_mesh_confirm(&s, 0.0f) == ARQ_MESH_E_STATE && arq_mesh_count(&s) == 0, "confirmar sin sesion");
    arq_mesh_begin(&s); CHECK(arq_mesh_confirm(&s, 0.0f) == ARQ_MESH_E_STATE, "confirmar durante PREP");
    arq_mesh_prep_done(&s); CHECK(arq_mesh_confirm(&s, 0.0f) == ARQ_MESH_E_STATE, "confirmar mientras la boquilla se mueve (MOVING)");
    CHECK(!arq_mesh_jog(&s, -1), "no se ajusta Z en MOVING");
    arq_mesh_arrived(&s, 1.0f);
    CHECK(!arq_mesh_begin(&s), "no se puede empezar otra sesion con una activa");
    float c[25]; CHECK(arq_mesh_corrections(&s, c) == ARQ_MESH_E_INCOMPLETE, "malla incompleta rechazada");
    for (int i = 0; i < 24; ++i) take(s, 0.0f);
    CHECK(arq_mesh_count(&s) == 24 && s.st == ARQ_MESH_ADJUST && arq_mesh_percent(&s) == 96, "24 puntos: aun ADJUST, 96 %%");
    CHECK(arq_mesh_corrections(&s, c) == ARQ_MESH_E_INCOMPLETE && !arq_mesh_finish(&s), "con 24 de 25 no se puede completar");
  }
  // ---- Cancelacion durante la medicion -----------------------------------------------------------------------------------------
  { arq_mesh_sess_t s; start(s); for (int i = 0; i < 10; ++i) take(s, 0.0f);
    arq_mesh_cancel(&s);
    CHECK(s.st == ARQ_MESH_ABORTED && s.done == 0 && arq_mesh_count(&s) == 0, "cancelar: ABORTED y sin datos (no queda una malla parcial)");
    float c[25]; CHECK(arq_mesh_corrections(&s, c) == ARQ_MESH_E_INCOMPLETE, "tras cancelar no hay malla que aplicar");
    CHECK(arq_mesh_confirm(&s, 0.0f) == ARQ_MESH_E_STATE, "confirmar tras cancelar");
    CHECK(arq_mesh_begin(&s) && s.st == ARQ_MESH_PREP && s.done == 0, "se puede empezar de nuevo desde cero");
    // cancelar en cada fase
    arq_mesh_sess_t a; arq_mesh_sess_reset(&a); arq_mesh_begin(&a); arq_mesh_cancel(&a); CHECK(a.st == ARQ_MESH_ABORTED, "cancelar en PREP");
    arq_mesh_sess_reset(&a); arq_mesh_begin(&a); arq_mesh_prep_done(&a); arq_mesh_cancel(&a); CHECK(a.st == ARQ_MESH_ABORTED, "cancelar en MOVING");
    arq_mesh_sess_t r; fill(r, [](uint8_t) -> float { return 0.0f; }); arq_mesh_cancel(&r); CHECK(r.st == ARQ_MESH_ABORTED && r.done == 0, "cancelar en REVIEW");
    arq_mesh_sess_t i; arq_mesh_sess_reset(&i); arq_mesh_cancel(&i); CHECK(i.st == ARQ_MESH_IDLE, "cancelar sin sesion no hace nada");
    arq_mesh_sess_t e; start(e); arq_mesh_fail(&e, ARQ_MESH_E_NOT_HOMED); CHECK(e.st == ARQ_MESH_ERROR && e.err == ARQ_MESH_E_NOT_HOMED && e.done == 0, "error: sin datos y con codigo");
  }
  // ---- Repeticion de un punto --------------------------------------------------------------------------------------------------
  { arq_mesh_sess_t s; start(s);
    for (int i = 0; i < 6; ++i) take(s, 0.10f);                      // puntos 0..4 y 9
    const uint8_t cur = s.cur;
    CHECK(arq_mesh_repeat(&s, 2) == ARQ_MESH_OK && s.st == ARQ_MESH_MOVING && s.cur == 2 && s.redo, "repetir el punto 2 (medido)");
    CHECK(!(s.done & (1u << 2)) && arq_mesh_count(&s) == 5, "el punto repetido queda PENDIENTE (5 de 25) hasta confirmarlo");
    arq_mesh_arrived(&s, 1.0f); CHECK(take(s, 0.33f) == ARQ_MESH_OK, "confirmar la repeticion");
    CHECK(near(s.z[2], 0.33f) && (s.done & (1u << 2)) && arq_mesh_count(&s) == 6, "valor nuevo registrado");
    CHECK(s.cur == cur && s.st == ARQ_MESH_ADJUST && !s.redo, "tras repetir se continua donde se estaba (punto %u)", s.cur);
    CHECK(arq_mesh_repeat(&s, cur) == ARQ_MESH_E_BAD_POINT, "no se repite un punto sin medir");
    CHECK(arq_mesh_repeat(&s, 25) == ARQ_MESH_E_BAD_POINT, "indice invalido");
    // repeticion estando en REVIEW
    arq_mesh_sess_t r; fill(r, [](uint8_t p) -> float { return 0.01f * p; });
    CHECK(arq_mesh_repeat(&r, 7) == ARQ_MESH_OK, "repetir en REVIEW");
    float c[25]; CHECK(arq_mesh_corrections(&r, c) == ARQ_MESH_E_INCOMPLETE, "con un punto en repeticion la malla esta incompleta");
    arq_mesh_arrived(&r, 1.0f); CHECK(arq_mesh_confirm(&r, 0.50f) == ARQ_MESH_OK && r.st == ARQ_MESH_REVIEW, "tras re-medir vuelve a REVIEW");
    CHECK(arq_mesh_corrections(&r, c) == ARQ_MESH_OK && near(c[7], 0.50f - 0.12f), "el valor repetido se usa");
    // repetir y cancelar: no se conserva el valor antiguo como si fuese bueno
    arq_mesh_sess_t q; fill(q, [](uint8_t) -> float { return 0.0f; });
    arq_mesh_repeat(&q, 3); arq_mesh_cancel(&q); CHECK(q.done == 0, "cancelar durante una repeticion borra todo");
  }
  // ---- Estadisticas ------------------------------------------------------------------------------------------------------------
  { float v[25]; for (int p = 0; p < 25; ++p) v[p] = 0;
    v[3] = 0.20f; v[17] = -0.30f;
    arq_mesh_stats_t st; arq_mesh_stats(v, 0x1FFFFFFu, &st);
    CHECK(near(st.max, 0.20f) && st.imax == 3 && near(st.min, -0.30f) && st.imin == 17 && near(st.range, 0.50f) && st.n == 25, "max/min/rango e indices");
    CHECK(near(st.mean, -0.004f) && near(st.sigma, sqrtf((0.04f + 0.09f) / 25 - 0.004f * 0.004f), 1e-4f), "media %.4f y sigma %.4f", st.mean, st.sigma);
    arq_mesh_stats(v, (1u << 3) | (1u << 17), &st); CHECK(st.n == 2 && near(st.range, 0.50f), "solo los puntos medidos (mascara)");
    arq_mesh_stats(v, 0, &st); CHECK(st.n == 0 && st.range == 0 && st.sigma == 0, "sin puntos");
    v[5] = NAN; arq_mesh_stats(v, 0x1FFFFFFu, &st); CHECK(st.n == 24, "los NaN no cuentan");
  }
  // ---- Validacion de la malla guardada en Marlin -------------------------------------------------------------------------------
  { float z[25] = {}; 
    CHECK(arq_mesh_check_stored(z, 15, 15, 67.5f, 67.5f, &AREA) == ARQ_MESH_OK, "malla valida");
    CHECK(arq_mesh_check_stored(z, 0, 0, 0, 0, &AREA) == ARQ_MESH_E_NO_MESH, "sin malla (rejilla sin definir)");
    CHECK(arq_mesh_check_stored(z, 15, 15, NAN, 67.5f, &AREA) == ARQ_MESH_E_NO_MESH, "paso NaN");
    CHECK(arq_mesh_check_stored(z, 10, 10, 70, 70, &AREA) == ARQ_MESH_E_LAYOUT, "malla con otra geometria (p. ej. la 3x3 antigua)");
    CHECK(arq_mesh_check_stored(z, 15.2f, 15, 67.5f, 67.5f, &AREA) == ARQ_MESH_E_LAYOUT, "inicio desplazado");
    z[4] = NAN; CHECK(arq_mesh_check_stored(z, 15, 15, 67.5f, 67.5f, &AREA) == ARQ_MESH_E_NAN, "valor NaN (malla incompleta)"); z[4] = 0;
    z[9] = 2.5f; CHECK(arq_mesh_check_stored(z, 15, 15, 67.5f, 67.5f, &AREA) == ARQ_MESH_E_RANGE, "valor fuera de rango"); z[9] = -2.0f;
    CHECK(arq_mesh_check_stored(z, 15, 15, 67.5f, 67.5f, &AREA) == ARQ_MESH_OK, "-2,00 mm exactos");
    const arq_mesh_area_t bad = { 1, 2, 1, 2 }; CHECK(arq_mesh_check_stored(z, 15, 15, 67.5f, 67.5f, &bad) == ARQ_MESH_E_LAYOUT, "area esperada invalida");
  }
  // ---- Escala y textos ---------------------------------------------------------------------------------------------------------
  { CHECK(near(arq_mesh_norm(0.1f, 0.2f), 0.5f) && near(arq_mesh_norm(-0.4f, 0.2f), -1.0f) && near(arq_mesh_norm(1.0f, 0.2f), 1.0f), "norm");
    CHECK(arq_mesh_norm(1.0f, 0.0f) == 0.0f && arq_mesh_norm(NAN, 1.0f) == 0.0f && arq_mesh_norm(1.0f, NAN) == 0.0f, "norm: casos degenerados");
    char b[16];
    arq_mesh_fmt_dev(b, sizeof b, 0.123f); CHECK(!strcmp(b, "+0.12"), "fmt +0.12 -> '%s'", b);
    arq_mesh_fmt_dev(b, sizeof b, -0.051f); CHECK(!strcmp(b, "-0.05"), "fmt -0.05 -> '%s'", b);
    arq_mesh_fmt_dev(b, sizeof b, -0.004f); CHECK(!strcmp(b, "0.00"), "nunca '-0.00' -> '%s'", b);
    arq_mesh_fmt_dev(b, sizeof b, 0.004f); CHECK(!strcmp(b, "0.00"), "+0.004 -> '%s'", b);
    arq_mesh_fmt_dev(b, sizeof b, 0.005f); CHECK(!strcmp(b, "+0.01") || !strcmp(b, "0.00"), "redondeo en 0,005 -> '%s'", b);
    arq_mesh_fmt_dev(b, sizeof b, -1.999f); CHECK(!strcmp(b, "-2.00"), "-1.999 -> '%s'", b);
    arq_mesh_fmt_dev(b, sizeof b, NAN); CHECK(!strcmp(b, "--"), "NaN -> '%s'", b);
    arq_mesh_fmt_dev(b, 4, 0.12f); CHECK(strlen(b) == 3, "buffer corto se trunca sin desbordar: '%s'", b);
    arq_mesh_fmt_z(b, sizeof b, 0.8f); CHECK(!strcmp(b, "0.80"), "Z 0.80 -> '%s'", b);
    arq_mesh_fmt_z(b, sizeof b, -0.4f); CHECK(!strcmp(b, "-0.40"), "Z -0.40 -> '%s'", b);
    arq_mesh_fmt_z(b, sizeof b, 10.0f); CHECK(!strcmp(b, "10.00"), "Z 10.00 -> '%s'", b);
    arq_mesh_fmt_z(b, 0, 1.0f);
    for (uint8_t e = 0; e < ARQ_MESH_E_COUNT + 2; ++e) CHECK(arq_mesh_err_text(e) && arq_mesh_err_text(e)[0], "texto de error %u", e);
  }
  printf("[mesh] %d comprobaciones, %d fallos\n", checks, fails);
  return fails ? 1 : 0;
}
