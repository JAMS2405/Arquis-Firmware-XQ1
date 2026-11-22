#!/usr/bin/env python3
"""Pruebas ESTATICAS de la pantalla Rendimiento (lectura de codigo y configuracion; no ejecutan el firmware).
Comprueban que los limites de la pantalla coinciden con los que Marlin 2.1.2.10 aplica de verdad y que la integracion esta completa."""
import re, sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]; M = ROOT / "Marlin"
rd = lambda p: (M / p).read_text(encoding="utf-8")
cfg, adv, ui = rd("Configuration.h"), rd("Configuration_adv.h"), "src/lcd/extui/mks_ui/"
fails = checks = 0
def check(cond, msg):
    global fails, checks
    checks += 1; print(("[perf-cfg] OK    " if cond else "[perf-cfg] FALLA ") + msg)
    if not cond: fails += 1
active = lambda src, n: re.search(r'^\s*#define\s+%s\b' % n, src, re.M) is not None

# --- Funciones de Marlin que la pantalla maneja deben estar activas (si no, la tarjeta sale deshabilitada, no falla) ---
check(active(adv, "LIN_ADVANCE"), "Linear Advance activo (M900 K)")
check(active(adv, "INPUT_SHAPING_X") and active(adv, "INPUT_SHAPING_Y"), "Input Shaping activo en X e Y (M593)")
check(not active(cfg, "CLASSIC_JERK") and active(cfg, "JUNCTION_DEVIATION_MM"), "Junction Deviation en uso (M205 J), no el jerk clasico")

# --- Limites de la pantalla frente a los de Marlin ---
logic = rd(ui + "arquis_perf_logic.cpp")
m205 = rd("src/gcode/config/M200-M205.cpp")
check("WITHIN(junc_dev, 0.01f, 0.3f)" in m205, "Marlin: M205 J acepta 0.01..0.3")
check(re.search(r'ARQ_PF_JD:.*p\.min = 0\.01f;\s+p\.max = 0\.3f', logic) is not None, "pantalla: JD limitada a 0.01..0.3 (igual que Marlin)")
m900 = rd("src/gcode/feature/advance/M900.cpp")
check("(0-10)" in m900 and re.search(r'ARQ_PF_K:.*p\.max = 2\.0f', logic) is not None, "pantalla: K <= 2.0, dentro del 0..10 de M900")
m593 = rd("src/gcode/feature/input_shaping/M593.cpp")
check("freq == 0.0f || freq > min_freq" in m593, "Marlin: M593 F acepta 0 (apagado) o por encima de un minimo diminuto")
stepper_h = rd("src/module/stepper.h"); stepper_c = rd("src/module/stepper.cpp")
check(re.search(r'^\s*constexpr uint16_t shaping_min_freq = SHAPING_MIN_FREQ', stepper_h, re.M) is not None, "stepper.h: shaping_min_freq existe a nivel global (la pantalla la usa)")
check("shaping_echoes = max_step_rate / shaping_min_freq / 2 + 3" in stepper_h, "stepper.h: el buffer de Input Shaping se dimensiona con shaping_min_freq (por debajo de ella se desborda)")
set_f = stepper_c[stepper_c.index("void Stepper::set_shaping_frequency"):]
check("planner.synchronize();" in set_f[:400] and "ShapingQueue::set_delay" in set_f[:900], "Marlin: set_shaping_frequency sincroniza el planificador (por eso solo se edita en reposo)")
check("(float)shaping_min_freq" in rd(ui + "arquis_perf.cpp"), "el controlador pasa a la logica el minimo compilado (shaping_min_freq)")
check(re.search(r'ARQ_PF_FX:.*p\.min = shaping_min', logic) and re.search(r'ARQ_PF_FY:.*p\.min = shaping_min', logic), "FX y FY usan shaping_min como minimo")
check("M204 P" in logic and "M204 T" in logic and "M201 X" in logic and "M201 Y" in logic and "M900 K" in logic and "M205 J" in logic and "M593 X F" in logic and "M593 Y F" in logic, "las 8 ordenes de Marlin estan en la tabla")
check("parser.seenval('P')" in m205 and "parser.seenval('T')" in m205, "Marlin: M204 admite P y T")
check("planner.set_max_acceleration(a, parser.value_axis_units(a))" in m205, "Marlin: M201 admite X/Y por separado")

# --- Lo que se guarda con M500 (la pantalla promete que Guardar conserva estos valores) ---
st = rd("src/module/settings.cpp")
check("EEPROM_WRITE(planner.settings);" in st and re.search(r'float\s+acceleration,.*?travel_acceleration', rd("src/module/planner.h"), re.S) and "max_acceleration_mm_per_s2[DISTINCT_AXES]" in rd("src/module/planner.h"),
      "EEPROM: planner.settings completo (aceleraciones M204 y aceleracion maxima M201) se guarda con M500")
for what, pat in (
                  ("Junction Deviation", "planner.junction_deviation_mm"), ("Linear Advance K", "EEPROM_WRITE(planner.extruder_advance_K)"),
                  ("shaping X", "EEPROM_WRITE(stepper.get_shaping_frequency(X_AXIS))"), ("shaping Y", "EEPROM_WRITE(stepper.get_shaping_frequency(Y_AXIS))")):
    check(pat in st, "EEPROM: %s se guarda con M500" % what)

# --- Controlador ---
ctl = rd(ui + "arquis_perf.cpp")
check("gcode.process_subcommands_now" in ctl and not re.search(r'planner\.settings\.\w+\s*=[^=]|extruder_advance_K\[\d\]\s*=[^=]|junction_deviation_mm\s*=[^=]|set_shaping_frequency', ctl),
      "todo cambio pasa por una orden de Marlin (que valida); el controlador no escribe variables de Marlin")
check("arq_pf_equal(id, read(id), v)" in ctl, "tras cada orden se comprueba que Marlin tomo el valor")
check("printing()" in ctl and "queue.has_commands_queued() || planner.has_blocks_queued()" in ctl, "solo se edita sin impresion y con la maquina quieta")
check("arq_pid_ctl_busy()" in ctl and "arq_mesh_ctl_session_active()" in ctl, "no se edita durante el autoajuste PID ni la medicion de la malla")
check("settings.save() || !settings.validate()" in ctl.replace("if (!settings.save() || !settings.validate())", "settings.save() || !settings.validate()"), "tras M500 se valida la EEPROM")
check(len(re.findall(r'#if\s+ENABLED\(LIN_ADVANCE\)|#if\s+DISABLED\(CLASSIC_JERK\)|#if\s+ENABLED\(INPUT_SHAPING_[XY]\)', ctl)) >= 4, "las lecturas de K, JD y shaping estan bajo su opcion de Marlin (compila aunque se desactiven)")

# --- Integracion ARQUIS ---
tools, drawui, drawh = rd(ui + "arquis_tools.cpp"), rd(ui + "draw_ui.cpp"), rd(ui + "draw_ui.h")
check("S_PERF" in tools and "arq_perf_draw()" in tools and '"Rendimiento"' in tools, "Ajustes: tarjeta Rendimiento")
check("arq_perf_clear()" in drawui and "arq_perf_draw()" in drawui, "draw_ui.cpp: dibujo y limpieza de PERF_UI")
check("PERF_UI" in drawh, "draw_ui.h: PERF_UI en DISP_STATE")
# La rejilla de Ajustes admite ARQ_OPT_MAX tarjetas: una de mas se pierde en silencio (la ultima, que es Apagar)
opt_max = int(re.search(r'#define ARQ_OPT_MAX\s+(\d+)', rd(ui + "arquis_ctrl_view.h")).group(1))
blk = tools[tools.index("void arq_settings_draw()"):tools.index("void arq_settings_clear()")]
n = 0
for m in re.finditer(r'grid_settings\.add\((S_\w+)', blk):
    tag = m.group(1)
    if tag == "S_LANG" and not re.search(r'define HAS_LANG_SELECT_SCREEN', rd("src/pins/stm32f4/pins_MKS_ROBIN_NANO_V3.h")): continue
    if tag == "S_WIFI" and not active(cfg, "MKS_WIFI_MODULE"): continue
    n += 1
check(n <= opt_max, "Ajustes: %d tarjetas activas en esta configuracion, caben (maximo %d)" % (n, opt_max))
view = rd(ui + "arquis_perf_view.cpp")
check(len(re.findall(r'arq_card_create|arq_button_create|arq_chip_create|arq_header_create', view)) <= 9, "la vista crea pocos objetos (heap de LVGL de 16 KB)")
# Sin el modulo, nada de Marlin cambia: la pantalla es solo codigo ARQUIS
check(not (M / "src/module/planner.cpp").read_text(encoding="utf-8").count("arq_perf"), "no se toca el nucleo de Marlin (planner.cpp)")
print("[perf-cfg] %d comprobaciones, %d fallos" % (checks, fails)); sys.exit(1 if fails else 0)
