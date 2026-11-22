// Pruebas de la logica de estados de ARQUIS UI (arquis_logic.cpp, el mismo archivo que va al firmware). Sin LVGL ni Marlin.
#include <stdio.h>
#include <string.h>
#include "arquis_logic.h"
#include <math.h>
static int fails = 0, total = 0;
#define EXPECT(c, ...) do { ++total; if (!(c)) { ++fails; printf("FALLA  "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static arq_pstate_t st(bool stopped, bool paused, bool printing, int hc, int ht, int bc, int bt) {
  const int16_t c[2] = { (int16_t)hc, (int16_t)bc }, t[2] = { (int16_t)ht, (int16_t)bt }, tol[2] = { 3, 3 };
  return arq_printer_state(stopped, paused, printing, 2, c, t, tol);
}
int main() {
  // Temperatura individual (histeresis 3, como TEMP_HYSTERESIS de Marlin)
  EXPECT(arq_temp_state(24, 0, 3) == ARQ_TEMP_OFF, "apagado y frio");
  EXPECT(arq_temp_state(50, 0, 3) == ARQ_TEMP_OFF, "apagado a 50 (limite) sigue 'apagado'");
  EXPECT(arq_temp_state(51, 0, 3) == ARQ_TEMP_COOLING, "apagado a 51 = enfriando");
  EXPECT(arq_temp_state(118, 0, 3) == ARQ_TEMP_COOLING, "apagado y caliente = enfriando");
  EXPECT(arq_temp_state(168, 210, 3) == ARQ_TEMP_HEATING, "calentando");
  EXPECT(arq_temp_state(206, 210, 3) == ARQ_TEMP_HEATING, "a 4 grados: aun calentando");
  EXPECT(arq_temp_state(207, 210, 3) == ARQ_TEMP_READY, "a 3 grados: objetivo alcanzado");
  EXPECT(arq_temp_state(210, 210, 3) == ARQ_TEMP_READY, "en objetivo");
  EXPECT(arq_temp_state(213, 210, 3) == ARQ_TEMP_READY, "3 grados por encima: objetivo");
  EXPECT(arq_temp_state(214, 210, 3) == ARQ_TEMP_COOLING, "por encima del objetivo: enfriando");
  EXPECT(arq_temp_state(-5, 0, 3) == ARQ_TEMP_OFF, "lectura negativa no rompe");

  // Estados globales (los 7 pedidos)
  EXPECT(st(false, false, false, 24, 0, 23, 0) == ARQ_PS_IDLE, "lista (todo apagado)");
  // Reposo = lo anterior pero sin tarjeta SD; solo afecta al estado IDLE
  EXPECT(arq_with_standby(ARQ_PS_IDLE, true)  == ARQ_PS_IDLE,    "lista: con tarjeta y todo apagado");
  EXPECT(arq_with_standby(ARQ_PS_IDLE, false) == ARQ_PS_STANDBY, "reposo: sin tarjeta y todo apagado");
  { const arq_pstate_t o[] = { ARQ_PS_HEATING, ARQ_PS_AT_TEMP, ARQ_PS_COOLING, ARQ_PS_PRINTING, ARQ_PS_PAUSED, ARQ_PS_ERROR, ARQ_PS_STANDBY };
    for (arq_pstate_t x : o) EXPECT(arq_with_standby(x, false) == x && arq_with_standby(x, true) == x, "reposo: no cambia el estado %d", (int)x); }
  EXPECT(st(false, false, false, 168, 210, 60, 60) == ARQ_PS_HEATING, "calentando (hotend)");
  EXPECT(st(false, false, false, 24, 0, 40, 60) == ARQ_PS_HEATING, "calentando (solo cama)");
  EXPECT(st(false, false, false, 210, 210, 60, 60) == ARQ_PS_AT_TEMP, "temperatura alcanzada");
  EXPECT(st(false, false, false, 210, 210, 24, 0) == ARQ_PS_AT_TEMP, "hotend alcanzado, cama apagada");
  EXPECT(st(false, false, false, 210, 210, 40, 60) == ARQ_PS_HEATING, "uno alcanzado y otro calentando = calentando");
  EXPECT(st(false, false, false, 118, 0, 41, 0) == ARQ_PS_COOLING, "enfriando");
  EXPECT(st(false, false, true, 210, 210, 60, 60) == ARQ_PS_PRINTING, "imprimiendo");
  EXPECT(st(false, false, true, 150, 210, 60, 60) == ARQ_PS_PRINTING, "imprimiendo prevalece sobre calentando");
  EXPECT(st(false, true, false, 205, 210, 60, 60) == ARQ_PS_PAUSED, "pausada");
  EXPECT(st(true, false, false, 24, 0, 23, 0) == ARQ_PS_ERROR, "error");
  EXPECT(st(true, true, true, 210, 210, 60, 60) == ARQ_PS_ERROR, "error prevalece sobre todo");
  EXPECT(st(false, true, true, 210, 210, 60, 60) == ARQ_PS_PAUSED, "pausada prevalece sobre imprimiendo");
  { const int16_t c[1] = { 30 }, t[1] = { 0 }, tol[1] = { 3 };
    EXPECT(arq_printer_state(false, false, false, 1, c, t, tol) == ARQ_PS_IDLE, "maquina sin cama (1 calentador)"); }
  EXPECT(arq_printer_state(false, false, false, 0, nullptr, nullptr, nullptr) == ARQ_PS_IDLE, "sin calentadores");

  // Tiempo restante (misma formula que Marlin; 0 = desconocido)
  EXPECT(arq_remaining_s(8100, 62) == 4964, "62%% a 2:15 -> restan 4964 s (%u)", (unsigned)arq_remaining_s(8100, 62));
  EXPECT(arq_remaining_s(3600, 50) == 3600, "50%% a 1 h -> resta 1 h");
  EXPECT(arq_remaining_s(30, 50) == 0, "menos de 60 s: desconocido");
  EXPECT(arq_remaining_s(3600, 0) == 0, "0%%: desconocido");
  EXPECT(arq_remaining_s(3600, 1) == 0, "1%%: desconocido (avance insuficiente)");
  EXPECT(arq_remaining_s(3600, 2) == 176400, "2%% a 1 h: calculado (176400 s)");
  EXPECT(arq_remaining_s(3600, 100) == 0, "100%%: nada que restar");
  EXPECT(arq_remaining_s(0xFFFFFFFF, 2) > 0, "sin desbordamiento con tiempos enormes");

  // Formato H:MM
  char b[16];
  arq_fmt_hm(b, sizeof b, 0);        EXPECT(!strcmp(b, "0:00"), "0 s -> %s", b);
  arq_fmt_hm(b, sizeof b, 59);       EXPECT(!strcmp(b, "0:00"), "59 s -> %s", b);
  arq_fmt_hm(b, sizeof b, 60);       EXPECT(!strcmp(b, "0:01"), "60 s -> %s", b);
  arq_fmt_hm(b, sizeof b, 8100);     EXPECT(!strcmp(b, "2:15"), "8100 s -> %s", b);
  arq_fmt_hm(b, sizeof b, 36000 + 5 * 60); EXPECT(!strcmp(b, "10:05"), "10 h 5 min -> %s", b);
  arq_fmt_hm(b, sizeof b, 100 * 3600); EXPECT(!strcmp(b, "100:00"), "100 h -> %s", b);
  { char tiny[4]; size_t n = arq_fmt_hm(tiny, sizeof tiny, 8100); EXPECT(strlen(tiny) < 4 && n >= 3, "buffer pequeno no desborda"); }

  // strlcpy propio
  { char d[6]; size_t n = arq_strlcpy(d, "abcdefghij", sizeof d); EXPECT(n == 10 && !strcmp(d, "abcde"), "strlcpy trunca y termina en NUL"); }
  { char d[6]; arq_strlcpy(d, "ab", sizeof d); EXPECT(!strcmp(d, "ab"), "strlcpy copia corto"); }

  // Fin de impresion (arq_finish_detect / arq_finish_new_job). Caso real del propietario: el archivo termino, el trabajo sigue "activo" hasta M1001.
  {
    arq_finish_state_t s = { false, 0 };
    arq_finish_in_t in = { true, false, false, false, true, true, 1000 };                     // imprimiendo con normalidad
    EXPECT(!arq_finish_detect(&s, &in), "imprimiendo: no hay fin");
    in.file_open = false; in.sd_done = true; in.job_active = true;                            // EOF: sdprintdone, trabajo activo hasta M1001
    EXPECT(arq_finish_detect(&s, &in), "fin de archivo con el trabajo aun activo SE DETECTA (el fallo original lo perdia)");
    in.once = true;
    EXPECT(!arq_finish_detect(&s, &in), "ya atendido: no se repite");
  }
  {
    arq_finish_state_t s = { false, 0 };
    arq_finish_in_t in = { true, false, false, false, false, false, 5000 };                   // la foto: WORKING, sin trabajo, sin archivo, ventana perdida
    EXPECT(!arq_finish_detect(&s, &in), "respaldo: empieza a contar");
    in.now_ms = 5000 + ARQ_FINISH_GONE_MS - 1;
    EXPECT(!arq_finish_detect(&s, &in), "respaldo: aun no pasaron 3 s");
    in.now_ms = 5000 + ARQ_FINISH_GONE_MS;
    EXPECT(arq_finish_detect(&s, &in), "respaldo: a los 3 s se detecta el fin (caso de la foto)");
  }
  {
    arq_finish_state_t s = { false, 0 };
    arq_finish_in_t in = { true, true, false, false, false, false, 0 };                       // cargando la miniatura: aun no empezo
    EXPECT(!arq_finish_detect(&s, &in), "preparando");
    in.now_ms = 60000; EXPECT(!arq_finish_detect(&s, &in), "preparando mucho rato: nunca es fin");
    in.preparing = false; in.job_active = true;                                               // empieza y hay trabajo: el contador se rearma
    EXPECT(!arq_finish_detect(&s, &in), "con trabajo activo no hay fin");
    in.job_active = false; in.now_ms = 61000;
    EXPECT(!arq_finish_detect(&s, &in) && s.armed, "el contador reempieza desde cero tras una interrupcion");
    in.job_active = false; in.file_open = true; in.now_ms = 70000;
    EXPECT(!arq_finish_detect(&s, &in), "con archivo abierto (preparando el inicio, pausa) nunca es fin");
  }
  {
    arq_finish_state_t s = { false, 0 };
    arq_finish_in_t in = { false, false, false, true, false, false, 0 };                      // pantalla sin impresion (p. ej. cancelada por el usuario)
    EXPECT(!arq_finish_detect(&s, &in), "sin impresion en la pantalla (IDLE/cancelada): nunca fin");
    in.working = true; in.sd_done = false; in.now_ms = 1; arq_finish_detect(&s, &in); in.now_ms = 10000;
    EXPECT(arq_finish_detect(&s, &in), "wrap-around de millis no rompe la resta");
    arq_finish_state_t w = { true, 0xFFFFFF00u }; arq_finish_in_t i2 = { true, false, false, false, false, false, 0x00000100u + ARQ_FINISH_GONE_MS };
    EXPECT(arq_finish_detect(&w, &i2), "millis() desbordado (wrap) sigue contando bien");
  }
  EXPECT(arq_finish_new_job(true, false), "archivo abierto y sin terminar = impresion nueva: descartar el aviso");
  EXPECT(!arq_finish_new_job(false, true), "terminada y archivo cerrado: el aviso sigue valido");
  EXPECT(!arq_finish_new_job(false, false), "tras M1001 (todo limpio): el aviso sigue valido");
  EXPECT(!arq_finish_new_job(true, true), "archivo abierto pero ya marcado como terminado: no es nueva");

  // ---- Velocidad real del cabezal ----
  {
    arq_speed_t v; arq_speed_reset(&v);
    arq_speed_sample(&v, 1000, 100, 100);                                   // primera muestra: solo guarda
    EXPECT(arq_speed_cur_mms(&v) == 0 && arq_speed_peak_mms(&v) == 0, "velocidad: sin segunda muestra no hay valor");
    arq_speed_sample(&v, 1100, 110, 100);                                   // 100 ms < ventana: se ignora
    EXPECT(arq_speed_peak_mms(&v) == 0, "velocidad: menos de 150 ms no se calcula");
    arq_speed_sample(&v, 1200, 120, 100);                                   // 20 mm en 200 ms = 100 mm/s
    EXPECT(arq_speed_peak_mms(&v) == 100, "velocidad: 20 mm en 200 ms = 100 mm/s (maximo)");
    EXPECT(arq_speed_cur_mms(&v) == 50, "velocidad: la actual esta suavizada (mitad en la primera ventana)");
    arq_speed_sample(&v, 1400, 140, 100);                                   // otros 100 mm/s
    EXPECT(arq_speed_cur_mms(&v) == 75, "velocidad: sigue subiendo hacia 100 (suavizado)");
    arq_speed_sample(&v, 1600, 140, 100);                                   // quieto
    EXPECT(arq_speed_cur_mms(&v) == 38 && arq_speed_peak_mms(&v) == 100, "velocidad: al parar baja y el maximo se conserva");
    for (uint32_t t = 1800; t <= 3400; t += 200) arq_speed_sample(&v, t, 140, 100);
    EXPECT(arq_speed_cur_mms(&v) == 0, "velocidad: quieto un rato = 0 mm/s");
    arq_speed_sample(&v, 3600, 5140, 100);                                // salto de 5000 mm en 200 ms = 25000 mm/s: no es velocidad
    EXPECT(arq_speed_peak_mms(&v) == 100, "velocidad: un salto de posicion (G92, home) se ignora");
    arq_speed_sample(&v, 3800, 5140 + 40, 100);                              // 40 mm en 200 ms tras el salto = 200 mm/s: ya vale
    EXPECT(arq_speed_peak_mms(&v) == 200, "velocidad: tras el salto vuelve a medir bien");
    arq_speed_sample(&v, 4000, 5180 + 0, 100 + 30);                          // 30 mm en Y en 200 ms = 150 mm/s (menor que el maximo)
    EXPECT(arq_speed_peak_mms(&v) == 200, "velocidad: el maximo no baja");
    arq_speed_reset(&v);
    EXPECT(arq_speed_cur_mms(&v) == 0 && arq_speed_peak_mms(&v) == 0 && !v.have, "velocidad: reset deja todo a cero");
    arq_speed_t w; arq_speed_reset(&w);
    arq_speed_sample(&w, 0xFFFFFF00u, 0, 0); arq_speed_sample(&w, 0x00000100u, 51.2f, 0);   // 512 ms a traves del desbordamiento de millis
    EXPECT(arq_speed_peak_mms(&w) == 100, "velocidad: millis() desbordado (wrap) sigue midiendo bien");
    arq_speed_t n; arq_speed_reset(&n); arq_speed_sample(&n, 0, 0, 0); arq_speed_sample(&n, 500, NAN, 0);
    EXPECT(arq_speed_peak_mms(&n) == 0, "velocidad: NaN no se mide");
  }

  printf("Pruebas de logica: %d de %d correctas%s\n", total - fails, total, fails ? "  <-- HAY FALLOS" : "");
  return fails ? 1 : 0;
}
