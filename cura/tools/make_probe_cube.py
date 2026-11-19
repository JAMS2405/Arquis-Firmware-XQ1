#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Genera el G-code PLANO de una probeta cubica (por defecto 30 mm) con la estructura de Cura, para probar el ondulado no planar.

NO es una laminacion de Cura: es un generador simple y determinista (2 perimetros, relleno rectilineo al 20 %, 4 capas solidas arriba y abajo,
retracciones en los viajes largos) pensado solo para una PROBETA. Usa el inicio y el final de G-code del propietario (perfil Arquis XQ1, copiados
de su G-code de Cura: temperaturas, M420 S1 con su malla, purga). Para una prueba mas fiel, lamina un cubo en Cura con tu perfil y pasalo por
nonplanar_warp.py: el resultado es el mismo flujo.

Uso: python3 cura/tools/make_probe_cube.py [--size 30] [--out probeta_cubo_30mm_plano.gcode]"""
import argparse
import math

START = """M140 S70
M105
M190 S70
M104 S215
M105
M109 S215
G28 ; Home de todos los ejes
M420 S1 ; Cargar la malla de nivelado (la de 3x3 que ya hiciste)

; --- Inicio de Extrusión ---
G92 E0 ; Reset Extrusor
G1 Z2.0 F3000 ; Levantar Z solo para viajar al inicio
G1 X0.1 Y20 Z0.3 F5000.0 ; Ir al inicio de purga

; --- Línea de Purga Continua ---
G1 X4.2 Y200.0 Z0.3 F1500.0 E15 ; Línea 1
G1 X4.4 Y200.0 Z0.3 F5000.0     ; Salto lateral (sin subir Z)
G1 X4.4 Y20 Z0.3 F1500.0 E30    ; Línea 2

; --- Movimiento de Salida Sin Levantar ---
G92 E0 ; Reset Extrusor
G1 X4.0 Y20 Z0.3 F5000.0 ; Pequeño movimiento lateral para "cortar" el hilo
M117 Que la fuerza te acompañe...
; A partir de aquí, el laminador toma el control y moverá a la altura de la primera capa.
M82 ;absolute extrusion mode
G92 E0
G92 E0
G1 F2100 E-0.8"""

END = """M140 S0
M107
; --- Finalización Optimizada ---
G91 ; Posicionamiento relativo
G1 E-5 F3000 ; Retracción rápida para evitar goteo (oosing)
G1 Z5 F3000 ; Levanta el eje Z 5mm rápido para no marcar la pieza
G90 ; Volver a posicionamiento absoluto

G1 X0 Y300.0 F3000 ; Mueve el carro a la izquierda y la cama al frente
M106 S0 ; Apagar ventilador de capa
M104 S0 ; Apagar Hotend
M140 S0 ; Apagar Cama caliente

; --- Final de seguridad ---
M84 X Y E ; Desactivar motores (X, Y y Extrusor), mantener Z con fuerza
M117 Impresion Terminada ; Mensaje en pantalla
M82 ;absolute extrusion mode
M104 S0
;End of Gcode"""

H, W = 0.2, 0.42                       # altura de capa y ancho de linea
K = (H * (W - H) + math.pi * H * H / 4) / (math.pi * (1.75 / 2) ** 2)   # mm de filamento por mm de linea
RET = 0.8


def build(size: float = 30.0, cx: float = 150.0, cy: float = 150.0) -> str:
    x0, y0 = cx - size / 2, cy - size / 2
    n = int(round(size / H))
    out = [";FLAVOR:Marlin", ";TIME:@T@", ";Layer height: %g" % H, ";Generated with ARQUIS make_probe_cube (probeta plana, NO es una laminacion de Cura)", ";", START, ";LAYER_COUNT:%d" % n]
    e = -RET
    t = 0.0
    pos = [4.0, 20.0]
    state = {"e": e, "f": None}

    def fw(f):
        if state["f"] != f:
            state["f"] = f
            return " F%g" % f
        return ""

    def travel(x, y, z=None, retract=True):
        d = math.hypot(x - pos[0], y - pos[1])
        nonlocal t
        if retract and d > 3.0:
            out.append("G1 F2100 E%.5f" % (state["e"] - RET)); state["f"] = 2100
        out.append("G0%s X%.3f Y%.3f%s" % (fw(6000), x, y, (" Z%.3f" % z) if z is not None else ""))
        if retract and d > 3.0:
            out.append("G1 F2100 E%.5f" % state["e"]); state["f"] = 2100
        t += d / 100.0
        pos[0], pos[1] = x, y

    def line(x, y, f):
        nonlocal t
        d = math.hypot(x - pos[0], y - pos[1])
        state["e"] += d * K
        out.append("G1%s X%.3f Y%.3f E%.5f" % (fw(f), x, y, state["e"]))
        t += d / (f / 60.0)
        pos[0], pos[1] = x, y

    for k in range(n):
        z = H * (k + 1)
        out.append(";LAYER:%d" % k)
        speed_o, speed_i, speed_f = (1200, 1200, 1200) if k == 0 else (1800, 2400, 3000)
        # perimetros (exterior e interior)
        for inset, sp in ((W / 2, speed_o), (W / 2 + W, speed_i)):
            a, b = x0 + inset, y0 + inset
            c, d = x0 + size - inset, y0 + size - inset
            travel(a, b, z)
            for px, py in ((c, b), (c, d), (a, d), (a, b)):
                line(px, py, sp)
        # relleno: solido en las 4 primeras y ultimas capas, 20 % en el resto; lineas alternando X / Y por capa
        solid = k < 4 or k >= n - 4
        sp_line = W if solid else W / 0.2
        lo, hi = 2 * W + W / 2 + 0.1, size - (2 * W + W / 2 + 0.1)
        cnt = max(int(round((hi - lo) / sp_line)), 1)
        rev = False
        for j in range(cnt + 1):
            p = lo + (hi - lo) * j / cnt
            (xa, ya), (xb, yb) = (((x0 + lo, y0 + p), (x0 + hi, y0 + p)) if k % 2 == 0 else ((x0 + p, y0 + lo), (x0 + p, y0 + hi)))
            if rev:
                (xa, ya), (xb, yb) = (xb, yb), (xa, ya)
            travel(xa, ya)
            line(xb, yb, speed_f)
            rev = not rev
        out.append(";TIME_ELAPSED:%.6f" % t)
    out.append(END)
    return "\n".join(out).replace("@T@", str(int(t))) + "\n"


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--size", type=float, default=30.0, help="lado y altura del cubo (mm)")
    ap.add_argument("--out", default="probeta_cubo_30mm_plano.gcode")
    a = ap.parse_args(argv)
    if not (5.0 <= a.size <= 100.0):
        print("--size debe estar entre 5 y 100 mm")
        return 2
    text = build(a.size)
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("escrito %s (%d lineas, %d capas). Es una probeta PLANA: pasala por nonplanar_warp.py." % (a.out, text.count("\n"), int(round(a.size / H))))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
