#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Verifica el volcado que genera el plugin DENTRO de Cura (menu ARQUIS > Diagnostico) y, opcionalmente, el G-code exportado por esa misma sesion.

Uso: python3 cura/tools/verify_cura_capture.py <arquis_cura_captura.json> [<exportado.gcode>] [--machine-bed 300x300]

Cada comprobacion termina en una de cuatro etiquetas:  OK  |  FALLA  |  NO EJERCITADO (la escena no permite probarlo)  |  PENDIENTE (requiere accion fisica).
La conclusion "VALIDADO EN CURA REAL" solo se emite si el volcado dice ``source: "cura"`` (lo escribio Cura) y no hay FALLA. Un volcado ``fake``
(pruebas automaticas con nodos falsos) NUNCA valida la API de Cura."""
import json
import os
import sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "plugins", "ARQUISPlugin"))
import arquis_bootstrap  # noqa: E402

arquis_bootstrap.load_engines()
from arquis_engines import machine_profile as mp  # noqa: E402
from arquis_engines import manifest_engine as me  # noqa: E402
from arquis_engines import object_manager_model as omm  # noqa: E402
from arquis_engines import protocol_limits as limits  # noqa: E402
from arquis_engines.gcode_scan_engine import scan_gcode  # noqa: E402
from arquis_engines.object_engine import SceneInstance, build_objects, split_name  # noqa: E402
from arquis_engines.validation_engine import validate_manifest_vs_gcode, validate_m486  # noqa: E402

OK, FAIL, NOTRUN, PENDING = "OK", "FALLA", "NO EJERCITADO", "PENDIENTE"


class Report:
    def __init__(self):
        self.rows = []

    def add(self, key, label, status, detail=""):
        self.rows.append((key, label, status, detail))

    def failed(self):
        return any(s == FAIL for _, _, s, _ in self.rows)


def _instances(d):
    return [SceneInstance(i["scene_index"], i["name"], *i["bbox"], group=i["group"], group_name=i["group_name"], shape=i["shape"], rot=i["rot"], z1=i["z1"])
            for i in d["instances"]]


def check_api(d, r):
    errs = list(d.get("errors", [])) + [e for n in d.get("nodes", []) for e in n.get("errors", [])]
    r.add("api", "Llamadas a la API de Cura sin errores (getName, getBoundingBox, callDecoration, getConvexHull, getMultiBuildPlateModel...)",
          FAIL if errs else OK, "; ".join(errs[:6]))


def check_scene(d, r):
    inst = d["instances"]
    leaves = [n for n in d["nodes"] if n["is_printable_leaf"]]
    r.add("escena", "Captura de instancias: una instancia por malla imprimible de la escena", OK if inst and len(inst) == len(leaves) else FAIL,
          "%d nodos, %d mallas imprimibles segun la API, %d instancias capturadas" % (len(d["nodes"]), len(leaves), len(inst)))


def check_geometry(d, r, machine_bed):
    m = d["machine"]
    W, D, zero = float(m["width"]), float(m["depth"]), bool(m["center_is_zero"])
    ox, oy = (0.0, 0.0) if zero else (W / 2.0, D / 2.0)
    by_index = {n["index"]: n for n in d["nodes"]}
    bad = []
    for i in d["instances"]:
        b = by_index[i["scene_index"]]["bbox_scene"]
        exp = (b["left"] + ox, -b["front"] + oy, b["right"] + ox, -b["back"] + oy)                 # conversion recalculada con los numeros de CURA
        if any(abs(a - e) > 0.01 for a, e in zip(i["bbox"], exp)):
            bad.append("%s: capturado %s, esperado %s" % (i["name"], [round(x, 2) for x in i["bbox"]], [round(x, 2) for x in exp]))
        x0, y0, x1, y1 = i["bbox"]
        if not (x1 > x0 and y1 > y0):
            bad.append("%s: caja sin area" % i["name"])
    r.add("geometria", "Posiciones y dimensiones: la conversion de ejes del plugin coincide con los numeros que da Cura (x = X + ancho/2, y = -Z + fondo/2)",
          FAIL if bad else (OK if d["instances"] else NOTRUN), "; ".join(bad[:4]))
    if machine_bed is not None:
        mism = abs(W - machine_bed[0]) > 1 or abs(D - machine_bed[1]) > 1
        r.add("cama", "Perfil de Cura = maquina ARQUIS (%.0f x %.0f mm): el mapa solo es valido con la cama real" % machine_bed, FAIL if mism else OK,
              "el perfil de Cura tiene %.0f x %.0f mm" % (W, D) if mism else "")
        inside = [i["name"] for i in d["instances"] if i["bbox"][0] < -0.5 or i["bbox"][1] < -0.5 or i["bbox"][2] > machine_bed[0] + 0.5 or i["bbox"][3] > machine_bed[1] + 0.5]
        r.add("dentro", "Todas las instancias caen dentro de la cama de la maquina", FAIL if inside else OK, ", ".join(inside[:5]))


def check_duplicates(d, r):
    groups = defaultdict(list)
    for i in d["instances"]:
        groups[split_name(i["name"])[0]].append(i)
    dups = {k: v for k, v in groups.items() if len(v) > 1}
    if not dups:
        r.add("duplicados", "Duplicados identicos: copias del mismo modelo con posiciones distintas", NOTRUN, "la escena no tiene copias; carga 4 copias de un STL (Ctrl+C / Ctrl+V)")
        return
    problems = []
    for k, v in dups.items():
        if len({tuple(round(x, 2) for x in i["bbox"]) for i in v}) != len(v):
            problems.append("%s: copias con la MISMA caja" % k)
    objs = build_objects(_instances(d), limits.DESIGN_CAPACITY)
    ids = [o.id for o in objs if o.cancelable]
    if len(set(ids)) != len(ids):
        problems.append("IDs repetidos")
    r.add("duplicados", "Duplicados identicos (%s): cada copia tiene su caja y su ID" % ", ".join("%s x%d" % (k, len(v)) for k, v in dups.items()),
          FAIL if problems else OK, "; ".join(problems))


def check_sync(d, r, machine_bed):
    objs = build_objects(_instances(d), limits.DESIGN_CAPACITY)
    W, D = float(d["machine"]["width"]), float(d["machine"]["depth"])
    bed = tuple(d["bed"])
    vm = omm.build_view_model(objs, bed, machine_bed=machine_bed)
    if not vm["mapValid"]:
        r.add("sync", "Mapa y lista sincronizados", NOTRUN, "mapa deshabilitado: " + vm["bedMismatch"])
        return
    bad = []
    for o in objs:
        key = o.id if o.cancelable else -(o.scene_order + 1)
        vm2 = omm.build_view_model(objs, bed, selected=key, machine_bed=machine_bed)
        if [x["id"] for x in vm2["rows"] if x["selected"]] != [key] or [x["id"] for x in vm2["markers"] if x["selected"]] != [key]:
            bad.append("seleccionar %s desde la lista no resalta el mismo marcador" % key)
        mk = next(x for x in vm2["markers"] if x["id"] == key)
        if omm.pick_id(vm2, (mk["x0"] + mk["x1"]) // 2, (mk["y0"] + mk["y1"]) // 2) != key and not any(
                p["id"] != key and p["x0"] <= (mk["x0"] + mk["x1"]) // 2 <= p["x1"] and p["y0"] <= (mk["y0"] + mk["y1"]) // 2 <= p["y1"] and p["w"] * p["h"] < mk["w"] * mk["h"] for p in vm2["markers"]):
            bad.append("tocar el marcador %s no lo selecciona" % key)
    r.add("sync", "Mapa y lista sincronizados (seleccion en las dos direcciones, %d objetos)" % len(objs), FAIL if bad else OK, "; ".join(bad[:3]))


def check_gcode(d, r, path, machine_bed):
    text = open(path, encoding="utf-8", errors="replace").read()
    chunks = [text]
    scan = scan_gcode(chunks)
    marker = scan.has_arquis_marker or ""
    r.add("gcode_marca", "G-code exportado por Cura con el script ARQUIS_M486 (marca ;ARQUIS_M486 ... STATUS=OK)", OK if "STATUS=OK" in marker else FAIL, marker or "sin marca: el script no se ejecuto")
    objs = [o for o in build_objects(_instances(d), limits.DESIGN_CAPACITY)]
    res = me.parse_text(text, (round(machine_bed[0] * 10), round(machine_bed[1] * 10)) if machine_bed else None)
    if res.manifest is None:
        r.add("manifiesto", "Manifiesto ARQUIS_OBJECTS_V1 presente y valido (con la cama de la maquina)", FAIL, "; ".join("%s %s" % (p.code, p.message) for p in res.problems[:3]))
        return
    m = res.manifest
    r.add("manifiesto", "Manifiesto ARQUIS_OBJECTS_V1 presente y valido (con la cama de la maquina)", OK, "%d objetos" % len(m.objects))
    by_id = {o.id: o for o in objs if o.cancelable}
    diff = []
    if len(m.objects) != len(by_id):
        diff.append("N manifiesto %d != instancias cancelables %d" % (len(m.objects), len(by_id)))
    for mo in m.objects:
        o = by_id.get(mo.id)
        if o is None:
            diff.append("id %d sin instancia" % mo.id)
            continue
        x0, y0, x1, y1 = o.bbox
        if abs(mo.cx - round((x0 + x1) / 2 * 10)) > 1 or abs(mo.cy - round((y0 + y1) / 2 * 10)) > 1 or abs(mo.w - round((x1 - x0) * 10)) > 1 or abs(mo.d - round((y1 - y0) * 10)) > 1:
            diff.append("id %d: posicion/tamano del manifiesto != escena de Cura" % mo.id)
    r.add("manifiesto_vs_escena", "Manifiesto = escena capturada en Cura (centro y tamano de cada ID, +-0,1 mm)", FAIL if diff else OK, "; ".join(diff[:4]))
    probs = [p for p in validate_manifest_vs_gcode(chunks, m, 1.0) + validate_m486(chunks, limits.DESIGN_CAPACITY, objs) if p.severity == "error"]
    r.add("gcode_vs_manifiesto", "G-code real vs manifiesto: lo que se imprime en cada M486 cae dentro de la caja de su objeto, M486 seguro (validador)", FAIL if probs else OK,
          "; ".join("%s" % p.code for p in probs[:5]))


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    machine = (float(mp.BED_X_MM), float(mp.BED_Y_MM))
    if "--machine-bed" in argv:
        w, _, h = argv[argv.index("--machine-bed") + 1].partition("x")
        machine = (float(w), float(h))
        args = [a for a in args if a != argv[argv.index("--machine-bed") + 1]]
    if not args:
        print(__doc__)
        return 2
    d = json.load(open(args[0], encoding="utf-8"))
    r = Report()
    assert d.get("schema") == "arquis_cura_capture_v1", "no es un volcado del plugin ARQUIS"
    check_api(d, r)
    check_scene(d, r)
    check_geometry(d, r, machine)
    check_duplicates(d, r)
    check_sync(d, r, machine)
    if len(args) > 1:
        check_gcode(d, r, args[1], machine)
    else:
        r.add("gcode", "G-code exportado, manifiesto y validacion", NOTRUN, "pasa el .gcode exportado como segundo argumento")
    r.add("orientacion", "Orientacion fisica ORG=FL (el origen X0 Y0 es la esquina frontal izquierda)", PENDING, "imprime cura/tools/make_orientation_gcode.py; hasta entonces ORG=FL es provisional")
    print("Captura: Cura %s, plugin %s, origen de los datos: %s" % (d.get("cura", {}).get("version") or "?", d.get("arquis_plugin") or "?", d.get("source")))
    for key, label, st, det in r.rows:
        print("  [%-13s] %s%s" % (st, label, ("\n                    -> " + det) if det else ""))
    real = d.get("source") == "cura"
    if r.failed():
        print("\nRESULTADO: FALLA (ver arriba)")
        return 1
    if not real:
        print("\nRESULTADO: las comprobaciones pasan, pero el volcado NO viene de Cura real (source=%r): NO VALIDA la integracion con Cura." % d.get("source"))
        return 0
    pend = [k for k, _, s, _ in r.rows if s == NOTRUN]
    if pend:
        print("\nRESULTADO: sin fallos en Cura real, pero hay comprobaciones SIN EJERCITAR (%s): la integracion NO se da por validada todavia." % ", ".join(pend))
    else:
        print("\nRESULTADO: VALIDADO EN CURA REAL (la orientacion fisica es independiente: sigue PENDIENTE hasta imprimir la prueba).")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
