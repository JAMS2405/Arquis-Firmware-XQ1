import math
from typing import List, Sequence

from arquis_engines.marlin_model import simulate
from arquis_engines.m486_engine import M486Options, process_gcode


def run(objs, chunks, **opt):
    from fixtures import instances_from
    return process_gcode(chunks, instances_from(objs), M486Options(**opt))


def flat(chunks: Sequence[str]) -> List[str]:
    return "\n".join(chunks).split("\n")


def m486_lines(chunks):
    return [l for l in flat(chunks) if l.startswith("M486")]


def check_cancel_k(out_chunks: Sequence[str], k: int, capacity: int = 30, legacy: bool = False, eps: float = 1e-6):
    """Cancela el objeto k desde el principio y compara con la ejecucion normal.

    Garantias comprobadas (todas con el modelo de Marlin, no con el motor ARQUIS):
      1. El objeto k NO mueve nada ni extruye (cada una de sus extrusiones queda omitida).
      2. Todo lo demas se ejecuta igual: mismas X, Y, Z tras cada orden (la altura Z nunca se pierde).
      3. El filamento fisico empujado por las ordenes que no son del objeto k es identico (retracciones equilibradas).
      4. El G-code final (aparcado, etc.) termina en la misma posicion.
    """
    base = simulate(out_chunks, [], capacity, legacy)
    canc = simulate(out_chunks, [k], capacity, legacy)
    assert len(base.moves) == len(canc.moves)
    skipped_extrusion = 0.0
    for b, c in zip(base.moves, canc.moves):
        assert b.line == c.line
        if b.ctx == k:
            if b.de > 1e-9 and b.xy_moved:                 # extrusion del objeto k
                assert c.skipped and abs(c.de) < eps and not c.xy_moved, "extrusion del objeto %d no omitida (linea %d)" % (k, b.line)
                skipped_extrusion += b.de
            continue
        assert not c.skipped, "linea %d (ctx %d) omitida al cancelar el objeto %d" % (b.line, b.ctx, k)
        assert abs(b.z - c.z) < eps, "linea %d: Z %.3f != %.3f al cancelar el objeto %d (altura perdida)" % (b.line, b.z, c.z, k)
        if b.xy_cmd:
            assert abs(b.x - c.x) < eps and abs(b.y - c.y) < eps, \
                "linea %d: destino XY %s != %s al cancelar el objeto %d" % (b.line, (b.x, b.y), (c.x, c.y), k)
        assert abs(b.de - c.de) < eps, "linea %d: filamento %.4f != %.4f al cancelar el objeto %d" % (b.line, b.de, c.de, k)
    assert abs(base.physical_e - canc.physical_e - skipped_extrusion) < 1e-4
    assert all(abs(a - b) < eps for a, b in zip(base.final, canc.final))
    return base, canc
