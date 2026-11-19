# SPDX-License-Identifier: LGPL-3.0-or-later
"""El motor de cancelacion REAL de Marlin (cancel_object.cpp + M486.cpp compilados en el PC con tests/capacity) frente al modelo en Python.

Dos motores, ambos REALES (codigo de Marlin, no un modelo):
  * ``stock``: el de la rama estable SIN modificar (copia en tests/capacity/stock_marlin) = el "antes" y el defecto de los indices.
  * ``dev``  : el de esta rama (Marlin/src, Etapa 3): rechaza los indices fuera de 0-29.
Son pruebas SIMULADAS en el PC (el codigo del motor es el real; la placa y el planificador no intervienen).
"""
import os
import shutil
import subprocess

import pytest

from arquis_engines.m486_engine import M486Options, process_gcode
from arquis_engines.marlin_model import simulate
from fixtures import grid_objects, instances_from, make_gcode
from real_gcode import derive_scene, load_reduced, split_like_cura

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CAP = os.path.join(ROOT, "tests", "capacity")
MARLIN = os.path.join(ROOT, "Marlin", "src")

pytestmark = pytest.mark.skipif(shutil.which("g++") is None or not os.path.exists(os.path.join(MARLIN, "feature", "cancel_object.cpp")),
                                reason="necesita g++ y el codigo de Marlin del repositorio")


def _build(tmp_path_factory, which):
    out = tmp_path_factory.mktemp("real_engine_" + which)
    subprocess.run([os.path.join(CAP, "build_real_engine.sh"), str(out), which], check=True, capture_output=True)
    return os.path.join(str(out), "harness")


@pytest.fixture(scope="module")
def harness(tmp_path_factory):
    return _build(tmp_path_factory, "stock")


@pytest.fixture(scope="module")
def harness_dev(tmp_path_factory):
    return _build(tmp_path_factory, "dev")


def run_real(harness, tmp_path, chunks, cancel):
    p = tmp_path / "t.gcode"
    p.write_text("\n".join(chunks), encoding="utf-8")
    r = subprocess.run([harness, str(p), ",".join(map(str, cancel)) or "-"], capture_output=True, text=True, check=True)
    return [tuple(map(int, l.split()))[:3] for l in r.stdout.splitlines()]


def model_trace(chunks, cancel, capacity=30, legacy=True):
    tr = simulate(chunks, cancel, capacity=capacity, legacy=legacy)
    return [(m.line, m.ctx, int(m.skipped)) for m in tr.moves]


def test_bit_macros_in_the_prelude_are_exactly_marlins():
    macros = open(os.path.join(MARLIN, "core", "macros.h"), encoding="utf-8").read()
    prelude = open(os.path.join(CAP, "shim", "prelude.h"), encoding="utf-8").read()
    for line in ("#define _BV(b) (1 << (b))", "#define TEST(n,b) (!!((n)&_BV(b)))", "#define SBI(A,B) (A |= _BV(B))", "#define CBI(A,B) (A &= ~_BV(B))"):
        assert line in macros and line in prelude


@pytest.mark.parametrize("travel_e", ["none", "both"])
def test_real_engine_cancels_each_of_the_30_design_objects(harness, tmp_path, travel_e):
    objs = grid_objects(30, "Llavero", height=1.0)
    res = process_gcode(make_gcode(objs, travel_e=travel_e), instances_from(objs), M486Options())
    assert res.status == "ok"
    for k in range(30):
        real = run_real(harness, tmp_path, res.chunks, [k])
        assert real == model_trace(res.chunks, [k]), "el motor real y el modelo difieren al cancelar el objeto %d" % k
        assert any(ctx == k and sk == 1 for _, ctx, sk in real)                           # el objeto k se omite de verdad
        assert not any(sk == 1 and ctx != k for _, ctx, sk in real)                       # y solo el k


def test_real_engine_with_nothing_cancelled_skips_nothing(harness, tmp_path):
    objs = grid_objects(30, "A", height=1.0)
    res = process_gcode(make_gcode(objs), instances_from(objs), M486Options())
    assert not any(sk for _, _, sk in run_real(harness, tmp_path, res.chunks, []))


@pytest.mark.parametrize("name,n", [("real_3_distintos", 3), ("real_4_iguales", 4)])
def test_real_engine_on_real_cura_gcode(harness, tmp_path, name, n):
    chunks = split_like_cura(load_reduced(name))
    res = process_gcode(chunks, derive_scene(chunks), M486Options())
    assert res.status == "ok"
    for k in range(n):
        assert run_real(harness, tmp_path, res.chunks, [k]) == model_trace(res.chunks, [k])


HOSTILE = [0, 1, 29, 30, 31, 32, 33, 63, 64, 100, 127, 128, 129, 200, 255, 256, 257, 300, 1000, 32767, -1, -2, -128, -129, -255, -256, -32768]


def test_python_model_reproduces_real_engine_for_hostile_indices(harness, tmp_path):
    """Valida el modelo 'motor actual': S/P/U/C/T con indices fuera de rango se comportan igual en el motor real y en el modelo."""
    lines = []
    for v in HOSTILE:
        lines += ["M486 T50", "M486 P5", "M486 P%d" % v, "G1 X1", "M486 S%d" % v, "G1 X2", "M486 U%d" % v, "G1 X3", "M486 S5", "G1 X4",
                  "M486 S%d P%d" % (v, v), "G1 X5", "M486 T%d" % v, "G1 X6", "M486 S-1", "G1 X7"]
    real = run_real(harness, tmp_path, ["\n".join(lines)], [])
    assert real == model_trace(["\n".join(lines)], [])
    assert len(real) == len(HOSTILE) * 7


def test_real_engine_current_limits_are_what_the_docs_say(harness, tmp_path):
    """Documenta el motor ACTUAL: valido 0-31 (mascara uint32_t); 32+ se trunca a int8_t y puede coincidir con otro objeto."""
    def active_after(v):
        t = ["M486 T50\nM486 S%d\nG1 X1\n" % v]
        return run_real(harness, tmp_path, t, [])[0][1]
    assert [active_after(v) for v in (0, 29, 30, 31)] == [0, 29, 30, 31]
    assert active_after(32) == 32 and active_after(127) == 127          # activos pero NO cancelables (mascara de 32 bits)
    assert active_after(256) == 0 and active_after(300) == 44           # alias: el defecto que la Etapa 3 debe cerrar
    assert active_after(128) == -128


# ---- Motor de esta rama (Etapa 3) -------------------------------------------------------------------------------------------------
@pytest.mark.parametrize("travel_e", ["none", "both"])
def test_dev_engine_cancels_each_of_the_30_design_objects(harness_dev, tmp_path, travel_e):
    objs = grid_objects(30, "Llavero", height=1.0)
    res = process_gcode(make_gcode(objs, travel_e=travel_e), instances_from(objs), M486Options())
    assert res.status == "ok"
    for k in range(30):
        real = run_real(harness_dev, tmp_path, res.chunks, [k])
        assert real == model_trace(res.chunks, [k], legacy=False), "motor nuevo y modelo difieren al cancelar el objeto %d" % k
        assert any(ctx == k and sk == 1 for _, ctx, sk in real) and not any(sk == 1 and ctx != k for _, ctx, sk in real)


@pytest.mark.parametrize("name,n", [("real_3_distintos", 3), ("real_4_iguales", 4)])
def test_dev_engine_on_real_cura_gcode(harness_dev, tmp_path, name, n):
    chunks = split_like_cura(load_reduced(name))
    res = process_gcode(chunks, derive_scene(chunks), M486Options())
    for k in range(n):
        assert run_real(harness_dev, tmp_path, res.chunks, [k]) == model_trace(res.chunks, [k], legacy=False)


def test_dev_engine_rejects_every_out_of_range_index_without_touching_another_object(harness_dev, tmp_path):
    """Hostiles de 16 y de 32 bits: ni S activa ni P cancela ningun objeto fuera de 0-29 (el defecto del motor estable: S256 -> objeto 0)."""
    def run(code):
        return run_real(harness_dev, tmp_path, ["M486 T30\n" + code + "\nG1 X1\n"], [])[0]          # (linea, activo, skipping)
    for v in HOSTILE + [30, 31, 65536, 65537, 2 ** 31 - 1, -2 ** 31, 2 ** 32, 2 ** 32 + 5]:
        if 0 <= v <= 29:
            continue
        _, active, skipping = run("M486 S%d" % v)
        assert (active, skipping) == (-1, 0), "S%d no debe activar nada (activo=%d)" % (v, active)
        p = subprocess.run([harness_dev, "/dev/stdin", "-"], input="M486 T30\nM486 P%d\nG1 X1\n" % v, capture_output=True, text=True, check=True).stdout.split()
        assert p[3] == "0", "P%d no debe cancelar nada (mascara=%s)" % (v, p[3])
    assert [run("M486 S%d" % v)[1] for v in (0, 15, 29)] == [0, 15, 29]
    assert run("M486 S-1")[1:] == (-1, 0)


def test_dev_engine_t_is_clamped_and_never_cancels(harness_dev, tmp_path):
    # M486 T200 -> 30 objetos como maximo; T-5 -> 0; no queda nada cancelado
    for code in ("M486 T200", "M486 T-5", "M486 T65536"):
        out = subprocess.run([harness_dev, "/dev/stdin", "-"], input=code + "\nG1 X1\n", capture_output=True, text=True, check=True).stdout.split()
        assert out[3] == "0" and out[2] == "0"


def test_dev_engine_matches_hardened_python_model_for_hostile_indices(harness_dev, tmp_path):
    lines = []
    for v in HOSTILE:
        lines += ["M486 T30", "M486 P5", "M486 P%d" % v, "G1 X1", "M486 S%d" % v, "G1 X2", "M486 U%d" % v, "G1 X3", "M486 S5", "G1 X4",
                  "M486 S%d P%d" % (v, v), "G1 X5", "M486 T%d" % v, "G1 X6", "M486 S-1", "G1 X7"]
    real = run_real(harness_dev, tmp_path, ["\n".join(lines)], [])
    assert real == model_trace(["\n".join(lines)], [], legacy=False)
