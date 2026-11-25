/**
 * ARQUIS - firmware unico XQ1 / Aquila D1: asistente del primer arranque y Ajustes > Maquina. Ver arquis_machine_ui.h.
 */
#include "arquis_machine_ui.h"

#if ARQ_HAS_MACHINE

#include "arquis_params.h"
#include "arquis_params_view.h"
#include "arquis_widgets.h"
#include "draw_ui.h"

#include "../../../MarlinCore.h"
#include "../../../module/motion.h"
#include "../../../module/planner.h"
#include "../../../module/temperature.h"
#include "../../../module/settings.h"
#include "../../../module/endstops.h"
#include "../../../gcode/queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern lv_group_t *g;

// ---- Estado ------------------------------------------------------------------------------------------------------
enum { M_MENU, M_WIZ };
enum { PG_MENU, PG_MODEL, PG_DIMS, PG_ES, PG_DIRS, PG_HOME, PG_OFS, PG_MECH, PG_REVIEW, PG_NUM };
enum { T_NONE, T_MOVE, T_HOME };
#define MECH_SUBS   6
#define REVIEW_SUBS (2 + (ARQ_MP_COUNT - ARQ_MP_STEPS_X + 3) / 4)
#define MENU_SUBS   4

static lv_obj_t    *scr, *chip;
static uint8_t      mode, page, sub, menu_sub, ret_page, ret_sub;
static bool         wiz_cancelable, rebuild, keep_saved, marlin_ok, heated_e;
static uint8_t      pick, detected, rec_prof;
static arq_mc_cfg_t work, base;
static uint8_t      es_stage[3];             // 0 nada, 1 abierto, 2 activado, 3 comprobado (abierto -> activado -> abierto)
static millis_t     es_open_ms[3], es_hit_ms[3];
static uint8_t      dir_ok, home_ok;         // comprobado en esta sesion (bits X, Y, Z, E)
static uint8_t      test_kind, test_axis;
static millis_t     test_t0, live_ms;
static uint8_t      edit_p;
static char         edit_buf[16], title_txt[40], chip_txt[96], dlg_txt[256], lead_txt[128], rname[4][28], notice_txt[160];

enum {
  ID_BACK = 1, ID_NEXT, ID_KEEP, ID_OFS,
  ID_EDIT = 100,                             // + parametro
  ID_PICK = 150,                             // + modelo
  ID_ESINV = 170, ID_DIRINV = 180, ID_DIRTEST = 190, ID_HOME = 200,   // + eje
  ID_M_MODEL = 300, ID_M_DIMS, ID_M_ES, ID_M_DIRS, ID_M_HOME, ID_M_STEPS, ID_M_FEED, ID_M_CUR, ID_M_IS,
  ID_M_RMECH, ID_M_RALL, ID_M_WIZ, ID_M_OTHER, ID_M_SAVE,
  DLG_INFO = 500, DLG_MOVE_GO, DLG_ANS_YES, DLG_ANS_NO, DLG_HOME_GO, DLG_HOME_YES, DLG_HOME_NO, DLG_HEAT_GO,
  DLG_SAVE_GO, DLG_DONE, DLG_EXIT_SAVE, DLG_EXIT_DISCARD, DLG_MODEL_GO, DLG_RMECH_GO, DLG_RALL_GO, DLG_ESKIP_GO,
  DLG_NOTICE, DLG_CANCEL, DLG_WIZ_QUIT, DLG_MSAVE_GO, DLG_OTHER_SAVE, DLG_OTHER_DISCARD
};

static const char AX[] = "XYZE";

static void ui_cb(lv_obj_t *obj, lv_event_t event);
static void nk_cb(lv_obj_t *obj, lv_event_t event);

static void add_to_group() {
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable && scr) arq_group_add_clickables(g, scr);
  #endif
}
static void remove_group() {
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) lv_group_remove_all_objs(g);
  #endif
}

// Numero con los decimales del parametro, sin ceros sobrantes (newlib-nano no formatea float con printf)
static void fmtv(char *b, size_t n, float v, uint8_t dec) {
  long m = 1; for (uint8_t i = 0; i < dec && i < 3; ++i) m *= 10;
  long x = lroundf(v * m); const bool neg = x < 0; if (neg) x = -x;
  char f[5] = { 0 };                                       // parte decimal sin ceros a la derecha
  for (int8_t i = (int8_t)(dec < 3 ? dec : 3) - 1; i >= 0; --i) { f[i] = (char)('0' + x % 10); x /= 10; }
  for (int8_t i = 3; i >= 0 && (f[i] == '0' || !f[i]); --i) f[i] = 0;
  if (!dec) x = lroundf(neg ? -v : v);
  snprintf(b, n, f[0] ? "%s%ld.%s" : "%s%ld", neg ? "-" : "", x, f);
}
static void fmtp(char *b, size_t n, uint8_t p, float v) { arq_mc_range_t r; arq_mc_range(ARQ_MC_XQ1, p, 1, &r); fmtv(b, n, v, r.dec); }

static bool es_verified(uint8_t a)  { return es_stage[a] == 3 || (work.flags & ARQ_MCF_ES_OK); }
static bool all_es()                { return es_verified(0) && es_verified(1) && es_verified(2); }
static bool dir_verified(uint8_t a) { return (dir_ok & (1 << a)) || (a < 3 ? (work.flags & ARQ_MCF_DIR_OK) : (work.flags & ARQ_MCF_E_OK)); }
static bool all_dir()               { return dir_verified(0) && dir_verified(1) && dir_verified(2); }

static void info(const char *title, const char *msg) { arq_dialog_open(title, msg, "Aceptar", nullptr, false, ui_cb, DLG_INFO, 0); }
static void ask(const char *title, const char *msg, const char *ok, uint16_t ok_id, bool danger = false, const char *no = "Cancelar", uint16_t no_id = DLG_CANCEL) {
  arq_dialog_open(title, msg, ok, no, danger, ui_cb, ok_id, no_id);
}

// P1: los valores mecanicos guardados solo se conservan si se confirma que son del modelo elegido (arq_mc_keep_check)
static uint8_t keep_status(uint8_t p) { return arq_mc_keep_check(p, marlin_ok, rec_prof, detected); }
static const char *keep_reason(uint8_t st) {
  switch (st) {
    case ARQ_KEEP_NO_EEPROM:   return "La EEPROM de Marlin no tiene datos v\xC3\xA1lidos: se cargan los valores de f\xC3\xA1" "brica del modelo.";
    case ARQ_KEEP_OTHER_MODEL: return "Los ajustes guardados son de la otra m\xC3\xA1quina: se cargan los valores de f\xC3\xA1" "brica del modelo elegido.";
    case ARQ_KEEP_UNCONFIRMED: return "No se puede confirmar que los ajustes guardados sean de este modelo: se cargan los de f\xC3\xA1" "brica.";
    default:                   return "Elige primero tu impresora.";
  }
}

// P2: aviso antes de borrar una malla que era de otras medidas de cama
static const char MESH_WARN[] = " Se borrar\xC3\xA1 la malla 5x5 guardada: era de otras medidas de cama.";
static const char *mesh_warn(const arq_mc_cfg_t *c) { return arq_mc_mesh_will_erase(c) ? MESH_WARN : ""; }
static void ask_with_mesh(const char *title, const char *lead, const char *ok, uint16_t ok_id, const char *no, uint16_t no_id) {
  snprintf(dlg_txt, sizeof(dlg_txt), "%s%s", lead, mesh_warn(&work));
  arq_dialog_open(title, dlg_txt, ok, no, false, ui_cb, ok_id, no_id);
}

static uint8_t last_live;
static void go(uint8_t pg, uint8_t sb) { page = pg; sub = sb; rebuild = true; last_live = 0xFF; }

// ---- Filas -------------------------------------------------------------------------------------------------------
static void row(uint8_t i, const char *name, uint8_t kind, const char *v1, int16_t id1, const char *v2 = "", int16_t id2 = -1) {
  arq_pl_row_t r; memset(&r, 0, sizeof(r));
  r.name = name; r.kind = kind; r.id1 = id1; r.id2 = id2;
  strncpy(r.v1, v1 ? v1 : "", sizeof(r.v1) - 1); strncpy(r.v2, v2 ? v2 : "", sizeof(r.v2) - 1);
  arq_pl_row(scr, i, &r, ui_cb);
}
static void row_param(uint8_t i, uint8_t p) {
  char v[14]; fmtp(v, sizeof(v), p, work.v[p]);
  const arq_mc_profile_t *pr = arq_mc_profile(work.profile);
  const bool prov = pr && (pr->pending & (1UL << p)) && fabsf(work.v[p] - pr->v[p]) < 0.0005f;
  snprintf(rname[i], sizeof(rname[i]), "%s%s", arq_mc_param_name(p), prov ? " *" : "");
  row(i, rname[i], ARQ_PL_EDIT1, v, ID_EDIT + p);
}
static void row_info(uint8_t i, const char *name, const char *v) { row(i, name, ARQ_PL_EDIT1, v, -1); }

static void set_chip(const char *t, arq_tint_t tint) {
  snprintf(chip_txt, sizeof(chip_txt), "%s", t);
  if (!chip) chip = arq_chip_create(scr, ARQ_MARGIN, 292, ARQ_W - 2 * ARQ_MARGIN, chip_txt, tint);
  else { arq_card_set_dot(chip, tint); arq_card_refresh(chip); }
}

static uint8_t step_of(uint8_t pg) {
  switch (pg) { case PG_MODEL: return 1; case PG_DIMS: return 2; case PG_ES: return 3; case PG_DIRS: return 4;
                case PG_HOME: case PG_OFS: return 5; case PG_MECH: return 6; case PG_REVIEW: return 7; default: return 0; }
}

static void title(const char *t) {
  if (mode == M_WIZ && step_of(page)) snprintf(title_txt, sizeof(title_txt), "%u/8  %s", step_of(page), t);
  else snprintf(title_txt, sizeof(title_txt), "%s", t);
}

// ---- Paginas -----------------------------------------------------------------------------------------------------
static const char *yesno(bool b) { return b ? "S\xC3\xAD" : "No"; }
static const char *es_txt(uint8_t a) { return (work.es_inv >> a) & 1 ? "Invertida" : "Normal"; }
static const char *dir_txt(uint8_t a) { return (work.dir_inv >> a) & 1 ? "Invertido" : "Normal"; }

static bool es_hit_raw(uint8_t a) {                 // misma regla que M119: READ(pin) != logica (la de la copia de trabajo)
  bool pin = false;
  switch (a) {
    case 0: pin = READ(X_MIN_PIN); break;
    case 1: pin = READ(Y_MIN_PIN); break;
    default: pin = READ(Z_MIN_PIN); break;
  }
  return pin != (bool)((work.es_inv >> a) & 1);
}

static void build_menu_rows() {
  const bool dirty = memcmp(&work, &base, sizeof(work)) != 0;
  switch (sub) {
    case 0:
      row(0, "Modelo", ARQ_PL_EDIT1, arq_mc_name(), ID_M_MODEL);
      row(1, "Dimensiones y recorridos", ARQ_PL_NAV, "", ID_M_DIMS);
      row(2, "Finales de carrera", ARQ_PL_NAV, "", ID_M_ES);
      row(3, "Direcciones de motores", ARQ_PL_NAV, "", ID_M_DIRS);
      break;
    case 1:
      row(0, "Homing y offsets", ARQ_PL_NAV, "", ID_M_HOME);
      row(1, "Pasos por mil\xC3\xADmetro", ARQ_PL_NAV, "", ID_M_STEPS);
      row(2, "Velocidades y aceleraciones", ARQ_PL_NAV, "", ID_M_FEED);
      row(3, "Corrientes de motores", ARQ_PL_NAV, "", ID_M_CUR);
      break;
    case 2:
      row(0, "Input Shaping", ARQ_PL_NAV, "", ID_M_IS);
      row(1, "Restaurar mec\xC3\xA1nica de f\xC3\xA1" "brica", ARQ_PL_NAV, "", ID_M_RMECH);
      row(2, "Restaurar todo de f\xC3\xA1" "brica", ARQ_PL_NAV, "", ID_M_RALL);
      row(3, "Repetir el asistente", ARQ_PL_NAV, "", ID_M_WIZ);
      break;
    default:
      row(0, "Otros par\xC3\xA1metros (MKS)", ARQ_PL_NAV, "", ID_M_OTHER);
      if (dirty) row(1, "Guardar cambios", ARQ_PL_NAV, "", ID_M_SAVE);
      break;
  }
  const uint8_t r = arq_mc_block_reason();
  if (dirty) set_chip("Cambios sin guardar (p\xC3\xA1gina 4: Guardar).", ARQ_TINT_WARN);
  else if (r) set_chip(arq_mc_block_text(r), ARQ_TINT_WARN);
  else set_chip("Perfil comprobado. No se edita imprimiendo.", ARQ_TINT_OK);
}

static void build_model() {
  for (uint8_t i = 1; i <= ARQ_MC_NPROF; ++i) {
    const arq_mc_profile_t *p = arq_mc_profile(i);
    snprintf(rname[i - 1], sizeof(rname[0]), "%s  %s", p->name, p->size_txt);
    row(i - 1, rname[i - 1], ARQ_PL_EDIT1, pick == i ? "Elegida" : "Elegir", ID_PICK + i);
  }
  uint8_t n = ARQ_MC_NPROF;
  const uint8_t st = keep_status(pick);
  if (st != ARQ_KEEP_OK) keep_saved = false;                // nunca se conserva lo que no esta confirmado
  if (marlin_ok) row(n++, "Ajustes mec\xC3\xA1nicos", ARQ_PL_EDIT1, keep_saved ? "Conservar" : "De f\xC3\xA1" "brica", ID_KEEP);
  if (detected) row_info(n++, "Detectada en la EEPROM", arq_mc_profile(detected)->name);
  if (!pick) set_chip("Elige tu impresora.", ARQ_TINT_AUTO);
  else if (st == ARQ_KEEP_OTHER_MODEL) set_chip("Ajustes guardados de otra m\xC3\xA1quina: de f\xC3\xA1" "brica.", ARQ_TINT_WARN);
  else if (st == ARQ_KEEP_UNCONFIRMED) set_chip("Ajustes sin confirmar para este modelo: de f\xC3\xA1" "brica.", ARQ_TINT_WARN);
  else if (st == ARQ_KEEP_NO_EEPROM) set_chip("EEPROM sin datos v\xC3\xA1lidos: de f\xC3\xA1" "brica.", ARQ_TINT_AUTO);
  else set_chip(keep_saved ? "Se conservan los ajustes guardados de este modelo."
                           : "Mec\xC3\xA1nica de f\xC3\xA1" "brica; se conservan los offsets M206.", ARQ_TINT_AUTO);
}

static void build_dims() {
  row_param(0, ARQ_MP_BED_X); row_param(1, ARQ_MP_BED_Y); row_param(2, ARQ_MP_Z_MAX);
  static char v[14];
  snprintf(v, sizeof(v), "%dx%d", (int)(work.v[ARQ_MP_BED_X] - 2 * PROBING_MARGIN), (int)(work.v[ARQ_MP_BED_Y] - 2 * PROBING_MARGIN));
  row_info(3, "\xC3\x81rea de la malla 5x5", v);
  set_chip("Medidas en mm. M\xC3\xA1ximo: el de f\xC3\xA1" "brica + 10 mm.", ARQ_TINT_AUTO);
}

static void es_rows() {
  for (uint8_t a = 0; a < 3; ++a) {
    snprintf(rname[a], sizeof(rname[a]), "Final %c%s", AX[a], es_verified(a) ? "  (comprobado)" : "");
    row(a, rname[a], ARQ_PL_EDIT2, es_txt(a), ID_ESINV + a, es_hit_raw(a) ? "Activado" : "Abierto", -1);
  }
  char v[14]; snprintf(v, sizeof(v), "%u de 3", (unsigned)(es_verified(0) + es_verified(1) + es_verified(2)));
  row_info(3, "Comprobados", v);
}
static void build_es() {
  es_rows();
  set_chip("Aleja los ejes; pulsa y suelta cada final.", ARQ_TINT_AUTO);
}

static void dirs_rows() {
  for (uint8_t a = 0; a < 4; ++a) {
    const char *t = dir_verified(a) ? "Correcto" : "Probar";
    if (a == 3 && !dir_verified(a) && thermalManager.wholeDegHotend(0) < EXTRUDE_MINTEMP + 5) t = "Calentar";
    snprintf(rname[a], sizeof(rname[a]), "Motor %c%s", AX[a], a == 3 ? " (opcional)" : "");
    row(a, rname[a], ARQ_PL_EDIT2, dir_txt(a), ID_DIRINV + a, t, ID_DIRTEST + a);
  }
}
static void dirs_chip() {
  if (heated_e) {
    char t[64]; snprintf(t, sizeof(t), "Hotend %d / %d \xC2\xB0" "C (con filamento).", thermalManager.wholeDegHotend(0), thermalManager.degTargetHotend(0));
    set_chip(t, ARQ_TINT_WARN);
  }
  else set_chip("Probar: 10 mm (Z 5 mm) lento, lejos del final.", ARQ_TINT_AUTO);
}
static void build_dirs() { dirs_rows(); dirs_chip(); }

static void build_home() {
  for (uint8_t a = 0; a < 3; ++a) {
    snprintf(rname[a], sizeof(rname[a]), "Home %c (hacia el m\xC3\xADnimo)", AX[a]);
    row(a, rname[a], ARQ_PL_EDIT1, (home_ok >> a) & 1 ? "Correcto" : "Probar", ID_HOME + a);
  }
  row(3, "Offsets de origen (M206)", ARQ_PL_NAV, "", ID_OFS);
  set_chip(mode == M_WIZ ? "Prueba el home de X, Y y Z (en ese orden)." : "Home opcional, para comprobar.", ARQ_TINT_AUTO);
}

static void build_ofs() {
  row_param(0, ARQ_MP_OFS_X); row_param(1, ARQ_MP_OFS_Y); row_param(2, ARQ_MP_OFS_Z);
  set_chip("Ajuste fino en vivo: Babystep (no se guarda).", ARQ_TINT_AUTO);
}

static const char * const mech_titles[MECH_SUBS] = {
  "Pasos por mm", "Velocidad m\xC3\xA1xima (mm/s)", "Aceleraci\xC3\xB3n m\xC3\xA1xima (mm/s\xC2\xB2)", "Aceleraciones (mm/s\xC2\xB2)",
  "Corrientes (mA)", "Input Shaping (Hz)"
};
static void build_mech() {
  static const uint8_t first[MECH_SUBS] = { ARQ_MP_STEPS_X, ARQ_MP_FEED_X, ARQ_MP_ACC_X, ARQ_MP_ACC_PRINT, ARQ_MP_CUR_X, ARQ_MP_SHP_FX };
  const uint8_t n = sub == 5 ? 2 : 4;
  for (uint8_t i = 0; i < n; ++i) row_param(i, first[sub] + i);
  const arq_mc_profile_t *pr = arq_mc_profile(work.profile);
  if (pr && pr->pending && (sub == 1 || sub == 2)) set_chip("* Z provisional: pendiente de validar.", ARQ_TINT_WARN);
  else if (sub == 5) set_chip("0 = desactivado. M\xC3\xADnimo: el del b\xC3\xBA" "fer de Input Shaping.", ARQ_TINT_AUTO);
  else set_chip("Pulsa un valor para cambiarlo.", ARQ_TINT_AUTO);
}

static void build_review() {                               // 2 paginas de resumen + TODOS los parametros (4 por pagina)
  static char v[3][14];
  const arq_mc_cfg_t &w = work;
  if (sub == 0) {
    snprintf(v[0], 14, "%dx%d", (int)w.v[ARQ_MP_BED_X], (int)w.v[ARQ_MP_BED_Y]);
    snprintf(v[1], 14, "%d", (int)w.v[ARQ_MP_Z_MAX]);
    snprintf(v[2], 14, "%c %c %c", w.es_inv & 1 ? 'I' : 'N', w.es_inv & 2 ? 'I' : 'N', w.es_inv & 4 ? 'I' : 'N');
    row_info(0, "Modelo", arq_mc_profile(w.profile) ? arq_mc_profile(w.profile)->name : "-");
    row_info(1, "Ancho x fondo (mm)", v[0]);
    row_info(2, "Altura Z (mm)", v[1]);
    row_info(3, "Finales X Y Z (N / Inv.)", v[2]);
  }
  else if (sub == 1) {
    snprintf(v[0], 14, "%c %c %c %c", w.dir_inv & 1 ? 'I' : 'N', w.dir_inv & 2 ? 'I' : 'N', w.dir_inv & 4 ? 'I' : 'N', w.dir_inv & 8 ? 'I' : 'N');
    row_info(0, "Motores X Y Z E (N / Inv.)", v[0]);
    row_info(1, "Finales comprobados", yesno(all_es()));
    row_info(2, "Motores comprobados", all_dir() ? (dir_verified(3) ? "X Y Z y E" : "X Y Z") : "No");
    row_info(3, "Home probado", mode == M_WIZ ? yesno(home_ok == 7) : "-");
  }
  else {
    const uint8_t p0 = ARQ_MP_STEPS_X + (sub - 2) * 4;
    for (uint8_t i = 0; i < 4 && p0 + i < ARQ_MP_COUNT; ++i) {
      char b[14]; fmtp(b, sizeof(b), p0 + i, w.v[p0 + i]);   // arq_pl_row copia el valor
      row(i, arq_mc_param_name(p0 + i), ARQ_PL_EDIT1, b, -1);
    }
  }
  set_chip(mode == M_WIZ ? "Revisa los valores; en la \xC3\xBAltima p\xC3\xA1gina, Guardar." : "Resumen.", ARQ_TINT_AUTO);
}

static void build_numpad() {
  snprintf(title_txt, sizeof(title_txt), "%s (%s)", arq_mc_param_name(edit_p), arq_mc_param_unit(edit_p));
  arq_numpad_build(scr, nk_cb, title_txt);
  arq_numpad_set(edit_buf, arq_numpad_value_ok(edit_buf));
}

static void build() {
  if (!scr) return;
  rebuild = false;
  remove_group();
  arq_pl_forget(); arq_numpad_forget();
  lv_obj_clean(scr);
  chip = nullptr;
  if (page == PG_NUM) { build_numpad(); add_to_group(); return; }
  switch (page) {
    case PG_MENU:   title("M\xC3\xA1quina"); break;
    case PG_MODEL:  title("Modelo de impresora"); break;
    case PG_DIMS:   title("Dimensiones"); break;
    case PG_ES:     title("Finales de carrera"); break;
    case PG_DIRS:   title("Direcci\xC3\xB3n de motores"); break;
    case PG_HOME:   title("Homing"); break;
    case PG_OFS:    title("Offsets de origen"); break;
    case PG_MECH:   title(mech_titles[sub]); break;
    default:        title("Revisi\xC3\xB3n"); break;
  }
  arq_pl_begin(scr, title_txt);
  if (!(mode == M_WIZ && page == PG_MODEL && !wiz_cancelable)) arq_pl_back(scr, ui_cb, ID_BACK);
  if (mode == M_WIZ && page != PG_OFS) arq_pl_page(scr, (page == PG_REVIEW && sub == REVIEW_SUBS - 1) ? "Guardar" : "Siguiente", ui_cb, ID_NEXT);
  else if (mode == M_MENU && (page == PG_MENU || page == PG_MECH)) arq_pl_page(scr, "M\xC3\xA1s", ui_cb, ID_NEXT);
  switch (page) {
    case PG_MENU:   build_menu_rows(); break;
    case PG_MODEL:  build_model(); break;
    case PG_DIMS:   build_dims(); break;
    case PG_ES:     build_es(); break;
    case PG_DIRS:   build_dirs(); break;
    case PG_HOME:   build_home(); break;
    case PG_OFS:    build_ofs(); break;
    case PG_MECH:   build_mech(); break;
    default:        build_review(); break;
  }
  add_to_group();
}

// ---- Entrada y salida --------------------------------------------------------------------------------------------
static void reset_session() {
  memset(es_stage, 0, sizeof(es_stage)); memset(es_open_ms, 0, sizeof(es_open_ms)); memset(es_hit_ms, 0, sizeof(es_hit_ms));
  dir_ok = home_ok = 0; test_kind = T_NONE; last_live = 0xFF;
}

static void open_screen() {
  if (!scr) { scr = lv_screen_create(ARQ_MACHINE_UI, ""); arq_screen_apply(scr); }
  build();
}

static void heater_off() {
  if (heated_e) { thermalManager.setTargetHotend(0, 0); heated_e = false; }
}

void arq_machine_ui_clear() {
  remove_group();
  arq_dialog_close();
  arq_pl_forget(); arq_numpad_forget();
  if (scr) lv_obj_del(scr);
  scr = chip = nullptr;
  // P3: al salir de la pantalla (por cualquier camino) no queda nada de las pruebas aplicado en RAM
  arq_mc_test_mode(false); test_kind = T_NONE;
  if (!arq_mc_printing() && !queue.has_commands_queued()) arq_mc_revert_ram();
}

// Vuelta desde otra pantalla (Otros parametros MKS): se relee lo que hay ahora, porque esas pantallas cambian Marlin
void arq_machine_ui_draw() {
  if (mode == M_MENU) { arq_mc_read_saved(&base); work = base; reset_session(); page = PG_MENU; sub = menu_sub; }
  open_screen();
}

static void start_wizard(const bool cancelable, const uint8_t preselect) {
  mode = M_WIZ; wiz_cancelable = cancelable;
  marlin_ok = settings.validate();                         // la EEPROM de Marlin tiene datos validos (se pueden conservar)
  detected = marlin_ok ? arq_mc_detect(planner.settings.axis_steps_per_mm[Z_AXIS], planner.settings.axis_steps_per_mm[E_AXIS]) : 0;
  rec_prof = arq_mc_profile_id();                          // modelo con el que se guardaron los datos (0 = sin registro)
  pick = preselect ? preselect : detected;
  arq_mc_read_saved(&base);                                // lo guardado, no lo que unas pruebas hayan dejado en RAM
  keep_saved = keep_status(pick) == ARQ_KEEP_OK;
  work = base; work.flags = 0;
  reset_session();
  page = PG_MODEL; sub = 0;
}

void arq_machine_wizard_draw() {
  disp_state_stack._disp_index = 0;                        // como la pantalla de inicio: no hay a donde volver
  ZERO(disp_state_stack._disp_state);
  scr = nullptr;
  start_wizard(false, arq_mc_profile_id());
  open_screen();
}

void arq_machine_menu_draw() {
  if (arq_mc_printing()) { arq_mc_notice("Ajustes > M\xC3\xA1quina no est\xC3\xA1 disponible durante una impresi\xC3\xB3n."); lv_draw_set(); return; }
  if (arq_mc_needs_wizard()) { arq_machine_wizard_draw(); return; }
  arq_mc_revert_ram();                                     // P3: se entra siempre con lo guardado
  mode = M_MENU; page = PG_MENU; sub = menu_sub = 0;
  arq_mc_read_saved(&base); work = base;
  reset_session();
  scr = nullptr;
  open_screen();
}

static void leave_menu() { heater_off(); arq_machine_ui_clear(); lv_draw_set(); }
static void finish_wizard() { heater_off(); arq_mc_test_mode(false); arq_machine_ui_clear(); lv_draw_ready_print(); }

static void discard_and_leave() {                          // deshace las pruebas aplicadas en RAM (sin M501: no toca nada mas de Marlin)
  arq_mc_test_mode(false);
  arq_mc_revert_ram();
  leave_menu();
}

static void go_other() {                                   // P3: a Otros parametros (MKS) sin nada pendiente en RAM
  heater_off();
  arq_mc_test_mode(false);
  arq_mc_revert_ram();
  arq_machine_ui_clear();
  lv_draw_machine_para();
}

// Construye la copia de trabajo al salir del paso 1
static void model_chosen() {
  const uint8_t st = keep_status(pick);
  if (st != ARQ_KEEP_OK) keep_saved = false;
  arq_mc_choose(pick, st, keep_saved, &base, &work);       // P1: nunca mezcla la mecanica de otro modelo
  // Primer arranque (sin registro): la malla guardada es de esta maquina solo si sus datos son de este modelo
  arq_mc_mesh_ref(st == ARQ_KEEP_OK ? &arq_mc_profile(pick)->v[ARQ_MP_BED_X] : nullptr);
  reset_session();
}

// ---- Pruebas de movimiento ---------------------------------------------------------------------------------------
static bool prepare_test() {
  if (arq_mc_printing()) { info("No disponible", "Espera a que termine la impresi\xC3\xB3n o el movimiento."); return false; }
  arq_mc_apply(&work, false);                              // finales, direcciones (bit shaft) y medidas de la copia de trabajo
  arq_mc_tmc_verify(true);
  arq_mc_test_mode(true);
  const uint8_t r = arq_mc_block_reason();
  if (r) { arq_mc_test_mode(false); info("No se puede mover", arq_mc_block_text(r)); return false; }
  return true;
}

static void run_move(const uint8_t a) {
  if (!prepare_test()) return;
  switch (a) {
    case 0: queue.inject(F("G91\nG1 X10 F600\nG90")); break;
    case 1: queue.inject(F("G91\nG1 Y10 F600\nG90")); break;
    case 2: queue.inject(F("G91\nG1 Z5 F300\nG90")); break;
    default: queue.inject(F("M83\nG1 E10 F120\nM82")); break;
  }
  test_kind = T_MOVE; test_axis = a; test_t0 = millis();
}

static void run_home(const uint8_t a) {
  if (!prepare_test()) return;
  set_axis_unhomed((AxisEnum)a);
  switch (a) {
    case 0: queue.inject(F("G28 X")); break;
    case 1: queue.inject(F("G28 Y")); break;
    default: queue.inject(F("G28 Z")); break;
  }
  test_kind = T_HOME; test_axis = a; test_t0 = millis();
}

static void test_finished() {
  arq_mc_test_mode(false);
  const uint8_t a = test_axis, kind = test_kind;
  test_kind = T_NONE;
  if (kind == T_MOVE) {
    if (a == 3) snprintf(dlg_txt, sizeof(dlg_txt), "\xC2\xBF" "El filamento entr\xC3\xB3 hacia la boquilla?");
    else snprintf(dlg_txt, sizeof(dlg_txt), "\xC2\xBF" "El eje %c se alej\xC3\xB3 de su final de carrera?", AX[a]);
    arq_dialog_open("Comprobar direcci\xC3\xB3n", dlg_txt, "S\xC3\xAD", "No", false, ui_cb, DLG_ANS_YES, DLG_ANS_NO);
  }
  else {
    snprintf(dlg_txt, sizeof(dlg_txt), "\xC2\xBF" "El eje %c fue a su final de carrera y se detuvo all\xC3\xAD?", AX[a]);
    ask("Comprobar home", dlg_txt, "S\xC3\xAD", DLG_HOME_YES, false, "No", DLG_HOME_NO);
  }
}

// ---- Guardar -----------------------------------------------------------------------------------------------------
static void set_tramming() {                               // posiciones de los 5 tornillos (los mismos margenes que draw_ui.cpp)
  const float bx = work.v[ARQ_MP_BED_X], by = work.v[ARQ_MP_BED_Y];
  gCfgItems.trammingPos[0].x = X_MIN_POS + 30; gCfgItems.trammingPos[0].y = Y_MIN_POS + 30;
  gCfgItems.trammingPos[1].x = bx - 30;        gCfgItems.trammingPos[1].y = Y_MIN_POS + 30;
  gCfgItems.trammingPos[2].x = bx - 30;        gCfgItems.trammingPos[2].y = by - 30;
  gCfgItems.trammingPos[3].x = X_MIN_POS + 30; gCfgItems.trammingPos[3].y = by - 30;
  gCfgItems.trammingPos[4].x = bx / 2;         gCfgItems.trammingPos[4].y = by / 2;
  update_spi_flash();
}

static bool do_save(const bool quiet = false) {
  if (mode == M_WIZ) work.flags = ARQ_MCF_DONE | ARQ_MCF_ES_OK | ARQ_MCF_DIR_OK | (dir_verified(3) ? ARQ_MCF_E_OK : 0);
  else {
    uint8_t f = ARQ_MCF_DONE;
    if (all_es()) f |= ARQ_MCF_ES_OK;
    if (all_dir()) f |= ARQ_MCF_DIR_OK;
    if (dir_verified(3)) f |= ARQ_MCF_E_OK;
    work.flags = f;
  }
  const bool bed_changed = arq_mc_mech_differs(&work, &base, ARQ_MP_BED_X) || arq_mc_mech_differs(&work, &base, ARQ_MP_BED_Y);
  arq_mc_test_mode(false);
  int bad = -1;
  const uint8_t r = arq_mc_commit(&work, &bad);
  if (r == ARQ_MCS_OK) {
    if (mode == M_WIZ || bed_changed) set_tramming();
    base = work;
    if (mode == M_WIZ)
      arq_dialog_open("Configuraci\xC3\xB3n guardada", "Se guard\xC3\xB3 en la EEPROM. Antes de imprimir se har\xC3\xA1 home (G28).", "Aceptar", nullptr, false, ui_cb, DLG_DONE, 0);
    else { if (!quiet) info("Guardado", "Cambios guardados en la EEPROM."); rebuild = true; }
    return true;
  }
  if (r == ARQ_MCS_E_INVALID && bad >= 0 && bad < ARQ_MP_COUNT) {
    arq_mc_range_t rg; arq_mc_range(work.profile, bad, arq_mc_shaping_min(), &rg);
    char lo[12], hi[12]; fmtv(lo, sizeof lo, rg.lo, rg.dec); fmtv(hi, sizeof hi, rg.hi, rg.dec);
    snprintf(dlg_txt, sizeof(dlg_txt), "%s fuera de rango (%s a %s %s). No se guard\xC3\xB3 nada.", arq_mc_param_name(bad), lo, hi, arq_mc_param_unit(bad));
  }
  else snprintf(dlg_txt, sizeof(dlg_txt), "%s",
    r == ARQ_MCS_E_PRINTING ? "Hay una impresi\xC3\xB3n o un movimiento en curso." :
    r == ARQ_MCS_E_MARLIN   ? "Error al guardar la EEPROM de Marlin (M500). El registro ARQUIS no se cambi\xC3\xB3." :
    r == ARQ_MCS_E_RECORD   ? "Error al escribir el registro ARQUIS. Repite Guardar." : "Configuraci\xC3\xB3n no v\xC3\xA1lida.");
  info("No se guard\xC3\xB3", dlg_txt);
  return false;
}

// ---- Navegacion --------------------------------------------------------------------------------------------------
static void wiz_next() {
  switch (page) {
    case PG_MODEL:
      if (!arq_mc_profile(pick)) { info("Elige un modelo", "Pulsa Elegir en tu impresora."); return; }
      model_chosen(); go(PG_DIMS, 0); return;
    case PG_DIMS: go(PG_ES, 0); return;
    case PG_ES:
      if (!all_es()) { info("Faltan finales", "Pulsa y suelta los finales X, Y y Z hasta que los tres est\xC3\xA9n comprobados."); return; }
      go(PG_DIRS, 0); return;
    case PG_DIRS:
      if (!all_dir()) { info("Faltan motores", "Prueba los motores X, Y y Z y confirma que se alejan de su final."); return; }
      if (!dir_verified(3)) { ask("Extrusor sin probar", "No probaste el extrusor. Se usar\xC3\xA1 el sentido del modelo. \xC2\xBF" "Continuar?", "Continuar", DLG_ESKIP_GO, false, "Volver", DLG_CANCEL); return; }
      heater_off(); go(PG_HOME, 0); return;
    case PG_HOME:
      if (home_ok != 7) { info("Falta el home", "Prueba el home de X, Y y Z antes de seguir."); return; }
      go(PG_MECH, 0); return;
    case PG_MECH: if (sub + 1 < MECH_SUBS) go(PG_MECH, sub + 1); else go(PG_REVIEW, 0); return;
    case PG_REVIEW:
      if (sub + 1 < REVIEW_SUBS) { go(PG_REVIEW, sub + 1); return; }
      snprintf(lead_txt, sizeof(lead_txt), "Se guardar\xC3\xA1 la configuraci\xC3\xB3n de %s en la EEPROM.", arq_mc_profile(work.profile)->name);
      ask_with_mesh("Guardar", lead_txt, "Guardar", DLG_SAVE_GO, "Volver", DLG_CANCEL);
      return;
    default: return;
  }
}

static void wiz_back() {
  switch (page) {
    case PG_MODEL:
      if (wiz_cancelable) ask("Salir del asistente", "Se descartan los cambios y las pruebas.", "Salir", DLG_WIZ_QUIT, false, "Seguir", DLG_CANCEL);
      return;
    case PG_DIMS:   go(PG_MODEL, 0); return;
    case PG_ES:     go(PG_DIMS, 0); return;
    case PG_DIRS:   go(PG_ES, 0); return;
    case PG_HOME:   heater_off(); go(PG_DIRS, 0); return;
    case PG_OFS:    go(PG_HOME, 0); return;
    case PG_MECH:   if (sub) go(PG_MECH, sub - 1); else go(PG_HOME, 0); return;
    case PG_REVIEW: if (sub) go(PG_REVIEW, sub - 1); else go(PG_MECH, MECH_SUBS - 1); return;
    default: return;
  }
}

static void menu_back() {
  if (page == PG_OFS) { go(PG_HOME, 0); return; }
  if (page != PG_MENU) { heater_off(); go(PG_MENU, menu_sub); return; }
  if (memcmp(&work, &base, sizeof(work)))
    ask_with_mesh("Cambios sin guardar", "\xC2\xBF" "Guardar los cambios en la EEPROM?", "Guardar", DLG_EXIT_SAVE, "Descartar", DLG_EXIT_DISCARD);
  else if (arq_mc_flags() != base.flags || dir_ok || home_ok || es_stage[0] || es_stage[1] || es_stage[2]) discard_and_leave();
  else leave_menu();
}

static void open_edit(const uint8_t p) {
  edit_p = p; fmtp(edit_buf, sizeof(edit_buf), p, work.v[p]);
  ret_page = page; ret_sub = sub;
  go(PG_NUM, 0);
}

// Restaurar mecanica: todo de fabrica del modelo activo, con los offsets M206 del usuario (mismo modelo, confirmado)
static void rmech_cfg(arq_mc_cfg_t *now, arq_mc_cfg_t *c) {
  arq_mc_read_saved(now);
  arq_mc_choose(arq_mc_profile_id(), ARQ_KEEP_OK, false, now, c);
  c->flags = arq_mc_flags();
  if (c->es_inv != now->es_inv) c->flags &= ~ARQ_MCF_ES_OK;
  if (c->dir_inv != now->dir_inv) c->flags &= ~(ARQ_MCF_DIR_OK | ARQ_MCF_E_OK);
}

// ---- Eventos -----------------------------------------------------------------------------------------------------
static void on_dialog(const int id) {
  arq_dialog_close();
  switch (id) {
    case DLG_MOVE_GO: run_move(test_axis); break;
    case DLG_HOME_GO: run_home(test_axis); break;
    case DLG_HEAT_GO: thermalManager.setTargetHotend(200, 0); heated_e = true; rebuild = true; break;
    case DLG_ANS_YES: dir_ok |= 1 << test_axis; if (test_axis == 3) heater_off(); rebuild = true; break;
    case DLG_ANS_NO:
      work.dir_inv ^= 1 << test_axis; dir_ok &= ~(1 << test_axis);
      work.flags &= ~(test_axis < 3 ? ARQ_MCF_DIR_OK : ARQ_MCF_E_OK);
      snprintf(dlg_txt, sizeof(dlg_txt), "Se invirti\xC3\xB3 el sentido del motor %c. Vuelve a probarlo.", AX[test_axis]);
      info("Sentido invertido", dlg_txt); rebuild = true; break;
    case DLG_HOME_YES: home_ok |= 1 << test_axis; rebuild = true; break;
    case DLG_HOME_NO:  info("Home incorrecto", "Revisa la l\xC3\xB3gica del final y la direcci\xC3\xB3n del motor de ese eje y repite la prueba."); break;
    case DLG_SAVE_GO:  do_save(); break;
    case DLG_DONE:     finish_wizard(); break;
    case DLG_EXIT_SAVE: if (do_save(true)) leave_menu(); break;
    case DLG_MSAVE_GO:  do_save(); break;
    case DLG_OTHER_SAVE: if (do_save(true)) go_other(); break;
    case DLG_OTHER_DISCARD: work = base; go_other(); break;
    case DLG_EXIT_DISCARD: discard_and_leave(); break;
    case DLG_MODEL_GO: start_wizard(true, 0); go(PG_MODEL, 0); break;
    case DLG_RMECH_GO: {
      arq_mc_cfg_t c, now; rmech_cfg(&now, &c);
      const bool erase = arq_mc_mesh_will_erase(&c);
      int bad; const uint8_t r = arq_mc_commit(&c, &bad);
      arq_mc_read_saved(&base); work = base; reset_session();
      info(r == ARQ_MCS_OK ? "Restaurado" : "No se restaur\xC3\xB3",
           r != ARQ_MCS_OK ? "No se pudo guardar la EEPROM." :
           erase ? "Valores mec\xC3\xA1nicos de f\xC3\xA1" "brica guardados. Se conservan PID y offsets M206; la malla se borr\xC3\xB3 (otras medidas)."
                 : "Valores mec\xC3\xA1nicos de f\xC3\xA1" "brica guardados. Se conservan PID, malla y offsets M206.");
      rebuild = true;
    } break;
    case DLG_RALL_GO: {
      const uint8_t r = arq_mc_restore_all();
      arq_mc_read_saved(&base); work = base; reset_session();
      info(r == ARQ_MCS_OK ? "Restaurado" : "No se restaur\xC3\xB3", r == ARQ_MCS_OK ? "Todos los ajustes vuelven a los de f\xC3\xA1" "brica del modelo." : "No se pudo guardar la EEPROM.");
      rebuild = true;
    } break;
    case DLG_ESKIP_GO: heater_off(); go(PG_HOME, 0); break;
    case DLG_WIZ_QUIT: arq_mc_test_mode(false); arq_mc_revert_ram(); heater_off(); mode = M_MENU; arq_mc_read_saved(&base); work = base; reset_session(); go(PG_MENU, 0); break;
    default: break;                                          // DLG_INFO, DLG_CANCEL, DLG_NOTICE
  }
}

static void on_row(const int id) {
  if (id >= ID_EDIT && id < ID_EDIT + ARQ_MP_COUNT) { open_edit(id - ID_EDIT); return; }
  if (id > ID_PICK && id <= ID_PICK + ARQ_MC_NPROF) {
    pick = id - ID_PICK;
    keep_saved = keep_status(pick) == ARQ_KEEP_OK;
    rebuild = true; return;
  }
  if (id >= ID_ESINV && id < ID_ESINV + 3) {
    const uint8_t a = id - ID_ESINV;
    work.es_inv ^= 1 << a; es_stage[a] = 0; es_open_ms[a] = 0; work.flags &= ~ARQ_MCF_ES_OK; home_ok = 0;
    rebuild = true; return;
  }
  if (id >= ID_DIRINV && id < ID_DIRINV + 4) {
    const uint8_t a = id - ID_DIRINV;
    work.dir_inv ^= 1 << a; dir_ok &= ~(1 << a); work.flags &= ~(a < 3 ? ARQ_MCF_DIR_OK : ARQ_MCF_E_OK); home_ok = 0;
    rebuild = true; return;
  }
  if (id >= ID_DIRTEST && id < ID_DIRTEST + 4) {
    test_axis = id - ID_DIRTEST;
    if (test_axis == 3) {
      if (thermalManager.wholeDegHotend(0) < EXTRUDE_MINTEMP + 5) {
        ask("Calentar el hotend", "Para probar el extrusor se calienta a 200 \xC2\xB0" "C. Carga filamento antes.", "Calentar", DLG_HEAT_GO);
        return;
      }
      ask("Probar el extrusor", "Se extruir\xC3\xA1n 10 mm despacio.", "Extruir", DLG_MOVE_GO);
      return;
    }
    snprintf(dlg_txt, sizeof(dlg_txt), "Lleva el eje %c a la mitad de su recorrido con la mano. Se mover\xC3\xA1 %s despacio.", AX[test_axis], test_axis == 2 ? "5 mm" : "10 mm");
    ask("Probar el motor", dlg_txt, "Mover", DLG_MOVE_GO);
    return;
  }
  if (id >= ID_HOME && id < ID_HOME + 3) {
    test_axis = id - ID_HOME;
    if (!es_verified(test_axis) || !dir_verified(test_axis)) { info("Antes, comprueba", "El final de carrera y la direcci\xC3\xB3n de ese eje (pasos 3 y 4)."); return; }
    snprintf(dlg_txt, sizeof(dlg_txt), "El eje %c ir\xC3\xA1 a su final de carrera. Si no se detiene al tocarlo, apaga la impresora.", AX[test_axis]);
    ask("Probar el home", dlg_txt, "Home", DLG_HOME_GO);
    return;
  }
  switch (id) {
    case ID_KEEP: {
      const uint8_t st = keep_status(pick);
      if (st != ARQ_KEEP_OK) { keep_saved = false; info("No se puede conservar", keep_reason(st)); }
      else keep_saved = !keep_saved;
      rebuild = true;
    } break;
    case ID_OFS:    go(PG_OFS, 0); break;
    case ID_M_MODEL: ask("Cambiar de modelo", "Si eliges otro modelo se cargan sus valores mec\xC3\xA1nicos de f\xC3\xA1" "brica (no se mezclan con los actuales). Habr\xC3\xA1 que repetir las pruebas.", "Cambiar", DLG_MODEL_GO); break;
    case ID_M_DIMS:  menu_sub = sub; go(PG_DIMS, 0); break;
    case ID_M_ES:    menu_sub = sub; go(PG_ES, 0); break;
    case ID_M_DIRS:  menu_sub = sub; go(PG_DIRS, 0); break;
    case ID_M_HOME:  menu_sub = sub; go(PG_HOME, 0); break;
    case ID_M_STEPS: menu_sub = sub; go(PG_MECH, 0); break;
    case ID_M_FEED:  menu_sub = sub; go(PG_MECH, 1); break;
    case ID_M_CUR:   menu_sub = sub; go(PG_MECH, 4); break;
    case ID_M_IS:    menu_sub = sub; go(PG_MECH, 5); break;
    case ID_M_RMECH: {
      arq_mc_cfg_t c, now; rmech_cfg(&now, &c);
      snprintf(dlg_txt, sizeof(dlg_txt), "Medidas, finales, motores, pasos, velocidades, corrientes y Shaping de f\xC3\xA1" "brica. Se conservan PID y offsets M206.%s",
               arq_mc_mesh_will_erase(&c) ? MESH_WARN : (arq_mc_mesh_valid() ? " Se conserva la malla." : ""));
      ask("Restaurar mec\xC3\xA1nica", dlg_txt, "Restaurar", DLG_RMECH_GO, true);
    } break;
    case ID_M_RALL:  ask("Restaurar todo", "Se pierden malla, PID, offsets M206, Linear Advance y Rendimiento. Se conserva el modelo.", "Restaurar", DLG_RALL_GO, true); break;
    case ID_M_WIZ:   start_wizard(true, arq_mc_profile_id()); go(PG_MODEL, 0); break;
    case ID_M_OTHER:
      if (memcmp(&work, &base, sizeof(work)))
        ask_with_mesh("Cambios sin guardar", "Guarda o descarta los cambios de M\xC3\xA1quina antes de abrir Otros par\xC3\xA1metros (MKS).",
                      "Guardar", DLG_OTHER_SAVE, "Descartar", DLG_OTHER_DISCARD);
      else go_other();
      break;
    case ID_M_SAVE:
      if (arq_mc_mesh_will_erase(&work)) ask_with_mesh("Guardar", "\xC2\xBF" "Guardar los cambios en la EEPROM?", "Guardar", DLG_MSAVE_GO, "Cancelar", DLG_CANCEL);
      else do_save();
      break;
    default: break;
  }
}

static void ui_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  if (id >= DLG_INFO) { on_dialog(id); return; }
  if (arq_dialog_is_open() || test_kind != T_NONE) return;
  if (arq_mc_printing() && id != ID_BACK) { info("No disponible", "Hay una impresi\xC3\xB3n o un movimiento en curso."); return; }
  if (id == ID_BACK) { if (mode == M_WIZ) wiz_back(); else menu_back(); return; }
  if (id == ID_NEXT) {
    if (mode == M_WIZ) wiz_next();
    else if (page == PG_MENU) { sub = (sub + 1) % MENU_SUBS; menu_sub = sub; rebuild = true; }
    else if (page == PG_MECH) { sub = (sub + 1) % MECH_SUBS; rebuild = true; }
    return;
  }
  on_row(id);
}

// Teclado numerico
static void nk_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (arq_dialog_is_open()) return;
  const int k = obj->mks_obj_id;
  size_t n = strlen(edit_buf);
  if (k == ARQ_NK_BACK || k == ARQ_NK_CANCEL) { go(ret_page, ret_sub); return; }
  if (k == ARQ_NK_OK) {
    const float v = atof(edit_buf);
    if (!arq_mc_value_ok(work.profile, edit_p, v, arq_mc_shaping_min())) {
      arq_mc_range_t r; arq_mc_range(work.profile, edit_p, arq_mc_shaping_min(), &r);
      char lo[12], hi[12]; fmtv(lo, sizeof lo, r.lo, r.dec); fmtv(hi, sizeof hi, r.hi, r.dec);
      snprintf(dlg_txt, sizeof(dlg_txt), "Permitido: %s%s a %s %s.", r.zero_ok ? "0, o " : "", lo, hi, arq_mc_param_unit(edit_p));
      info("Fuera de rango", dlg_txt);
      return;
    }
    work.v[edit_p] = arq_mc_round(edit_p, v);
    go(ret_page, ret_sub);
    return;
  }
  if (k >= ARQ_NK_1 && k <= ARQ_NK_9) { if (n < 10) { edit_buf[n] = (char)('0' + k); edit_buf[n + 1] = '\0'; } }
  else if (k == ARQ_NK_0) { if (n < 10) { edit_buf[n] = '0'; edit_buf[n + 1] = '\0'; } }
  else if (k == ARQ_NK_POINT) { if (!strchr(edit_buf, '.') && n < 9) { if (!n || (n == 1 && edit_buf[0] == '-')) strcat(edit_buf, "0"); strcat(edit_buf, "."); } }
  else if (k == ARQ_NK_MINUS) { if (n == 0) strcpy(edit_buf, "-"); }
  else if (k == ARQ_NK_BKSP) { if (n) edit_buf[n - 1] = '\0'; }
  else if (k == ARQ_NK_CLEAR) edit_buf[0] = '\0';
  arq_numpad_set(edit_buf, arq_numpad_value_ok(edit_buf));
}

// ---- Tarea periodica ---------------------------------------------------------------------------------------------
static void notice_cb(lv_obj_t *obj, lv_event_t event) { if (event == LV_EVENT_RELEASED) arq_dialog_close(); }

void arq_machine_tick() {
  arq_mc_idle();

  if (!arq_dialog_is_open()) {                               // avisos (movimiento rechazado, driver restaurado...)
    const char *t = arq_mc_take_notice();
    if (t) { snprintf(notice_txt, sizeof(notice_txt), "%s", t); arq_dialog_open("Aviso", notice_txt, "Aceptar", nullptr, false, notice_cb, DLG_NOTICE, 0); }
  }

  if (!scr) return;
  if (rebuild) { build(); return; }

  const millis_t ms = millis();
  if (test_kind != T_NONE) {                                 // fin de una prueba: cola y planificador vacios
    const bool idle = ELAPSED(ms, test_t0 + 500) && !queue.has_commands_queued() && !planner.has_blocks_queued();
    const bool done = test_kind == T_MOVE ? idle : (idle && axis_was_homed((AxisEnum)test_axis));
    if (done || ELAPSED(ms, test_t0 + 120000UL)) test_finished();
    return;
  }

  if (!ELAPSED(ms, live_ms)) return;
  live_ms = ms + 150;
  if (page == PG_ES) {                                       // estado de los finales en vivo y comprobacion
    bool changed = false;
    uint8_t live = 0;
    for (uint8_t a = 0; a < 3; ++a) {
      const bool hit = es_hit_raw(a);
      live |= (hit ? 1 : 0) << a;
      const uint8_t st = es_stage[a];
      if (st == 0 && !hit) { if (!es_open_ms[a]) es_open_ms[a] = ms; else if (ELAPSED(ms, es_open_ms[a] + 1000)) es_stage[a] = 1; }
      else if (st == 0) es_open_ms[a] = 0;
      if (st == 1 && hit) { es_stage[a] = 2; es_hit_ms[a] = ms; }
      if (st == 2 && !hit && ELAPSED(ms, es_hit_ms[a] + 100)) es_stage[a] = 3;
      changed |= (es_stage[a] != st);
    }
    if (changed || live != last_live) { last_live = live; es_rows(); }
    if (changed && all_es()) set_chip("Los tres finales est\xC3\xA1n comprobados.", ARQ_TINT_OK);
  }
  else if (page == PG_DIRS && heated_e) {
    const uint8_t tq = (uint8_t)(thermalManager.wholeDegHotend(0) / 2);
    if (tq != last_live) { last_live = tq; dirs_rows(); dirs_chip(); }
  }
}

#endif // ARQ_HAS_MACHINE
