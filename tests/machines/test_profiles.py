#!/usr/bin/env python3
"""Pruebas de los perfiles de maquina (Marlin/machine/*.h): el XQ1 queda IDENTICO a antes y cada perfil solo cambia lo que declara.
Usa el preprocesador de gcc (-dM -E) sobre Configuration.h / Configuration_adv.h REALES; no compila Marlin."""
import re, subprocess, sys, pathlib, tempfile, shutil
ROOT = pathlib.Path(__file__).resolve().parents[2]; M = ROOT / "Marlin"
BASELINE = "c5b5cfd"      # ultimo commit con la configuracion SIN perfiles (XQ1 tal y como estaba)
fails = checks = 0
def check(cond, msg):
    global fails, checks
    checks += 1; print(("[machines] OK    " if cond else "[machines] FALLA ") + msg)
    if not cond: fails += 1

def dump(marlin_dir, defs=()):
    base = ["g++", "-E", "-dM", "-x", "c++", "-I", str(marlin_dir), "-I", str(marlin_dir / "src/core"), "-DSTM32F4", *defs, "-include", str(marlin_dir / "src/core/macros.h")]
    out = {}
    for args in ([str(marlin_dir / "Configuration.h")], ["-include", str(marlin_dir / "Configuration.h"), str(marlin_dir / "Configuration_adv.h")]):
        r = subprocess.run(base + args, capture_output=True, text=True)
        if r.returncode: print(r.stderr[:600]); return None
        for l in r.stdout.splitlines():
            m = re.match(r'#define (\w+)(?:\(.*?\))? ?(.*)$', l)
            if m: out[m.group(1)] = m.group(2).strip()
    return out

xq1 = dump(M); aq = dump(M, ["-DARQUIS_MACHINE_AQUILA_D1"])
# Overrides propios del perfil XQ1 (Marlin/machine/xq1.h): son los unicos valores en que el XQ1 puede diferir de la configuracion anterior
xq1_prof = (M / "machine/xq1.h").read_text(encoding="utf-8")
xq1_declared = {m.group(1): m.group(2).strip() for m in re.finditer(r'^#define\s+(\w+)\s+(.*?)\s*(?://.*)?$', xq1_prof, re.M)}
check(xq1 is not None and aq is not None, "Configuration.h y Configuration_adv.h se preprocesan con y sin perfil")
if xq1 is None or aq is None: sys.exit(1)

# 1) El XQ1 queda identico a la configuracion anterior a los perfiles
tmp = pathlib.Path(tempfile.mkdtemp())
try:
    (tmp / "src").mkdir(); shutil.copytree(M / "src/core", tmp / "src/core")
    ok = True
    for f in ("Configuration.h", "Configuration_adv.h"):
        r = subprocess.run(["git", "-C", str(ROOT), "show", "%s:Marlin/%s" % (BASELINE, f)], capture_output=True, text=True)
        if r.returncode: ok = False; break
        (tmp / f).write_text(r.stdout, encoding="utf-8")
    if not ok: print("[machines] (sin el commit %s en este clon: se omite la comparacion con la configuracion anterior)" % BASELINE)
    else:
        old = dump(tmp)
        diff = {k: (old.get(k), xq1.get(k)) for k in set(old) | set(xq1) if old.get(k) != xq1.get(k) and k not in xq1_declared}
        check(old is not None and not diff, "XQ1 (perfil por defecto): todas las macros iguales a la configuracion anterior salvo las que declara xq1.h %s (%d macros)%s" % (sorted(xq1_declared), len(xq1), "" if not diff else " DIFF: %s" % list(diff.items())[:5]))
        check(all(xq1.get(k) == v for k, v in xq1_declared.items()), "XQ1: cada valor de xq1.h es el que ve Marlin (%d valores)" % len(xq1_declared))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

# 2) Aquila D1: cambia exactamente lo que declara su perfil
prof = (M / "machine/aquila_d1.h").read_text(encoding="utf-8")
declared = {m.group(1): m.group(2).strip() for m in re.finditer(r'^#define\s+(\w+)\s+(.*?)\s*(?://.*)?$', prof, re.M)}
changed = {k for k in set(xq1) | set(aq) if xq1.get(k) != aq.get(k)}
# derivadas de las medidas (X_MAX_POS = X_BED_SIZE...) cambian solas: se aceptan si dependen de algo declarado
derived = {"X_MAX_POS", "Y_MAX_POS", "X_BED_SIZE", "Y_BED_SIZE", "X_CENTER", "Y_CENTER", "X_MAX_BED", "Y_MAX_BED", "X_BED_SIZE"}
unexpected = sorted(k for k in changed if k not in declared and k not in derived and k not in xq1_declared and not k.startswith("ARQUIS_MACHINE"))   # (lo que declara xq1.h solo cambia en el XQ1)
check(not unexpected, "Aquila D1: no cambia nada que su perfil no declare %s" % (unexpected or ""))
check(all(aq.get(k) == v for k, v in declared.items()), "Aquila D1: cada valor del perfil es el que ve Marlin (%d valores)" % len(declared))
# 3) Valores de su Configuration.h original (Aquila_D1.zip)
expected = {"X_BED_SIZE": "230", "Y_BED_SIZE": "240", "Z_MAX_POS": "240", "X_MAX_POS": "X_BED_SIZE", "Y_MAX_POS": "Y_BED_SIZE",   # (X/Y_MAX_POS siguen a la medida de la cama)
           
            "DEFAULT_AXIS_STEPS_PER_UNIT": "{ 160, 160, 803.41, 781.08 }", "DEFAULT_MAX_ACCELERATION": "{ 4500, 3500, 150, 10000 }",
            "INVERT_X_DIR": "false", "INVERT_Y_DIR": "false", "INVERT_E0_DIR": "true",
            "X_MIN_ENDSTOP_INVERTING": "false", "Y_MIN_ENDSTOP_INVERTING": "false"}
for k, v in expected.items(): check(aq.get(k) == v, "Aquila D1: %s = %s (su archivo original)" % (k, v))
check(aq.get("CUSTOM_MACHINE_NAME") == '"Aquila D1"', "Aquila D1: nombre")
for k in ("Z_MIN_ENDSTOP_INVERTING", "DEFAULT_KP", "DEFAULT_BED_KP", "X_CURRENT", "SHAPING_FREQ_X", "PIDTEMPBED", "ENABLE_LEVELING_AFTER_G28", "GRID_MAX_POINTS_X"):
    check(aq.get(k) == xq1.get(k), "Aquila D1 hereda %s del firmware comun (%s)" % (k, aq.get(k)))
check(xq1.get("X_BED_SIZE") == "300" and xq1.get("CUSTOM_MACHINE_NAME") == '"Arquis XQ1"', "XQ1 sigue siendo 300 x 300 'Arquis XQ1'")

# 4) Cada valor de un perfil esta protegido con #ifndef; cada perfil tiene entorno de PlatformIO
cfg = (M / "Configuration.h").read_text(encoding="utf-8") + (M / "Configuration_adv.h").read_text(encoding="utf-8")
for f in sorted((M / "machine").glob("*.h")):
    if f.name in ("machine.h", "_plantilla.h"): continue
    names = re.findall(r'^#define\s+(\w+)', f.read_text(encoding="utf-8"), re.M)
    miss = [n for n in names if not re.search(r'#ifndef %s\b' % n, cfg)]
    check(not miss, "%s: todos sus valores estan protegidos con #ifndef en Configuration*.h %s" % (f.name, miss or ""))
ini = (ROOT / "ini/arquis.ini").read_text(encoding="utf-8"); mh = (M / "machine/machine.h").read_text(encoding="utf-8")
for macro in re.findall(r'defined\((ARQUIS_MACHINE_\w+)\)', mh): check("-D" + macro in ini, "ini/arquis.ini define -D%s" % macro)
check("ini/arquis.ini" in (ROOT / "platformio.ini").read_text(encoding="utf-8"), "platformio.ini incluye ini/arquis.ini")
check('#include "machine/machine.h"' in (M / "Configuration.h").read_text(encoding="utf-8") and '#include "machine/machine.h"' in (M / "Configuration_adv.h").read_text(encoding="utf-8"), "ambos Configuration*.h incluyen el selector")
print("[machines] %d comprobaciones, %d fallos" % (checks, fails)); sys.exit(1 if fails else 0)
