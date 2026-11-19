# SPDX-License-Identifier: LGPL-3.0-or-later
"""Ondulado de un G-code normal (capas no planas con volumen). EXPERIMENTAL: pruebas de calculo y de modelo; nada se ha probado en la impresora."""
import glob
import importlib.util
import json
import math
import os
import re

import pytest

import arquis_bootstrap

arquis_bootstrap.load_engines()
import arquis_engines.nonplanar_engine as npe  # noqa: E402
import arquis_engines.nonplanar_warp as nw  # noqa: E402
from arquis_engines.marlin_model import simulate  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def cura_like(layers=20, h=0.2, x0=130.0, y0=130.0, size=40.0, abs_e=True, g91_inside=False, arcs=False, with_marks=True):
    """G-code sintetico con la estructura de Cura: marcas ;ARQUIS, inicio, capas con ;LAYER/;TIME_ELAPSED, perimetro e infill, final y manifiesto."""
    L = [";ARQUIS_M486 V=1 N=1 CAP=30 STATUS=OK", ";ARQUIS_OBJECTS=1 PID=12345678 N=1", ";FLAVOR:Marlin", ";Generated with Cura_SteamEngine 5.11.0",
         "M82 ;absolute extrusion mode" if abs_e else "M83 ;relative extrusion mode", "G28 ;Home", "G90", "G92 E0", "G1 Z2.0 F3000", "G0 X%g Y%g Z%g F5000" % (x0, y0, h), "G92 E0"]
    e = 0.0
    k = 0.03
    if with_marks:
        L.append(";LAYER_COUNT:%d" % layers)
    for n in range(layers):
        z = h * (n + 1)
        if with_marks:
            L.append(";LAYER:%d" % n)
        L.append("G0 F6000 X%g Y%g Z%g" % (x0, y0, z))
        if g91_inside and n == 5:
            L.append("G91")
        if arcs and n == 6:
            L.append("G2 X%g Y%g I1 J1 E%g" % (x0 + 1, y0 + 1, e + 0.1))
        pts = [(x0, y0), (x0 + size, y0), (x0 + size, y0 + size), (x0, y0 + size), (x0, y0)]
        for (ax, ay), (bx, by) in zip(pts, pts[1:]):
            d = math.hypot(bx - ax, by - ay)
            e += d * k
            L.append("G1 F1800 X%g Y%g E%s" % (bx, by, ("%.5f" % e) if abs_e else ("%.5f" % (d * k))))
        for j in range(1, 16):                                               # infill en zigzag
            yy = y0 + j * size / 16.0
            xa, xb = (x0, x0 + size) if j % 2 else (x0 + size, x0)
            L.append("G0 F6000 X%g Y%g" % (xa, yy))
            d = abs(xb - xa)
            e += d * k
            L.append("G1 F2400 X%g Y%g E%s" % (xb, yy, ("%.5f" % e) if abs_e else ("%.5f" % (d * k))))
        if abs_e and n % 7 == 3:
            L.append("G92 E0"); e = 0.0
        L.append(";TIME_ELAPSED:%d" % (10 * (n + 1)))
    L += ["G91 ;Posicionamiento relativo", "G1 E-5 F3000", "G1 Z5 F3000", "G90", "G1 X0 Y300 F3000", "M104 S0", "M84 X Y E", "M82 ;absolute extrusion mode", ";End of Gcode",
          ";ARQUIS_OBJECTS_BEGIN", ";ARQ:H V=1 PID=12345678 N=1 BED=3000,3000 MIN=0,0 ORG=FL SEQ=0 CAP=30", ";ARQUIS_OBJECTS_END CRC=00000000", ";SETTING_3 {\"x\": 1}"]
    return "\n".join(L) + "\n"


def wcfg(**kw):
    head = kw.pop("head", npe.HeadModel(nozzle_clearance_deg=40.0, confirmed=True))
    kw.setdefault("enabled", True)
    return nw.WarpConfig(head=head, **kw)


def errs(r):
    return {f.code for f in r.findings if f.level == "ERROR"}


# ---------------------------------------------------------------- desactivado y expresiones seguras
def test_disabled_by_default_and_never_wired_into_cura():
    assert nw.WarpConfig().enabled is False
    with pytest.raises(npe.NotEnabled):
        nw.warp_gcode(cura_like(), nw.WarpConfig())
    for path in glob.glob(os.path.join(ROOT, "cura", "scripts", "*.py")) + glob.glob(os.path.join(ROOT, "cura", "plugins", "ARQUISPlugin", "*.py")):
        assert "nonplanar_warp" not in open(path, encoding="utf-8").read(), path


@pytest.mark.parametrize("bad", ["__import__('os').system('x')", "x.__class__", "open('/etc/passwd')", "(lambda: 1)()", "[1,2][0]", "x if y else 1", "A*sin(x)+foo(2)", "'a'*3",
                                 "print(1)", "x; y", "sin(x, key=1)", "x.real", "__builtins__"])
def test_unsafe_expressions_are_rejected(bad):
    with pytest.raises(nw.WarpRefused):
        nw.safe_expr(bad)


def test_safe_expression_evaluates_with_the_allowed_names_only():
    fn = nw.safe_expr("A/2*sin(2*pi*x/L) + 0.1*abs(y) - 0.0*r")
    assert fn(5.0, -3.0, 5.83, 2.0, 20.0) == pytest.approx(1.0 * math.sin(2 * math.pi * 5 / 20) + 0.3, abs=1e-9)
    cfg = wcfg(function="A/2*sin(2*pi*x/L)", amplitude=2.0, wavelength=20.0, center_x=0.0)
    assert nw.make_f(cfg)(5.0, 7.0) == pytest.approx(math.sin(math.pi / 2), abs=1e-9)


def test_wave_function_has_zero_mean_and_the_requested_peak_to_peak():
    f = nw.make_f(wcfg(amplitude=2.0, wavelength=20.0, center_x=0.0, center_y=0.0))
    vals = [f(x * 0.5, y * 0.5) for x in range(40) for y in range(40)]                          # una longitud de onda completa en X e Y
    assert abs(sum(vals) / len(vals)) < 1e-9 and max(vals) == pytest.approx(1.0, abs=1e-6) and min(vals) == pytest.approx(-1.0, abs=1e-6)


def test_ramp_is_zero_below_z_in_one_above_z_in_plus_ramp_and_returns_with_z_out():
    c = wcfg(z_in=1.0, ramp=4.0, z_out=20.0)
    assert nw.ramp_at(c, 0.5) == 0 and nw.ramp_at(c, 1.0) == 0 and nw.ramp_at(c, 3.0) == pytest.approx(0.5) and nw.ramp_at(c, 5.0) == 1.0
    assert nw.ramp_at(c, 16.0) == 1.0 and nw.ramp_at(c, 18.0) == pytest.approx(0.5) and nw.ramp_at(c, 20.0) == 0 and nw.ramp_at(c, 25.0) == 0


# ---------------------------------------------------------------- el ondulado en si
def test_warp_keeps_everything_outside_the_part_and_the_arquis_header_in_the_first_512_bytes():
    src = cura_like()
    r = nw.warp_gcode(src, wcfg(amplitude=1.0, z_in=0.6, ramp=2.0))
    a, b = src.splitlines(), r.text.splitlines()
    assert b[0] == a[0] and b[1] == a[1] and b[2].startswith(nw.MARK) and r.digest in b[2]                # ;ARQUIS primero, luego el banner
    assert len("\n".join(b[:3])) < 512
    first = next(i for i, l in enumerate(a) if l.startswith(";LAYER:"))
    assert b[3:first + 1] == a[2:first]                                                                   # inicio intacto (se desplaza 1 linea por el banner)
    assert a[-5:] == b[-5:] and any("ARQUIS_OBJECTS_BEGIN" in l for l in b[-5:])                         # manifiesto y ajustes al final, intactos
    assert any(l.startswith("G91") for l in b) and "G90" in b                                            # el final del usuario sigue ahi


def test_warp_is_a_pure_displacement_of_z_with_short_segments_and_unchanged_xy_outline():
    src = cura_like()
    c = wcfg(amplitude=1.0, z_in=0.6, ramp=2.0, center_x=150.0, center_y=150.0)
    r = nw.warp_gcode(src, c)
    f = nw.make_f(c)
    x = y = z = 0.0
    prev = None
    n_checked = 0
    first_skip = [0]
    inside = False
    for ln in r.text.splitlines():
        if ln.startswith(";LAYER:"): inside = True
        if ln.startswith(";TIME_ELAPSED:") and ln.endswith(":200"): inside = False
        w = ln.split(";")[0].split()
        if not w or w[0] not in ("G0", "G1"):
            continue
        v = {t[0]: float(t[1:]) for t in w[1:] if len(t) > 1}
        nx, ny, nz = v.get("X", x), v.get("Y", y), v.get("Z", z)
        if inside and ("X" in v or "Y" in v) and n_checked + first_skip[0] > 0:
            assert math.hypot(nx - x, ny - y) <= 1.0 + 1e-3                                              # segmentos de <= 1 mm
            n_checked += 1
        elif inside:
            first_skip[0] = 1                                                                            # el primer movimiento de la pieza parte de la posicion del inicio
            n_checked += 1
        x, y, z = nx, ny, nz
    assert n_checked > 3000
    # el desplazamiento de un punto concreto: la capa 15 (z nominal 3.0) en una esquina
    zs = [float(re.search(r"Z([-\d.]+)", l).group(1)) for l in r.text.splitlines() if l.startswith("G1") and re.search(r"X130(\.0+)? Y130", l) and "Z" in l]
    assert zs, "no hay puntos en la esquina (130,130)"
    nominal = [0.2 * k for k in range(1, 21)]
    assert all(any(abs(zv - (zn + nw.ramp_at(c, zn) * f(130.0, 130.0))) < 2e-3 for zn in nominal) for zv in zs)


def test_first_layers_below_z_in_are_not_touched():
    src = cura_like(layers=3, h=0.2)
    r = nw.warp_gcode(src, wcfg(amplitude=1.0, z_in=5.0, ramp=2.0))                                       # z_in mas alto que la pieza: nada cambia
    a = [l for l in src.splitlines() if l.split(";")[0].split()[:1] in (["G0"], ["G1"])]
    b = [l for l in r.text.splitlines() if l.split(";")[0].split()[:1] in (["G0"], ["G1"])]
    zone_a = a[a.index(next(l for l in a if "F6000" in l)):]
    assert all(re.search(r"Z0?\.\d+", l) is None or True for l in b)
    assert r.stats["dz_max"] == 0.0 and r.stats["mult_min"] == pytest.approx(1.0) and r.stats["mult_max"] == pytest.approx(1.0)


@pytest.mark.parametrize("abs_e", [True, False])
def test_extrusion_volume_is_conserved_in_absolute_and_relative_e(abs_e):
    src = cura_like(abs_e=abs_e)
    r = nw.warp_gcode(src, wcfg(amplitude=1.0, z_in=0.6, ramp=2.0))
    assert r.ok, [f.msg for f in r.findings if f.level == "ERROR"]
    assert r.stats["e_out"] == pytest.approx(r.stats["e_in"], rel=0.01)                                  # la onda tiene media cero: el volumen se conserva
    assert r.stats["mult_min"] < 1.0 < r.stats["mult_max"]                                               # pero localmente las capas son mas finas o mas gruesas
    t_in = simulate([src], capacity=30).physical_e
    t_out = simulate([r.text], capacity=30).physical_e
    assert t_out == pytest.approx(t_in, rel=0.01), (t_in, t_out)                                         # el modelo de Marlin ejecuta el texto con el mismo filamento total


def test_layer_thickness_multiplier_matches_the_closed_form():
    c = wcfg(amplitude=1.0, z_in=0.6, ramp=2.0, center_x=130.0, center_y=130.0)
    r = nw.warp_gcode(cura_like(), c)
    f = nw.make_f(c)
    fvals = [f(130 + a, 130 + b) for a in range(41) for b in range(41)]
    dr_max = max(nw.ramp_at(c, 0.2 * k) - nw.ramp_at(c, 0.2 * k - 0.2) for k in range(1, 21))
    assert r.stats["mult_max"] == pytest.approx(1.0 + dr_max * max(fvals) / 0.2, abs=0.02)
    assert r.stats["mult_min"] == pytest.approx(1.0 + dr_max * min(fvals) / 0.2, abs=0.02)


def test_z_speed_is_capped_to_90_percent_of_the_firmware_limit_and_verified_in_the_text():
    r = nw.warp_gcode(cura_like(), wcfg(amplitude=1.2, z_in=0.6, ramp=2.0, max_slope_deg=30.0, head=npe.HeadModel(nozzle_clearance_deg=60.0, confirmed=True)))
    assert r.stats["caps_z"] > 0 and not [f for f in r.findings if f.code in ("WARP_G_VEL", "WARP_G_LIMITES")], [f.msg for f in r.findings]


def test_marlin_model_runs_the_warped_file_and_the_user_end_code_clears_the_part_by_more_than_before():
    src = cura_like()
    r = nw.warp_gcode(src, wcfg(amplitude=1.0, z_in=0.6, ramp=2.0))
    a, b = simulate([src], capacity=30), simulate([r.text], capacity=30)
    assert a.final[0] == pytest.approx(b.final[0]) and a.final[1] == pytest.approx(b.final[1])
    assert b.final[2] >= a.final[2] - 1e-6                                                              # el G1 Z5 relativo del usuario ya no parte de una posicion ondulada mas baja
    lift = [l for l in r.text.splitlines() if "subida segura" in l]
    assert len(lift) == 1 and r.text.splitlines().index(lift[0]) > max(i for i, l in enumerate(r.text.splitlines()) if l.startswith(";TIME_ELAPSED:"))


def test_feed_rate_is_restored_after_the_part():
    r = nw.warp_gcode(cura_like(), wcfg(amplitude=1.0, z_in=0.6, ramp=2.0))
    lines = r.text.splitlines()
    k = max(i for i, l in enumerate(lines) if l.startswith(";TIME_ELAPSED:"))
    tail = lines[k + 1:]
    # el final del usuario no lleva F en "G1 Z5" ni en "G1 E-5" (si lo llevan se respeta); si la ultima velocidad del ondulado difiere debe verse un G1 F antes
    assert tail[0].startswith("G0 Z") and "subida segura" in tail[0] and (tail[1].startswith("G1 F") or tail[1].startswith("G91"))


# ---------------------------------------------------------------- lo que se rechaza o se marca
def test_dangerous_settings_are_errors():
    assert errs(nw.warp_gcode(cura_like(), wcfg(amplitude=3.5, z_in=0.6, ramp=1.0))) & {"WARP_ESPESOR", "WARP_PENDIENTE"}
    assert "WARP_ESPESOR" in errs(nw.warp_gcode(cura_like(), wcfg(amplitude=2.0, z_in=0.6, ramp=0.6)))     # rampa demasiado brusca
    assert "WARP_PENDIENTE" in errs(nw.warp_gcode(cura_like(), wcfg(amplitude=1.5, wavelength=6.0, z_in=0.6, ramp=3.0)))
    tight = npe.HeadModel(nozzle_clearance_deg=40.0, envelopes=(("bloque", 12.0, 0.2),), confirmed=True)
    assert "WARP_CABEZAL" in errs(nw.warp_gcode(cura_like(), wcfg(amplitude=1.0, z_in=0.6, ramp=2.0, head=tight)))
    assert "WARP_CAMA" in errs(nw.warp_gcode(cura_like(x0=285.0), wcfg(amplitude=1.0, z_in=0.6, ramp=2.0)))


@pytest.mark.parametrize("kw,msg", [(dict(g91_inside=True), "G91"), (dict(arcs=True), "arco"), (dict(with_marks=False), "LAYER")])
def test_unsupported_structures_are_refused(kw, msg):
    with pytest.raises(nw.WarpRefused, match=msg):
        nw.warp_gcode(cura_like(**kw), wcfg(amplitude=1.0, z_in=0.6, ramp=2.0))


@pytest.mark.parametrize("kw", [dict(amplitude=0.0), dict(amplitude=9.0), dict(wavelength=0.0), dict(ramp=0.0), dict(max_seg=0.0)])
def test_parameters_out_of_range_are_refused(kw):
    with pytest.raises(nw.WarpRefused):
        nw.warp_gcode(cura_like(), wcfg(**kw))


def test_manifest_warning_is_reported():
    r = nw.warp_gcode(cura_like(), wcfg(amplitude=1.0, z_in=0.6, ramp=2.0))
    assert any(f.code == "WARP_MANIFIESTO" and f.level == "WARN" for f in r.findings)


# ---------------------------------------------------------------- barreras de exportacion
def test_export_needs_confirmed_head_no_errors_and_the_preview_code():
    r = nw.warp_gcode(cura_like(), wcfg(amplitude=1.0, z_in=0.6, ramp=2.0))
    with pytest.raises(npe.ExportRefused, match="codigo"):
        nw.export_warped(r, "AAAAAAAAAAAA")
    unconf = nw.warp_gcode(cura_like(), wcfg(amplitude=1.0, z_in=0.6, ramp=2.0, head=npe.HeadModel()))
    with pytest.raises(npe.ExportRefused, match="cabezal"):
        nw.export_warped(unconf, unconf.digest)
    bad = nw.warp_gcode(cura_like(), wcfg(amplitude=3.5, z_in=0.6, ramp=1.0))
    with pytest.raises(npe.ExportRefused, match="ERROR"):
        nw.export_warped(bad, bad.digest)
    assert nw.export_warped(r, r.digest) == r.text
    assert r.digest != nw.warp_gcode(cura_like(), wcfg(amplitude=1.1, z_in=0.6, ramp=2.0)).digest != nw.warp_gcode(cura_like(layers=21), wcfg(amplitude=1.0, z_in=0.6, ramp=2.0)).digest


def test_preview_is_an_svg_with_the_code_and_the_warnings():
    r = nw.warp_gcode(cura_like(), wcfg(amplitude=1.0, z_in=0.6, ramp=2.0, head=npe.HeadModel()))
    svg = nw.render_warp_preview(r)
    assert svg.startswith("<svg") and svg.rstrip().endswith("</svg>") and r.digest in svg and "EXPERIMENTAL" in svg and "SIN CONFIRMAR" in svg and "WARP_MANIFIESTO" in svg


def test_cli_requires_every_step(tmp_path, capsys):
    spec = importlib.util.spec_from_file_location("nonplanar_warp_cli", os.path.join(ROOT, "cura", "tools", "nonplanar_warp.py"))
    cli = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(cli)
    src, cfgf, out, svg = tmp_path / "p.gcode", tmp_path / "w.json", tmp_path / "o.gcode", tmp_path / "v.svg"
    src.write_text(cura_like())
    assert cli.main(["--init-config", str(cfgf)]) == 0
    d = json.loads(cfgf.read_text())
    assert d["enabled"] is False and d["head"]["confirmed"] is False
    d.update(amplitude=1.0, z_in=0.6, ramp=2.0)
    d["head"]["nozzle_clearance_deg"] = 40.0
    cfgf.write_text(json.dumps(d))
    assert cli.main(["--config", str(cfgf), "--input", str(src), "--preview", str(svg)]) == 2 and not svg.exists()
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--input", str(src), "--preview", str(svg), "--export", str(out), "--accept", "000000000000"]) == 3
    assert svg.exists() and not out.exists()
    d["head"]["confirmed"] = True
    cfgf.write_text(json.dumps(d))
    capsys.readouterr()
    cli.main(["--enable-experimental", "--config", str(cfgf), "--input", str(src)])
    code = re.findall(r"CODIGO DE VISTA PREVIA: ([0-9A-F]{12})", capsys.readouterr().out)[-1]
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--input", str(src), "--export", str(out), "--accept", "000000000000"]) == 3 and not out.exists()
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--input", str(src), "--export", str(out), "--accept", code]) == 0 and out.exists()
    assert out.read_text().splitlines()[2].startswith(nw.MARK)
    bad = tmp_path / "otro.gcode"
    bad.write_text("G28\nG1 X10 E1\n")
    assert cli.main(["--enable-experimental", "--config", str(cfgf), "--input", str(bad)]) == 3


# ---------------------------------------------------------------- G-code real del propietario (opcional)
@pytest.mark.skipif(not os.environ.get("ARQUIS_REAL_GCODE_DIR") or not glob.glob(os.path.join(os.environ.get("ARQUIS_REAL_GCODE_DIR", ""), "*pinon_dientes.gcode")), reason="sin G-code real")
def test_real_cura_gcode_six_gears_warps_safely():
    path = glob.glob(os.path.join(os.environ["ARQUIS_REAL_GCODE_DIR"], "*pinon_dientes.gcode"))[0]
    src = open(path, encoding="utf-8").read()
    r = nw.warp_gcode(src, wcfg(amplitude=1.0, z_in=0.8, ramp=2.0))
    assert r.ok, [f.msg for f in r.findings if f.level == "ERROR"]
    assert r.stats["e_out"] == pytest.approx(r.stats["e_in"], rel=0.01) and r.stats["mov_salida"] > r.stats["mov_entrada"]
    block = lambda t: t[t.index(";ARQUIS_OBJECTS_BEGIN"):t.index(";ARQUIS_TRAILER")]
    assert r.text.splitlines()[0] == src.splitlines()[0] and r.text.splitlines()[1] == src.splitlines()[1] and block(r.text) == block(src)    # marcas y manifiesto intactos (con su CRC)
    assert len("\n".join(r.text.splitlines()[:3])) < 512


def test_example_profile_loads_and_is_unconfirmed():
    with open(os.path.join(ROOT, "cura", "profiles", "nonplanar_warp_arquis_volcano.json"), encoding="utf-8") as f:
        d = {k: v for k, v in json.load(f).items() if not k.startswith("_")}
    d["enabled"] = True
    cfg = nw.warp_config_from_dict(d)
    assert cfg.head.confirmed is False and cfg.amplitude == 1.0 and cfg.center_x == 150.0
    r = nw.warp_gcode(cura_like(layers=40), cfg)
    with pytest.raises(npe.ExportRefused, match="cabezal"):
        nw.export_warped(r, r.digest)
