#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""ARQUIS Non-Planar: ONDULADO de un G-code normal (capas no planas con volumen). EXPERIMENTAL, desactivado por defecto, sin pruebas fisicas.

Toma el G-code de Cura de una pieza normal y le suma a Z una onda que crece con la altura (con compensacion de extrusion), para que las
capas queden onduladas y entrelazadas. Flujo obligatorio:
  1. --init-config warp.json                      escribe una configuracion con el cabezal SIN CONFIRMAR
  2. (tu) editas warp.json: amplitude, wavelength, z_in, ramp... y head.confirmed=true cuando midas tu cabezal
  3. --enable-experimental --config warp.json --input pieza.gcode --preview vista.svg
        calcula, verifica y escribe la VISTA PREVIA e imprime su CODIGO. No escribe G-code.
  4. (tu) revisas vista.svg
  5. --enable-experimental --config warp.json --input pieza.gcode --export pieza_ondulada.gcode --accept <CODIGO>
Sin --enable-experimental no hace nada. No esta conectado a Cura."""
import argparse
import json
import os
import sys
from dataclasses import asdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "plugins", "ARQUISPlugin"))
import arquis_bootstrap  # noqa: E402

arquis_bootstrap.load_engines()
import arquis_engines.nonplanar_engine as npe  # noqa: E402
import arquis_engines.nonplanar_warp as nw  # noqa: E402


def template() -> dict:
    d = asdict(nw.WarpConfig())
    d.pop("machine")
    d["enabled"] = False
    d["_leeme"] = ("EXPERIMENTAL. amplitude = pico a pico (mm); wavelength (mm); z_in = Z donde empieza (por encima de la primera capa); ramp = mm de transicion; "
                   "z_out = Z donde termina (null = hasta arriba); function = 'wave', 'radial' o una expresion de x, y, r, A, L. head.* son valores SIN MEDIR hasta que confirmes.")
    return d


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--init-config", metavar="ARCHIVO.json")
    ap.add_argument("--config", metavar="ARCHIVO.json")
    ap.add_argument("--enable-experimental", action="store_true")
    ap.add_argument("--input", metavar="PIEZA.gcode")
    ap.add_argument("--preview", metavar="SALIDA.svg")
    ap.add_argument("--export", metavar="SALIDA.gcode")
    ap.add_argument("--accept", metavar="CODIGO")
    a = ap.parse_args(argv)
    if a.init_config:
        with open(a.init_config, "w", encoding="utf-8") as f:
            json.dump(template(), f, indent=2, ensure_ascii=False)
        print("escrito %s (cabezal SIN CONFIRMAR). Editalo antes de continuar." % a.init_config)
        return 0
    if not a.enable_experimental:
        print("Ondulador DESACTIVADO por defecto (experimental, sin pruebas fisicas). Usa --enable-experimental para ejecutarlo.")
        return 2
    if not a.input:
        print("Falta --input pieza.gcode")
        return 2
    d = {}
    if a.config:
        with open(a.config, encoding="utf-8") as f:
            d = {k: v for k, v in json.load(f).items() if not k.startswith("_")}
    d["enabled"] = True
    try:
        cfg = nw.warp_config_from_dict(d)
        with open(a.input, encoding="utf-8", errors="replace") as f:
            res = nw.warp_gcode(f.read(), cfg)
    except nw.WarpRefused as e:
        print("RECHAZADO: %s" % e)
        return 3
    print("Hallazgos:")
    for fd in res.findings:
        print("  [%s] %s: %s" % (fd.level, fd.code, fd.msg))
    print("Resumen: " + ", ".join("%s=%.3f" % kv for kv in res.stats.items()))
    print("CODIGO DE VISTA PREVIA: %s" % res.digest)
    if a.preview:
        with open(a.preview, "w", encoding="utf-8") as f:
            f.write(nw.render_warp_preview(res))
        print("vista previa escrita en %s (abrela en un navegador y revisala)" % a.preview)
    if a.export:
        try:
            text = nw.export_warped(res, a.accept or "")
        except npe.ExportRefused as e:
            print("EXPORTACION RECHAZADA: %s" % e)
            return 3
        with open(a.export, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("G-code ONDULADO EXPERIMENTAL escrito en %s. Sin pruebas fisicas: una mano en el paro de emergencia." % a.export)
    return 0 if res.ok else 1


if __name__ == "__main__":
    sys.exit(main())
