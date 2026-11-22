#!/usr/bin/env python3
"""Pruebas ESTATICAS del autoajuste PID (lectura de codigo y configuracion; no ejecutan el firmware)."""
import re, sys, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[2]; M = ROOT / "Marlin"
rd = lambda p: (M / p).read_text(encoding="utf-8")
cfg, temp, ui = rd("Configuration.h"), rd("src/module/temperature.cpp"), "src/lcd/extui/mks_ui/"
fails = checks = 0
def check(cond, msg):
    global fails, checks
    checks += 1; print(("[pid-cfg] OK    " if cond else "[pid-cfg] FALLA ") + msg)
    if not cond: fails += 1
active = lambda n: re.search(r'^\s*#define\s+%s\b' % n, cfg, re.M) is not None

check(active("PIDTEMP"), "PID de hotend activo (PIDTEMP)")
check(active("PIDTEMPBED"), "PID de cama activo (PIDTEMPBED): sin el, M303 E-1 no existe")
check(re.search(r'#define DEFAULT_BED_KP\s+[\d.]+', cfg) and re.search(r'#define DEFAULT_BED_KI\s+[\d.]+', cfg) and re.search(r'#define DEFAULT_BED_KD\s+[\d.]+', cfg), "valores iniciales de PID de cama definidos")
check(not active("PID_FAN_SCALING") and not active("PID_EXTRUSION_SCALING"), "sin escalado de PID por ventilador/extrusion (el resultado de M303 se aplica tal cual)")
check(active("ARQUIS_UI"), "ARQUIS_UI activo")

# Parche minimo de temperature.cpp (todo bajo ARQUIS_UI)
check("arquis_pid_status.h" in temp, "temperature.cpp incluye arquis_pid_status.h")
for name, pat in (("inicio", "arq_pid_begin(ncycles)"), ("ciclo", "arq_pid_cycle(cycles)"), ("exito", "arq_pid_end(ARQ_PID_R_DONE, tune_pid.p, tune_pid.i, tune_pid.d)"),
                  ("timeout", "arq_pid_end(ARQ_PID_R_TIMEOUT"), ("fin sin resultado = cancelado", "arq_pid_finish()"), ("atender pantalla", "arq_pid_idle()")):
    check(pat in temp, "parche: %s" % name)
check(temp.count("arq_pid_end(ARQ_PID_R_TOO_HIGH") == 2, "parche: temperatura demasiado alta en las 2 salidas (objetivo y sobreimpulso)")
check(len(re.findall(r'TERN_\(ARQUIS_UI, arq_pid', temp)) == 8, "el parche son 8 lineas, todas bajo TERN_(ARQUIS_UI, ...) (sin ARQUIS_UI el codigo de Marlin queda identico)")
# El resultado solo se anota antes del 'goto EXIT_M303' y SIN aplicarlo (la orden no lleva U)
i = temp.index("arq_pid_end(ARQ_PID_R_DONE"); check(temp.index("goto EXIT_M303", i) - i < 80, "el resultado se anota justo antes de salir con exito")
# Cancelacion con el mecanismo propio de Marlin
check("while (wait_for_heatup) { // Can be interrupted with M108" in temp, "Marlin: el bucle de M303 termina con wait_for_heatup = false (M108); asi cancela la pantalla")
j = temp.index("while (wait_for_heatup)"); k = temp.index("EXIT_M303:", j)
check("disable_all_heaters();" in temp[j:k], "Marlin: al salir del bucle sin exito apaga los calentadores")
check("disable_all_heaters();" in temp[temp.index("SERIAL_ECHOPGM(STR_PID_AUTOTUNE); SERIAL_ECHOLNPGM(STR_PID_AUTOTUNE_START);"):j], "Marlin: M303 apaga TODOS los calentadores al empezar (la pantalla lo avisa)")
check("_temp_error(heater_id, FPSTR(str_t_heating_failed)" in temp and "_temp_error(heater_id, FPSTR(str_t_thermal_runaway)" in temp, "Marlin: la proteccion termica sigue activa durante el ajuste (WATCH_PID)")
check("hal.idletask();" in temp[j:k] and "ui.update()" in temp[j:k], "Marlin: el bucle no atiende LVGL por si mismo (por eso arq_pid_idle())")
m303 = rd("src/gcode/temp/M303.cpp")
check("parser.boolval('U')" in m303 and "cycles < 3" in m303, "M303: U aplica el resultado y exige >= 3 ciclos")
# Aplicacion del resultado: M301/M304 con la misma convencion que M303 (valores sin escalar)
m301, m304 = rd("src/gcode/config/M301.cpp"), rd("src/gcode/config/M304.cpp")
check("SET_HOTEND_PID(Ki, e, parser.value_float())" in m301 and "thermalManager.updatePID();" in m301, "M301: Ki/Kd sin escalar y actualiza el regulador")
check("pid.set_Ki(parser.value_float())" in m304 and "pid.set_Kd(parser.value_float())" in m304, "M304: Ki/Kd sin escalar")
check("temp_bed.pid.set(in_pid)" in temp and "pid.set(in_pid)" in temp, "M303 U usa set(raw) = la misma convencion que M301/M304")
# EEPROM: PID de cama dentro de los datos guardados
st = rd("src/module/settings.cpp")
check("EEPROM_WRITE(thermalManager.temp_bed.pid)" in st or "bed_pid" in st, "EEPROM: el PID de cama se guarda con M500")
# Integracion ARQUIS
tools, drawui = rd(ui + "arquis_tools.cpp"), rd(ui + "draw_ui.cpp")
check("S_PID" in tools and "arq_pid_draw()" in tools, "Ajustes: tarjeta PID")
check("arq_pid_poll()" in drawui and "arq_pid_clear()" in drawui and "arq_pid_draw()" in drawui, "draw_ui.cpp: poll, dibujo y limpieza de PID_UI")
ctl = rd(ui + "arquis_pid.cpp")
logic = rd(ui + "arquis_pid_logic.cpp")
check('"M303 E%d S%d C%d"' in logic and not re.search(r'M303[^"]*\bU\d?\b', logic), "la orden M303 no lleva U: no aplica nada por si sola")
check("wait_for_heatup = false" in ctl, "cancelar usa wait_for_heatup = false")
check("printing()" in ctl and "arq_mesh_ctl_session_active()" in ctl, "no arranca con impresion en curso ni durante la nivelacion")
check("settings.validate()" in ctl, "tras M500 se valida la EEPROM")
print("[pid-cfg] %d comprobaciones, %d fallos" % (checks, fails)); sys.exit(1 if fails else 0)
