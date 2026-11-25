// Pruebas de la logica REAL de perfiles (Marlin/src/lcd/extui/mks_ui/arquis_machine_logic.cpp). Uso: tests/machines/run.sh
#include "arquis_machine_logic.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(c, msg) do { ++checks; if (!(c)) { ++fails; printf("[machine] FALLA %s (linea %d)\n", msg, __LINE__); } } while (0)

static void make(uint8_t *buf, uint8_t profile, uint8_t flags, uint16_t seq) {
  arq_mc_cfg_t c; arq_mc_factory(profile, &c); c.flags = flags;
  arq_mc_rec_t r; arq_mc_rec_pack(&r, &c, seq);
  memset(buf, 0xFF, ARQ_MC_REC_MAX); memcpy(buf, &r, sizeof(r));
}

int main() {
  const float SMIN = 25;   // shaping_min_freq del firmware (SHAPING_FREQ_Y = 25)
  // 1) Tabla de fabrica
  const arq_mc_profile_t *x = arq_mc_profile(ARQ_MC_XQ1), *a = arq_mc_profile(ARQ_MC_AQUILA_D1);
  CHECK(x && a && !arq_mc_profile(0) && !arq_mc_profile(3), "existen exactamente 2 perfiles");
  CHECK(!strcmp(x->name, "Arquis XQ1") && !strcmp(a->name, "Aquila D1"), "nombres");
  CHECK(x->v[ARQ_MP_BED_X] == 300 && x->v[ARQ_MP_BED_Y] == 300 && x->v[ARQ_MP_Z_MAX] == 450, "XQ1 300x300x450");
  CHECK(a->v[ARQ_MP_BED_X] == 230 && a->v[ARQ_MP_BED_Y] == 240 && a->v[ARQ_MP_Z_MAX] == 240, "Aquila 230x240x240");
  CHECK(fabsf(x->v[ARQ_MP_STEPS_Z] - 791.74f) < 1e-3 && fabsf(x->v[ARQ_MP_STEPS_E] - 748.72f) < 1e-3, "XQ1 pasos Z/E");
  CHECK(fabsf(a->v[ARQ_MP_STEPS_Z] - 803.41f) < 1e-3 && fabsf(a->v[ARQ_MP_STEPS_E] - 781.08f) < 1e-3, "Aquila pasos Z/E");
  CHECK(x->v[ARQ_MP_STEPS_X] == 160 && a->v[ARQ_MP_STEPS_Y] == 160, "pasos X/Y 160");
  CHECK(x->es_inv == 7 && a->es_inv == ARQ_MC_BZ, "logica de finales (XQ1 X/Y/Z invertidos; Aquila solo Z)");
  CHECK(x->dir_inv == (ARQ_MC_BX | ARQ_MC_BY) && a->dir_inv == ARQ_MC_BE, "direcciones (XQ1 X/Y; Aquila E0)");
  CHECK(x->pending == ((1UL << ARQ_MP_FEED_Z) | (1UL << ARQ_MP_ACC_Z)) && a->pending == 0, "XQ1: Z 5 mm/s y 100 mm/s2 marcados provisionales");
  CHECK(x->v[ARQ_MP_FEED_Z] == 5 && x->v[ARQ_MP_ACC_Z] == 100 && a->v[ARQ_MP_FEED_Z] == 45 && a->v[ARQ_MP_ACC_Z] == 150, "Z velocidad/aceleracion");
  CHECK(a->v[ARQ_MP_ACC_Y] == 3500 && x->v[ARQ_MP_ACC_Y] == 4500, "aceleracion Y");

  // 2) Validacion
  arq_mc_cfg_t c;
  arq_mc_factory(ARQ_MC_XQ1, &c);  CHECK(arq_mc_validate(&c, SMIN) == -1, "XQ1 de fabrica es valido");
  arq_mc_factory(ARQ_MC_AQUILA_D1, &c); CHECK(arq_mc_validate(&c, SMIN) == -1, "Aquila de fabrica es valido");
  c.v[ARQ_MP_BED_X] = 241; CHECK(arq_mc_validate(&c, SMIN) == ARQ_MP_BED_X, "Aquila: ancho X > fabrica + 10 se rechaza");
  c.v[ARQ_MP_BED_X] = 240; CHECK(arq_mc_validate(&c, SMIN) == -1, "Aquila: ancho X = fabrica + 10 se acepta");
  c.v[ARQ_MP_FEED_Z] = 46; CHECK(arq_mc_validate(&c, SMIN) == ARQ_MP_FEED_Z, "Z > 45 mm/s se rechaza");
  c.v[ARQ_MP_FEED_Z] = 45; c.v[ARQ_MP_STEPS_X] = 1600; CHECK(arq_mc_validate(&c, SMIN) == ARQ_MP_STEPS_X, "pasos X = 1600 (error de tecleo) se rechaza");
  c.v[ARQ_MP_STEPS_X] = 160; c.v[ARQ_MP_CUR_E] = 1200; CHECK(arq_mc_validate(&c, SMIN) == ARQ_MP_CUR_E, "corriente 1200 mA se rechaza");
  c.v[ARQ_MP_CUR_E] = 700; c.v[ARQ_MP_SHP_FX] = 20; CHECK(arq_mc_validate(&c, SMIN) == ARQ_MP_SHP_FX, "shaping 20 Hz < minimo del bufer se rechaza");
  c.v[ARQ_MP_SHP_FX] = 0; CHECK(arq_mc_validate(&c, SMIN) == -1, "shaping 0 (desactivado) se acepta");
  c.v[ARQ_MP_OFS_Z] = NAN; CHECK(arq_mc_validate(&c, SMIN) == ARQ_MP_OFS_Z, "NaN se rechaza");
  c.v[ARQ_MP_OFS_Z] = 0; c.profile = 0; CHECK(arq_mc_validate(&c, SMIN) == ARQ_MP_COUNT, "sin modelo no es valido");
  c.profile = ARQ_MC_AQUILA_D1; c.dir_inv = 16; CHECK(arq_mc_validate(&c, SMIN) == ARQ_MP_COUNT, "mascara de direccion invalida");
  // El XQ1 admite su altura 450; el Aquila no (su maximo es 250)
  arq_mc_factory(ARQ_MC_AQUILA_D1, &c); c.v[ARQ_MP_Z_MAX] = 450; CHECK(arq_mc_validate(&c, SMIN) == ARQ_MP_Z_MAX, "Aquila no admite Z 450");

  // 3) Cambio de modelo conservando la mecanica guardada
  arq_mc_factory(ARQ_MC_XQ1, &c); arq_mc_take_geometry(ARQ_MC_AQUILA_D1, &c);
  CHECK(c.profile == ARQ_MC_AQUILA_D1 && c.v[ARQ_MP_BED_X] == 230 && c.es_inv == ARQ_MC_BZ && c.dir_inv == ARQ_MC_BE, "geometria del Aquila");
  CHECK(fabsf(c.v[ARQ_MP_STEPS_Z] - 791.74f) < 1e-3, "la mecanica se conserva (pasos del XQ1)");
  arq_mc_factory(ARQ_MC_AQUILA_D1, &c);
  CHECK(fabsf(c.v[ARQ_MP_STEPS_Z] - 803.41f) < 1e-3 && c.v[ARQ_MP_BED_X] == 230 && c.flags == 0, "fabrica completa no deja valores del perfil anterior");

  // 4) Deteccion por la EEPROM de Marlin
  CHECK(arq_mc_detect(791.74f, 748.72f) == ARQ_MC_XQ1, "detecta XQ1");
  CHECK(arq_mc_detect(803.41f, 781.08f) == ARQ_MC_AQUILA_D1, "detecta Aquila");
  CHECK(arq_mc_detect(791.74f, 760.0f) == ARQ_MC_NONE, "pasos E retocados: no detecta");

  // 5) CRC y registro
  CHECK(arq_mc_crc16((const uint8_t *)"123456789", 9) == 0x29B1, "CRC-16/CCITT-FALSE de referencia");
  CHECK(sizeof(arq_mc_rec_t) == 32, "registro de 32 bytes");
  uint8_t A[ARQ_MC_REC_MAX], B[ARQ_MC_REC_MAX];
  make(A, ARQ_MC_AQUILA_D1, ARQ_MCF_DONE | ARQ_MCF_ES_OK | ARQ_MCF_DIR_OK, 7);
  CHECK(arq_mc_rec_check(A, sizeof A) == ARQ_MC_OK, "registro recien escrito es valido");
  arq_mc_cfg_t u; memset(&u, 0, sizeof(u)); arq_mc_rec_unpack(A, &u);
  CHECK(u.profile == ARQ_MC_AQUILA_D1 && u.flags == 7 && u.es_inv == ARQ_MC_BZ && u.dir_inv == ARQ_MC_BE && u.v[ARQ_MP_BED_Y] == 240 && u.v[ARQ_MP_Z_MAX] == 240, "persistencia: se recupera igual");
  uint8_t T[ARQ_MC_REC_MAX];
  for (int i = 0; i < 32; ++i) { memcpy(T, A, sizeof T); T[i] ^= 0x10; if (arq_mc_rec_check(T, sizeof T) == ARQ_MC_OK) { CHECK(false, "un bit cambiado en cualquier byte se detecta"); break; } }
  memcpy(T, A, sizeof T); T[0] = 0; CHECK(arq_mc_rec_check(T, sizeof T) == ARQ_MC_E_MAGIC, "sin magic");
  memset(T, 0xFF, sizeof T); CHECK(arq_mc_rec_check(T, sizeof T) == ARQ_MC_E_MAGIC, "EEPROM borrada (0xFF) no es valida");
  memset(T, 0x00, sizeof T); CHECK(arq_mc_rec_check(T, sizeof T) == ARQ_MC_E_MAGIC, "EEPROM a cero no es valida");
  memcpy(T, A, sizeof T); T[4] = 2; CHECK(arq_mc_rec_check(T, sizeof T) == ARQ_MC_E_FMT, "formato futuro no se interpreta");
  memcpy(T, A, sizeof T); T[5] = 20; CHECK(arq_mc_rec_check(T, sizeof T) == ARQ_MC_E_LEN, "longitud corta");
  // Valores fuera de rango aunque el CRC sea correcto
  { arq_mc_cfg_t k; arq_mc_factory(ARQ_MC_AQUILA_D1, &k); k.v[ARQ_MP_BED_X] = 300; arq_mc_rec_t r; arq_mc_rec_pack(&r, &k, 1);
    memset(T, 0xFF, sizeof T); memcpy(T, &r, sizeof r); CHECK(arq_mc_rec_check(T, sizeof T) == ARQ_MC_E_RANGE, "Aquila con cama 300 en la EEPROM se rechaza"); }
  { arq_mc_cfg_t k; arq_mc_factory(ARQ_MC_XQ1, &k); k.profile = 3; arq_mc_rec_t r; arq_mc_rec_pack(&r, &k, 1);
    memset(T, 0xFF, sizeof T); memcpy(T, &r, sizeof r); CHECK(arq_mc_rec_check(T, sizeof T) == ARQ_MC_E_PROFILE, "modelo desconocido"); }
  { arq_mc_cfg_t k; arq_mc_factory(ARQ_MC_XQ1, &k); k.flags = 0x80; arq_mc_rec_t r; arq_mc_rec_pack(&r, &k, 1);
    memset(T, 0xFF, sizeof T); memcpy(T, &r, sizeof r); CHECK(arq_mc_rec_check(T, sizeof T) == ARQ_MC_E_FLAGS, "flags desconocidos"); }
  // Migracion: un registro de formato 1 con campos anadidos al final (len 40) sigue siendo legible
  { memcpy(T, A, 30); T[5] = 40; memset(T + 30, 0xAB, 8); const uint16_t crc = arq_mc_crc16(T, 38); memcpy(T + 38, &crc, 2);
    CHECK(arq_mc_rec_check(T, sizeof T) == ARQ_MC_OK, "registro mas largo (campos nuevos) se acepta");
    arq_mc_cfg_t k; memset(&k, 0, sizeof k); arq_mc_rec_unpack(T, &k); CHECK(k.profile == ARQ_MC_AQUILA_D1 && k.v[ARQ_MP_BED_X] == 230, "y se leen los campos conocidos"); }

  // 6) Dos copias
  make(A, ARQ_MC_XQ1, ARQ_MCF_DONE, 10); make(B, ARQ_MC_AQUILA_D1, ARQ_MCF_DONE, 11);
  CHECK(arq_mc_rec_pick(A, B) == 1 && arq_mc_rec_target(A, B) == 0, "se usa la mas reciente y se escribe la otra");
  make(A, ARQ_MC_XQ1, ARQ_MCF_DONE, 0xFFFF); make(B, ARQ_MC_AQUILA_D1, ARQ_MCF_DONE, 0x0000);
  CHECK(arq_mc_rec_pick(A, B) == 1, "vuelta del contador 0xFFFF -> 0");
  make(A, ARQ_MC_XQ1, ARQ_MCF_DONE, 5); B[9] ^= 1;
  CHECK(arq_mc_rec_pick(A, B) == 0 && arq_mc_rec_target(A, B) == 1, "copia B danada: se usa A y se reescribe B");
  memset(A, 0xFF, sizeof A); memset(B, 0xFF, sizeof B);
  CHECK(arq_mc_rec_pick(A, B) == -1 && arq_mc_rec_target(A, B) == 0, "EEPROM nueva: ninguna valida (asistente)");
  // Corte de luz a mitad de escritura: la copia que se escribia queda a medias -> se sigue usando la anterior
  make(A, ARQ_MC_XQ1, ARQ_MCF_DONE | ARQ_MCF_ES_OK | ARQ_MCF_DIR_OK, 20);
  make(B, ARQ_MC_XQ1, ARQ_MCF_DONE, 19);
  uint8_t NEWB[ARQ_MC_REC_MAX]; make(NEWB, ARQ_MC_AQUILA_D1, ARQ_MCF_DONE, 21);
  bool ok_all = true;
  for (int cut = 0; cut < 32; ++cut) {                 // se interrumpe tras escribir 'cut' bytes de la copia nueva
    uint8_t W[ARQ_MC_REC_MAX]; memcpy(W, B, sizeof W); memcpy(W, NEWB, cut);
    const int8_t k = arq_mc_rec_pick(A, W);
    if (k != 0) { ok_all = false; printf("  corte en %d -> copia %d\n", cut, k); }
  }
  CHECK(ok_all, "cualquier corte durante la escritura deja la configuracion anterior valida");
  { uint8_t W[ARQ_MC_REC_MAX]; memcpy(W, NEWB, sizeof W); CHECK(arq_mc_rec_pick(A, W) == 1, "escritura completa: se usa la nueva"); }

  // 7) Direcciones por el bit shaft del TMC2209 (compilado: INVERT_X_DIR = INVERT_Y_DIR = true, Z y E0 false)
  const uint8_t COMPILED = ARQ_MC_BX | ARQ_MC_BY;
  CHECK(arq_mc_shaft_mask(x->dir_inv, COMPILED) == 0, "XQ1: ningun driver invertido (igual que antes)");
  CHECK(arq_mc_shaft_mask(a->dir_inv, COMPILED) == (ARQ_MC_BX | ARQ_MC_BY | ARQ_MC_BE), "Aquila: X, Y y E0 invertidos en el driver");
  CHECK(arq_mc_shaft_mask(ARQ_MC_BZ, 0) == ARQ_MC_BZ, "Z invertido a mano");

  // 8) Textos y redondeo
  CHECK(!strcmp(arq_mc_param_unit(ARQ_MP_CUR_X), "mA") && !strcmp(arq_mc_param_name(ARQ_MP_STEPS_E), "Pasos E"), "nombres y unidades");
  CHECK(fabsf(arq_mc_round(ARQ_MP_STEPS_Z, 803.4149f) - 803.41f) < 1e-3 && arq_mc_round(ARQ_MP_CUR_X, 650.4f) == 650, "redondeo");

  // 9) P1: cambio de modelo sin mezclar la mecanica. 'saved' = lo que hay: registro + mecanica de Marlin.
  {
    const uint8_t X = ARQ_MC_XQ1, Q = ARQ_MC_AQUILA_D1, N = ARQ_MC_NONE;
    auto mech_is = [](const arq_mc_cfg_t &c, uint8_t prof) {      // TODA la mecanica (pasos..Shaping) es la de 'prof'
      const arq_mc_profile_t *p = arq_mc_profile(prof);
      for (uint8_t i = ARQ_MP_STEPS_X; i <= ARQ_MP_SHP_FY; ++i) if (fabsf(c.v[i] - p->v[i]) > 1e-4f) return false;
      return true;
    };
    auto geo_is = [](const arq_mc_cfg_t &c, uint8_t prof) {
      const arq_mc_profile_t *p = arq_mc_profile(prof);
      return c.v[ARQ_MP_BED_X] == p->v[ARQ_MP_BED_X] && c.v[ARQ_MP_BED_Y] == p->v[ARQ_MP_BED_Y] && c.v[ARQ_MP_Z_MAX] == p->v[ARQ_MP_Z_MAX]
          && c.es_inv == p->es_inv && c.dir_inv == p->dir_inv && c.profile == prof;
    };
    arq_mc_cfg_t xs, qs, out;                                        // datos guardados de cada maquina (con M206 del usuario)
    arq_mc_factory(X, &xs); xs.flags = 0x0F; xs.v[ARQ_MP_OFS_Z] = -1.25f;
    arq_mc_factory(Q, &qs); qs.flags = 0x0F; qs.v[ARQ_MP_OFS_X] = 2;
    auto det = [](const arq_mc_cfg_t &c) { return arq_mc_detect(c.v[ARQ_MP_STEPS_Z], c.v[ARQ_MP_STEPS_E]); };

    // keep_check: tabla de decision
    CHECK(arq_mc_keep_check(Q, true, X, X) == ARQ_KEEP_OTHER_MODEL, "XQ1 -> Aquila con pasos de fabrica del XQ1: no se conserva");
    CHECK(arq_mc_keep_check(X, true, Q, Q) == ARQ_KEEP_OTHER_MODEL, "Aquila -> XQ1 con pasos del Aquila: no se conserva");
    CHECK(arq_mc_keep_check(Q, true, X, N) == ARQ_KEEP_OTHER_MODEL, "XQ1 -> Aquila con pasos calibrados (no detectado): no se conserva");
    CHECK(arq_mc_keep_check(X, true, Q, N) == ARQ_KEEP_OTHER_MODEL, "Aquila -> XQ1 con pasos calibrados: no se conserva");
    CHECK(arq_mc_keep_check(Q, true, N, N) == ARQ_KEEP_UNCONFIRMED, "sin registro y no detectado: no se puede confirmar");
    CHECK(arq_mc_keep_check(Q, true, N, Q) == ARQ_KEEP_OK, "sin registro y detectado = elegido: se puede conservar");
    CHECK(arq_mc_keep_check(Q, true, N, X) == ARQ_KEEP_OTHER_MODEL, "sin registro y detectado el otro: no");
    CHECK(arq_mc_keep_check(X, true, X, N) == ARQ_KEEP_OK, "mismo modelo con pasos calibrados (registro de ese modelo): si");
    CHECK(arq_mc_keep_check(X, true, X, Q) == ARQ_KEEP_OTHER_MODEL, "registro XQ1 pero pasos de fabrica del Aquila: contradiccion -> no");
    CHECK(arq_mc_keep_check(X, false, X, X) == ARQ_KEEP_NO_EEPROM, "EEPROM de Marlin no valida: no");
    CHECK(arq_mc_keep_check(N, true, X, X) == ARQ_KEEP_NO_MODEL, "sin modelo elegido: no");

    // XQ1 -> Aquila: el usuario intenta Conservar en todos los casos
    for (int variant = 0; variant < 3; ++variant) {
      arq_mc_cfg_t s = xs;
      if (variant == 1) s.v[ARQ_MP_STEPS_E] = 760.5f;                // pasos E calibrados a mano
      if (variant == 2) s.v[ARQ_MP_STEPS_Z] = 795;                   // pasos Z modificados
      const uint8_t st = arq_mc_keep_check(Q, true, X, det(s));
      const bool kept = arq_mc_choose(Q, st, true, &s, &out);
      CHECK(!kept && mech_is(out, Q) && geo_is(out, Q), "XQ1 -> Aquila (fabrica / E calibrado / Z modificado): mecanica y geometria del Aquila");
      CHECK(out.v[ARQ_MP_OFS_Z] == 0 && out.v[ARQ_MP_OFS_X] == 0 && out.flags == 0, "XQ1 -> Aquila: M206 del XQ1 NO se conserva; hay que repetir pruebas");
    }
    // Aquila -> XQ1
    for (int variant = 0; variant < 3; ++variant) {
      arq_mc_cfg_t s = qs;
      if (variant == 1) s.v[ARQ_MP_STEPS_E] = 795.2f;
      if (variant == 2) s.v[ARQ_MP_STEPS_Z] = 810;
      const uint8_t st = arq_mc_keep_check(X, true, Q, det(s));
      const bool kept = arq_mc_choose(X, st, true, &s, &out);
      CHECK(!kept && mech_is(out, X) && geo_is(out, X) && out.v[ARQ_MP_OFS_X] == 0, "Aquila -> XQ1 (fabrica / E calibrado / Z modificado): todo del XQ1, sin M206 del Aquila");
    }
    // Sin registro (primera instalacion) y pasos calibrados: no se puede confirmar -> fabrica aunque pida Conservar
    { arq_mc_cfg_t s = qs; s.profile = N; s.v[ARQ_MP_STEPS_E] = 777;
      const uint8_t st = arq_mc_keep_check(Q, true, N, det(s));
      CHECK(!arq_mc_choose(Q, st, true, &s, &out) && mech_is(out, Q) && out.v[ARQ_MP_OFS_X] == 0, "primera instalacion sin confirmar: fabrica y sin M206"); }
    // Perfil detectado correctamente (primera instalacion): se conserva
    { arq_mc_cfg_t s = qs; s.profile = N; s.v[ARQ_MP_ACC_Y] = 3000;   // el usuario habia bajado la aceleracion Y
      const uint8_t st = arq_mc_keep_check(Q, true, N, det(s));
      CHECK(arq_mc_choose(Q, st, true, &s, &out) && out.v[ARQ_MP_ACC_Y] == 3000 && geo_is(out, Q) && out.v[ARQ_MP_OFS_X] == 2,
            "detectado = elegido: se conservan su mecanica y su M206; geometria de fabrica (sin registro)"); }
    // Mismo modelo con registro y E calibrado: Conservar mantiene todo, incluida la geometria guardada
    { arq_mc_cfg_t s = xs; s.v[ARQ_MP_STEPS_E] = 752; s.v[ARQ_MP_Z_MAX] = 440;
      const uint8_t st = arq_mc_keep_check(X, true, X, det(s));
      CHECK(arq_mc_choose(X, st, true, &s, &out) && out.v[ARQ_MP_STEPS_E] == 752 && out.v[ARQ_MP_Z_MAX] == 440 && out.v[ARQ_MP_OFS_Z] == -1.25f,
            "mismo modelo confirmado: Conservar mantiene pasos, medidas y M206"); }
    // Mismo modelo confirmado, eligiendo De fabrica: mecanica de fabrica, M206 del usuario
    { const uint8_t st = arq_mc_keep_check(X, true, X, det(xs));
      CHECK(!arq_mc_choose(X, st, false, &xs, &out) && mech_is(out, X) && out.v[ARQ_MP_OFS_Z] == -1.25f, "De fabrica en el mismo modelo: conserva M206"); }
    // M206 fuera de rango nunca se arrastra
    { arq_mc_cfg_t s = xs; s.v[ARQ_MP_OFS_Z] = 35;
      arq_mc_choose(X, ARQ_KEEP_OK, false, &s, &out); CHECK(out.v[ARQ_MP_OFS_Z] == 0, "M206 fuera de rango: 0");
      arq_mc_choose(X, ARQ_KEEP_OK, true, &s, &out);  CHECK(out.v[ARQ_MP_OFS_Z] == 0, "M206 fuera de rango (Conservar): 0"); }
    // Barrido: ningun escenario de cambio de modelo deja un valor mecanico del modelo anterior
    int mixed = 0;
    for (uint8_t from = 1; from <= 2; ++from) for (uint8_t to = 1; to <= 2; ++to) if (from != to)
      for (int rec = 0; rec < 2; ++rec) for (int cal = 0; cal < 4; ++cal) for (int want = 0; want < 2; ++want) {
        arq_mc_cfg_t s; arq_mc_factory(from, &s); s.profile = rec ? from : N;
        if (cal & 1) s.v[ARQ_MP_STEPS_E] += 7.3f;
        if (cal & 2) s.v[ARQ_MP_STEPS_Z] += 4.1f;
        const uint8_t st = arq_mc_keep_check(to, true, s.profile, det(s));
        arq_mc_choose(to, st, want, &s, &out);
        if (!mech_is(out, to) || !geo_is(out, to)) ++mixed;
      }
    CHECK(mixed == 0, "barrido de 32 combinaciones de cambio de modelo: 0 con mecanica o geometria del modelo anterior");
  }

  // 10) P5: clasificacion de GCONF leido por UART (TMCStepper 0.8.10: sin respuesta valida -> CRCerror y 0)
  {
    const uint32_t CONF = ARQ_GCONF_PDN_DISABLE | ARQ_GCONF_MSTEP_REG;
    CHECK(arq_mc_tmc_classify(CONF, false, false) == ARQ_TMC_R_OK, "GCONF configurado, shaft 0 esperado: correcto");
    CHECK(arq_mc_tmc_classify(CONF | ARQ_GCONF_SHAFT, false, true) == ARQ_TMC_R_OK, "shaft 1 esperado: correcto");
    CHECK(arq_mc_tmc_classify(CONF, false, true) == ARQ_TMC_R_SHAFT, "shaft distinto");
    CHECK(arq_mc_tmc_classify(0x101, false, true) == ARQ_TMC_R_RESET, "valores de reinicio del TMC2209 (0x101): reiniciado");
    CHECK(arq_mc_tmc_classify(0, true, false) == ARQ_TMC_R_NOREPLY, "CRCerror (sin respuesta valida): no se interpreta el valor");
    CHECK(arq_mc_tmc_classify(CONF | ARQ_GCONF_SHAFT, true, true) == ARQ_TMC_R_NOREPLY, "CRCerror manda aunque el valor parezca bueno");
    CHECK(arq_mc_tmc_classify(0, false, false) == ARQ_TMC_R_RESET, "GCONF = 0 con CRC valido: no configurado (no se toma por correcto)");
  }

  printf("[machine] %d comprobaciones, %d fallos\n", checks, fails);
  return fails ? 1 : 0;
}
