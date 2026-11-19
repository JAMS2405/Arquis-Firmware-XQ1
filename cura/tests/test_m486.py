import random

import pytest

from arquis_engines.m486_engine import M486Options, process_gcode
from arquis_engines.marlin_model import simulate
from arquis_engines.validation_engine import validate_m486
from fixtures import Obj, grid_objects, instances_from, make_gcode
from helpers import check_cancel_k, flat, m486_lines, run


def _ok(res):
    assert res.status == "ok", res.summary()
    return res


def test_three_different_objects():
    objs = [Obj("Soporte", 20, 20, 60, 50, 3.0), Obj("Llavero", 120, 30, 140, 50, 2.0), Obj("Caja", 200, 150, 240, 190, 4.0)]
    res = _ok(run(objs, make_gcode(objs)))
    assert [o.id for o in res.objects] == [0, 1, 2]
    ml = m486_lines(res.chunks)
    assert ml.count("M486 T3") == 1
    assert {int(l.split("S")[1]) for l in ml if " S" in l} == {-1, 0, 1, 2}
    for k in range(3):
        check_cancel_k(res.chunks, k)


def test_ten_objects_mixed_heights():
    objs = grid_objects(10, "Pieza", height=2.0)
    objs[3].height = 1.0
    objs[7].height = 3.0
    res = _ok(run(objs, make_gcode(objs)))
    for k in range(10):
        check_cancel_k(res.chunks, k)


def test_thirty_identical_objects_are_independent_and_all_cancelable():
    """Capacidad de diseno: 30 copias del mismo modelo (mismo nombre), IDs 0-29, cada una cancelable por separado."""
    objs = grid_objects(30, "Llavero", w=20, d=20, gap=6, bed=300)
    res = _ok(run(objs, make_gcode(objs)))
    assert [o.id for o in res.objects] == list(range(30)) and [o.instance for o in res.objects] == list(range(30))
    assert m486_lines(res.chunks).count("M486 T30") == 1
    for k in range(30):
        check_cancel_k(res.chunks, k)


def test_fifty_identical_objects_by_default_only_30_are_cancelable():
    objs = grid_objects(50, "Llavero", w=20, d=20, gap=6, bed=300)
    res = _ok(run(objs, make_gcode(objs)))
    assert any(p.code == "W_OVER_CAPACITY" for p in res.problems)
    assert res.stats["cancelable"] == 30 and "M486 T30" in m486_lines(res.chunks)
    assert [o.id for o in res.objects][:30] == list(range(30)) and all(o.id == -1 for o in res.objects[30:])
    for k in (0, 17, 29):
        check_cancel_k(res.chunks, k)
    assert not any(m.skipped for m in simulate(res.chunks, []).moves)         # los 20 sobrantes se imprimen siempre


def test_fifty_identical_objects_expansion_path_needs_only_limits_not_code():
    """Ampliar en el futuro: basta subir el techo (limits.json + firmware); el motor no cambia."""
    objs = grid_objects(50, "Llavero", w=20, d=20, gap=6, bed=300)
    res = _ok(run(objs, make_gcode(objs), capacity=50, ceiling=64))
    assert len(res.objects) == 50 and [o.id for o in res.objects] == list(range(50))
    assert m486_lines(res.chunks).count("M486 T50") == 1
    for k in (0, 1, 17, 31, 32, 33, 49):
        check_cancel_k(res.chunks, k, capacity=64)


def test_scene_order_is_not_gcode_order_each_id_is_the_object_at_that_position():
    """Las copias salen en el G-code en un orden distinto al de la escena: el id debe seguir a la POSICION, no al orden."""
    objs = grid_objects(8, "Llavero")
    rnd = random.Random(7)

    def shuffled(layer, ids):
        ids = ids[:]
        rnd.shuffle(ids)
        return ids

    chunks = make_gcode(objs, order=shuffled)
    res = _ok(run(objs, chunks))
    # cada extrusion bajo M486 S<i> debe caer dentro de la caja de la instancia i
    for k in range(8):
        base, canc = check_cancel_k(res.chunks, k)
    base = simulate(res.chunks, [])
    for m in base.moves:
        if m.ctx >= 0 and m.de > 0 and m.xy_moved:
            o = objs[m.ctx]
            assert o.x0 - 0.5 <= m.x <= o.x1 + 0.5 and o.y0 - 0.5 <= m.y <= o.y1 + 0.5


def test_hundred_objects_over_capacity_are_printed_but_not_cancelable():
    objs = grid_objects(100, "Pieza", w=15, d=15, gap=4, height=1.0)
    res = _ok(run(objs, make_gcode(objs, final_z_in_last_mesh=True)))
    assert any(p.code == "W_OVER_CAPACITY" for p in res.problems)
    assert res.stats["cancelable"] == 30
    assert "M486 T30" in m486_lines(res.chunks)
    ids = {int(l.split("S")[1]) for l in m486_lines(res.chunks) if " S" in l}
    assert max(ids) == 29 and -1 in ids and all(i < 30 for i in ids)
    for k in (0, 15, 29):
        check_cancel_k(res.chunks, k)
    # los 36 restantes se imprimen siempre: nada se omite sin cancelar
    base = simulate(res.chunks, [])
    assert not any(m.skipped for m in base.moves)


def test_capacity_option_up_to_ceiling_32():
    objs = grid_objects(40, "Pieza", height=1.0)
    res = _ok(run(objs, make_gcode(objs), capacity=32))
    assert res.stats["cancelable"] == 32 and "M486 T32" in m486_lines(res.chunks)
    assert [o.id for o in res.objects].count(-1) == 8


def test_capacity_out_of_range_rejected():
    import arquis_engines.protocol_limits as L
    assert M486Options().capacity == L.DESIGN_CAPACITY == 30
    for bad in (0, -1, 33, 64, 129):
        with pytest.raises(ValueError):
            M486Options(capacity=bad)                            # por encima del techo actual (32) no se acepta
    M486Options(capacity=64, ceiling=64)                          # ampliacion futura: el techo se sube a proposito
    with pytest.raises(ValueError):
        M486Options(capacity=64, ceiling=129)                     # y nunca por encima del limite del protocolo (int8_t)


def test_prime_tower_skirt_and_support_are_never_cancelable():
    objs = grid_objects(4, "Pieza", height=1.5)
    chunks = make_gcode(objs, tower=(250, 250, 270, 270), skirt=True, support_nonmesh=True)
    res = _ok(run(objs, chunks))
    base = simulate(res.chunks, [])
    for k in range(4):
        _, canc = check_cancel_k(res.chunks, k)
        # la torre (250..270) se sigue construyendo igual
        tower_moves = [m for m in canc.moves if m.de > 0 and m.xy_moved and 250 <= m.x <= 270 and 250 <= m.y <= 270]
        base_tower = [m for m in base.moves if m.de > 0 and m.xy_moved and 250 <= m.x <= 270 and 250 <= m.y <= 270]
        assert len(tower_moves) == len(base_tower) > 0


@pytest.mark.parametrize("e_mode", ["abs", "rel"])
@pytest.mark.parametrize("final_z", [True, False])
def test_retractions_and_z_survive_cancelling_first_or_last_object_of_a_layer(e_mode, final_z):
    objs = grid_objects(6, "A", height=2.0)
    res = _ok(run(objs, make_gcode(objs, e_mode=e_mode, final_z_in_last_mesh=final_z)))
    for k in range(6):
        check_cancel_k(res.chunks, k)


def test_naive_per_mesh_insertion_loses_z_and_filament_the_model_detects_it():
    """Justifica R1/R2: insertar solo 'M486 S<id>' en cada ';MESH:' (como hacen los scripts sencillos) rompe Z y retracciones."""
    objs = grid_objects(3, "A", height=1.5)
    chunks = make_gcode(objs, final_z_in_last_mesh=True)
    res = _ok(run(objs, chunks))
    from arquis_engines.gcode_scan_engine import scan_gcode
    sc = scan_gcode(chunks)
    # id por tramo, el mismo que usa el motor
    from arquis_engines.matching_engine import match_sections
    from arquis_engines.object_engine import build_objects
    mt = match_sections(sc, build_objects(instances_from(objs), 30), 1.0)
    naive, first_layer = [], False
    for ci, chunk in enumerate(chunks):
        out = []
        for li, line in enumerate(chunk.split("\n")):
            if sc.first_layer_pos == (ci, li):
                out.append("M486 T3")
            out.append(line)
            s = sc.start_at.get((ci, li))
            if s is not None and sc.sections[s].kind == "mesh":
                out.append("M486 S%d" % mt.section_object[s])      # estilo cura-M486: solo en las lineas ;MESH:
        naive.append("\n".join(out))
    broke = 0
    for k in range(3):
        base = simulate(naive, [])
        canc = simulate(naive, [k])
        zs_b = [round(m.z, 3) for m in base.moves if m.ctx != k]
        zs_c = [round(m.z, 3) for m in canc.moves if m.ctx != k]
        if zs_b != zs_c or abs(base.final[2] - canc.final[2]) > 1e-9 or base.final != canc.final:
            broke += 1
    assert broke >= 2, "la insercion ingenua deberia fallar al cancelar al menos algunos objetos (fallaron %d de 3)" % broke
    # y el motor ARQUIS no
    for k in range(3):
        check_cancel_k(res.chunks, k)


def test_end_gcode_runs_even_if_last_object_is_cancelled():
    objs = grid_objects(3, "A", height=1.0)
    res = _ok(run(objs, make_gcode(objs)))
    sim = simulate(res.chunks, [0, 1, 2][-1:])
    flat_out = flat(res.chunks)
    last_s = max(i for i, l in enumerate(flat_out) if l.startswith("M486 S"))
    # tras el ultimo ;TIME_ELAPSED: el contexto es -1
    last_time = max(i for i, l in enumerate(flat_out) if l.startswith(";TIME_ELAPSED"))
    ctxs = [l for l in flat_out[:last_time + 6] if l.startswith("M486 S")]
    assert flat_out[last_time:].count("M486 S-1") >= 0
    assert simulate(res.chunks, []).final == simulate(res.chunks, [2]).final == (5.0, 295.0, simulate(res.chunks, []).final[2])


def test_one_at_a_time_mode():
    objs = grid_objects(4, "A", height=1.0)
    res = _ok(run(objs, make_gcode(objs, one_at_a_time=True)))
    for k in range(4):
        check_cancel_k(res.chunks, k)


def test_groups_are_one_object_with_shared_id():
    objs = [Obj("parte1", 20, 20, 40, 40, 1.5, group=100, group_name="Soporte+Tapa"),
            Obj("parte2", 60, 20, 80, 40, 1.5, group=100, group_name="Soporte+Tapa"),
            Obj("Llavero", 150, 20, 170, 40, 1.5)]
    res = _ok(run(objs, make_gcode(objs)))
    assert [o.id for o in res.objects] == [0, 1] and res.objects[0].name == "Soporte+Tapa" and len(res.objects[0].leaves) == 2
    assert "M486 T2" in m486_lines(res.chunks)
    base, canc = check_cancel_k(res.chunks, 0)
    # ambas partes del grupo se omiten
    assert sum(1 for m in canc.moves if m.skipped and m.de == 0 and m.ctx == 0) > 0


def test_position_id_order_option():
    objs = [Obj("A", 200, 200, 220, 220, 1.0), Obj("B", 10, 10, 30, 30, 1.0), Obj("C", 100, 100, 120, 120, 1.0)]
    res = _ok(run(objs, make_gcode(objs), id_order="position"))
    assert [(o.name, o.id) for o in sorted(res.objects, key=lambda o: o.id)] == [("B", 0), ("C", 1), ("A", 2)]
    for k in range(3):
        check_cancel_k(res.chunks, k)


def test_idempotent_second_pass_is_skipped_and_unchanged():
    objs = grid_objects(3, "A", height=1.0)
    first = _ok(run(objs, make_gcode(objs)))
    again = run(objs, first.chunks)
    assert again.status == "skipped" and again.chunks == first.chunks


def _tokens(line):
    code = line.split(";", 1)[0].split()
    return code[0], sorted(code[1:])


@pytest.mark.parametrize("travel_e", ["none", "retract", "both"])
def test_original_print_commands_are_preserved(travel_e):
    """Solo se anaden lineas M486 y la marca; las lineas originales siguen ahi. Las partidas (Z/E) se recomponen y deben dar las mismas palabras."""
    objs = grid_objects(5, "A", height=1.5)
    chunks = make_gcode(objs, travel_e=travel_e)
    res = _ok(run(objs, chunks))
    kept = [l for l in flat(res.chunks) if not (l.startswith("M486 ") or l.startswith(";ARQUIS_M486"))]
    orig = flat(chunks)
    p = 0
    for o in orig:
        l1 = kept[p]
        if o.startswith(";") or not o.strip() or l1 == o:
            assert l1 == o, (l1, o)
            p += 1
            continue
        if _tokens(l1)[1] == sorted(_tokens(o)[1]) and ";ARQ_" not in l1:        # linea sin partir (los tokens coinciden)
            p += 1
            continue
        l2 = kept[p + 1]                                                          # linea partida: dos lineas, una marcada ;ARQ_
        assert (";ARQ_" in l1) != (";ARQ_" in l2), (l1, l2, o)
        assert sorted(set(_tokens(l1)[1]) | set(_tokens(l2)[1])) == sorted(set(_tokens(o)[1])), (l1, l2, o)
        p += 2
    assert p == len(kept)


def test_tolerance_allows_small_scene_offset_with_warning():
    objs = grid_objects(3, "A", height=1.0)
    gcode = make_gcode(objs)
    shifted = [Obj(o.name, o.x0 + 0.6, o.y0, o.x1 + 0.6, o.y1, o.height) for o in objs]    # escena 0.6 mm desplazada
    res = process_gcode(gcode, instances_from(shifted), M486Options(tolerance_mm=1.0))
    assert res.status == "ok" and any(p.code == "W_TOLERANCE_USED" for p in res.problems)
    bad = process_gcode(gcode, instances_from(shifted), M486Options(tolerance_mm=0.1))
    assert bad.status == "blocked"


def test_lines_inserted_overhead_is_reported():
    objs = grid_objects(30, "A", w=15, d=15, gap=4, height=2.0)
    chunks = make_gcode(objs)
    res = _ok(run(objs, chunks))
    before = sum(len(c) for c in chunks)
    after = sum(len(c) for c in res.chunks)
    assert 0 < after - before < 0.25 * before, (before, after)
    assert res.stats["m486_lines"] > 0


@pytest.mark.parametrize("travel_e", ["none", "retract", "both"])
@pytest.mark.parametrize("travel_in", ["mesh", "nonmesh"])
@pytest.mark.parametrize("e_mode", ["abs", "rel"])
def test_both_travel_layouts(travel_in, e_mode, travel_e):
    objs = grid_objects(5, "A", height=1.5)
    res = _ok(run(objs, make_gcode(objs, travel_in=travel_in, e_mode=e_mode, travel_e=travel_e)))
    for k in range(5):
        check_cancel_k(res.chunks, k)


@pytest.mark.parametrize("seed", range(40))
def test_randomized_scenes_cancel_any_object_safely(seed):
    rnd = random.Random(seed)
    n = rnd.randint(1, 24)
    names = ["Llavero", "Soporte", "Tapa", "Base"]
    cells = [(i % 6, i // 6) for i in range(36)]
    rnd.shuffle(cells)
    objs = []
    for i in range(n):
        cx, cy = cells[i]
        w, d = rnd.uniform(8, 30), rnd.uniform(8, 30)
        x0, y0 = 10 + cx * 45 + rnd.uniform(0, 10), 10 + cy * 45 + rnd.uniform(0, 10)
        objs.append(Obj(rnd.choice(names), x0, y0, x0 + w, y0 + d, rnd.choice([0.7, 1.1, 1.5, 2.3])))
    chunks = make_gcode(objs, e_mode=rnd.choice(["abs", "rel"]), travel_in=rnd.choice(["mesh", "nonmesh"]), travel_e=rnd.choice(["none", "retract", "both"]),
                        final_z_in_last_mesh=rnd.random() < 0.7, skirt=rnd.random() < 0.5,
                        tower=(280, 280, 295, 295) if rnd.random() < 0.4 else None, one_at_a_time=rnd.random() < 0.2,
                        order=(lambda layer, ids, r=rnd: r.sample(ids, len(ids))) if rnd.random() < 0.5 else None)
    res = _ok(run(objs, chunks))
    for k in sorted({0, n - 1, rnd.randrange(n)}):
        check_cancel_k(res.chunks, k)
