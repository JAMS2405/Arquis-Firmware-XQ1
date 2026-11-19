#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Genera, a partir de limits.json (UNICA fuente de limites y constantes), los archivos que consumen Cura y el firmware:
   - cura/plugins/ARQUISPlugin/engines/protocol_limits.py
   - shared/object_protocol/generated/arquis_object_limits.h
Uso:  python3 shared/object_protocol/gen_limits.py          (escribe)
      python3 shared/object_protocol/gen_limits.py --check  (falla si algun archivo generado esta desactualizado)"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
PY_OUT = os.path.join(ROOT, "cura", "plugins", "ARQUISPlugin", "engines", "protocol_limits.py")
H_OUT = os.path.join(HERE, "generated", "arquis_object_limits.h")


def load():
    with open(os.path.join(HERE, "limits.json"), encoding="utf-8") as f:
        j = json.load(f)
    d, c, p = j["design_capacity"], j["capacity_ceiling"], j["protocol_max_objects"]
    assert 1 <= d <= c <= p, "se exige 1 <= design_capacity <= capacity_ceiling <= protocol_max_objects"
    return j


def render_py(j):
    return '''# SPDX-License-Identifier: LGPL-3.0-or-later
# ARCHIVO GENERADO por shared/object_protocol/gen_limits.py a partir de shared/object_protocol/limits.json. NO EDITAR A MANO.
"""Constantes del protocolo ARQUIS_OBJECTS_V1 (la fuente es shared/object_protocol/limits.json)."""

PROTOCOL_NAME = %(protocol)r
PROTOCOL_VERSION = %(version)d

DESIGN_CAPACITY = %(design_capacity)d                  # objetos simultaneos de diseno: IDs cancelables 0..DESIGN_CAPACITY-1
MAX_CANCELABLE_ID = DESIGN_CAPACITY - 1
CAPACITY_CEILING = %(capacity_ceiling)d                # mayor capacidad que admite el motor del firmware actual (mascara uint32_t)
PROTOCOL_MAX_OBJECTS = %(protocol_max_objects)d               # limite absoluto del protocolo (contadores int8_t de Marlin)

NAME_MAX_BYTES = %(name_max_bytes)d                     # nombres de objeto en el manifiesto (UTF-8)
COORD_UNIT_MM = %(coord_unit_mm)r                    # el manifiesto usa enteros en decimas de mm
''' % j


def render_h(j):
    return '''// SPDX-License-Identifier: LGPL-3.0-or-later
// ARCHIVO GENERADO por shared/object_protocol/gen_limits.py a partir de shared/object_protocol/limits.json. NO EDITAR A MANO.
#pragma once
// ARQUIS_OBJECTS_V%(version)d: limites y constantes compartidos entre Cura y el firmware.
#define ARQ_OBJ_PROTOCOL_VERSION   %(version)d
#define ARQ_OBJ_DESIGN_CAPACITY    %(design_capacity)d   // objetos simultaneos de diseno = IDs cancelables 0..ARQ_OBJ_MAX_ID
#define ARQ_OBJ_MAX_ID             (ARQ_OBJ_DESIGN_CAPACITY - 1)
#define ARQ_OBJ_CAPACITY_CEILING   %(capacity_ceiling)d   // maximo que admite el motor M486 actual (mascara uint32_t)
#define ARQ_OBJ_PROTOCOL_MAX       %(protocol_max_objects)d  // limite absoluto (int8_t)
#define ARQ_OBJ_NAME_MAX           %(name_max_bytes)d   // bytes UTF-8 de un nombre en el manifiesto
#define ARQ_OBJ_NAME_BUF           (ARQ_OBJ_NAME_MAX + 1)
#define ARQ_OBJ_COORD_UNIT_DMM     1    // 1 unidad = 0,1 mm

#ifdef __cplusplus
static_assert(ARQ_OBJ_DESIGN_CAPACITY >= 1 && ARQ_OBJ_DESIGN_CAPACITY <= ARQ_OBJ_CAPACITY_CEILING, "capacidad de diseno fuera de rango");
static_assert(ARQ_OBJ_CAPACITY_CEILING <= 32, "la mascara canceled es uint32_t: subir el techo exige cambiar el motor de cancelacion");
static_assert(ARQ_OBJ_CAPACITY_CEILING <= ARQ_OBJ_PROTOCOL_MAX, "el techo no puede superar el limite del protocolo");
#endif
''' % j


MACHINE_JSON = os.path.join(ROOT, "shared", "machines", "arquis_xq1.json")
MACHINE_OUT = os.path.join(ROOT, "cura", "plugins", "ARQUISPlugin", "engines", "machine_profile.py")


def render_machine():
    with open(MACHINE_JSON, encoding="utf-8") as f:
        m = json.load(f)
    return '''# SPDX-License-Identifier: LGPL-3.0-or-later
# ARCHIVO GENERADO por shared/object_protocol/gen_limits.py a partir de shared/machines/arquis_xq1.json. NO EDITAR A MANO.
"""Geometria de la maquina real de ARQUIS (la fuente es shared/machines/arquis_xq1.json; una prueba la compara con Marlin/Configuration.h)."""

MACHINE_NAME = %r
BED_X_MM = %r
BED_Y_MM = %r
HEIGHT_Z_MM = %r
MIN_X_MM = %r
MIN_Y_MM = %r
ORIGIN = %r                       # PROVISIONAL mientras ORIGIN_CONFIRMED sea False (prueba de orientacion fisica pendiente)
ORIGIN_CONFIRMED = %r
CENTER_IS_ZERO = %r
''' % (m["name"], m["bed_x_mm"], m["bed_y_mm"], m["height_z_mm"], m["min_x_mm"], m["min_y_mm"], m["origin"], m["origin_confirmed"], m["center_is_zero"])


def main():
    j = load()
    outs = {PY_OUT: render_py(j), H_OUT: render_h(j), MACHINE_OUT: render_machine()}
    if "--check" in sys.argv:
        bad = [p for p, t in outs.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
        if bad:
            print("DESACTUALIZADO (ejecuta gen_limits.py):", *bad, sep="\n  ")
            return 1
        print("limites generados al dia")
        return 0
    for p, t in outs.items():
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(t)
        print("escrito", os.path.relpath(p, ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
