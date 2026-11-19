// Arnes de PC para el modulo REAL del firmware Marlin/src/lcd/extui/mks_ui/arquis_objects.cpp (sin modificar ni copiar).
//   arq_cli parse <archivo> <trozo> <cama_w_dmm> <cama_d_dmm>   lee cabecera (512 B) y cola (16 KB) y escribe el resultado en JSON
//   arq_cli batch                                                 funciones del mapa/estado: una orden por linea en stdin
// Lo ejecutan los tests de Python (tests/firmware/test_*.py) contra los vectores compartidos y el parser de referencia de Cura.
#include "arquis_objects.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iterator>
#include <iostream>
#include <algorithm>

static const struct { uint32_t bit; const char *name; } ERR[] = {
  {ARQ_E_TRUNCATED, "E_MANIFEST_TRUNCATED"}, {ARQ_E_FORMAT, "E_MANIFEST_FORMAT"}, {ARQ_E_CRC, "E_MANIFEST_CRC"}, {ARQ_E_TRAILER, "E_MANIFEST_TRAILER"},
  {ARQ_E_VERSION, "E_VERSION"}, {ARQ_E_NAME_DUP, "E_NAME_DUPLICATE"}, {ARQ_E_NAME_LONG, "E_NAME_TOO_LONG"}, {ARQ_E_NAME_INDEX, "E_NAME_INDEX"},
  {ARQ_E_COUNT, "E_COUNT"}, {ARQ_E_OVER_CAP, "E_OVER_CAPACITY"}, {ARQ_E_CAP, "E_CAP"}, {ARQ_E_ID_DUP, "E_ID_DUPLICATE"}, {ARQ_E_ID_RANGE, "E_ID_RANGE"},
  {ARQ_E_SIZE, "E_SIZE"}, {ARQ_E_Z, "E_Z"}, {ARQ_E_ROT, "E_ROT"}, {ARQ_E_OUTSIDE, "E_OUTSIDE_BED"}, {ARQ_E_ORDER, "E_ORDER"},
  {ARQ_E_TRAILER_MM, "E_TRAILER_MISMATCH"}, {ARQ_E_HEADER_MM, "E_HEADER_MISMATCH"}, {ARQ_E_BED, "E_BED_MISMATCH"}
};

static std::string esc(const char *s) { std::string o; for (; *s; ++s) { if (*s == '"' || *s == '\\') o += '\\'; o += *s; } return o; }

static int cmd_parse(int argc, char **argv) {
  if (argc < 6) return 2;
  std::ifstream f(argv[2], std::ios::binary);
  std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  const size_t chunk = (size_t)atoi(argv[3]);
  const unsigned bw = (unsigned)atoi(argv[4]), bd = (unsigned)atoi(argv[5]);
  arq_man_reset();
  arq_man_head((const uint8_t *)data.data(), (uint16_t)(data.size() < ARQ_MAN_HEAD_WINDOW ? data.size() : ARQ_MAN_HEAD_WINDOW));
  arq_man_tail_begin();
  const size_t start = data.size() > ARQ_MAN_TAIL_WINDOW ? data.size() - ARQ_MAN_TAIL_WINDOW : 0;
  for (size_t i = start; i < data.size(); i += chunk) arq_man_tail_feed((const uint8_t *)data.data() + i, (uint16_t)(std::min(chunk, data.size() - i)));
  const uint8_t st = arq_man_tail_finish((uint16_t)bw, (uint16_t)bd);
  printf("{\"status\":\"%s\",\"errors\":[", st == ARQ_MAN_OK ? "ok" : st == ARQ_MAN_NONE ? "none" : "bad");
  bool first = true;
  if (st == ARQ_MAN_BAD) for (auto &e : ERR) if (arq_man_errors() & e.bit) { printf("%s\"%s\"", first ? "" : ",", e.name); first = false; }
  printf("]");
  if (st == ARQ_MAN_OK) {
    printf(",\"print_id\":\"%08X\",\"bed\":[%u,%u,%d,%d],\"org\":\"%s\",\"seq\":%u,\"cap\":%u,\"n\":%u,\"n_names\":%u,\"objects\":[", (unsigned)arq_obj.pid, arq_obj.bed_w, arq_obj.bed_d,
           arq_obj.min_x, arq_obj.min_y, arq_obj.org, arq_obj.seq, arq_obj.cap, arq_obj.n, arq_obj.n_names);
    for (unsigned i = 0; i < arq_obj.n; ++i) {
      const ArqObjRec &r = arq_obj.rec[i];
      char lab[24]; arq_obj_label(lab, sizeof(lab), (uint8_t)i);
      printf("%s{\"id\":%u,\"name\":\"%s\",\"label\":\"%s\",\"instance\":%u,\"x\":%d,\"y\":%d,\"width\":%u,\"depth\":%u,\"shape\":\"%c\",\"z0\":%u,\"ztop\":%u,\"order\":%u}", i ? "," : "",
             i, esc(arq_obj.names[r.name_idx]).c_str(), esc(lab).c_str(), r.inst, r.cx, r.cy, r.w, r.d, (char)r.shape, r.z0, r.ztop, r.order);
    }
    printf("]");
  }
  printf("}\n");
  return 0;
}

// Cada linea de stdin es una orden con enteros; cada respuesta, una linea.
static int cmd_batch() {
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream is(line);
    std::string c; is >> c;
    std::vector<long> a; long v; std::string shape;
    if (c == "marker") { for (int i = 0; i < 17 && (is >> v); ++i) a.push_back(v); is >> shape; }
    else while (is >> v) a.push_back(v);
    auto bedview = [&](size_t o, ArqBed &b, ArqView &w) {
      b = { (int32_t)a[o], (int32_t)a[o + 1], (int32_t)a[o + 2], (int32_t)a[o + 3] };
      w = { (int16_t)a[o + 4], (int16_t)a[o + 5], (int16_t)a[o + 6], (int16_t)a[o + 7], (uint8_t)a[o + 8], a[o + 9] != 0, (int32_t)a[o + 10], (int32_t)a[o + 11] };
    };
    ArqBed b; ArqView w;
    if (c == "div_round") printf("%d\n", arq_div_round((int32_t)a[0], (int32_t)a[1]));
    else if (c == "to_px") { bedview(0, b, w); int32_t x, y; arq_to_px((int32_t)a[12], (int32_t)a[13], b, w, &x, &y); printf("%d %d\n", x, y); }
    else if (c == "marker") { bedview(0, b, w); ArqMarker m; arq_marker((int32_t)a[13], (int32_t)a[14], (int32_t)a[15], (int32_t)a[16], shape[0], b, w, &m);
      printf("%d %d %d %d %d %d\n", m.x0, m.y0, m.x1, m.y1, m.cx, m.cy); }
    else if (c == "pick") {                       // pick n (id x0 y0 x1 y1)*n px py
      const size_t n = (size_t)a[0]; const long px = a[1 + 5 * n], py = a[2 + 5 * n];
      int16_t best_id = -1; ArqMarker best{};
      for (size_t i = 0; i < n; ++i) {
        ArqMarker m{ (int16_t)a[2 + 5 * i], (int16_t)a[3 + 5 * i], (int16_t)a[4 + 5 * i], (int16_t)a[5 + 5 * i], 0, 0 };
        const int16_t id = (int16_t)a[1 + 5 * i];
        if (arq_marker_hit(m, px, py) && arq_marker_beats(m, (uint8_t)id, best, best_id)) { best = m; best_id = id; }
      }
      printf("%d\n", best_id);
    }
    else if (c == "object_state") printf("%u\n", arq_obj_state(a[0], a[1], a[2], a[3], (uint16_t)a[4], (uint16_t)a[5], (uint8_t)a[6], (uint16_t)a[7], (int16_t)a[8]));
    else if (c == "crc32") { uint8_t buf[256]; uint16_t n = 0; for (long x : a) buf[n++] = (uint8_t)x; printf("%08X\n", ~arq_crc32_update(0xFFFFFFFFu, buf, n)); }
    else printf("?\n");
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc >= 2 && !strcmp(argv[1], "parse")) return cmd_parse(argc, argv);
  if (argc >= 2 && !strcmp(argv[1], "batch")) return cmd_batch();
  fprintf(stderr, "uso: arq_cli parse <archivo> <trozo> <cama_w_dmm> <cama_d_dmm> | arq_cli batch\n");
  return 2;
}
