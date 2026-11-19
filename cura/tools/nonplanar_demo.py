#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""ARQUIS Non-Planar (Etapa 5) - ejemplo pequeno y controlado. EXPERIMENTAL, desactivado por defecto, sin pruebas fisicas.

Flujo obligatorio (cada paso es una barrera de seguridad):
  1. --init-config nonplanar.json            escribe una configuracion con el cabezal SIN CONFIRMAR y sin temperaturas
  2. (tu) mide tu hotend, edita el archivo: head.confirmed=true, temperaturas, y los valores del cabezal
  3. --enable-experimental --config nonplanar.json --preview vista.svg
        calcula, verifica y escribe la VISTA PREVIA; imprime su CODIGO. No escribe G-code.
  4. (tu) abre vista.svg y la revisas
  5. --enable-experimental --config nonplanar.json --export prueba_nonplanar.gcode --accept <CODIGO>
        solo exporta si no hay hallazgos ERROR, el cabezal esta confirmado y el codigo coincide.
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
import arquis_engines.nonplanar_surface as nps  # noqa: E402


def template() -> dict:
    d = asdict(npe.NPConfig())
    d.pop("machine")                        # la maquina es la del firmware y no se edita aqui
    d["enabled"] = False
    d["_leeme"] = ("EXPERIMENTAL. head.* son valores SIN MEDIR: mide tu hotend (radio y altura del bloque calefactor y del ducto sobre la punta, "
                   "pendiente maxima que tolera el cono) y pon head.confirmed=true. bed_temp y nozzle_temp son obligatorias para exportar.")
    return d


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--init-config", metavar="ARCHIVO.json")
    ap.add_argument("--config", metavar="ARCHIVO.json")
    ap.add_argument("--enable-experimental", action="store_true", help="activa el motor (esta desactivado por defecto)")
    ap.add_argument("--preview", metavar="SALIDA.svg")
    ap.add_argument("--export", metavar="SALIDA.gcode")
    ap.add_argument("--accept", metavar="CODIGO", help="codigo de la vista previa revisada")
    ap.add_argument("--analyze-stl", metavar="MODELO.stl", help="SOLO analiza la superficie de un STL (pendiente, relieve, agujeros); no genera G-code")
    ap.add_argument("--cell", type=float, default=1.0, help="tamano de celda del analisis (mm)")
    a = ap.parse_args(argv)
    if a.init_config:
        with open(a.init_config, "w", encoding="utf-8") as f:
            json.dump(template(), f, indent=2, ensure_ascii=False)
        print("escrito %s (cabezal SIN CONFIRMAR, sin temperaturas). Editalo antes de continuar." % a.init_config)
        return 0
    if not a.enable_experimental:
        print("Motor no planar DESACTIVADO por defecto (experimental, sin pruebas fisicas). Usa --enable-experimental para ejecutarlo.")
        return 2
    d = {}
    if a.config:
        with open(a.config, encoding="utf-8") as f:
            d = {k: v for k, v in json.load(f).items() if not k.startswith("_")}
    d["enabled"] = True
    cfg = npe.config_from_dict(d)
    if a.analyze_stl:
        with open(a.analyze_stl, "rb") as f:
            surf = nps.rasterize(nps.read_stl(f.read()), a.cell)
        rep = nps.analyze_surface(surf, cfg.eff_max_slope_deg, min(cfg.max_z_dev * 2, 2 * npe.HARD_MAX_Z_DEV_MM))
        print("Superficie de %s: %d triangulos, %d celdas (%.0f mm2), Z %.2f..%.2f mm, relieve %.2f mm, pendiente max %.1f deg (limite %.1f), espesor %.2f..%.2f mm"
              % (os.path.basename(a.analyze_stl), surf.triangles, rep.cells, rep.area_mm2, rep.z_min, rep.z_max, rep.relief, rep.slope_max_deg, rep.slope_limit_deg, rep.thick_min, rep.thick_max))
        for ln in rep.findings:
            print("  " + ln)
        if a.preview:
            with open(a.preview, "w", encoding="utf-8") as f:
                f.write(nps.render_surface_preview(surf, rep, os.path.basename(a.analyze_stl)))
            print("vista previa escrita en %s" % a.preview)
        print("SOLO ANALISIS: el motor aun no genera trayectorias desde un modelo.")
        return 0 if rep.ok else 1
    plan = npe.build_plan(cfg)
    print("Hallazgos:")
    for f in plan.findings:
        print("  [%s] %s: %s" % (f.level, f.code, f.msg))
    print("Resumen: " + ", ".join("%s=%.3f" % (k, v) for k, v in plan.stats.items()))
    print("CODIGO DE VISTA PREVIA: %s" % plan.digest)
    if a.preview:
        with open(a.preview, "w", encoding="utf-8") as f:
            f.write(npe.render_preview(plan))
        print("vista previa escrita en %s (abrela en un navegador y revisala)" % a.preview)
    rc = 0 if plan.ok else 1
    if a.export:
        try:
            text = npe.export_gcode(plan, a.accept or "")
        except npe.ExportRefused as e:
            print("EXPORTACION RECHAZADA: %s" % e)
            return 3
        with open(a.export, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("G-code EXPERIMENTAL escrito en %s (%d lineas). Sin pruebas fisicas: una mano en el paro de emergencia." % (a.export, text.count("\n")))
    return rc


if __name__ == "__main__":
    sys.exit(main())
