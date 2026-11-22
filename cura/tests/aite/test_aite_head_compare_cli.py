# SPDX-License-Identifier: LGPL-3.0-or-later
"""Perfil del cabezal, comparador de volumen y linea de comandos."""
import json

import pytest

from aite.cli import main
from aite.columns import ColumnGrid, LayerStack, set_to_rows
from aite.compare import compare_stacks
from aite.head import HeadProfile, HeadProfileError, Primitive, load_head_profile
from aite.parts import make_box, make_cross_part

SMALL = ["--trunk", "6x6x8", "--branch-len", "4", "--branch-section", "3x3", "--branch-z0", "3"]


# ---------------------------------------------------------------- cabezal
def test_default_profile_gives_the_report_cone():
    h = load_head_profile()
    t = {name: a for name, _, _, a in h.table()}
    assert t["conducto izquierdo (vista frontal, eje X)"] == pytest.approx(27.8, abs=0.1)
    assert t["conducto derecho (vista frontal, eje X)"] == pytest.approx(22.25, abs=0.1)
    assert t["bloque calefactor con manguito, lado del riel X"] == pytest.approx(26.4, abs=0.1)
    assert t["bloque calefactor, lado opuesto al riel"] == pytest.approx(45.4, abs=0.1)
    assert h.cone_alpha_deg() == pytest.approx(20.1, abs=0.1)
    assert "rueda" in h.limiting().name and h.single_cone


def test_thermal_margin_only_affects_hot_parts():
    h = load_head_profile()
    base = {name: a for name, _, _, a in h.table()}
    h.thermal_margin_mm = 1.0
    hot = {name: a for name, _, _, a in h.table()}
    assert hot["bloque calefactor, lado opuesto al riel"] == pytest.approx(41.5, abs=0.1)
    assert hot["rueda del carro, borde lejano (conservador)"] == base["rueda del carro, borde lejano (conservador)"]
    assert hot["conducto derecho (vista frontal, eje X)"] == base["conducto derecho (vista frontal, eje X)"]


def test_cone_is_the_minimum_of_all_primitives():
    h = HeadProfile("x", [Primitive("a", 10, 10), Primitive("b", 4, 10), Primitive("c", 10, 4)])
    assert h.cone_alpha_deg() == pytest.approx(21.8, abs=0.1)
    assert h.limiting().name == "b"
    with pytest.raises(ValueError):
        HeadProfile("vacio").cone_alpha_deg()


def test_profile_loader_rejects_bad_files(tmp_path):
    for content in ("no json", json.dumps({}), json.dumps({"primitives": []}), json.dumps({"primitives": [{"name": "x", "eta": -1, "rho": 5}]}),
                    json.dumps({"primitives": [{"name": "x", "eta": 1}]}),
                    json.dumps({"primitives": [{"name": "x", "eta": 1, "rho": 5}], "thermal_margin_mm": -1})):
        p = tmp_path / "h.json"
        p.write_text(content)
        with pytest.raises(HeadProfileError):
            load_head_profile(str(p))
    with pytest.raises(HeadProfileError):
        load_head_profile(str(tmp_path / "no_existe.json"))


# ---------------------------------------------------------------- comparador
def _stack(mesh, cell=0.25, h=0.2):
    return LayerStack.from_grid(ColumnGrid(mesh, cell), h)


def test_identical_stacks_have_no_difference():
    s = _stack(make_cross_part((6, 6, 8), 4, (3, 3), 3.0))
    c = compare_stacks(s, s)
    assert c.missing_mm3 == 0 and c.excess_mm3 == 0 and c.model_mm3 == c.deposited_mm3 == pytest.approx(6 * 6 * 8 + 24 * 3)
    assert c.missing_pct == 0 and c.excess_pct == 0


def test_missing_and_excess_volumes_are_measured():
    model = _stack(make_box((4.0, 4.0, 2.0)))
    dep = _stack(make_box((4.0, 4.0, 2.0)))
    # quita la capa 3 entera (4x4 mm x 0,2 mm = 3,2 mm3) y anade un bloque de 1x1 mm x 0,2 mm fuera del modelo
    dep.rows[3] = [0] * dep.ny
    dep.rows.append([0] * dep.ny)
    dep.rows[-1] = set_to_rows([(i, j) for i in range(4) for j in range(4)], dep.ny)       # 16 celdas de 0,25 mm = 1 mm2 en la capa 10
    c = compare_stacks(model, dep)
    assert c.missing_mm3 == pytest.approx(3.2)
    assert c.excess_mm3 == pytest.approx(1.0 * 0.2)
    assert c.missing_pct == pytest.approx(100 * 3.2 / 32.0)


def test_tolerance_band_ignores_a_one_cell_shift():
    model = _stack(make_box((4.0, 4.0, 2.0)))
    dep = _stack(make_box((4.0, 4.0, 2.0)))
    model.nx = dep.nx = model.nx + 2                                                         # margen para que el desplazamiento no salga de la rejilla
    dep.rows = [[(r << 1) & ((1 << dep.nx) - 1) for r in layer] for layer in dep.rows]      # desplaza una celda en x
    strict = compare_stacks(model, dep)
    assert strict.missing_mm3 > 0 and strict.excess_mm3 > 0
    tolerant = compare_stacks(model, dep, eps_xy_cells=1)
    assert tolerant.missing_mm3 == 0 and tolerant.excess_mm3 == 0


def test_vertical_tolerance_ignores_a_one_layer_shift():
    model = _stack(make_box((4.0, 4.0, 2.0)))
    dep = _stack(make_box((4.0, 4.0, 2.0)))
    dep.rows = [[0] * dep.ny] + dep.rows[:-1]
    assert compare_stacks(model, dep).missing_mm3 > 0
    assert compare_stacks(model, dep, eps_z_layers=1).missing_mm3 == 0


def test_incompatible_stacks_are_rejected():
    with pytest.raises(ValueError):
        compare_stacks(_stack(make_box((4.0, 4.0, 2.0))), _stack(make_box((4.0, 4.0, 2.0)), h=0.1))
    with pytest.raises(ValueError):
        compare_stacks(_stack(make_box((4.0, 4.0, 2.0))), _stack(make_box((5.0, 4.0, 2.0))))


# ---------------------------------------------------------------- linea de comandos
def test_cli_make_info_analyze_end_to_end(tmp_path, capsys):
    stl = str(tmp_path / "pieza.stl")
    assert main(["make-test-part", stl] + SMALL) == 0
    assert "estanca: si" in capsys.readouterr().out
    assert main(["info", stl, "--strict"]) == 0
    out = capsys.readouterr().out
    assert "Estanca: si" in out and "Volumen: 360.00" in out                     # 6x6x8 + 2 x 4x3x3
    # cono unico del perfil (20,1) y beta_c supuesto de 48,4: el 90 queda fuera
    assert main(["analyze", stl]) == 0
    out = capsys.readouterr().out
    assert "regiones sin apoyo: 2 (infactibles: 2)" in out and "VEREDICTO: hay zonas" in out and "SUPUESTO" in out
    assert main(["analyze", stl, "--strict"]) == 1
    capsys.readouterr()
    assert main(["analyze", stl, "--beta-c", "55", "--alpha-max", "45", "--strict"]) == 0
    assert "todas las zonas" in capsys.readouterr().out


def test_cli_3mf_and_head_command(tmp_path, capsys):
    p = str(tmp_path / "pieza.3mf")
    assert main(["make-test-part", p, "--single"] + SMALL) == 0
    capsys.readouterr()
    assert main(["info", p]) == 0 and "Volumen: 324.00" in capsys.readouterr().out
    assert main(["head"]) == 0
    out = capsys.readouterr().out
    assert "Cono unico" in out and "20.1" in out


def test_cli_errors_return_2(tmp_path, capsys):
    assert main(["info", str(tmp_path / "no_existe.stl")]) == 2
    bad = tmp_path / "mala.stl"
    bad.write_bytes(b"basura")
    assert main(["analyze", str(bad)]) == 2
    assert main(["head", str(tmp_path / "nada.json")]) == 2
    assert main(["make-test-part", str(tmp_path / "x.stl"), "--branch-len", "15.3"]) == 2
    assert "error:" in capsys.readouterr().err


def test_cli_strict_flags_open_mesh(tmp_path, capsys):
    from aite.mesh import write_stl_binary
    m = make_box((4.0, 4.0, 4.0))
    m.triangles.pop()
    p = str(tmp_path / "abierta.stl")
    write_stl_binary(m, p)
    assert main(["info", p]) == 0
    assert main(["info", p, "--strict"]) == 1
    assert "Estanca: NO" in capsys.readouterr().out
