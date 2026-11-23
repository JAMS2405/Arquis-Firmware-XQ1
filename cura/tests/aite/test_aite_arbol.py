# SPDX-License-Identifier: LGPL-3.0-or-later
"""Generador del arbol de prueba (aite.arbol) y su verificador independiente (aite.gcode_check)."""
import importlib.util
import math
import os
import re

import pytest

from aite.arbol import (MAQUINA_XQ1, Arbol, GeometriaRama, Proceso, Rama, alfa_minima, contacto_de_la_punta, espiral, generar, largo_polilineas,
                        probeta_rama, validar)
from aite.cli import main
from aite.gcode_check import estimar_tiempo, holgura_permitida, verificar
from aite.head import load_head_profile

CONO = load_head_profile().cone_alpha_deg()


# ---------------------------------------------------------------- fixtures (se generan una sola vez: la verificacion tarda unos segundos)
@pytest.fixture(scope="module")
def probeta():
    texto, c = generar(probeta_rama())
    return texto, c, verificar(texto, c)


@pytest.fixture(scope="module")
def arbol_completo():
    texto, c = generar()
    return texto, c, verificar(texto, c)


def _g1(texto):
    """Lista de (n, dict) con las palabras de cada G1."""
    out = []
    for n, raw in enumerate(texto.splitlines(), 1):
        l = raw.split(";", 1)[0].strip()
        if l.startswith("G1"):
            out.append((n, {t[0]: float(t[1:]) for t in l.split()[1:]}))
    return out


# ---------------------------------------------------------------- espiral
def test_spiral_stays_inside_and_fills_the_rectangle():
    w, s = 0.66, 0.5958
    for a, b in [(9.34, 9.34), (21.0, 9.34), (3.0, 3.0), (1.0, 1.0), (1.9, 1.9), (12.0, 1.9), (0.2, 9.34), (0.0, 9.34), (9.34, 0.4)]:
        polis = espiral(a, b, s, w)
        assert polis, (a, b)
        for poli in polis:
            for p, q in poli:
                assert -1e-9 <= p <= a + 1e-9 and -1e-9 <= q <= b + 1e-9, (a, b, p, q)
        # relleno: cada celda de la mitad de un cordon (0,33 mm) del rectangulo esta a menos de w/2 de alguna linea
        pts = [(p, q) for poli in polis for (p, q) in poli]
        tramos = [(u, v) for poli in polis for u, v in zip(poli, poli[1:])]
        for gx in range(0, 11):
            for gy in range(0, 11):
                x, y = a * gx / 10.0, b * gy / 10.0
                d = min(_dist_seg((x, y), u, v) for (u, v) in tramos) if tramos else min(math.dist((x, y), pt) for pt in pts)
                assert d <= w / 2 + 0.17, (a, b, x, y, d)         # 0,17 = lo que deja el relleno redondeado entre dos lineas separadas s


def _dist_seg(p, u, v):
    dx, dy = v[0] - u[0], v[1] - u[1]
    l2 = dx * dx + dy * dy
    t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((p[0] - u[0]) * dx + (p[1] - u[1]) * dy) / l2))
    return math.hypot(p[0] - u[0] - t * dx, p[1] - u[1] - t * dy)


def test_spiral_rings_are_separated_by_the_step_and_close():
    polis = espiral(9.34, 9.34, 0.5958, 0.66)
    anillos = [p for p in polis if len(p) == 5]
    assert len(anillos) == 8 and all(a[0] == a[-1] for a in anillos)                # cerrados
    assert [round(a[0][0], 4) for a in anillos] == [round(i * 0.5958, 4) for i in range(8)]
    assert largo_polilineas([[(p, q, 0.0) for p, q in poli] for poli in polis]) > 150


def test_spiral_of_a_thin_strip_is_one_line():
    assert len(espiral(0.3, 9.34, 0.6, 0.66)) == 1
    assert len(espiral(9.34, 0.3, 0.6, 0.66)) == 1


# ---------------------------------------------------------------- fisica y validacion
def test_default_process_numbers():
    p = Proceso()
    assert alfa_minima(p) == pytest.approx(16.86, abs=0.02)                         # 90 - atan(0,5 * 0,66 / 0,10)
    assert contacto_de_la_punta(p) == pytest.approx(0.547, abs=0.002)
    assert p.paso(0.1) == pytest.approx(0.66 - 0.1 * (1 - math.pi / 4), abs=1e-12)
    assert CONO == pytest.approx(20.1, abs=0.1)
    assert any("25 %" in a for a in validar(Arbol(), p))                              # avisa que la capa de 0,10 es el 17 % de la boquilla


def test_validation_rejects_layers_that_cannot_hold_a_flat_floor():
    # capa inclinada de 0,2 con ancho 0,66: hace falta alfa >= 90 - atan(1,65) = 31 grados: el cabezal no lo permite
    with pytest.raises(ValueError, match="no hay ventana"):
        validar(Arbol(), Proceso(capa_incl=0.2, capa_plana=0.3))
    with pytest.raises(ValueError, match="fuera de la ventana"):
        validar(Arbol(), Proceso(alfa=12.0))
    with pytest.raises(ValueError, match="fuera de la ventana"):
        validar(Arbol(), Proceso(alfa=20.0))                                         # dentro del cono (20,1) pero sin los 1,0 grados de margen


@pytest.mark.parametrize("kw", [dict(ancho=0.3), dict(capa_incl=0.04), dict(capa_zona=0.4), dict(c_min=1.0), dict(t_boq=300), dict(t_voladizo=240, t_boq=230),
                                dict(t_cama=200), dict(ventilador=300), dict(flujo=0), dict(capa_plana=0.6, capa_incl=0.1)])
def test_validation_rejects_bad_parameters(kw):
    with pytest.raises(ValueError):
        validar(Arbol(), Proceso(**kw))


def test_validation_rejects_bad_trees():
    p = Proceso()
    for ramas in ([], [Rama(2, 18.0)], [Rama(1, 3.0)], [Rama(1, 45.0)], [Rama(1, 18.0, largo=200.0)], [Rama(1, 18.0, ancho=11.0)],
                  [Rama(1, 18.0), Rama(-1, 20.0)], [Rama(1, 18.0, alto=1.0)]):
        with pytest.raises(ValueError):
            validar(Arbol(ramas=ramas), p)
    with pytest.raises(ValueError, match="no cabe en Z"):
        validar(Arbol(altura=300.0, ramas=[Rama(1, 18.0)]), Proceso(seco=True), maquina=dict(MAQUINA_XQ1, z_max=250.0))
    assert any("50" in a for a in validar(Arbol(ramas=[Rama(1, 18.0, largo=45.0)]), p))             # ancho total > 50 mm se avisa


# ---------------------------------------------------------------- geometria de la rama
def test_branch_layers_land_on_the_floor_plane_and_step_by_t_over_sin_alpha():
    p = Proceso()
    g = GeometriaRama(Rama(1, 18.0), p)
    capas = g.capas()
    assert 130 <= len(capas) <= 140
    # las capas sin tope de techo ni de extremo tienen la punta justo sobre el piso (z' = dz) y avanzan t/sen(alfa)
    puntas = [c for c in capas if c["z_hi"] == pytest.approx(g.dz, abs=1e-9)]
    assert len(puntas) > 40
    for a, b in zip(puntas, puntas[1:]):
        assert b["x_hi"] - a["x_hi"] == pytest.approx(p.capa_incl / math.sin(math.radians(p.alfa)), abs=1e-9)
    # el desplazamiento de una capa a la siguiente medido SOBRE el plano de la capa = t*tan(90-alfa): 0,299 mm = el 45 % del ancho
    paso_en_plano = (puntas[1]["x_hi"] - puntas[0]["x_hi"]) * math.cos(math.radians(p.alfa))
    assert paso_en_plano == pytest.approx(p.capa_incl * math.tan(math.radians(90 - p.alfa)), abs=1e-9)
    assert 1.0 - paso_en_plano / p.ancho == pytest.approx(contacto_de_la_punta(p), abs=1e-9)
    # las capas primeras (rincon pared-piso) no caben; las ultimas quedan limitadas por el techo
    assert capas[0]["k"] >= 2


def test_every_branch_bead_center_stays_inside_the_branch():
    p = Proceso()
    r = Rama(1, 18.0)
    g = GeometriaRama(r, p)
    for c in g.capas():
        for pq in ((0.0, 0.0), (c["U"], g.V), (c["U"] / 2, g.V / 2)):
            x, y, z = g.punto(c, pq)
            assert -p.traslape_pared + g.ext - 1e-9 <= x <= r.largo - g.ext + 1e-9
            assert g.dz - 1e-9 <= z <= r.alto - g.dz + 1e-9
            assert abs(y) <= g.V / 2 + 1e-9


# ---------------------------------------------------------------- generacion
def test_probeta_passes_the_independent_verification(probeta):
    texto, c, inf = probeta
    assert inf.ok, inf.texto()
    d = inf.datos
    assert d["capas_inclinadas"] == c.stats["capas_inclinadas"] == 135
    assert 0.97 <= d["volumen_ratio"] <= 1.05
    assert d["holgura_min"] >= 1.0                                                    # holgura del cabezal contra lo ya impreso
    assert d["hueco_grande_pct"] < 0.5 and d["sobra_pct"] < 1.0
    assert d["vz_max"] < MAQUINA_XQ1["vmax_z"]
    assert 20 < d["tiempo_min"] < 80


def test_flat_floor_in_the_simulated_section(probeta):
    texto, c, inf = probeta
    clave = [k for k in inf.datos if k.startswith("piso_")][0]
    m = re.search(r"piso ([+-][0-9.]+) a ([+-][0-9.]+) mm", inf.datos[clave])
    bajo, alto = float(m.group(1)), float(m.group(2))
    assert bajo >= -0.05 and alto <= 0.15                                             # el material mas bajo queda a +-0,1 mm del plano pedido


def test_full_tree_passes_and_has_two_branches_at_different_heights(arbol_completo):
    texto, c, inf = arbol_completo
    assert inf.ok, inf.texto()
    assert [r["lado"] for r in c.stats["ramas"]] == [1, -1]
    z1, z2 = (r["z_piso"] for r in c.stats["ramas"])
    assert z1 == pytest.approx(18.0, abs=0.2) and z2 == pytest.approx(34.0, abs=0.2) and z2 > z1 + 8.0
    assert c.arbol.ancho_total() == 50.0
    assert inf.datos["capas_inclinadas"] == 270


def test_tree_fits_in_50mm_width_and_bed(arbol_completo):
    texto, c, inf = arbol_completo
    xs = [w["X"] for n, w in _g1(texto) if "X" in w]
    ys = [w["Y"] for n, w in _g1(texto) if "Y" in w]
    print_xs = [w["X"] for n, w in _g1(texto) if "X" in w and "E" in w and w.get("Z", 0) > 1.0]
    assert max(print_xs) - min(print_xs) <= 50.0 + 1e-6                               # lo impreso cabe en 5 cm de ancho
    assert min(xs) >= 10.0 and max(xs) <= 290.0 and min(ys) >= 10.0 and max(ys) <= 290.0


def test_temperature_and_fan_sequence_around_each_branch(arbol_completo):
    texto, c, inf = arbol_completo
    L = [l.split(";", 1)[0].strip() for l in texto.splitlines()]
    L = [l for l in L if l]
    iM109R = [i for i, l in enumerate(L) if l == "M109 R220"]
    assert len(iM109R) == 2
    for i in iM109R:
        assert L[i - 1] == "M104 S220" and L[i - 2] == "M106 S255"                    # ventilador al maximo y baja la temperatura ANTES de esperar
    vuelta = [i for i, l in enumerate(L) if l == "M109 S230"]
    assert len(vuelta) == 3                                                          # arranque + una vuelta a 230 tras cada rama
    assert "M106 S100" in L                                                           # ventilador normal tras las ramas
    # el aparcamiento aparta el cabezal: va a X=220 (70 mm del centro) a una altura mayor que todo lo impreso
    apar = [(n, w) for n, w in _g1(texto) if w.get("X") == 220.0]
    assert len(apar) >= 4 and all(w["Z"] > 20.0 for n, w in apar)
    assert L[-1] == "M501" and "M104 S0" in L and "M140 S0" in L


def test_retract_and_prime_are_balanced(arbol_completo):
    texto, c, inf = arbol_completo
    e, retraido = 0.0, 0
    for n, w in _g1(texto):
        if "E" in w and "X" not in w and "Y" not in w and "Z" not in w:
            de = w["E"] - e
            if de < 0:
                retraido += 1
                assert de == pytest.approx(-1.0, abs=1e-4) and retraido == 1, n          # nunca dos retracciones seguidas
            elif de > 0 and retraido:
                retraido = 0
        if "E" in w:
            e = w["E"]
    assert retraido == 1                                                              # el archivo termina con el filamento retraido


def test_first_travel_after_homing_is_high(arbol_completo):
    texto, c, inf = arbol_completo
    L = [l for l in texto.splitlines() if l and not l.startswith(";")]
    i28 = L.index("G28")
    assert L[i28 + 1].startswith("G1 Z5")


def test_dry_run_has_no_extrusion_heating_or_fan():
    texto, c = generar(probeta_rama(), Proceso(seco=True))
    cod = [l.split(";", 1)[0].strip() for l in texto.splitlines()]
    assert not any(re.search(r"\bE-?\d", l) for l in cod if not l.startswith("G92"))
    assert not any(l.startswith(("M104", "M109", "M140", "M190", "M106")) for l in cod)
    zs = [w["Z"] for n, w in _g1(texto) if "Z" in w and "X" in w]
    assert min(zs) >= 10.0 - 1e-6                                                     # 10 mm sobre la cama
    inf = verificar(texto, c, con_seccion=False)
    assert inf.ok, inf.texto()


def test_dry_run_rejects_heating_commands_if_tampered():
    texto, c = generar(probeta_rama(), Proceso(seco=True))
    malo = texto.replace("G21\n", "G21\nM104 S200\n", 1)
    assert any("seco" in p for p in verificar(malo, c, con_seccion=False).problemas)


def test_generation_is_deterministic():
    a, _ = generar(probeta_rama())
    b, _ = generar(probeta_rama())
    assert a == b


def test_no_park_variant_has_no_parking_moves():
    texto, c = generar(probeta_rama(), Proceso(aparcar=False))
    assert not any(w.get("X") == 220.0 for n, w in _g1(texto))
    assert "M109 R220" in texto
    assert verificar(texto, c, con_seccion=False).ok


# ---------------------------------------------------------------- el verificador SI detecta fallos
def test_verifier_catches_a_head_collision():
    # con un cono permisivo (45) el generador acepta alfa = 30; el verificador, con el cabezal real, tiene que ver el choque
    # (rama de 14 mm de alto: con 8 mm las capas empinadas quedan cortadas por el techo y la pared nunca sube lo bastante para chocar)
    p = Proceso(alfa=30.0)
    texto, c = generar(probeta_rama(p, 20.0, 14.0), p, cono_deg=45.0)
    inf = verificar(texto, c, con_seccion=False)
    assert any("CHOCA" in x for x in inf.problemas), inf.texto()
    assert inf.datos["holgura_min"] < 0


def test_verifier_catches_text_level_errors(probeta):
    texto, c, _ = probeta
    casos = {
        "orden no permitida": texto.replace("G21\n", "G21\nM999\n", 1),
        "fuera de": texto.replace("G1 Z5.000 F600", "G1 Z9999.000 F600", 1),
        "antes de calentar": texto.replace("M109 S230\n", "", 1),
        "terminar con M501": texto.rstrip().rsplit("\n", 1)[0] + "\n",
        "movimiento antes de G28": texto.replace("G28\n", "", 1),
    }
    for fragmento, malo in casos.items():
        assert malo != texto, fragmento
        inf = verificar(malo, c, con_seccion=False)
        assert any(fragmento in x for x in inf.problemas), (fragmento, inf.problemas[:3])


def test_verifier_catches_a_removed_layer(probeta):
    texto, c, _ = probeta
    L = texto.splitlines()
    # quita el contenido de 6 capas inclinadas seguidas del medio de la rama (deja las etiquetas)
    idx = [i for i, l in enumerate(L) if l.startswith(";AITE inclinada")]
    a, b = idx[60], idx[66]
    malo = "\n".join(L[:a + 1] + [l for l in L[a + 1:b] if l.startswith((";", "M204"))] + L[b:]) + "\n"
    inf = verificar(malo, c)
    assert not inf.ok
    assert any("huecos" in x or "volumen" in x or "mm de recorrido" in x or "dentro del material" in x for x in inf.problemas), inf.problemas[:3]


PROLOGO = ["G21", "G90", "M82", "G92 E0", "M140 S70", "M190 S70", "M104 S180", "G28", "G1 Z5.000 F600", "M109 S230"]
EPILOGO = ["M104 S0", "M140 S0", "M501"]


def test_verifier_catches_a_travel_through_printed_material():
    _, c = generar(probeta_rama())
    prog = PROLOGO + [";AITE plano t=0.3000 w=0.6600", "G1 X140.000 Y150.000 Z10.000 F9000", "G1 X150.000 Y150.000 Z10.000 E1.0 F1200",
                      "G1 X145.000 Y150.000 Z5.000 F9000"] + EPILOGO                  # vuelve por DENTRO del cordon recien impreso
    inf = verificar("\n".join(prog) + "\n", c, con_seccion=False)
    assert any("por dentro del material" in x for x in inf.problemas), inf.problemas
    alto = [l.replace("Z5.000 F9000", "Z10.600 F9000") for l in prog]                 # el mismo viaje, pero por encima
    assert not any("por dentro del material" in x for x in verificar("\n".join(alto) + "\n", c, con_seccion=False).problemas)


def test_verifier_catches_a_tall_wall_next_to_the_nozzle():
    # una pared de 10 mm y la boquilla a 5 mm de ella a ras de cama: eleva la pared 10 mm sobre la punta a rho = 5 < 8 mm (bloque calefactor)
    _, c = generar(probeta_rama())
    prog = PROLOGO + [";AITE plano t=0.3000 w=0.6600", "G1 X140.000 Y150.000 Z10.000 F9000", "G1 X140.000 Y150.000 Z20.000 E1.0 F600",
                      "G1 X145.000 Y150.000 Z10.000 F9000"] + EPILOGO
    inf = verificar("\n".join(prog) + "\n", c, con_seccion=False)
    assert any("CHOCA" in x for x in inf.problemas), inf.problemas


# ---------------------------------------------------------------- modelo del cabezal y tiempo
def test_head_allowance_profile():
    h = load_head_profile()
    assert holgura_permitida(h, 5.0) == pytest.approx(8.0)                             # bajo el bloque calefactor
    assert holgura_permitida(h, 18.0) == pytest.approx(8.245)                          # bajo el conducto derecho
    assert holgura_permitida(h, 30.0) == pytest.approx(14.5)                           # rueda
    assert holgura_permitida(h, 45.0) == math.inf


def test_time_estimator_straight_and_corner():
    recto = [(100.0, 50.0, (1.0, 0.0, 0.0), 1000.0)]
    t = estimar_tiempo(recto)
    assert t == pytest.approx(50.0 / 1000.0 * 2 + (100.0 - 2 * 1.25) / 50.0, abs=0.01)   # acelera, crucero, frena
    dos = [(10.0, 20.0, (1.0, 0.0, 0.0), 500.0), (10.0, 20.0, (0.0, 1.0, 0.0), 500.0)]
    unidos = [(10.0, 20.0, (1.0, 0.0, 0.0), 500.0), (10.0, 20.0, (1.0, 0.0, 0.0), 500.0)]
    assert estimar_tiempo(dos) > estimar_tiempo(unidos)                                # la esquina de 90 grados frena


# ---------------------------------------------------------------- limites de la maquina y CLI
def test_machine_limits_match_the_tower_generator():
    ruta = os.path.join(os.path.dirname(__file__), "..", "..", "..", "herramientas", "torres", "generar_torres.py")
    spec = importlib.util.spec_from_file_location("generar_torres", ruta)
    gt = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gt)
    m = gt.MAQUINAS["xq1"]
    for k in ("cama", "z_max", "vmax_xy", "vmax_z", "boquilla_max", "cama_max"):
        assert float(MAQUINA_XQ1[k][0] if k == "cama" else MAQUINA_XQ1[k]) == float(m[k][0] if k == "cama" else m[k]), k
    assert MAQUINA_XQ1["margen"] == gt.MARGEN
    assert MAQUINA_XQ1["amax_z"] == 150.0                                              # DEFAULT_MAX_ACCELERATION { 4500, 4500, 150, 10000 }


def test_cli_writes_the_files_and_names_them(tmp_path, capsys):
    assert main(["arbol", "--probeta", "--salida", str(tmp_path) + "/"]) == 0
    assert (tmp_path / "probeta_rama90_PETG_xq1.gcode").exists()
    assert main(["arbol", "--probeta", "--seco", "--salida", str(tmp_path)]) == 0
    assert (tmp_path / "probeta_rama90_PETG_xq1_SECO.gcode").exists()
    out = capsys.readouterr().out
    assert "Sin problemas" in out and "Holgura minima" in out and "escrito" in out
    txt = (tmp_path / "probeta_rama90_PETG_xq1.gcode").read_text()
    assert txt.isascii() and "\r" not in txt


def test_cli_rejects_invalid_parameters_and_writes_nothing(tmp_path, capsys):
    assert main(["arbol", "--probeta", "--capa-incl", "0.2", "--salida", str(tmp_path)]) == 2
    assert main(["arbol", "--probeta", "--alfa", "40", "--salida", str(tmp_path)]) == 2
    assert main(["arbol", "--altura", "20", "--salida", str(tmp_path)]) == 2
    assert not list(tmp_path.iterdir())
    assert "error:" in capsys.readouterr().err
