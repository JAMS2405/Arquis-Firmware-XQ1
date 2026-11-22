# SPDX-License-Identifier: LGPL-3.0-or-later
"""Linea de comandos de AITE (Fase 1). Solo analiza y genera mallas de prueba: NO escribe G-code.

    python -m aite info pieza.stl
    python -m aite analyze pieza.stl [--beta-c 55 | --c-min 0.5] [--alpha-max 20 | --head perfil.json] [--margin 3] [--strict]
    python -m aite head [perfil.json]
    python -m aite make-test-part salida.stl [--trunk 20x20x40] [--branch-len 15] [--branch-section 10x10] [--branch-z0 15] [--axis x|y] [--single]

Codigos de salida: 0 correcto; 1 con --strict si hay regiones infactibles o la malla no es estanca; 2 entrada invalida.
"""
import argparse
import sys
from typing import List, Optional, Tuple

from . import __version__
from .head import HeadProfileError, load_head_profile
from .mesh import MeshError, analyze_mesh, load_mesh, write_3mf, write_stl_binary
from .overhang import analyze_overhangs
from .parts import make_cross_part
from .physics import beta_c_deg, max_feasible_overhang


def _dims(s: str, n: int) -> Tuple[float, ...]:
    try:
        v = tuple(float(x) for x in s.lower().split("x"))
    except ValueError:
        raise argparse.ArgumentTypeError("formato invalido: %r (use por ejemplo 20x20x40)" % s)
    if len(v) != n or min(v) <= 0:
        raise argparse.ArgumentTypeError("se esperaban %d cotas positivas separadas por x: %r" % (n, s))
    return v


def _cmd_info(a) -> int:
    rep = analyze_mesh(load_mesh(a.file))
    (x0, y0, z0), (x1, y1, z1) = rep.bbox
    print("Malla: %d vertices, %d triangulos" % (rep.n_vertices, rep.n_triangles))
    print("Caja: X %.2f..%.2f  Y %.2f..%.2f  Z %.2f..%.2f mm (%.2f x %.2f x %.2f)" % (x0, x1, y0, y1, z0, z1, x1 - x0, y1 - y0, z1 - z0))
    print("Volumen: %.2f mm3" % rep.volume)
    if rep.watertight:
        print("Estanca: si")
        return 0
    print("Estanca: NO")
    for p in rep.problems():
        print("  - " + p)
    return 1 if a.strict else 0


def _cmd_head(a) -> int:
    h = load_head_profile(a.profile)
    print("Cabezal: %s" % h.name)
    if h.source:
        print("Fuente: %s" % h.source)
    print("Margen termico: %.2f mm" % h.thermal_margin_mm)
    print("%-52s %8s %8s %10s" % ("Pieza", "eta mm", "rho mm", "alfa max"))
    for name, eta, rho, al in h.table():
        print("%-52s %8.2f %8.2f %9.1f" % (name[:52], eta, rho, al) + "°")
    lim = h.limiting()
    print("Cono unico (la menor): %.1f°, fijado por: %s" % (h.cone_alpha_deg(), lim.name))
    return 0


def _cmd_analyze(a) -> int:
    mesh = load_mesh(a.file)
    notes: List[str] = []
    if a.beta_c is not None:
        bc = a.beta_c
    else:
        bc = beta_c_deg(a.layer_h, a.bead_w, a.c_min)
        notes.append("beta_c = %.1f° calculado con c_min = %.2f (SUPUESTO; se mide en el ensayo E2)" % (bc, a.c_min))
    if a.alpha_max is not None:
        am = a.alpha_max
    else:
        h = load_head_profile(a.head)
        am = h.cone_alpha_deg()
        notes.append("alfa_max = %.1f° del cono unico del perfil del cabezal (%s)" % (am, h.limiting().name))
    res = analyze_overhangs(mesh, bc, am, a.layer_h, a.cell, a.margin, a.min_cells)
    rep = res.mesh_report
    print("AITE %s - analisis de voladizos" % __version__)
    print("Malla: %d triangulos, volumen %.1f mm3, estanca: %s" % (rep.n_triangles, rep.volume, "si" if rep.watertight else "NO"))
    for p in rep.problems():
        print("  ! " + p)
    if res.bad_columns:
        print("  ! %d columnas con numero impar de cruces: resultados poco fiables" % res.bad_columns)
    print("Parametros: beta_c = %.1f°, alfa_max = %.1f°, margen = %.1f°, capa = %.2f mm, celda = %.2f mm" % (bc, am, a.margin, a.layer_h, a.cell))
    for n in notes:
        print("  nota: " + n)
    print("Voladizo maximo sin soporte (beta_c + alfa_max): %.1f°" % max_feasible_overhang(bc, am))
    f = res.facets
    print("Facetas (area de superficie, sin la base): con voladizo > beta_c: %.1f mm2; fuera de la ventana: %.1f mm2; beta_s maximo %.1f°"
          % (f.overhang_area, f.infeasible_area, f.max_beta_deg))
    if f.normals_flipped:
        print("  ! las normales apuntan hacia dentro: se analizo con ellas invertidas (el modelo no se modifica)")
    for lo, hi, area in f.bins:
        if area > 0:
            print("    %2.0f-%2.0f°: %8.1f mm2" % (lo, hi, area))
    print("Capas: %d, regiones sin apoyo: %d (infactibles: %d)" % (res.layers, len(res.regions), len(res.infeasible_regions)))
    shown = sorted(res.regions, key=lambda r: (-(r.deficit_deg or 0.0), r.layer))[:a.top]
    if shown:
        print("  %5s %8s %-9s %-5s %9s %9s %7s %9s %8s" % ("capa", "z mm", "tipo", "lados", "area mm2", "prof. mm", "beta_s", "inclin.", "deficit"))
        for r in shown:
            bs = "%.0f°" % r.beta_s_deg if r.beta_s_deg is not None else "?"
            rt = "%.0f°" % r.required_tilt_deg if r.required_tilt_deg is not None else "?"
            df = "%.0f°" % r.deficit_deg if r.deficit_deg is not None else "?"
            print("  %5d %8.2f %-9s %-5s %9.2f %9.2f %7s %9s %8s" % (r.layer, r.z_mm, r.kind, r.sides or "-", r.area_mm2, r.depth_mm, bs, rt, df))
        if len(res.regions) > len(shown):
            print("  (%d regiones mas; use --top para ver mas)" % (len(res.regions) - len(shown)))
    bad = bool(res.infeasible_regions) or res.facets.infeasible_area > 0.0
    if bad:
        print("VEREDICTO: hay zonas que no se pueden imprimir sin soporte con este cono; deficit maximo %.1f° (el modelo no se modifica)." % res.max_deficit_deg)
    else:
        print("VEREDICTO: todas las zonas con voladizo caen dentro de la ventana de inclinacion.")
    return 1 if (a.strict and (bad or not rep.watertight)) else 0


def _cmd_make(a) -> int:
    mesh = make_cross_part(a.trunk, a.branch_len, a.branch_section, a.branch_z0, a.axis, not a.single, a.cell)
    if a.out.lower().endswith(".3mf"):
        write_3mf(mesh, a.out)
    else:
        write_stl_binary(mesh, a.out)
    rep = analyze_mesh(mesh)
    print("Pieza escrita en %s: %d triangulos, volumen %.1f mm3, estanca: %s" % (a.out, rep.n_triangles, rep.volume, "si" if rep.watertight else "NO"))
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="aite", description="AITE Fase 1: analisis geometrico de voladizos (no genera G-code)")
    sub = p.add_subparsers(dest="cmd", required=True)
    i = sub.add_parser("info", help="datos y estanqueidad de una malla")
    i.add_argument("file")
    i.add_argument("--strict", action="store_true")
    i.set_defaults(fn=_cmd_info)
    h = sub.add_parser("head", help="perfil del cabezal y cono unico")
    h.add_argument("profile", nargs="?", default=None)
    h.set_defaults(fn=_cmd_head)
    a = sub.add_parser("analyze", help="detecta voladizos y calcula la inclinacion necesaria")
    a.add_argument("file")
    a.add_argument("--beta-c", type=float, default=None, help="angulo critico de apoyo (grados); si falta se calcula con --c-min")
    a.add_argument("--c-min", type=float, default=0.5, help="apoyo minimo entre cordones (0..1) para calcular beta_c (SUPUESTO hasta medir E2)")
    a.add_argument("--alpha-max", type=float, default=None, help="inclinacion maxima de capa (grados); si falta se toma del perfil del cabezal")
    a.add_argument("--head", default=None, help="JSON del perfil del cabezal (por defecto el de la XQ1)")
    a.add_argument("--margin", type=float, default=0.0, help="margen de seguridad en grados sobre la inclinacion necesaria")
    a.add_argument("--layer-h", type=float, default=0.2)
    a.add_argument("--bead-w", type=float, default=0.45)
    a.add_argument("--cell", type=float, default=0.25)
    a.add_argument("--min-cells", type=int, default=2)
    a.add_argument("--top", type=int, default=12)
    a.add_argument("--strict", action="store_true")
    a.set_defaults(fn=_cmd_analyze)
    m = sub.add_parser("make-test-part", help="genera la pieza de prueba: tronco con ramas horizontales")
    m.add_argument("out")
    m.add_argument("--trunk", type=lambda s: _dims(s, 3), default=(20.0, 20.0, 40.0))
    m.add_argument("--branch-len", type=float, default=15.0)
    m.add_argument("--branch-section", type=lambda s: _dims(s, 2), default=(10.0, 10.0))
    m.add_argument("--branch-z0", type=float, default=15.0)
    m.add_argument("--axis", choices=("x", "y"), default="x")
    m.add_argument("--single", action="store_true", help="una sola rama")
    m.add_argument("--cell", type=float, default=0.5)
    m.set_defaults(fn=_cmd_make)
    return p


def main(argv: Optional[List[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.fn(args)
    except (MeshError, HeadProfileError, ValueError, OSError) as e:
        print("error: %s" % e, file=sys.stderr)
        return 2
