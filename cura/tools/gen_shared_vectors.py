#!/usr/bin/env python3
"""Genera los VECTORES DE PRUEBA compartidos del protocolo ARQUIS_OBJECTS_V1 en shared/object_protocol/test_vectors/:
   - bed_map_v1.json     entradas y salidas EXACTAS (enteros) de las funciones del mapa de cama
   - manifest_v1.json    textos de G-code (cabecera + cola) con el resultado esperado del parser
El parser de Python (referencia) los cumple por construccion; el parser y el mapa del firmware (Etapa 3) deben cumplirlos igual.
Uso: python3 cura/tools/gen_shared_vectors.py [--check]"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "cura", "plugins", "ARQUISPlugin"))
import arquis_bootstrap  # noqa: E402

arquis_bootstrap.load_engines()
import arquis_engines.bed_map_engine as bm  # noqa: E402
import arquis_engines.manifest_engine as me  # noqa: E402

OUT = os.path.join(ROOT, "shared", "object_protocol", "test_vectors")


def bed_map_vectors():
    cases = []
    beds = [bm.Bed(0, 0, 3000, 3000), bm.Bed(0, 0, 2200, 2200), bm.Bed(0, 0, 4000, 2000), bm.Bed(-1500, -1500, 3000, 3000)]
    views = [bm.View(6, 36, 266, 266), bm.View(6, 36, 266, 266, 2), bm.View(6, 36, 266, 266, 4, 400, 400), bm.View(0, 0, 200, 120, 1)]
    pts = [(0, 0), (1, 1), (400, 400), (1500, 1500), (2999, 1), (3000, 3000), (-1500, -1500), (123, 2877), (1100, 1100)]
    for b in beds:
        for v in views:
            for x, y in pts:
                cases.append({"fn": "to_px", "bed": list(map(int, (b.min_x, b.min_y, b.width, b.depth))),
                              "view": [v.x, v.y, v.w, v.h, v.zoom, v.center_x, v.center_y], "in": [x, y], "out": list(bm.to_px(x, y, b, v))})
    b, v = beds[0], views[0]
    objs = [(0, 1500, 1500, 1000, 1000, "S"), (1, 400, 400, 40, 40, "S"), (2, 1500, 500, 2000, 150, "R"), (3, 500, 500, 800, 300, "C"),
            (4, 2999, 2999, 600, 600, "C"), (5, 1, 1, 1, 1, "R"), (6, 1040, 1000, 100, 100, "S"), (7, 1000, 1000, 100, 100, "S")]
    marks = [bm.marker(i, cx, cy, w, d, sh, b, v) for i, cx, cy, w, d, sh in objs]
    for (i, cx, cy, w, d, sh), m in zip(objs, marks):
        cases.append({"fn": "marker", "bed": [0, 0, 3000, 3000], "view": [6, 36, 266, 266, 1, None, None], "in": [i, cx, cy, w, d, sh],
                      "out": [m.x0, m.y0, m.x1, m.y1, m.cx, m.cy]})
    for px, py in [(m.cx, m.cy) for m in marks] + [(0, 0), (140, 170), (marks[0].x0, marks[0].y0)]:
        cases.append({"fn": "pick", "markers": [[m.id, m.x0, m.y0, m.x1, m.y1] for m in marks], "in": [px, py], "out": bm.pick(marks, px, py)})
    cases.append({"fn": "overlaps", "markers": [[m.id, m.x0, m.y0, m.x1, m.y1] for m in marks], "out": [list(p) for p in bm.overlaps(marks)]})
    for a in (-7, -1, 0, 1, 7, 99, 100001):
        for d in (1, 2, 3, 250, 3000):
            cases.append({"fn": "div_round", "in": [a, d], "out": bm.div_round(a, d)})
    st = []
    for seq in (False, True):
        for z_print in (0, 2, 20, 21):
            for order, active in ((0, -1), (0, 0), (1, 0), (0, 1)):
                for canceled in (False, True):
                    for available, trusted in ((True, True), (False, True), (True, False)):
                        kw = dict(available=available, trusted=trusted, canceled=canceled, seq=seq, z0=2, ztop=20, order=order, z_print=z_print, active_order=active)
                        st.append({"fn": "object_state", "in": kw, "out": bm.object_state(**kw)})
    return {"version": 1, "comment": "Aritmetica entera; div_round(a,b)=floor((2a+b)/(2b)) con division por SUELO. Y crece hacia arriba en pantalla (atras).",
            "constants": {"MIN_MARKER_PX": bm.MIN_MARKER_PX, "VIEW_PAD_PX": bm.VIEW_PAD_PX}, "states": bm.STATE_TEXT, "cases": cases + st}


def make(n, names, **mut):
    m = me.Manifest(bed_w=3000, bed_d=3000, cap=30)
    m.names = list(names)
    for i in range(n):
        m.objects.append(me.ManifestObject(i, i % len(names), i // len(names), 200 + (i % 6) * 450, 200 + (i // 6) * 450, 300, 300, (i * 7) % 360,
                                           "CSR"[i % 3], 3, 40 + i, i))
    for k, v in mut.items():
        setattr(m, k, v)
    m.pid = me._pid(m, me.body_lines(m, with_pid=False)[1:])
    return m


def render(m):
    h, b = me.render_block(m)
    return h, b


def manifest_vectors():
    V = []
    head = ";FLAVOR:Marlin\n;TIME:100\n"
    settings = "".join(";SETTING_3 " + "x" * 70 + "\n" for _ in range(40))

    def add(name, text, bed_check=None, note=""):
        r = me.parse_text(text, bed_check)
        exp = {"ok": r.ok, "errors": sorted({p.code for p in r.problems if p.severity == "error"}),
               "warnings": sorted({p.code for p in r.problems if p.severity == "warning"})}
        if r.ok:
            exp["manifest"] = r.manifest.to_dict()
        V.append({"name": name, "note": note, "bed_check": list(bed_check) if bed_check else None, "text": text, "expect": exp})

    g1 = make(1, ["Pieza"]); h, b = render(g1); add("valid_one_object", h + head + "G28\n" + b)
    g30 = make(30, ["Llavero", "Soporte", "Tapa"]); h, b = render(g30); good = h + head + "G1 X1\n" + b
    add("valid_30_objects_3_names", good, note="caso tipico de diseno: 30 objetos, IDs 0-29")
    add("valid_30_with_settings_tail", good + settings, note="Cura anade ;SETTING_3 despues del G-code")
    add("valid_30_crlf", good.replace("\n", "\r\n"), note="Cura en Windows escribe CRLF; el CRC es sobre lineas normalizadas")
    add("valid_30_bed_check_ok", good, (3000, 3000))
    gu = make(2, ["Piñón ñandú"], ); h, b = render(gu); add("valid_utf8_name", h + head + b, note="nombre UTF-8 (13 bytes)")
    add("no_manifest_old_gcode", head + "G28\nG1 X1 Y1 E1\n" * 20, note="G-code antiguo: no es un error; se imprime normal")
    add("no_manifest_beyond_tail_window", good + "".join(";SETTING_3 " + "x" * 70 + "\n" for _ in range(300)), note="el bloque queda fuera de los ultimos 16 KB")
    add("bad_bed_mismatch", good, (2200, 2200), note="la cama del manifiesto no es la de la maquina")
    add("bad_crc_data_changed", good.replace(";ARQ:O 0 0 0", ";ARQ:O 0 0 1", 1))
    add("bad_truncated_no_end", good[:good.index(";ARQUIS_OBJECTS_END")])
    add("bad_no_trailer", good[:good.index(";ARQUIS_TRAILER")])
    add("bad_trailer_pid", good.replace("PID=%08X N=30\n" % g30.pid, "PID=DEADBEEF N=30\n"))
    add("bad_header_mismatch", good.replace("N=30\n", "N=29\n", 1))
    for name, fn in (("dup_id", lambda m: setattr(m.objects[1], "id", 0)), ("id_range", lambda m: setattr(m.objects[29], "id", 40)),
                     ("outside_bed", lambda m: setattr(m.objects[0], "cx", 99999)), ("size_zero", lambda m: setattr(m.objects[0], "w", 0)),
                     ("z0_above_ztop", lambda m: setattr(m.objects[0], "z0", 999)), ("rotation", lambda m: setattr(m.objects[0], "rot", 400)),
                     ("name_index", lambda m: setattr(m.objects[0], "name_idx", 9)), ("name_too_long", lambda m: m.names.__setitem__(0, "x" * 40)),
                     ("order", lambda m: setattr(m.objects[0], "order", 5)), ("cap_above_ceiling", lambda m: setattr(m, "cap", 33)),
                     ("version_2", lambda m: setattr(m, "version", 2))):
        m = make(30, ["Llavero", "Soporte", "Tapa"]); fn(m); m.pid = me._pid(m, me.body_lines(m, with_pid=False)[1:])
        h, b = render(m); add("bad_" + name, h + head + b)
    m = make(31, ["Llavero"]); h, b = render(m); add("bad_over_capacity_31", h + head + b, note="31 objetos > 30")
    return {"version": 1, "tail_window": me.TAIL_WINDOW, "header_window": me.HEADER_WINDOW,
            "comment": "El firmware debe dar el mismo ok/errores/warnings que el parser de referencia para cada texto.", "cases": V}


def main():
    files = {"bed_map_v1.json": bed_map_vectors(), "manifest_v1.json": manifest_vectors()}
    rc = 0
    for fn, data in files.items():
        txt = json.dumps(data, indent=1, ensure_ascii=False, sort_keys=False) + "\n"
        p = os.path.join(OUT, fn)
        if "--check" in sys.argv:
            if not os.path.exists(p) or open(p, encoding="utf-8").read() != txt:
                print("DESACTUALIZADO:", p); rc = 1
            else:
                print("al dia:", os.path.relpath(p, ROOT))
        else:
            os.makedirs(OUT, exist_ok=True)
            open(p, "w", encoding="utf-8", newline="\n").write(txt)
            print("escrito", os.path.relpath(p, ROOT), "(%d casos)" % len(data["cases"]))
    return rc


if __name__ == "__main__":
    sys.exit(main())
