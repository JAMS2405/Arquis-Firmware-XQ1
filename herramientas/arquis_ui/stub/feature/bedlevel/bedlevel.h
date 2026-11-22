// STUB: firmas reales de Marlin 2.1.2.10 leidas en feature/bedlevel/bedlevel.h y abl/bbl.h
#pragma once
struct xy_pos_stub { float x, y; void set(const float px, const float py) { x = px; y = py; } };
typedef xy_pos_stub xy_pos_t;
typedef float bed_mesh_t[5][5];
class LevelingBilinear {
public:
  static bed_mesh_t z_values;
  static xy_pos_t grid_spacing, grid_start;
  static void set_grid(const xy_pos_t& _grid_spacing, const xy_pos_t& _grid_start);
  static void refresh_bed_level();
};
extern LevelingBilinear bedlevel;
extern bool g29_in_progress;                                 // bedlevel.h real (con PROBE_MANUALLY)
bool leveling_is_valid();
void set_bed_leveling_enabled(const bool enable=true);
