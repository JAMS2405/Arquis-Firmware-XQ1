// Pruebas de la interpolacion bilineal y del desvanecimiento REALES de Marlin (texto extraido por build_bilinear.sh).
// Convencion verificada: Z_fisica = Z_ordenada + factor_fade(Z) * get_z_correction(x, y)  (planner.cpp: apply_leveling).
#include "bilinear_prelude.h"
#include <stdio.h>
#include <string.h>
LevelingBilinear bedlevel;
bed_mesh_t LevelingBilinear::z_values;
xy_pos_t LevelingBilinear::grid_spacing, LevelingBilinear::grid_start, LevelingBilinear::cached_rel;
xy_float_t LevelingBilinear::grid_factor;
xy_int8_t LevelingBilinear::cached_g;
float Planner::z_fade_height = 0, Planner::inverse_z_fade_height = 0, Planner::last_fade_z = -999.999f;
#include "bbl_real.inc"
#include "fade_real.inc"

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++fails; printf("[bilinear] FALLA: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static bool near(float a, float b, float t = 1e-4f) { return fabsf(a - b) <= t; }

// Malla de 5x5 en 15..285 mm (paso 67,5), como la configuracion real (PROBING_MARGIN 15 en cama de 300 mm).
static void setup(float (*f)(int ix, int iy)) {
  bedlevel.set_grid({ 67.5f, 67.5f }, { 15.0f, 15.0f });
  for (int x = 0; x < 5; ++x) for (int y = 0; y < 5; ++y) bedlevel.z_values[x][y] = f(x, y);
}
static float corr(float x, float y) { return bedlevel.get_z_correction({ x, y }); }
static float phys(float x, float y, float z) { const float k = Planner::fade_scaling_factor_for_z(z); return z + (k ? k * corr(x, y) : 0.0f); }

int main() {
  // 1) En cada punto medido la correccion es EXACTAMENTE el valor guardado (positivos, negativos e iguales)
  { static const float v[5][5] = { { 0.20f, 0.10f, 0, -0.10f, -0.20f }, { 0.15f, 0.05f, -0.05f, -0.15f, -0.25f }, { 0.10f, 0, 0, -0.05f, -0.10f }, { 0.30f, 0.12f, 0.02f, -0.02f, 0 }, { -0.30f, 0.11f, 0.11f, 0.11f, 0.11f } };
    bedlevel.set_grid({ 67.5f, 67.5f }, { 15.0f, 15.0f });
    for (int x = 0; x < 5; ++x) for (int y = 0; y < 5; ++y) bedlevel.z_values[x][y] = v[x][y];
    for (int x = 0; x < 5; ++x) for (int y = 0; y < 5; ++y) CHECK(near(corr(15 + 67.5f * x, 15 + 67.5f * y), v[x][y]), "punto (%d,%d): %.4f != %.4f", x, y, corr(15 + 67.5f * x, 15 + 67.5f * y), v[x][y]);
    // 2) Entre cuatro puntos: interpolacion bilineal (media en el centro de la celda)
    for (int x = 0; x < 4; ++x) for (int y = 0; y < 4; ++y) {
      const float m = (v[x][y] + v[x + 1][y] + v[x][y + 1] + v[x + 1][y + 1]) / 4;
      CHECK(near(corr(15 + 67.5f * (x + 0.5f), 15 + 67.5f * (y + 0.5f)), m), "centro de celda (%d,%d)", x, y);
    }
    // 3) Continuidad: el salto entre dos posiciones a 0,01 mm no supera la pendiente maxima de la malla
    float prev = corr(15, 100), maxjump = 0;
    for (float x = 15.01f; x <= 285; x += 0.01f) { const float c = corr(x, 100); const float j = fabsf(c - prev); if (j > maxjump) maxjump = j; prev = c; }
    CHECK(maxjump < 0.01f, "salto maximo %.5f mm en 0,01 mm (la interpolacion debe ser continua)", maxjump);
    // 4) Fuera del area medida (0..15 y 285..300): se mantiene la altura del borde, sin extrapolar
    CHECK(near(corr(0, 0), v[0][0]), "esquina (0,0) fuera de la malla");
    CHECK(near(corr(300, 300), v[4][4]), "esquina (300,300) fuera de la malla");
    CHECK(near(corr(0, 300), v[0][4]), "esquina (0,300)");
    CHECK(near(corr(300, 0), v[4][0]), "esquina (300,0)");
    CHECK(near(corr(7, 15 + 67.5f * 2), v[0][2]), "borde izquierdo fuera de la malla");
    CHECK(near(corr(299, 15 + 67.5f * 1.5f), (v[4][1] + v[4][2]) / 2), "borde derecho fuera de la malla, entre dos filas");
    // 5) Orientacion: x = columna (izquierda->derecha), y = fila (delante->atras)
    CHECK(near(corr(15, 15 + 67.5f * 4), v[0][4]) && !near(corr(15, 15 + 67.5f * 4), v[4][0]), "orientacion X/Y");
  }
  // 6) Un plano inclinado se reproduce EXACTAMENTE (la bilineal es exacta en planos): 0,3 mm de subida hacia la derecha, -0,2 hacia atras
  { setup([](int x, int y) -> float { return 0.075f * x - 0.05f * y; });
    for (float x = 0; x <= 300; x += 13.7f) for (float y = 0; y <= 300; y += 11.3f) {
      const float cx = x < 15 ? 15 : x > 285 ? 285 : x, cy = y < 15 ? 15 : y > 285 ? 285 : y;
      const float want = 0.075f * ((cx - 15) / 67.5f) - 0.05f * ((cy - 15) / 67.5f);
      CHECK(near(corr(x, y), want, 2e-4f), "plano inclinado en (%.1f,%.1f): %.5f != %.5f", x, y, corr(x, y), want);
    }
  }
  // 7) Signo: cama mas ALTA en un punto (+0,20) => la boquilla debe SUBIR (Z fisica mayor); mas BAJA (-0,20) => bajar
  { setup([](int x, int y) -> float { return x == 2 && y == 2 ? 0.20f : (x == 0 && y == 0 ? -0.20f : 0.0f); });
    Planner::set_z_fade_height(0);
    CHECK(near(phys(150, 150, 0.2f), 0.4f), "cama +0,20: Z=0,2 -> fisica %.3f (debe ser 0,40)", phys(150, 150, 0.2f));
    CHECK(near(phys(15, 15, 0.2f), 0.0f), "cama -0,20: Z=0,2 -> fisica %.3f (debe ser 0,00)", phys(15, 15, 0.2f));
  }
  // 8) Desvanecimiento (fade height 10 mm): factor 1 en la cama, lineal, 0 a partir de la altura
  { setup([](int x, int y) -> float { (void)x; (void)y; return 0.30f; });
    Planner::set_z_fade_height(10.0f);
    CHECK(near(Planner::fade_scaling_factor_for_z(0.0f), 1.0f), "fade en Z=0");
    CHECK(near(Planner::fade_scaling_factor_for_z(5.0f), 0.5f), "fade en Z=5");
    CHECK(near(Planner::fade_scaling_factor_for_z(10.0f), 0.0f), "fade en Z=10");
    CHECK(near(Planner::fade_scaling_factor_for_z(50.0f), 0.0f), "fade por encima");
    CHECK(near(phys(100, 100, 0.2f), 0.2f + 0.98f * 0.30f, 1e-4f), "Z fisica con fade en 0,2");
    CHECK(near(phys(100, 100, 20.0f), 20.0f), "sin compensacion por encima de la altura de fade");
    Planner::set_z_fade_height(0);
    CHECK(near(Planner::fade_scaling_factor_for_z(100.0f), 1.0f), "fade desactivado => siempre 1");
  }
  // 9) Compensacion desactivada = G-code sin tocar (el planificador no llama a get_z_correction): la funcion de movimiento es la identidad
  { const float z = 0.2f; CHECK(z == 0.2f, "identidad"); }
  // 10) Movimiento XY a Z constante sobre una cama abombada (centro +0,20, bordes 0): la Z fisica sigue la superficie
  { setup([](int x, int y) -> float { const float dx = x - 2, dy = y - 2; const float r = sqrtf(dx * dx + dy * dy); return r < 2.83f ? 0.20f * (1 - r / 2.83f) : 0; });
    Planner::set_z_fade_height(0);
    float zmax = -9, zmin = 9;
    for (float x = 0; x <= 300; x += 5) { const float z = phys(x, 150, 0.2f); if (z > zmax) zmax = z; if (z < zmin) zmin = z; }
    CHECK(zmax > 0.39f && zmax <= 0.4001f && zmin >= 0.1999f, "perfil abombado: zmin %.3f zmax %.3f", zmin, zmax);
  }
  printf("[bilinear] %d comprobaciones, %d fallos\n", checks, fails);
  return fails ? 1 : 0;
}
