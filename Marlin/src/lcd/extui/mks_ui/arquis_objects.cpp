/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "arquis_objects.h"

#include <string.h>

ArqObjStore arq_obj;

// ---------------------------------------------------------------------------------------------------------------------------------
// CRC-32 IEEE (el mismo de zlib.crc32), por nibbles: 64 B de tabla en flash, sin tabla de 1 KB en RAM
// ---------------------------------------------------------------------------------------------------------------------------------
static const uint32_t crc_tab[16] = {
  0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
  0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

static inline uint32_t crc_byte(uint32_t c, uint8_t b) {
  c ^= b;
  c = (c >> 4) ^ crc_tab[c & 15];
  return (c >> 4) ^ crc_tab[c & 15];
}

uint32_t arq_crc32_update(uint32_t raw, const uint8_t *p, uint16_t n) {
  while (n--) raw = crc_byte(raw, *p++);
  return raw;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Lectura de numeros y literales de una linea (equivalen a las expresiones regulares del parser de referencia, ASCII)
// ---------------------------------------------------------------------------------------------------------------------------------
static bool lit(const char *&p, const char *s) {
  const size_t n = strlen(s);
  if (strncmp(p, s, n)) return false;
  p += n;
  return true;
}

static bool num(const char *&p, uint32_t &v) {          // \d+ (satura en 2^32-1: un valor enorme solo debe fallar la validacion posterior)
  if (*p < '0' || *p > '9') return false;
  uint32_t r = 0;
  while (*p >= '0' && *p <= '9') {
    const uint32_t d = (uint32_t)(*p++ - '0');
    r = (r > (0xFFFFFFFFUL - d) / 10UL) ? 0xFFFFFFFFUL : r * 10UL + d;
  }
  v = r;
  return true;
}

static bool snum(const char *&p, int32_t &v) {          // -?\d+ (satura en +-2^31-1)
  bool neg = false;
  if (*p == '-') { neg = true; ++p; }
  uint32_t u;
  if (!num(p, u)) return false;
  if (u > 0x7FFFFFFFUL) u = 0x7FFFFFFFUL;
  v = neg ? -(int32_t)u : (int32_t)u;
  return true;
}

static bool hex8(const char *&p, uint32_t &v) {         // [0-9A-F]{8}
  uint32_t r = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    const char c = p[i];
    uint8_t d;
    if (c >= '0' && c <= '9') d = (uint8_t)(c - '0'); else if (c >= 'A' && c <= 'F') d = (uint8_t)(c - 'A' + 10); else return false;
    r = (r << 4) | d;
  }
  p += 8;
  v = r;
  return true;
}

static inline bool in_i16(int32_t v) { return v >= -32768 && v <= 32767; }
static inline bool in_u16(int32_t v) { return v >= 0 && v <= 65535; }
static inline bool starts(const char *s, const char *prefix) { return !strncmp(s, prefix, strlen(prefix)); }

// ---------------------------------------------------------------------------------------------------------------------------------
// Estado del lector
// ---------------------------------------------------------------------------------------------------------------------------------
enum : uint8_t { PH_IDLE = 0, PH_BODY, PH_AFTER_END, PH_DONE };
enum : uint8_t { F_BEGIN = 1, F_END_SEEN = 2, F_END_BAD = 4, F_TR_SEEN = 8, F_TR_OK = 16, F_H_SEEN = 32, F_FATAL = 64, F_O_SEEN = 128 };

static struct {
  uint8_t  phase, flags;
  bool     hdr_found;                                   // marca ;ARQUIS_OBJECTS= en la cabecera
  bool     overlong;                                    // la linea actual no cabe en buf
  uint8_t  cr_run;                                      // CR finales pendientes de la linea actual
  uint8_t  status;
  uint16_t len;
  char     buf[ARQ_MAN_LINE_MAX + 1];
  uint32_t crc_cur, crc_prev;                           // CRC en bruto: con la linea actual / hasta la linea anterior
  uint32_t end_crc, tr_crc, tr_pid, hdr_pid;
  uint16_t tr_n, hdr_n, hdr_ver;
  uint32_t err_names, err_sem, err_final, fatal_code;   // errores de las lineas M / de las lineas O / resultado / error de formato del cuerpo
  uint32_t names_mask, ids_seen, orders_seen;           // conjuntos por bit (indices < 32)
  bool     name_big, order_bad;                         // algun indice de nombre >= capacidad / algun orden >= 32 o repetido
  uint32_t n_decl, version;
  uint16_t n_obj, max_id;
  uint32_t pid;
  uint8_t  names_cnt;
} P;

static void clear_store() { memset(&arq_obj, 0, sizeof(arq_obj)); }

void arq_man_reset() {
  memset(&P, 0, sizeof(P));
  P.status = ARQ_MAN_NONE;
  clear_store();
}

uint8_t  arq_man_status() { return P.status; }
uint32_t arq_man_errors() { return P.err_final; }

// Cabecera ";ARQUIS_OBJECTS=<v> PID=<8 hex> N=<n>" (opcional; si existe debe coincidir con el bloque)
void arq_man_head(const uint8_t *buf, uint16_t n) {
  P.hdr_found = false;
  uint16_t i = 0;
  while (i < n) {
    uint16_t j = i;
    char line[48];
    uint8_t l = 0;
    while (j < n && buf[j] != '\n') { if (l < sizeof(line) - 1) line[l++] = (char)buf[j]; ++j; }
    const bool whole = (uint16_t)(j - i) <= (uint16_t)(sizeof(line) - 1);
    line[l] = 0;
    if (whole && starts(line, ";ARQUIS_OBJECTS=")) {
      const char *p = line + 16;
      uint32_t v, pid, nn;
      if (num(p, v) && lit(p, " PID=") && hex8(p, pid) && lit(p, " N=") && num(p, nn)) {
        while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
        if (!*p) {
          P.hdr_found = true;
          P.hdr_ver = v > 65535 ? 65535 : (uint16_t)v;
          P.hdr_pid = pid;
          P.hdr_n = nn > 65535 ? 65535 : (uint16_t)nn;
          return;                                       // la primera que encaja (re.search)
        }
      }
    }
    i = (uint16_t)(j + 1);
  }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Lineas del bloque
// ---------------------------------------------------------------------------------------------------------------------------------
static void body_fatal(uint32_t code) { if (!(P.flags & F_FATAL)) { P.flags |= F_FATAL; P.fatal_code = code; } }

static void line_h(const char *s, bool overlong) {
  const char *p = s + 7;                                // ";ARQ:H "
  uint32_t ver, pid, n, bw, bd, cap, seq;
  int32_t mx, my;
  char o0, o1;
  if (overlong || (P.flags & F_H_SEEN) || !lit(p, "V=") || !num(p, ver) || !lit(p, " PID=") || !hex8(p, pid) || !lit(p, " N=") || !num(p, n) || !lit(p, " BED=") ||
      !num(p, bw) || !lit(p, ",") || !num(p, bd) || !lit(p, " MIN=") || !snum(p, mx) || !lit(p, ",") || !snum(p, my) || !lit(p, " ORG=")) { body_fatal(ARQ_E_FORMAT); return; }
  o0 = p[0]; o1 = p[1];
  if (o0 < 'A' || o0 > 'Z' || o1 < 'A' || o1 > 'Z') { body_fatal(ARQ_E_FORMAT); return; }
  p += 2;
  if (!lit(p, " SEQ=") || (*p != '0' && *p != '1')) { body_fatal(ARQ_E_FORMAT); return; }
  seq = (uint32_t)(*p++ - '0');
  if (!lit(p, " CAP=") || !num(p, cap) || *p) { body_fatal(ARQ_E_FORMAT); return; }
  if (bw > 65535 || bd > 65535 || !in_i16(mx) || !in_i16(my)) { body_fatal(ARQ_E_FORMAT); return; }     // no cabe en el almacenamiento
  P.flags |= F_H_SEEN;
  P.version = ver; P.pid = pid; P.n_decl = n;
  arq_obj.pid = pid; arq_obj.bed_w = (uint16_t)bw; arq_obj.bed_d = (uint16_t)bd; arq_obj.min_x = (int16_t)mx; arq_obj.min_y = (int16_t)my;
  arq_obj.org[0] = o0; arq_obj.org[1] = o1; arq_obj.org[2] = 0;
  arq_obj.seq = (uint8_t)seq; arq_obj.cap = cap > 255 ? 255 : (uint8_t)cap;
}

static void line_m(const char *s, bool overlong) {
  const char *p = s + 7;                                // ";ARQ:M "
  uint32_t idx;
  if (overlong || !(P.flags & F_H_SEEN) || (P.flags & F_O_SEEN) || !num(p, idx) || *p != ' ' || !p[1]) { body_fatal(ARQ_E_FORMAT); return; }
  ++p;
  const size_t nl = strlen(p);
  if (idx >= ARQ_OBJ_DESIGN_CAPACITY) { P.err_names |= ARQ_E_NAME_INDEX; P.name_big = true; return; }
  if (P.names_mask & (1UL << idx)) P.err_names |= ARQ_E_NAME_DUP;
  P.names_mask |= 1UL << idx;
  if (nl > ARQ_OBJ_NAME_MAX) P.err_names |= ARQ_E_NAME_LONG;
  const size_t c = nl > ARQ_OBJ_NAME_MAX ? ARQ_OBJ_NAME_MAX : nl;
  memcpy(arq_obj.names[idx], p, c);
  arq_obj.names[idx][c] = 0;
}

static void line_o(const char *s, bool overlong) {
  const char *p = s + 7;                                // ";ARQ:O "
  uint32_t id, nidx, inst, w, d, rot, z0, ztop, order;
  int32_t cx, cy;
  char shape;
  if (overlong || !(P.flags & F_H_SEEN)) { body_fatal(ARQ_E_FORMAT); return; }
  if (!num(p, id) || !lit(p, " ") || !num(p, nidx) || !lit(p, " ") || !num(p, inst) || !lit(p, " ") || !snum(p, cx) || !lit(p, " ") || !snum(p, cy) || !lit(p, " ") ||
      !num(p, w) || !lit(p, " ") || !num(p, d) || !lit(p, " ") || !num(p, rot) || !lit(p, " ")) { body_fatal(ARQ_E_FORMAT); return; }
  shape = *p++;
  if ((shape != 'C' && shape != 'S' && shape != 'R') || !lit(p, " ") || !num(p, z0) || !lit(p, " ") || !num(p, ztop) || !lit(p, " ") || !num(p, order) || *p) {
    body_fatal(ARQ_E_FORMAT); return;
  }
  if (inst > 255 || !in_i16(cx) || !in_i16(cy) || w > 65535 || d > 65535 || z0 > 65535 || ztop > 65535) { body_fatal(ARQ_E_FORMAT); return; }   // no cabe en el almacenamiento
  P.flags |= F_O_SEEN;
  if (P.n_obj < 65535) ++P.n_obj;
  // ---- comprobaciones por objeto (se informan solo si el bloque llega a la validacion semantica, como en la referencia)
  if (id < 32) { if (P.ids_seen & (1UL << id)) P.err_sem |= ARQ_E_ID_DUP; P.ids_seen |= 1UL << id; }
  const uint16_t id16 = id > 65535 ? 65535 : (uint16_t)id;
  if (id16 > P.max_id) P.max_id = id16;
  if (nidx >= ARQ_OBJ_DESIGN_CAPACITY || !(P.names_mask & (1UL << nidx))) P.err_sem |= ARQ_E_NAME_INDEX;
  if (w == 0 || d == 0) P.err_sem |= ARQ_E_SIZE;
  if (z0 > ztop) P.err_sem |= ARQ_E_Z;
  if (rot > 359) P.err_sem |= ARQ_E_ROT;
  const int32_t x0 = cx - (int32_t)(w / 2), x1 = cx + (int32_t)(w / 2), y0 = cy - (int32_t)(d / 2), y1 = cy + (int32_t)(d / 2);
  if (x1 < arq_obj.min_x || y1 < arq_obj.min_y || x0 > (int32_t)arq_obj.min_x + arq_obj.bed_w || y0 > (int32_t)arq_obj.min_y + arq_obj.bed_d) P.err_sem |= ARQ_E_OUTSIDE;
  if (order < 32) { if (P.orders_seen & (1UL << order)) P.order_bad = true; P.orders_seen |= 1UL << order; } else P.order_bad = true;
  // ---- se guarda en rec[id] (ids validos < capacidad; un id repetido o fuera de rango ya invalida el bloque)
  if (id < ARQ_OBJ_DESIGN_CAPACITY) {
    ArqObjRec &r = arq_obj.rec[id];
    r.cx = (int16_t)cx; r.cy = (int16_t)cy; r.w = (uint16_t)w; r.d = (uint16_t)d; r.z0 = (uint16_t)z0; r.ztop = (uint16_t)ztop;
    r.name_idx = nidx > 255 ? 255 : (uint8_t)nidx; r.inst = (uint8_t)inst; r.order = order > 255 ? 255 : (uint8_t)order; r.shape = (uint8_t)shape;
  }
}

// Procesa una linea completa (ya sin los CR finales). El CRC de las lineas del cuerpo se acumula byte a byte en feed().
static void on_line() {
  P.buf[P.len] = 0;
  const char *s = P.buf;
  const bool over = P.overlong;
  if (!over && !strcmp(s, ";ARQUIS_OBJECTS_BEGIN")) {       // el ULTIMO BEGIN manda: reinicia
    clear_store();
    const bool hf = P.hdr_found; const uint16_t hv = P.hdr_ver, hn = P.hdr_n; const uint32_t hp = P.hdr_pid;
    memset(&P, 0, sizeof(P));
    P.hdr_found = hf; P.hdr_ver = hv; P.hdr_n = hn; P.hdr_pid = hp;
    P.flags = F_BEGIN; P.phase = PH_BODY; P.crc_prev = P.crc_cur = 0xFFFFFFFFUL;
    return;
  }
  switch (P.phase) {
    case PH_BODY:
      if (starts(s, ";ARQUIS_OBJECTS_END")) {                              // linea END: no entra en el CRC (crc_prev queda con el cuerpo)
        P.flags |= F_END_SEEN;
        const char *p = s + 19;
        uint32_t c;
        if (!over && lit(p, " CRC=") && hex8(p, c) && !*p) P.end_crc = c; else P.flags |= F_END_BAD;
        P.phase = PH_AFTER_END;
        return;
      }
      P.crc_cur = crc_byte(P.crc_cur, '\n');                               // la linea (sin CR) + '\n' entra en el CRC
      P.crc_prev = P.crc_cur;
      if (P.flags & F_FATAL) return;                                       // tras el primer error de formato, el resto solo cuenta para el CRC
      if (starts(s, ";ARQ:H ")) line_h(s, over);
      else if (starts(s, ";ARQ:M ")) line_m(s, over);
      else if (starts(s, ";ARQ:O ")) line_o(s, over);
      else body_fatal(ARQ_E_FORMAT);
      return;
    case PH_AFTER_END:
      if (starts(s, ";ARQUIS_TRAILER")) {                                  // la primera linea de trailer tras END (formato incorrecto = ausente)
        P.flags |= F_TR_SEEN;
        const char *p = s + 15;
        uint32_t c, pid, n;
        if (!over && lit(p, " CRC=") && hex8(p, c) && lit(p, " PID=") && hex8(p, pid) && lit(p, " N=") && num(p, n) && !*p) {
          P.flags |= F_TR_OK; P.tr_crc = c; P.tr_pid = pid; P.tr_n = n > 65535 ? 65535 : (uint16_t)n;
        }
        P.phase = PH_DONE;
      }
      return;
    default: return;
  }
}

void arq_man_tail_begin() {
  const bool hf = P.hdr_found; const uint16_t hv = P.hdr_ver, hn = P.hdr_n; const uint32_t hp = P.hdr_pid;
  memset(&P, 0, sizeof(P));
  P.hdr_found = hf; P.hdr_ver = hv; P.hdr_n = hn; P.hdr_pid = hp;
  P.status = ARQ_MAN_NONE;
  clear_store();
}

static void end_line() {
  P.cr_run = 0;                                         // rstrip("\r")
  on_line();
  P.len = 0; P.overlong = false;
  if (P.phase == PH_BODY) P.crc_cur = P.crc_prev;       // la siguiente linea parte del CRC de las anteriores
}

static void put_byte(uint8_t b) {
  if (P.phase == PH_BODY) P.crc_cur = crc_byte(P.crc_cur, b);
  if (P.len < ARQ_MAN_LINE_MAX) P.buf[P.len++] = (char)b; else P.overlong = true;
}

void arq_man_tail_feed(const uint8_t *buf, uint16_t n) {
  for (uint16_t i = 0; i < n; ++i) {
    const uint8_t b = buf[i];
    if (b == '\n') { end_line(); continue; }
    if (b == '\r') { if (P.cr_run < 255) ++P.cr_run; continue; }
    while (P.cr_run) { put_byte('\r'); --P.cr_run; }    // un CR que no era final de linea forma parte de ella
    put_byte(b);
  }
}

static inline uint32_t popcount32(uint32_t v) { uint8_t c = 0; while (v) { v &= v - 1; ++c; } return c; }
static inline int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

uint8_t arq_man_tail_finish(uint16_t bed_w_dmm, uint16_t bed_d_dmm) {
  if (P.len || P.cr_run || P.overlong) end_line();      // ultima linea sin '\n' final
  uint32_t e = 0;
  P.status = ARQ_MAN_BAD;
  if (!(P.flags & F_BEGIN)) { P.status = ARQ_MAN_NONE; clear_store(); return P.status; }
  // El mismo orden de comprobaciones que parse_regions() del parser de referencia
  if (!(P.flags & F_END_SEEN)) e = ARQ_E_TRUNCATED;
  else if (P.flags & F_END_BAD) e = ARQ_E_FORMAT;
  else if (P.end_crc != ~P.crc_prev) e = ARQ_E_CRC;
  else if (!(P.flags & F_TR_OK)) e = ARQ_E_TRAILER;
  else if ((P.flags & F_FATAL)) e = P.err_names | P.fatal_code;
  else if (!(P.flags & F_H_SEEN)) e = P.err_names | ARQ_E_FORMAT;
  else if (P.version != ARQ_OBJ_PROTOCOL_VERSION) e = P.err_names | ARQ_E_VERSION;
  else {
    e = P.err_names | P.err_sem;
    const uint32_t cnt = popcount32(P.names_mask);
    if (P.name_big || P.names_mask != ((cnt >= 32) ? 0xFFFFFFFFUL : ((1UL << cnt) - 1))) e |= ARQ_E_NAME_INDEX;
    if (P.n_decl != P.n_obj) e |= ARQ_E_COUNT;
    if (P.n_obj > ARQ_OBJ_DESIGN_CAPACITY || P.n_obj > arq_obj.cap) e |= ARQ_E_OVER_CAP;
    if (arq_obj.cap > ARQ_OBJ_CAPACITY_CEILING) e |= ARQ_E_CAP;
    uint32_t lim = P.n_obj < arq_obj.cap ? P.n_obj : arq_obj.cap;
    if (lim > ARQ_OBJ_DESIGN_CAPACITY) lim = ARQ_OBJ_DESIGN_CAPACITY;
    if (P.n_obj && P.max_id >= lim) e |= ARQ_E_ID_RANGE;
    if (P.n_obj && (P.order_bad || (P.n_obj <= 32 && P.orders_seen != ((P.n_obj >= 32) ? 0xFFFFFFFFUL : ((1UL << P.n_obj) - 1))))) e |= ARQ_E_ORDER;
    if (P.tr_crc != ~P.crc_prev || P.tr_pid != P.pid || P.tr_n != P.n_obj) e |= ARQ_E_TRAILER_MM;
    if (P.hdr_found && (P.hdr_ver != P.version || P.hdr_pid != P.pid || P.hdr_n != P.n_obj)) e |= ARQ_E_HEADER_MM;
    if (bed_w_dmm && bed_d_dmm && (iabs((int32_t)arq_obj.bed_w - bed_w_dmm) > ARQ_MAN_BED_TOL_DMM || iabs((int32_t)arq_obj.bed_d - bed_d_dmm) > ARQ_MAN_BED_TOL_DMM)) e |= ARQ_E_BED;
  }
  P.err_final = e;
  if (e) { clear_store(); P.status = ARQ_MAN_BAD; }
  else {
    arq_obj.n = (uint8_t)P.n_obj;
    arq_obj.n_names = (uint8_t)popcount32(P.names_mask);
    P.status = ARQ_MAN_OK;
  }
  return P.status;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Nombres y estados
// ---------------------------------------------------------------------------------------------------------------------------------
uint8_t arq_obj_label(char *out, uint8_t n, uint8_t id) {
  if (!n) return 0;
  if (id >= arq_obj.n) { out[0] = 0; return 0; }
  const ArqObjRec &r = arq_obj.rec[id];
  uint8_t same = 0;
  for (uint8_t i = 0; i < arq_obj.n; ++i) if (arq_obj.rec[i].name_idx == r.name_idx) ++same;
  uint8_t l = 0;
  const char *nm = r.name_idx < ARQ_OBJ_DESIGN_CAPACITY ? arq_obj.names[r.name_idx] : "";
  while (nm[l] && l < n - 1) { out[l] = nm[l]; ++l; }
  if (same > 1) {                                       // " <copia+1>"
    char t[5]; uint8_t k = 0;
    uint16_t v = (uint16_t)r.inst + 1;
    do { t[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < sizeof(t));
    if (l + 1 + k < n) { out[l++] = ' '; while (k) out[l++] = t[--k]; }
  }
  out[l] = 0;
  return l;
}

uint8_t arq_obj_state(bool available, bool trusted, bool canceled, bool seq, uint16_t z0, uint16_t ztop, uint8_t order, uint16_t z_print, int16_t active_order) {
  if (!available || !trusted) return ARQ_ST_UNAVAILABLE;
  if (canceled) return ARQ_ST_CANCELED;
  if (seq) {
    if (active_order < 0 || order > active_order) return ARQ_ST_PENDING;
    return order == active_order ? ARQ_ST_PRINTING : ARQ_ST_DONE;
  }
  if (z_print < z0) return ARQ_ST_PENDING;
  return z_print > ztop ? ARQ_ST_DONE : ARQ_ST_PRINTING;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Mapa de la cama
// ---------------------------------------------------------------------------------------------------------------------------------
int32_t arq_div_round(int32_t a, int32_t b) {           // floor((2a + b) / (2b)) con division por SUELO (b > 0)
  const int32_t n = 2 * a + b, d = 2 * b;
  return n >= 0 ? n / d : -((-n + d - 1) / d);
}

static void scale(const ArqBed &bed, const ArqView &v, int32_t *num, int32_t *den) {
  const int32_t side = (v.w < v.h ? v.w : v.h) - 2 * ARQ_MAP_PAD_PX;
  *num = side * v.zoom;
  *den = bed.w > bed.d ? bed.w : bed.d;
  if (*den < 1) *den = 1;
}

void arq_to_px(int32_t x, int32_t y, const ArqBed &bed, const ArqView &v, int32_t *px, int32_t *py) {
  int32_t num, den;
  scale(bed, v, &num, &den);
  const int32_t cx = v.has_center ? v.cx : bed.min_x + bed.w / 2, cy = v.has_center ? v.cy : bed.min_y + bed.d / 2;
  *px = v.x + v.w / 2 + arq_div_round(num * (x - cx), den);
  *py = v.y + v.h / 2 - arq_div_round(num * (y - cy), den);
}

void arq_marker(int32_t cx, int32_t cy, int32_t w, int32_t d, char shape, const ArqBed &bed, const ArqView &v, ArqMarker *m) {
  int32_t num, den, pcx, pcy;
  scale(bed, v, &num, &den);
  arq_to_px(cx, cy, bed, v, &pcx, &pcy);
  int32_t pw = arq_div_round(num * w, den), ph = arq_div_round(num * d, den);
  if (pw < ARQ_MAP_MIN_MARKER_PX) pw = ARQ_MAP_MIN_MARKER_PX;
  if (ph < ARQ_MAP_MIN_MARKER_PX) ph = ARQ_MAP_MIN_MARKER_PX;
  if (shape == 'C') { if (pw > ph) ph = pw; else pw = ph; }
  const int32_t x0 = pcx - pw / 2, y0 = pcy - ph / 2;
  m->x0 = (int16_t)x0; m->y0 = (int16_t)y0; m->x1 = (int16_t)(x0 + pw - 1); m->y1 = (int16_t)(y0 + ph - 1); m->cx = (int16_t)pcx; m->cy = (int16_t)pcy;
}

bool arq_marker_hit(const ArqMarker &m, int32_t px, int32_t py) { return px >= m.x0 && px <= m.x1 && py >= m.y0 && py <= m.y1; }

static inline int32_t area(const ArqMarker &m) { return (int32_t)(m.x1 - m.x0 + 1) * (m.y1 - m.y0 + 1); }

bool arq_marker_beats(const ArqMarker &m, uint8_t id, const ArqMarker &best, int16_t best_id) {
  if (best_id < 0) return true;
  const int32_t a = area(m), b = area(best);
  return a < b || (a == b && id > best_id);
}

// ---------------------------------------------------------------------------------------------------------------------------------
bool arq_box_within(const ArqObjRec &r, int32_t x0_mm, int32_t y0_mm, int32_t x1_mm, int32_t y1_mm, int32_t margin_dmm) {
  const int32_t hx = r.w / 2, hy = r.d / 2;
  const int32_t mx0 = (int32_t)r.cx - hx - margin_dmm, mx1 = (int32_t)r.cx + hx + margin_dmm;
  const int32_t my0 = (int32_t)r.cy - hy - margin_dmm, my1 = (int32_t)r.cy + hy + margin_dmm;
  return x0_mm * 10 >= mx0 && x1_mm * 10 <= mx1 && y0_mm * 10 >= my0 && y1_mm * 10 <= my1;
}
