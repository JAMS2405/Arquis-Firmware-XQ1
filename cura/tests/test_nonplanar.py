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
