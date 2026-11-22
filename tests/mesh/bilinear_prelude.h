// Prelude para compilar, SIN modificarla, la interpolacion bilineal REAL de Marlin (LevelingBilinear::get_z_correction de
// Marlin/src/feature/bedlevel/abl/bbl.cpp) y el factor de desvanecimiento REAL del planificador (Planner::fade_scaling_factor_for_z).
// Los tipos son equivalentes minimos de los de Marlin (xy_pos_t...). El texto de las funciones se EXTRAE del codigo del repositorio al
// compilar (build_bilinear.sh), de modo que si alguien cambia la interpolacion de Marlin, estas pruebas lo ven.
#pragma once
#include <stdint.h>
#include <math.h>
#define DISABLED(x) 1
#define ENABLED(x) 0           // subdivision y extrapolacion fuera de la malla desactivadas (tests/mesh/test_config.py lo comprueba)
#define GRID_MAX_POINTS_X 5
#define GRID_MAX_POINTS_Y 5
#define FLOOR(x) floorf(x)
#define _MIN(a,b) ((a)<(b)?(a):(b))
#define constrain(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
#define NOLESS(v,n) do{ if ((v) < (n)) (v) = (n); }while(0)
typedef float const_float_t_;
struct xy_float_t { float x, y; };
struct xy_int8_t {
  int8_t x, y;
  bool operator!=(const xy_int8_t &o) const { return x != o.x || y != o.y; }
};
struct xy_pos_t {
  float x, y;
  xy_pos_t operator-(const xy_pos_t &o) const { return { x - o.x, y - o.y }; }
  xy_pos_t asFloat() const { return *this; }
  xy_pos_t reciprocal() const { return { 1.0f / x, 1.0f / y }; }
};
typedef float bed_mesh_t[GRID_MAX_POINTS_X][GRID_MAX_POINTS_Y];
class LevelingBilinear {
public:
  static bed_mesh_t z_values;
  static xy_pos_t grid_spacing, grid_start;
  static xy_float_t grid_factor;
  static xy_pos_t cached_rel;
  static xy_int8_t cached_g;
  static float get_z_correction(const xy_pos_t &raw);
  static void set_grid(const xy_pos_t &sp, const xy_pos_t &st) {
    grid_spacing = sp; grid_start = st; grid_factor = { 1.0f / sp.x, 1.0f / sp.y };
    cached_rel = { -999.999f, -999.999f }; cached_g = { -99, -99 };     // como LevelingBilinear::refresh_bed_level()
  }
};
extern LevelingBilinear bedlevel;
struct Planner {
  static float z_fade_height, inverse_z_fade_height, last_fade_z;
  static float fade_scaling_factor_for_z(const float rz);
  static void set_z_fade_height(const float zfh) { z_fade_height = zfh > 0 ? zfh : 0; inverse_z_fade_height = z_fade_height ? 1.0f / z_fade_height : 0; last_fade_z = -999.999f; }
};
