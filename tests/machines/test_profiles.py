#!/usr/bin/env python3
"""Firmware UNICO XQ1 / Aquila D1 (perfiles en tiempo de ejecucion). Comprueba, sin compilar Marlin:
 1) que la TABLA de perfiles de fabrica (arquis_machine_logic.cpp) coincide con la configuracion real de cada maquina tal y
    como se compilaba antes (Configuration.h / Configuration_adv.h + Marlin/machine/xq1.h o aquila_d1.h, preprocesador de gcc);
 2) que la configuracion comun no cambia salvo lo previsto (logica de finales en tiempo de ejecucion);
 3) las restricciones: stepper.cpp, planificador, endstops.cpp, Input Shaping y malla sin tocar; USB solo CDC; un solo entorno."""
import re, subprocess, sys, pathlib, tempfile, shutil
ROOT = pathlib.Path(__file__).resolve().parents[2]; M = ROOT / "Marlin"
BASE = "a98dd6ab50ae2a596344a789c344d9f81be08b14"   # ultimo commit con perfiles de compilacion (antes del firmware unico)
fails = checks = 0
def check(cond, msg):
    global fails, checks
    checks += 1; print(("[machines] OK    " if cond else "[machines] FALLA ") + msg)
    if not cond: fails += 1

def dump(mdir, defs=()):
    base = ["g++", "-E", "-dM", "-x", "c++", "-I", str(mdir), "-I", str(M / "src/core"), "-DSTM32F4", *defs, "-include", str(M / "src/core/macros.h")]
    out = {}
    for args in ([str(mdir / "Configuration.h")], ["-include", str(mdir / "Configuration.h"), str(mdir / "Configuration_adv.h")]):
        r = subprocess.run(base + args, capture_output=True, text=True)
        if r.returncode: print(r.stderr[:800]); return None
        for l in r.stdout.splitlines():
            m = re.match(r'#define (\w+)(?:\(.*?\))? ?(.*)$', l)
            if m: out[m.group(1)] = m.group(2).strip()
    return out

def nums(s): return [float(x) for x in re.findall(r'-?\d+(?:\.\d+)?', s or "")]

# ---- 1) Configuracion antigua por maquina: arbol temporal con el selector de compilacion de antes ----
tmp = pathlib.Path(tempfile.mkdtemp())
try:
    (tmp / "machine").mkdir()
    for f in ("Configuration.h", "Configuration_adv.h"): shutil.copy(M / f, tmp / f)
    for f in ("xq1.h", "aquila_d1.h"): shutil.copy(M / "machine" / f, tmp / "machine" / f)
    (tmp / "machine/machine.h").write_text('#pragma once\n#if defined(ARQUIS_MACHINE_AQUILA_D1)\n  #include "aquila_d1.h"\n#else\n  #include "xq1.h"\n#endif\n')
    old = {"xq1": dump(tmp), "aq": dump(tmp, ["-DARQUIS_MACHINE_AQUILA_D1"])}
finally:
    shutil.rmtree(tmp, ignore_errors=True)
adv = (M / "Configuration_adv.h").read_text(encoding="utf-8")
def adv_val(name):
    m = re.search(r'#define\s+%s\s+(\S+)' % name, adv); return float(nums(m.group(1))[0]) if m else None

check(all(old.values()), "configuracion de cada maquina reconstruida con el preprocesador")
if not all(old.values()): sys.exit(1)

# Tabla de perfiles del firmware
src = (M / "src/lcd/extui/mks_ui/arquis_machine_logic.cpp").read_text(encoding="utf-8")
body = src[src.index("profiles[ARQ_MC_NPROF] = {"):src.index("};", src.index("profiles[ARQ_MC_NPROF] = {"))]
entries = re.findall(r'\{\s*"([^"]+)",\s*"[^"]+",(.*?)\{(.*?)\},\s*(.*?)\}', body, re.S)
check(len(entries) == 2, "la tabla tiene 2 perfiles (%d)" % len(entries))
BITS = {"ARQ_MC_BX": 1, "ARQ_MC_BY": 2, "ARQ_MC_BZ": 4, "ARQ_MC_BE": 8}
def mask(expr): return sum(BITS[b] for b in re.findall(r'ARQ_MC_B[XYZE]', expr))
table = {}
for name, masks, vals, _ in entries:
    m = [x.split("//")[0] for x in masks.strip().strip(",").split("\n") if x.strip()]
    es, dr = mask(m[0]), mask(m[1])
    v = nums(re.sub(r'//[^\n]*', '', vals))
    table[name] = (es, dr, v)
for key, name in (("xq1", "Arquis XQ1"), ("aq", "Aquila D1")):
    c = old[key]; es, dr, v = table[name]
    check(c["CUSTOM_MACHINE_NAME"] == '"%s"' % name, "%s: nombre" % name)
    exp = [float(c["X_BED_SIZE"]), float(c["Y_BED_SIZE"]), float(c["Z_MAX_POS"])] + nums(c["DEFAULT_AXIS_STEPS_PER_UNIT"]) \
        + nums(c["DEFAULT_MAX_FEEDRATE"]) + nums(c["DEFAULT_MAX_ACCELERATION"]) \
        + [float(c["DEFAULT_ACCELERATION"]), float(c["DEFAULT_RETRACT_ACCELERATION"]), float(c["DEFAULT_TRAVEL_ACCELERATION"]), float(c["JUNCTION_DEVIATION_MM"])] \
        + [adv_val("X_CURRENT"), adv_val("Y_CURRENT"), adv_val("Z_CURRENT"), adv_val("E0_CURRENT")] \
        + [float(nums(c["SHAPING_FREQ_X"])[0]), float(nums(c["SHAPING_FREQ_Y"])[0]), 0, 0, 0]
    check(len(v) == len(exp) and all(abs(a - b) < 1e-6 for a, b in zip(v, exp)),
          "%s: valores de fabrica = configuracion real (medidas, pasos, velocidades, aceleraciones, JD, corrientes, Shaping)%s" % (name, "" if v == exp else " tabla=%s real=%s" % (v, exp)))
    es_real = (c["X_MIN_ENDSTOP_INVERTING"] == "true") * 1 | (c["Y_MIN_ENDSTOP_INVERTING"] == "true") * 2 | (c["Z_MIN_ENDSTOP_INVERTING"] == "true") * 4
    dr_real = (c["INVERT_X_DIR"] == "true") * 1 | (c["INVERT_Y_DIR"] == "true") * 2 | (c["INVERT_Z_DIR"] == "true") * 4 | (c["INVERT_E0_DIR"] == "true") * 8
    check(es == es_real, "%s: logica de finales X/Y/Z = %s (configuracion: %d)" % (name, es, es_real))
    check(dr == dr_real, "%s: direcciones X/Y/Z/E0 = %s (configuracion: %d)" % (name, dr, dr_real))
check("P(ARQ_MP_FEED_Z) | P(ARQ_MP_ACC_Z)" in src, "XQ1: Z 5 mm/s y 100 mm/s2 marcados como provisionales (pendientes de validacion fisica)")
# Lo comun a las dos maquinas (no esta en la tabla porque no cambia)
for k in ("TEMP_SENSOR_0", "TEMP_SENSOR_BED", "HEATER_0_MAXTEMP", "BED_MAXTEMP", "DEFAULT_KP", "DEFAULT_BED_KP", "MOTHERBOARD", "X_DRIVER_TYPE", "E0_DRIVER_TYPE",
          "THERMAL_PROTECTION_PERIOD", "THERMAL_PROTECTION_BED_PERIOD", "GRID_MAX_POINTS_X", "PROBING_MARGIN", "X_HOME_DIR", "Z_HOME_DIR"):
    check(old["xq1"].get(k) == old["aq"].get(k), "comun a las dos: %s = %s" % (k, old["xq1"].get(k)))

# ---- 2) Configuracion del firmware unico ----
new = dump(M)
check(new is not None, "Configuration.h / Configuration_adv.h del firmware unico se preprocesan")
if new is None: sys.exit(1)
changed = sorted(k for k in set(new) | set(old["xq1"]) if new.get(k) != old["xq1"].get(k))
expected = {"X_MIN_ENDSTOP_INVERTING", "Y_MIN_ENDSTOP_INVERTING", "Z_MIN_ENDSTOP_INVERTING",      # en tiempo de ejecucion
            "DEFAULT_MAX_FEEDRATE", "DEFAULT_MAX_ACCELERATION"}                                  # (xq1.h ya no se incluye: valores de compilacion comunes)
check(set(changed) <= expected, "solo cambian las macros previstas respecto al XQ1 anterior %s" % (sorted(set(changed) - expected) or ""))
check(new.get("X_MIN_ENDSTOP_INVERTING") == "((arq_mc_es_inv & 0x01) != 0)", "logica del final X en tiempo de ejecucion")
check(new.get("X_BED_SIZE") == "300" and new.get("Z_MAX_POS") == "450", "envolvente de compilacion = maquina mas grande (300 x 300 x 450)")
check(new.get("INVERT_X_DIR") == "true" and new.get("INVERT_E0_DIR") == "false", "direcciones compiladas = XQ1 (el Aquila invierte con GCONF.shaft)")

# ---- 3) Restricciones ----
def unchanged(path):
    r = subprocess.run(["git", "-C", str(ROOT), "diff", "--quiet", BASE, "--", path]); return r.returncode == 0
for p in ("Marlin/src/module/stepper.cpp", "Marlin/src/module/stepper.h", "Marlin/src/module/planner.cpp", "Marlin/src/module/planner.h",
          "Marlin/src/module/endstops.cpp", "Marlin/src/module/endstops.h", "Marlin/src/module/stepper/indirection.h",
          "Marlin/src/gcode/feature/input_shaping/M593.cpp", "Marlin/src/feature/bedlevel", "Marlin/src/lcd/extui/mks_ui/arquis_mesh.cpp",
          "Marlin/src/lcd/extui/mks_ui/arquis_mesh_logic.cpp", "Marlin/src/lcd/extui/mks_ui/arquis_pid.cpp", "Marlin/src/lcd/extui/mks_ui/arquis_perf.cpp",
          "Marlin/src/lcd/extui/mks_ui/wifi_module.cpp", "Marlin/src/module/temperature.cpp"):
    check(unchanged(p), "sin cambios respecto a %s: %s" % (BASE[:7], p))
ini = (ROOT / "ini/arquis.ini").read_text(encoding="utf-8"); f4 = (ROOT / "ini/stm32f4.ini").read_text(encoding="utf-8")
check(not re.search(r"^\[env:", ini, re.M), "ini/arquis.ini: ya no hay entorno por maquina (un solo BIN)")
check("-DUSBD_USE_CDC_MSC" not in f4.split("[env:mks_robin_nano_v3_1_usb_flash_drive_msc]")[1].split("[env:")[0] and "-DUSBHOST" in f4.split("[env:mks_robin_nano_v3_1_usb_flash_drive_msc]")[1].split("[env:")[0],
      "entorno del BIN: sin MSC y quitando USB host (build_unflags)")
st = (M / "src/module/settings.cpp").read_text(encoding="utf-8")
check("static_assert(EEPROM_OFFSET + sizeof(SettingsData) <= ARQ_MC_EE_A" in st, "settings.cpp comprueba que los datos de Marlin no llegan al registro ARQUIS")
check("arq_mc_boot_load()" in st and "arq_mc_on_reset()" in st, "settings.cpp carga el registro y aplica la mecanica del perfil en reset()")
tr = (M / "src/module/stepper/trinamic.cpp").read_text(encoding="utf-8")
check("gconf.shaft = arq_mc_shaft_bit(AXIS_ID)" in tr, "trinamic.cpp: sentido del modelo en GCONF.shaft (TMC2209)")
print("[machines] %d comprobaciones, %d fallos" % (checks, fails)); sys.exit(1 if fails else 0)
