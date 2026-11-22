#!/usr/bin/env python3
"""ARQUIS - genera las torres de prueba (ringing, aceleracion, K y caudal) para varios materiales de una vez.

  python3 herramientas/torres/generar_materiales.py                       -> XQ1, carpeta herramientas/torres/listos/<MATERIAL>/
  python3 herramientas/torres/generar_materiales.py --maquina aquila      -> Aquila D1
  python3 herramientas/torres/generar_materiales.py --solo TPU PETG       -> solo esos materiales
  python3 herramientas/torres/generar_materiales.py --salida C:/torres

Para cambiar temperaturas o anadir un material, edita la tabla MATERIALES (abajo). Cada archivo se verifica antes de escribirse
(lo hace generar_torres.py). Nota: el ringing (frecuencia y aceleracion) es mecanico y no depende del material; se generan igual
por comodidad y para que cada carpeta tenga el juego completo con las temperaturas de ese filamento.
"""
import argparse, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import generar_torres as gt

# boquilla/cama en C. ventilador 0-255 (desde la capa 3). velocidad: pared de las torres de ringing y K (mm/s).
# caudal: (inicio, paso) en mm3/s para la torre de caudal maximo (8 tramos). K: Linear Advance (inicio, paso).
MATERIALES = {
    "ASA":  {"boquilla": 265, "cama": 100, "ventilador": 0,   "velocidad": 100, "caudal": (6.0, 2.0),  "k": (0.0, 0.01)},
    "ABS":  {"boquilla": 250, "cama": 90,  "ventilador": 0,   "velocidad": 100, "caudal": (6.0, 2.0),  "k": (0.0, 0.01)},
    "PETG": {"boquilla": 230, "cama": 70,  "ventilador": 100, "velocidad": 80,  "caudal": (4.0, 1.5),  "k": (0.0, 0.01)},
    "TPU":  {"boquilla": 235, "cama": 70,  "ventilador": 128, "velocidad": 30,  "caudal": (1.5, 0.5),  "k": (0.0, 0.01)},
}

def main(argv=None):
    ap = argparse.ArgumentParser(description="Torres de prueba ARQUIS para varios materiales.")
    ap.add_argument("--maquina", default="xq1", choices=sorted(gt.MAQUINAS))
    ap.add_argument("--salida", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "listos"))
    ap.add_argument("--solo", nargs="+", metavar="MATERIAL", help="solo estos materiales (%s)" % ", ".join(MATERIALES))
    a = ap.parse_args(argv)
    mats = [m.upper() for m in a.solo] if a.solo else list(MATERIALES)
    for m in mats:
        if m not in MATERIALES:
            print("ERROR: material desconocido %s (tabla: %s)" % (m, ", ".join(MATERIALES)), file=sys.stderr); return 2
    rc = 0
    for m in mats:
        d = MATERIALES[m]; carpeta = os.path.join(a.salida, m)
        base = ["--maquina", a.maquina, "--material", m, "--temp-boquilla", str(d["boquilla"]), "--temp-cama", str(d["cama"]), "--ventilador", str(d["ventilador"])]
        trabajos = [
            ["ringing-freq", "--velocidad", str(d["velocidad"])],
            ["ringing-accel", "--velocidad", str(d["velocidad"])],
            ["k", "--velocidad", str(d["velocidad"]), "--k-inicio", str(d["k"][0]), "--k-paso", str(d["k"][1])],
            ["caudal", "--c-inicio", str(d["caudal"][0]), "--c-paso", str(d["caudal"][1])],
        ]
        os.makedirs(carpeta, exist_ok=True)
        print("== %s: boquilla %d C, cama %d C" % (m, d["boquilla"], d["cama"]))
        for t in trabajos:
            rc |= gt.main([t[0], "--salida", carpeta] + base + t[1:])
    return rc

if __name__ == "__main__":
    sys.exit(main())
