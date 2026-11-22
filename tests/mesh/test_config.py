#!/usr/bin/env python3
"""Pruebas ESTATICAS (lectura de codigo y configuracion; no ejecutan el firmware) de la nivelacion manual 5x5.
Cubren: A (configuracion) y las partes de D (impresion) que dependen de como Marlin esta configurado o escrito."""
import re, sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]
M = ROOT / "Marlin"
cfg = (M / "Configuration.h").read_text(encoding="utf-8")
fails = checks = 0

def active(name):            # #define activo (no comentado) con ese nombre
    return re.search(r'^\s*#define\s+%s\b' % re.escape(name), cfg, re.M) is not None
def value(name):
    m = re.search(r'^\s*#define\s+%s\s+([^\s/]+)' % re.escape(name), cfg, re.M)
    return m.group(1) if m else None
def check(cond, msg):
    global fails, checks
    checks += 1
    print(("[config] OK    " if cond else "[config] FALLA ") + msg)
    if not cond: fails += 1

# ---- A. Configuracion --------------------------------------------------------------------------------------------------
check(active("AUTO_BED_LEVELING_BILINEAR"), "sistema bilineal activo")
check(active("PROBE_MANUALLY"), "PROBE_MANUALLY activo (sin sonda fisica)")
grids = re.findall(r'^\s*#define\s+GRID_MAX_POINTS_X\s+(\d+)', cfg, re.M)
check("5" in grids and not any(g != "5" for g in grids[:1]), "GRID_MAX_POINTS_X = 5 en la seccion ABL (%s)" % grids)
check(re.search(r'#define GRID_MAX_POINTS_Y GRID_MAX_POINTS_X', cfg) is not None, "GRID_MAX_POINTS_Y = GRID_MAX_POINTS_X (malla 5x5 = 25 puntos)")
for forbidden in ("BLTOUCH", "FIX_MOUNTED_PROBE", "NOZZLE_AS_PROBE", "Z_PROBE_SLED", "Z_PROBE_ALLEN_KEY", "SOLENOID_PROBE", "BD_SENSOR", "TOUCH_MI_PROBE",
                  "AUTO_BED_LEVELING_UBL", "MESH_BED_LEVELING", "AUTO_BED_LEVELING_LINEAR", "AUTO_BED_LEVELING_3POINT", "ABL_BILINEAR_SUBDIVISION", "EXTRAPOLATE_BEYOND_GRID"):
    check(not active(forbidden), "%s NO esta activo" % forbidden)
check(active("EEPROM_SETTINGS"), "EEPROM_SETTINGS activo")
check(active("ENABLE_LEVELING_FADE_HEIGHT") and value("DEFAULT_LEVELING_FADE_HEIGHT") == "10.0", "fade height 10 mm")
check(active("SEGMENT_LEVELED_MOVES") and value("LEVELED_SEGMENT_LENGTH") == "5.0", "movimientos segmentados cada 5 mm (compensacion durante XY)")
check(active("ENABLE_LEVELING_AFTER_G28") and not active("RESTORE_LEVELING_AFTER_G28"), "compensacion activa tras G28 (ENABLE_LEVELING_AFTER_G28)")
check(re.search(r'#define NOZZLE_TO_PROBE_OFFSET \{ 0, 0, 0 \}', cfg) is not None, "NOZZLE_TO_PROBE_OFFSET = 0,0,0 (la boquilla es el punto de medicion)")
margin = int(value("PROBING_MARGIN"))
bed_x, bed_y = int(value("X_BED_SIZE")), int(value("Y_BED_SIZE"))
check((bed_x, bed_y) == (300, 300), "cama 300x300")
check(0 < margin < 40, "PROBING_MARGIN = %d mm (area de malla %d..%d mm, paso %.2f mm)" % (margin, margin, bed_x - margin, (bed_x - 2 * margin) / 4))
start_z = float(value("MANUAL_PROBE_START_Z"))
check(0.5 <= start_z <= 2.0, "altura inicial de medicion MANUAL_PROBE_START_Z = %.1f mm (segura y razonable)" % start_z)
check(int(value("Z_CLEARANCE_BETWEEN_PROBES")) >= 3, "altura de desplazamiento entre puntos >= 3 mm")
check(int(value("Z_MAX_POS")) > 10, "Z_MAX_POS permite las alturas del asistente")
check(not re.search(r'^\s*#define\s+LCD_BED_LEVELING\b', cfg, re.M), "LCD_BED_LEVELING sigue desactivado (incompatible con TFT_LVGL_UI)")
check(active("ARQUIS_UI"), "ARQUIS_UI activo")

# ---- D. Comportamiento de Marlin que la integracion supone (lectura del codigo real) ------------------------------------
g28 = (M / "src/gcode/calibrate/G28.cpp").read_text(encoding="utf-8")
check("parser.boolval('L', TERN1(RESTORE_LEVELING_AFTER_G28, planner.leveling_active))" in g28 and "if (leveling_restore_state) set_bed_leveling_enabled()" in g28,
      "G28: con ENABLE_LEVELING_AFTER_G28 activa la compensacion al terminar y 'G28 L0' la deja apagada")
check("set_bed_leveling_enabled(false)" in g28 or "TEMPORARY_BED_LEVELING_STATE" in g28 or "planner.leveling_active" in g28, "G28 apaga la compensacion mientras homea")
check("TERN_(PROBE_MANUALLY, g29_in_progress = false);" in g28, "G28 cancela un G29 manual en curso")
bl = (M / "src/feature/bedlevel/bedlevel.cpp").read_text(encoding="utf-8")
check("!enable || leveling_is_valid()" in bl, "set_bed_leveling_enabled(true) NO activa una malla inexistente")
st = (M / "src/module/settings.cpp").read_text(encoding="utf-8")
check("const bool ubl_active = TERN(AUTO_BED_LEVELING_UBL, planner.leveling_active, false);" in st,
      "EEPROM: con BILINEAR no se guarda si la compensacion estaba activa -> tras reiniciar queda apagada (por eso ENABLE_LEVELING_AFTER_G28)")
check("if (!validating) set_bed_leveling_enabled(false);" in st and "EEPROM_READ(bedlevel.z_values);" in st, "EEPROM: la malla bilineal se guarda y se recupera (z_values + rejilla)")
check("grid_max_x == (GRID_MAX_POINTS_X) && grid_max_y == (GRID_MAX_POINTS_Y)" in st, "EEPROM: una malla de otro tamano (3x3 antigua) se descarta como obsoleta")
check("EEPROM_WRITE(bedlevel.z_values);" in st, "EEPROM: M500 escribe la malla")
planner = (M / "src/module/planner.cpp").read_text(encoding="utf-8")
check("raw.z += fade_scaling_factor * bedlevel.get_z_correction(raw);" in planner, "planificador: Z_fisica = Z + fade * correccion (convencion de signos)")
setcpp = (M / "src/module/settings.h").read_text(encoding="utf-8")
check("static bool validate();" in setcpp and "static bool load();" in setcpp, "settings.validate()/load() existen (el asistente valida antes de cargar)")

# ---- Integracion ARQUIS ----------------------------------------------------------------------------------------------------
tools = (M / "src/lcd/extui/mks_ui/arquis_tools.cpp").read_text(encoding="utf-8")
check("arq_mesh_draw()" in tools and "#if ARQ_HAS_MESH" in tools, "Herramientas: la tarjeta 'Malla 5x5' abre el asistente")
check("AUTO_BED_LEVELING_BILINEAR) && !ARQ_HAS_MESH" in tools, "ya no se inyecta el G29 guardado en la placa (no hay sonda)")
check('queue.inject(F("G28L0"))' in tools, "tornillos (5 puntos): Home sin activar la compensacion")
ui = (M / "src/lcd/extui/mks_ui/draw_ui.cpp").read_text(encoding="utf-8")
check("arq_mesh_poll();" in ui and "arq_mesh_clear(); break;" in ui and "arq_mesh_draw(); break;" in ui, "draw_ui.cpp: poll, dibujo y limpieza de MESHLEVELING_UI")
ctl = (M / "src/lcd/extui/mks_ui/arquis_mesh.cpp").read_text(encoding="utf-8")
literals = " ".join(re.findall(r'"(?:[^"\\]|\\.)*"', ctl))      # solo cadenas: lo que se inyectaria como G-code
for bad in ("G29", "M420", "M421"):
    check(bad not in literals, "el controlador no inyecta %s" % bad)
check("reset_bed_level" not in re.sub(r'//[^\n]*', '', ctl), "el controlador no llama a reset_bed_level")
check(len(re.findall(r'bedlevel\.z_values\[[^\]]+\]\[[^\]]+\]\s*=', ctl)) == 1, "solo hay UNA escritura de la malla de Marlin (arq_mesh_ctl_commit)")
body = ctl[ctl.index("uint8_t arq_mesh_ctl_load()"):]
check(body.index("settings.validate()") < body.index("if (!settings.load())"), "cargar: se valida la EEPROM antes de settings.load() (que reiniciaria todo si falla)")
print("[config] %d comprobaciones, %d fallos" % (checks, fails))
sys.exit(1 if fails else 0)
