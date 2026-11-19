# SPDX-License-Identifier: LGPL-3.0-or-later
"""G-code REAL de Cura 5.11.0 (aportado por el propietario): 3 modelos distintos y 4 copias del mismo STL."""
import re

import pytest

from arquis_engines.m486_engine import M486Options, process_gcode
from arquis_engines.marlin_model import simulate
from arquis_engines.validation_engine import validate_m486
from helpers import check_cancel_k, flat, m486_lines
from real_gcode import derive_scene, load_full, load_reduced, split_like_cura

CASES = [("real_3_distintos", "86e52ddd-3_diferentes.gcode", 3), ("real_4_iguales", "7718e7dd-4_iguales.gcode", 4)]


def _process(text, chunking, rename=None):
    chunks = [text] if chunking == "single" else split_like_cura(text)
    inst = derive_scene(chunks, rename=rename)
    return chunks, inst, process_gcode(chunks, inst, M486Options())


@pytest.mark.parametrize("chunking", ["single", "by_layer"])
@pytest.mark.parametrize("name,_full,n", CASES)
def test_real_gcode_is_processed_and_every_object_cancels_safely(name, _full, n, chunking):
    chunks, inst, res = _process(load_reduced(name), chunking)
    assert res.status == "ok", res.summary()
    assert len(res.objects) == n and res.stats["cancelable"] == n
    assert validate_m486(res.chunks, 30, res.objects) == [] or all(p.severity != "error" for p in validate_m486(res.chunks, 30, res.objects))
    for k in range(n):
        check_cancel_k(res.chunks, k)


def test_real_retractions_are_g0_with_e_inside_the_object_and_get_split():
    """Hallazgo con G-code real: CuraEngine escribe 'G0 X.. Y.. E<retraido>' dentro del tramo del objeto."""
    text = load_reduced("real_3_distintos")
    assert re.search(r"^G0 X[\d.]+ Y[\d.]+ E[\d.]+$", text, re.M)
    _, _, res = _process(text, "single")
    assert res.stats["travel_e_splits"] > 0 and res.stats["filament_protected"] > 0
    # ninguna linea con E queda dentro de un objeto salvo las extrusiones reales (G1 con XY y E creciente)
    assert not [p for p in validate_m486(res.chunks, 30, res.objects) if p.severity == "error"]


def test_real_gcode_layout_structure():
    chunks, _, res = _process(load_reduced("real_4_iguales"), "by_layer")
    out = flat(res.chunks)
    first_layer = next(i for i, l in enumerate(out) if l == ";LAYER:0")
    assert out[first_layer - 1] == "M486 T4"                                   # M486 T justo antes de la capa 0, tras el G-code de inicio
    assert sum(1 for l in out if l.startswith("M486 T")) == 1
    last_time = max(i for i, l in enumerate(out) if l.startswith(";TIME_ELAPSED:"))
    tail = out[last_time:]
    assert any(l.startswith("G1 X0 Y310") for l in tail)                       # el G-code final del usuario sigue ahi
    ctx = -1
    for l in out[:last_time + 1]:
        if l.startswith("M486 S"):
            ctx = int(l.split("S")[1].split()[0])
    # al terminar la ultima capa el contexto vuelve a S-1 antes de cualquier movimiento del G-code final
    first_motion = next(i for i, l in enumerate(tail) if l.startswith(("G0", "G1")))
    assert "M486 S-1" in tail[:first_motion + 1] or ctx == -1
    for k in range(4):
        base, canc = check_cancel_k(res.chunks, k)
        assert base.final == canc.final                                        # aparca igual aunque se cancele el ultimo objeto


def test_real_identical_copies_renamed_to_the_same_name_are_still_distinguished():
    """Cura 5.11 renombra las copias (pinon.stl(1)...); si no lo hiciera (nombres iguales), la geometria las distingue igualmente."""
    text = load_reduced("real_4_iguales")
    renamed = re.sub(r"^;MESH:pinon\.stl\(\d\)$", ";MESH:pinon.stl", text, flags=re.M)
    assert renamed.count(";MESH:pinon.stl\n") == 16 and ";MESH:pinon.stl(1)" not in renamed      # 4 copias x 4 capas, todas con el mismo nombre
    chunks = split_like_cura(renamed)
    # escena: 4 instancias con el MISMO nombre y las cajas reales de cada copia (derivadas del original sin renombrar)
    inst = derive_scene(split_like_cura(text), rename=lambda n: "pinon.stl")
    assert {i.name for i in inst} == {"pinon.stl"} and len(inst) == 4
    res = process_gcode(chunks, inst, M486Options())
    assert res.status == "ok", res.summary()
    assert [o.instance for o in res.objects] == [0, 1, 2, 3] and [o.id for o in res.objects] == [0, 1, 2, 3]
    base = simulate(res.chunks, [])
    for m in base.moves:                                                       # cada extrusion bajo S<i> cae en la caja de la copia i
        if m.ctx >= 0 and m.de > 1e-9 and m.xy_moved:
            b = inst[m.ctx]
            assert b.x0 <= m.x <= b.x1 and b.y0 <= m.y <= b.y1
    for k in range(4):
        check_cancel_k(res.chunks, k)


def test_real_scene_that_does_not_match_blocks_and_leaves_gcode_untouched():
    text = load_reduced("real_4_iguales")
    chunks = split_like_cura(text)
    inst = derive_scene(chunks)
    from arquis_engines.object_engine import SceneInstance
    moved = [SceneInstance(i.scene_index, i.name, i.x0 + 12, i.y0, i.x1 + 12, i.y1) for i in inst]
    res = process_gcode(chunks, moved, M486Options())
    assert res.status == "blocked" and not m486_lines(res.chunks)
    assert res.chunks[1:] == chunks[1:]


@pytest.mark.parametrize("name,fname,n", CASES)
def test_full_real_files_if_available(name, fname, n):
    text = load_full(fname)
    if text is None:
        pytest.skip("defina ARQUIS_REAL_GCODE_DIR con los G-code completos")
    chunks, inst, res = _process(text, "by_layer")
    assert res.status == "ok", res.summary()
    for k in range(n):
        check_cancel_k(res.chunks, k)
