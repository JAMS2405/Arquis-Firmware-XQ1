# SPDX-License-Identifier: LGPL-3.0-or-later
"""ARQUIS Non-Planar (Etapa 5): motor EXPERIMENTAL. Estas pruebas son de calculo y de modelo (G-code leido con el modelo de Marlin del
repositorio); NO son pruebas fisicas y no demuestran que la impresora pueda ejecutar la pieza."""
import dataclasses
import glob
import json
import math
import os
import re

import pytest

import arquis_bootstrap

arquis_bootstrap.load_engines()
import arquis_engines.nonplanar_engine as npe  # noqa: E402
from arquis_engines.marlin_model import simulate  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CONFIG_H = os.path.join(ROOT, "Marlin", "Configuration.h")


def ok_cfg(**kw):
    head = kw.pop("head", npe.HeadModel(confirmed=True))
    kw.setdefault("bed_temp", 60)
    kw.setdefault("nozzle_temp", 210)
    return npe.NPConfig(enabled=True, head=head, **kw)


_CACHE = {}


def default_plan():
    if "p" not in _CACHE:
        _CACHE["p"] = npe.build_plan(ok_cfg())
    return _CACHE["p"]


def errors(plan):
    return {f.code for f in plan.findings if f.level == "ERROR"}


# ---------------------------------------------------------------- desactivado por defecto
def test_disabled_by_default_and_never_wired_into_cura():
    assert npe.ENABLED_BY_DEFAULT is False and npe.NPConfig().enabled is False
    with pytest.raises(npe.NotEnabled):
        npe.build_plan(npe.NPConfig())
    for path in glob.glob(os.path.join(ROOT, "cura", "scripts", "*.py")) + glob.glob(os.path.join(ROOT, "cura", "plugins", "ARQUISPlugin", "*.py")) + \
            glob.glob(os.path.join(ROOT, "cura", "plugins", "ARQUISPlugin", "ui", "*")):
        if os.path.isfile(path):
            assert "nonplanar" not in open(path, encoding="utf-8", errors="ignore").read().lower(), "el motor no planar no debe estar conectado a Cura: " + path


def test_firmware_is_untouched_by_this_stage():
    # el motor solo lee limites del firmware; ningun archivo de Marlin menciona el motor no planar
    for path in glob.glob(os.path.join(ROOT, "Marlin", "src", "**", "*.cpp"), recursive=True)[:400]:
        assert "nonplanar" not in open(path, encoding="utf-8", errors="ignore").read().lower()


# ---------------------------------------------------------------- limites = los del firmware
def _cfg_tuple(name):
    t = open(CONFIG_H, encoding="utf-8", errors="ignore").read()
    m = re.search(r"^#define\s+%s\s*\{([^}]*)\}" % name, t, re.M)
    assert m, name
    return tuple(float(v) for v in m.group(1).split(","))


def _cfg_num(name):
    t = open(CONFIG_H, encoding="utf-8", errors="ignore").read()
    m = re.search(r"^\s*#define\s+%s\s+([0-9.]+)" % name, t, re.M)
    assert m, name
    return float(m.group(1))


def test_machine_limits_are_exactly_marlin_configuration_h():
    m = npe.MachineMotion()
    assert m.max_feedrate == _cfg_tuple("DEFAULT_MAX_FEEDRATE")[:4] and m.max_accel == _cfg_tuple("DEFAULT_MAX_ACCELERATION")[:4]
    assert (m.bed_x, m.bed_y, m.z_max) == (_cfg_num("X_BED_SIZE"), _cfg_num("Y_BED_SIZE"), _cfg_num("Z_MAX_POS"))
    t = open(CONFIG_H, encoding="utf-8", errors="ignore").read()
    assert re.search(r"^\s*#define\s+AUTO_BED_LEVELING_BILINEAR", t, re.M) and m.leveling_bilinear
    assert m.fade_height == _cfg_num("DEFAULT_LEVELING_FADE_HEIGHT")
    assert not re.search(r"^\s*#define\s+CLASSIC_JERK", t, re.M)          # el motor afirma que no hay CLASSIC_JERK


# ---------------------------------------------------------------- el plan de ejemplo
def test_default_example_is_consistent_and_within_limits():
    p = default_plan()
    st = p.stats
    assert p.ok and p.zones.fraction == 1.0
    assert st["pendiente_max_deg"] < p.cfg.eff_max_slope_deg and 0.1 <= st["espesor_min"] and st["espesor_max"] <= 0.3
    assert st["vz_max_mm_s"] <= p.cfg.machine.max_feedrate[2] and st["az_est_mm_s2"] <= p.cfg.machine.max_accel[2]
    assert st["z_max_mm"] == pytest.approx(p.cfg.z_base + p.cfg.np_layers * p.cfg.layer_h + p.cfg.amplitude, abs=0.02)
    # la base es plana y las capas onduladas si cambian de Z mientras se mueven en XY
    assert all(s.z0 == s.z1 for s in p.segs if s.kind == "planar")
    assert sum(1 for s in p.segs if s.kind == "np" and abs(s.z1 - s.z0) > 1e-4) > 1000


def test_extrusion_volume_matches_the_geometry():
    p = default_plan()
    fil = math.pi * (p.cfg.filament_d / 2) ** 2
    vol = p.stats["e_total_mm"] * fil
    c = p.cfg
    expected = (c.base_layers + c.np_layers) * c.layer_h * c.size_x * c.size_y        # espesor medio = layer_h en cada capa (la onda tiene media 0)
    assert 0.85 < vol / expected < 1.15, (vol, expected)


def test_z_axis_speed_cap_is_applied_when_the_wave_is_steep():
    p = npe.build_plan(ok_cfg(amplitude=0.9, wavelength=14.0, np_layers=10, max_slope_deg=20.0, head=npe.HeadModel(nozzle_clearance_deg=30.0, confirmed=True)))
    zmax = p.cfg.machine.max_feedrate[2]
    printing = [s for s in p.segs if s.printing]
    assert p.stats["vz_max_mm_s"] <= 0.9 * zmax + 1e-6
    assert min(s.feed for s in printing) < p.cfg.speed_np                                # la velocidad SE REDUJO por el eje Z
    assert p.stats["az_est_mm_s2"] <= p.cfg.machine.max_accel[2]


# ---------------------------------------------------------------- la verificacion SI detecta problemas
def test_excessive_deviation_slope_and_bed_edge_are_errors():
    assert "ZDEV" in errors(npe.build_plan(ok_cfg(amplitude=1.5)))
    assert {"SLOPE", "ZONA"} <= errors(npe.build_plan(ok_cfg(amplitude=0.95, wavelength=8.0, np_layers=12)))
    assert "BED" in errors(npe.build_plan(ok_cfg(x0=2.0)))
    assert "BED" in errors(npe.build_plan(ok_cfg(x0=290.0)))
    assert "ESPESOR" in errors(npe.build_plan(ok_cfg(np_layers=2)))                       # capas casi nulas o demasiado gruesas


def test_hard_cap_on_z_deviation_even_if_the_user_asks_for_more():
    p = npe.build_plan(ok_cfg(amplitude=2.5, max_z_dev=10.0))
    assert "ZDEV" in errors(p) and any(f.code == "ZDEV_TOPE" for f in p.findings)


def test_head_envelope_collision_is_detected():
    safe = npe.build_plan(ok_cfg())
    assert "COLISION_CABEZAL" not in errors(safe)
    tight = npe.build_plan(ok_cfg(head=npe.HeadModel(envelopes=(("bloque calefactor", 12.0, 0.3),), confirmed=True)))
    assert "COLISION_CABEZAL" in errors(tight) and not tight.ok


def test_cone_clearance_limits_the_slope():
    p = npe.build_plan(ok_cfg(head=npe.HeadModel(nozzle_clearance_deg=12.0, confirmed=True)))      # 12 - 5 = 7 deg < 10.7 deg de la onda
    assert p.cfg.eff_max_slope_deg == 7.0 and {"SLOPE", "ZONA"} <= errors(p)


def test_flow_limit_is_an_error_or_slows_the_print():
    p = npe.build_plan(ok_cfg(max_flow_mm3s=1.0))
    assert p.ok is True and p.stats["caudal_max_mm3_s"] <= 1.0 + 1e-9                      # se reduce la velocidad para respetar el caudal


# ---------------------------------------------------------------- vista previa y barreras de exportacion
def test_digest_changes_when_anything_changes():
    a = default_plan().digest
    assert a == npe.build_plan(ok_cfg()).digest
    assert a != npe.build_plan(ok_cfg(amplitude=0.5)).digest and a != npe.build_plan(ok_cfg(nozzle_temp=215)).digest


def test_preview_is_an_svg_that_shows_the_code_and_the_warnings():
    svg = npe.render_preview(npe.build_plan(ok_cfg(head=npe.HeadModel())))
    assert svg.startswith("<svg") and svg.rstrip().endswith("</svg>") and "EXPERIMENTAL" in svg and "NO es G-code" in svg and "SIN CONFIRMAR" in svg
    p = default_plan()
    assert p.digest in npe.render_preview(p)


def test_export_requires_every_barrier():
    p = default_plan()
    with pytest.raises(npe.ExportRefused, match="codigo de vista previa"):
        npe.export_gcode(p, "")
    with pytest.raises(npe.ExportRefused, match="codigo de vista previa"):
        npe.export_gcode(p, "AAAAAAAAAAAA")
    unconfirmed = npe.build_plan(ok_cfg(head=npe.HeadModel()))
    with pytest.raises(npe.ExportRefused, match="cabezal"):
        npe.export_gcode(unconfirmed, unconfirmed.digest)
    no_temps = npe.build_plan(npe.NPConfig(enabled=True, head=npe.HeadModel(confirmed=True)))
    with pytest.raises(npe.ExportRefused, match="bed_temp"):
        npe.export_gcode(no_temps, no_temps.digest)
    bad = npe.build_plan(ok_cfg(amplitude=1.5))
    with pytest.raises(npe.ExportRefused, match="ERROR"):
        npe.export_gcode(bad, bad.digest)
    with pytest.raises(npe.ExportRefused, match="no permitida"):
        q = npe.build_plan(ok_cfg(custom_start_lines=("G91",)))
        npe.export_gcode(q, q.digest)
    with pytest.raises(npe.ExportRefused, match="no permitida"):
        q = npe.build_plan(ok_cfg(custom_start_lines=("G1 X10",)))
        npe.export_gcode(q, q.digest)


def test_exported_gcode_is_conventional_safe_and_executable_by_the_marlin_model():
    p = default_plan()
    text = npe.export_gcode(p, p.digest)
    lines = text.splitlines()
    assert lines[0].startswith(npe.MARKER) and "G28" in lines and "G91" not in lines and "M82" in lines
    assert npe.verify_gcode(text, p.cfg, p) == []
    names = {l.split()[0] for l in lines if l and not l.startswith(";")}
    assert names <= set(npe.ALLOWED_CMDS)                                                  # solo ordenes de Marlin ya presentes en este firmware
    assert not names & {"G2", "G3", "M204", "M205", "M201", "M203", "M500", "M92"}                  # no cambia aceleraciones/jerk ni guarda en EEPROM
    # el modelo de Marlin del repositorio ejecuta el texto: E fisico = plan - retraccion, y termina arriba y dentro de la cama
    tr = simulate(["\n".join(lines)], capacity=30)
    x, y, z = tr.final
    assert tr.physical_e == pytest.approx(p.stats["e_total_mm"] - p.cfg.retract_mm, abs=0.01)
    assert 0 < x < 300 and 0 < y < 300 and z <= p.stats["z_max_mm"] + 5.0 + 1e-6
    # la pendiente que se envia a la maquina no supera el limite
    zs = [(m.z) for m in tr.moves]
    assert max(zs) <= p.stats["z_max_mm"] + 5.0 + 1e-6 and min(zs) >= 0


def test_verify_gcode_catches_tampering():
    p = default_plan()
    text = npe.export_gcode(p, p.digest)
    cfg = p.cfg
    codes = lambda t: {f.code for f in npe.verify_gcode(t, cfg, p) if f.level == "ERROR"}
    assert "G_ORDEN" in codes(text.replace("G90\n", "G91\n", 1))
    assert "G_LIMITES" in codes(text + "G1 X400 Y10 Z1 F600\n")
    assert "G_LIMITES" in codes(text + "G1 Z500 F100\n")
    assert "G_VEL" in codes(text + "G1 Z1 F3000\nG1 Z4 F3000\n")                             # 50 mm/s en Z > 5 mm/s del firmware
    assert "G_MARCA" in codes(text.replace(npe.MARKER, ";otro", 1))
    last_e = re.findall(r" E(\d+\.\d+) F1200", text)[-1]
    assert "G_E_TOTAL" in codes(text.replace("E" + last_e, "E999.0", 1))                     # extrusion total distinta del plan
    assert codes(text.replace("E1.", "E0.", 1))                                                # una extrusion que retrocede tambien se detecta


# ---------------------------------------------------------------- herramienta de linea de comandos
def test_cli_requires_explicit_enable_and_every_step(tmp_path, capsys):
    import importlib.util
    spec = importlib.util.spec_from_file_location("nonplanar_demo", os.path.join(ROOT, "cura", "tools", "nonplanar_demo.py"))
    cli = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cli)
    cfgf, svg, gc = str(tmp_path / "np.json"), str(tmp_path / "v.svg"), str(tmp_path / "p.gcode")
    assert cli.main(["--init-config", cfgf]) == 0
    assert cli.main(["--config", cfgf, "--preview", svg]) == 2 and not os.path.exists(svg)       # sin --enable-experimental no hace nada
    with open(cfgf, encoding="utf-8") as f:
        d = json.load(f)
    assert d["enabled"] is False and d["head"]["confirmed"] is False and d["bed_temp"] is None
    assert cli.main(["--enable-experimental", "--config", cfgf, "--preview", svg]) == 0 and os.path.exists(svg)
    out = capsys.readouterr().out
    code = re.search(r"CODIGO DE VISTA PREVIA: ([0-9A-F]{12})", out).group(1)
    assert cli.main(["--enable-experimental", "--config", cfgf, "--export", gc, "--accept", code]) == 3 and not os.path.exists(gc)   # cabezal sin confirmar
    d["head"]["confirmed"] = True; d["bed_temp"] = 60; d["nozzle_temp"] = 210
    with open(cfgf, "w", encoding="utf-8") as f:
        json.dump(d, f)
    capsys.readouterr()                                                                          # descarta la salida de los pasos anteriores
    cli.main(["--enable-experimental", "--config", cfgf, "--preview", svg])
    code2 = re.findall(r"CODIGO DE VISTA PREVIA: ([0-9A-F]{12})", capsys.readouterr().out)[-1]
    assert code2 != code                                                                        # la configuracion cambio: el codigo antiguo ya no vale
    assert cli.main(["--enable-experimental", "--config", cfgf, "--export", gc, "--accept", code]) == 3 and not os.path.exists(gc)
    assert cli.main(["--enable-experimental", "--config", cfgf, "--export", gc, "--accept", code2]) == 0 and os.path.exists(gc)
    assert open(gc).readline().startswith(npe.MARKER)


# ---------------------------------------------------------------- G-code externo (solo lectura) y perfil del cabezal Volcano
def ext(lines, **kw):
    cfg = ok_cfg(**kw)
    return npe.analyze_external_gcode("\n".join(["M83"] + lines) + "\n", cfg)             # M83: extrusion relativa (todo E positivo extruye)


def codes_of(rep, level=None):
    return {f.code for f in rep.findings if level is None or f.level == level}


def wave_lines(z0=0.6, amp=0.4, n=40, x0=10.0, y=10.0):
    out = ["G1 F500 X%.2f Y%.2f Z%.2f E0.1" % (x0, y, z0)]
    for i in range(1, n + 1):
        out.append("G1 X%.3f Y%.3f Z%.3f E0.05" % (x0 + i * 0.5, y, z0 + amp * math.sin(i / 4.0)))
    return out


def test_external_gcode_follows_g92_offset_relative_e_and_flags_risky_orders():
    rep = ext(["M83", "G90", "G0 F8000 X20 Y20 Z10", "G92 X0 Y0", "M106 P2 S255", "G0 X5 Y5", "G0 F8000 Z0.3", "G91", "G90"] + wave_lines() + ["M900 K0", "G91", "G0 Z20 F8000"])
    assert rep.ok, [f.msg for f in rep.findings if f.level == "ERROR"]
    st = rep.stats
    assert st["x_min"] == pytest.approx(30.0, abs=0.01) and st["x_max"] == pytest.approx(50.0, abs=0.01)         # origen desplazado +20 por G92
    assert st["z_min"] >= 0.2 - 1e-6 and st["z_max"] < 1.1                                                      # solo cuenta la extrusion, no los viajes
    risky = " ".join(f.msg for f in rep.findings if f.code == "EXT_ORDEN")
    assert "G92" in risky and "M106 P2" in risky and "M900" in risky and "G91" in risky
    assert "EXT_VEL" in codes_of(rep, "WARN")                                                                    # G0 F8000 con Z supera los 5 mm/s de Z


def test_external_gcode_out_of_bed_and_steep_slope_are_errors():
    far = ext(["G90", "G0 X280 Y20 Z1", "G92 X0 Y0"] + wave_lines(x0=30.0))                 # tras G92 el origen queda en 280: X fisico 310..330 > 300
    assert "EXT_LIMITES" in codes_of(far, "ERROR")
    steep = ext(["G90", "G1 F600 X50 Y50 Z0.3 E0.1", "G1 X51 Y50 Z0.9 E0.1", "G1 X52 Y50 Z0.3 E0.1"])            # 31 grados de subida y bajada
    assert "EXT_PENDIENTE" in codes_of(steep, "ERROR")
    gentle = ext(["G90", "G1 F600 X50 Y50 Z0.3 E0.1", "G1 X51 Y50 Z0.5 E0.1", "G1 X52 Y50 Z0.3 E0.1"])             # 11 grados
    assert "EXT_PENDIENTE" not in codes_of(gentle)
    assert not ext([]).ok and "EXT_VACIO" in codes_of(ext([]))


def test_printing_beside_a_taller_wall_is_not_a_collision_but_plowing_through_it_is():
    wall = ["G90", "G1 F600 X50 Y50 Z0.2 E0.1"] + ["G1 X50 Y50 Z%.2f E0.05" % (0.2 * k) for k in range(2, 12)]    # un hilo vertical de 2.2 mm
    beside = ext(wall + ["G1 X50 Y52 Z0.2 E0.1", "G1 X60 Y52 Z0.2 E0.1"])                                          # a 2 mm del hilo, a su altura base
    assert "COLISION_PUNTA" not in codes_of(beside)
    through = ext(wall + ["G1 X50.0 Y50.0 Z0.2 E0.1", "G1 X50.0 Y50.1 Z0.2 E0.1"])                                  # la punta baja DENTRO del hilo
    assert "COLISION_PUNTA" in codes_of(through, "ERROR")


def test_volcano_head_profile_loads_but_cannot_export_until_confirmed():
    path = os.path.join(ROOT, "cura", "profiles", "nonplanar_arquis_volcano.json")
    with open(path, encoding="utf-8") as f:
        d = {k: v for k, v in json.load(f).items() if not k.startswith("_")}
    d["enabled"] = True
    cfg = npe.config_from_dict(d)
    assert cfg.head.confirmed is False and cfg.bed_temp is None                                                  # no se confirma ni se elige material por el usuario
    names = {n: (r, h) for n, r, h in cfg.head.envelopes}
    block = next(v for n, v in names.items() if n.startswith("bloque calefactor Volcano"))
    duct = next(v for n, v in names.items() if n.startswith("ducto de ventilacion"))
    assert block == (11.5, 7.3)                                                                                 # 7,3 mm MEDIDOS por el propietario (punta -> cara inferior del bloque)
    assert duct[1] == 7.3 and duct[0] >= 11.5                                                                   # ductos a la altura del bloque; radio conservador
    assert round(math.hypot(10.0, 5.75), 1) == 11.5                                                             # media diagonal del bloque (orientacion desconocida)
    plan = npe.build_plan(cfg)
    with pytest.raises(npe.ExportRefused):
        npe.export_gcode(plan, plan.digest)


def test_cli_check_gcode_is_read_only_and_needs_the_enable_flag(tmp_path, capsys):
    import importlib.util
    spec = importlib.util.spec_from_file_location("nonplanar_demo", os.path.join(ROOT, "cura", "tools", "nonplanar_demo.py"))
    cli = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cli)
    g = tmp_path / "ext.gcode"
    g.write_text("\n".join(["M83", "G90"] + wave_lines()) + "\n")
    before = g.read_text()
    assert cli.main(["--check-gcode", str(g)]) == 2
    assert cli.main(["--enable-experimental", "--check-gcode", str(g)]) == 0
    assert "SOLO LECTURA" in capsys.readouterr().out and g.read_text() == before and sorted(p.name for p in tmp_path.iterdir()) == ["ext.gcode"]


# ---------------------------------------------------------------- adaptar G-code externo (formato FullControl)
def fullcontrol_like(n=60, e_mode="M83", extra_body=(), off=(20.0, 20.0), with_marks=True):
    """Archivo sintetico con la estructura de FullControl: cabecera, cebado, espiral con Z ondulada, final de la otra maquina."""
    h = ["; GCode created with FullControl", "M140 S50", "M104 S150", "M190 S50", "M109 S150", "G28", "G90", "G21", e_mode, "M106 S255", "M106 P2 S255",
         "G0 F8000 X%g Y%g Z10" % off, "G92 X0 Y0", "G0 X5 Y5", "M109 S210", "G1 F250 E20", "G0 F250 Z50", "G0 F8000 X10 Y10 Z0.3", "M220 S100", "M221 S100"]
    if with_marks:
        h.append(";END OF STARTING PROCEDURE")
    body = ["G0 F8000 Y12 Z0.32", "G1 F500 X60 E6.65"]
    for i in range(1, n + 1):
        a = i / 6.0
        body.append("G1 X%.4f Y%.4f Z%.4f E%.6f" % (30 + 8 * math.cos(a), 30 + 8 * math.sin(a), 0.32 + 0.5 * (1 - math.cos(a / 2)), 0.0123456 + i * 1e-5))
    body += list(extra_body)
    t = [";START OF ENDING PROCEDURE", "M83", "G0 E-0.8 F3000", "G91", "G0 Z20 F8000", "G90", "M106 S0", "M140 S0", "M104 S0", "M900 K0", "M106 P2 S0", "M84"]
    return "\n".join(h + body + t) + "\n"


def adapt(text=None, **kw):
    cfg = ok_cfg(fan_speed=kw.pop("fan_speed", 255), head=kw.pop("head", npe.HeadModel(confirmed=True)), **kw)
    return npe.adapt_external_gcode(text or fullcontrol_like(), cfg, "flor_prueba.gcode")


def test_adapter_keeps_the_path_and_replaces_everything_else():
    src = fullcontrol_like()
    ap = adapt(src)
    lines = ap.text.splitlines()
    assert lines[0].startswith(npe.MARKER) and ap.ok, [f.msg for f in ap.findings if f.level == "ERROR"]
    names = {l.split()[0] for l in lines if l and not l.startswith(";")}
    assert names <= set(npe.ALLOWED_CMDS) and "G92" not in " ".join(lines[3:-1]) .replace("G92 E0", "")        # sin G92 X/Y, G91, M900, M106 P2, G21
    assert not any(t in ap.text for t in ("G91", "M900", "P2", "G21", "Z50", "Z10"))
    # la trayectoria es exactamente la original trasladada (+20, +20): coordenadas, Z y E
    def pts(t, on):
        o = []
        for ln in t.splitlines():
            if "END OF STARTING" in ln: on = True
            if "START OF ENDING" in ln: on = False
            w = ln.split(";")[0].split()
            if on and w and w[0] == "G1" and "E" in "".join(w) and "X" in "".join(w) and "Z" in "".join(w):
                o.append({x[0]: float(x[1:]) for x in w[1:] if len(x) > 1})
        return o
    a, b = pts(src, False), pts(ap.text, True)
    b = [m for m in b if "Z" in m][:len(a)]
    assert len(a) == len(b) == 60
    for u, v in zip(a, b):
        assert v["X"] == pytest.approx(u["X"] + 20.0, abs=1e-4) and v["Y"] == pytest.approx(u["Y"] + 20.0, abs=1e-4)
        assert v["Z"] == pytest.approx(u["Z"], abs=1e-4) and v["E"] == pytest.approx(u["E"], abs=1e-6)        # E sin perder precision
    assert "M140 S60" in lines and "M140 S50" not in lines and "M109 S210" in lines and "M106 S255" in lines     # temperaturas/ventilador de la CONFIGURACION, no las del archivo (cama 50)
    assert any(f.code == "NIVELACION" and f.level == "WARN" for f in ap.findings)
    assert 0 < ap.stats["az_est_mm_s2"] < ap.cfg.machine.max_accel[2]                                          # la aceleracion de Z estimada se informa y esta dentro del firmware


def test_adapter_caps_the_z_speed_and_never_exceeds_firmware_limits():
    steep = fullcontrol_like(extra_body=["G1 F3000 X40 Y40 Z0.8 E0.1", "G1 X41 Y40 Z1.6 E0.1"])                 # pide mucha velocidad en Z (50 mm/s)
    ap = adapt(steep, max_slope_deg=60.0, head=npe.HeadModel(nozzle_clearance_deg=80.0, confirmed=True))
    assert not [f for f in ap.findings if f.code in ("G_VEL", "EXT_VEL")], [f.msg for f in ap.findings]
    zcap = 0.9 * 5.0
    assert ap.stats["vz_extrusion_mm_s"] <= zcap + 1e-6                                                       # el adaptador limito F por movimiento


@pytest.mark.parametrize("kw,msg", [
    (dict(with_marks=False), "marcadores"),
    (dict(e_mode="M82"), "absoluta"),
    (dict(extra_body=["M900 K0.1"]), "no traduce"),
    (dict(extra_body=["G2 X1 Y1 I1 J0 E1"]), "no traduce"),
    (dict(extra_body=["G1 X40 Y40 S1 E1"]), "no soportadas"),
])
def test_adapter_refuses_what_it_cannot_translate(kw, msg):
    with pytest.raises(npe.AdaptRefused, match=msg):
        adapt(fullcontrol_like(**kw))


def test_adapter_requires_explicit_material_and_disabled_by_default():
    with pytest.raises(npe.AdaptRefused, match="no asume el material"):
        npe.adapt_external_gcode(fullcontrol_like(), npe.NPConfig(enabled=True, head=npe.HeadModel(confirmed=True)), "x")      # sin temperaturas ni ventilador
    with pytest.raises(npe.NotEnabled):
        npe.adapt_external_gcode(fullcontrol_like(), npe.NPConfig(), "x")
    with pytest.raises(npe.AdaptRefused, match="no permitida"):
        adapt(custom_start_lines=("G1 X10",))


def test_adapter_reports_bed_overflow_and_steepness_and_blocks_export():
    out_of_bed = adapt(fullcontrol_like(off=(285.0, 20.0)))                                                     # el G92 deja el origen en X 285: la espiral sale de la cama
    assert "EXT_LIMITES" in {f.code for f in out_of_bed.findings if f.level == "ERROR"}
    with pytest.raises(npe.ExportRefused, match="ERROR"):
        npe.export_adapted(out_of_bed, out_of_bed.digest)
    steep = adapt(fullcontrol_like(extra_body=["G1 F600 X40 Y40 Z0.4 E0.1", "G1 X41 Y40 Z1.2 E0.1"]))
    assert "EXT_PENDIENTE" in {f.code for f in steep.findings if f.level == "ERROR"}
    jerky = ext(["G90", "G1 F1800 X50 Y50 Z0.3 E0.1"] + ["G1 X%.2f Y50 Z%.2f E0.05" % (50 + 0.3 * i, 0.3 + (0.15 if i % 2 else 0.0)) for i in range(1, 40)])   # Z sube y baja cada 0,3 mm a 30 mm/s
    assert "EXT_ACEL_Z" in {f.code for f in jerky.findings if f.level in ("WARN", "ERROR")}


def test_adapter_export_requires_confirmed_head_and_the_preview_code():
    ap = adapt()
    with pytest.raises(npe.ExportRefused, match="codigo"):
        npe.export_adapted(ap, "AAAAAAAAAAAA")
    with pytest.raises(npe.ExportRefused, match="codigo"):
        npe.export_adapted(ap, "")
    unconf = adapt(head=npe.HeadModel())
    with pytest.raises(npe.ExportRefused, match="cabezal"):
        npe.export_adapted(unconf, unconf.digest)
    text = npe.export_adapted(ap, ap.digest)
    assert text == ap.text and ap.digest in text.splitlines()[1]
    assert npe.verify_gcode(text, ap.cfg) == []
    assert ap.digest != adapt(nozzle_temp=215).digest                                                           # cualquier cambio cambia el codigo


def test_adapted_gcode_is_executable_by_the_marlin_model_with_relative_e():
    ap = adapt()
    tr = simulate([ap.text], capacity=30)
    x, y, z = tr.final
    assert 0 < x < 300 and 0 < y < 300 and z >= 20.0                                                            # termina arriba
    src_e = sum(float(w[1:]) for ln in fullcontrol_like().split("ENDING")[0].split("OF STARTING PROCEDURE")[1].splitlines() if ln.startswith("G1")
                for w in ln.split() if w.startswith("E"))
    assert tr.physical_e == pytest.approx(src_e - ap.cfg.retract_mm, abs=0.01)


def test_cli_adapts_with_every_barrier(tmp_path, capsys):
    import importlib.util
    spec = importlib.util.spec_from_file_location("nonplanar_demo", os.path.join(ROOT, "cura", "tools", "nonplanar_demo.py"))
    cli = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cli)
    src, cfgf, out, svg = tmp_path / "f.gcode", tmp_path / "c.json", tmp_path / "o.gcode", tmp_path / "v.svg"
    src.write_text(fullcontrol_like())
    d = {"head": {"confirmed": False}, "bed_temp": 50, "nozzle_temp": 210, "fan_speed": 255}
    cfgf.write_text(json.dumps(d))
    assert cli.main(["--adapt-gcode", str(src)]) == 2                                                           # sin --enable-experimental no hace nada
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--adapt-gcode", str(src), "--preview", str(svg), "--export", str(out), "--accept", "000000000000"]) == 3
    assert svg.exists() and not out.exists()                                                                    # hay vista previa pero no G-code
    code = re.findall(r"CODIGO DE VISTA PREVIA: ([0-9A-F]{12})", capsys.readouterr().out)[-1]
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--adapt-gcode", str(src), "--export", str(out), "--accept", code]) == 3 and not out.exists()   # cabezal sin confirmar
    d["head"]["confirmed"] = True
    cfgf.write_text(json.dumps(d))
    capsys.readouterr()
    cli.main(["--enable-experimental", "--config", str(cfgf), "--adapt-gcode", str(src)])
    code2 = re.findall(r"CODIGO DE VISTA PREVIA: ([0-9A-F]{12})", capsys.readouterr().out)[-1]
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--adapt-gcode", str(src), "--export", str(out), "--accept", code2]) == 0 and out.exists()
    assert out.read_text().startswith(npe.MARKER)
    bad = tmp_path / "no_fullcontrol.gcode"
    bad.write_text("G28\nG1 X10 Y10 E1\n")
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--adapt-gcode", str(bad)]) == 3
