#!/usr/bin/env python3
"""ARQUIS - torres de prueba para ajustar el movimiento de la impresora (ringing, Linear Advance, caudal maximo).

Genera G-code que CAMBIA UN PARAMETRO A MEDIDA QUE SUBE la pieza. Se imprime, se mira en que tramo queda mejor y ese valor se
escribe en la pantalla "Ajustes > Rendimiento" de la impresora (y se guarda). Solo biblioteca estandar de Python 3.

  python3 generar_torres.py ringing-freq  --salida torre_ringing_frecuencia.gcode
  python3 generar_torres.py ringing-accel --salida torre_ringing_aceleracion.gcode
  python3 generar_torres.py k             --salida torre_k.gcode
  python3 generar_torres.py caudal        --salida torre_caudal.gcode
  python3 generar_torres.py todas         --salida carpeta/            (las cuatro)
  Opciones comunes: --maquina xq1|aquila  --temp-boquilla 210  --temp-cama 60  --material PLA  (ver --help)

Seguridad: el G-code SIEMPRE se verifica leyendo su texto (verificar_gcode) antes de escribirlo: ordenes permitidas, limites de cama,
de velocidad, de temperatura y de los valores de M204/M201/M593/M900. Si algo no cumple, no se escribe el archivo.
Al terminar, el G-code ejecuta M501 (vuelve a los valores GUARDADOS en la EEPROM): los cambios de la torre no se quedan en la impresora.
"""
import argparse, math, os, re, sys

VERSION = 1

# Limites de cada maquina: los mismos que aplica su firmware (una prueba los compara con Marlin/Configuration.h y Marlin/machine/*.h).
MAQUINAS = {
    "xq1":    {"nombre": "Arquis XQ1", "cama": (300, 300), "z_max": 450, "amax": (4500, 4500), "vmax_xy": 500, "vmax_z": 45,
               "acc": 3000, "jd": 0.05, "shaping_min": 25.0, "boquilla_max": 270, "cama_max": 105},
    "aquila": {"nombre": "Aquila D1",  "cama": (230, 240), "z_max": 240, "amax": (4500, 3500), "vmax_xy": 500, "vmax_z": 45,
               "acc": 3000, "jd": 0.05, "shaping_min": 25.0, "boquilla_max": 270, "cama_max": 105},
}
MARGEN = 10.0          # mm hasta el borde de la cama
ACC_MIN, ACC_MAX = 200, 20000          # limites de la pantalla Rendimiento para M204 / M201
FREC_MAX = 200.0
CUERDA = 3.0           # mm: longitud de cada segmento del cilindro (pocas ordenes por segundo: Marlin no se queda sin ordenes a 250 mm/s)
K_MAX = 2.0

# ---------------------------------------------------------------------------------------------------------------------------
class Gcode:
    """Escribe G-code con E absoluto (M82), llevando la posicion y el filamento extruido."""
    def __init__(self, filamento):
        self.out, self.x, self.y, self.z, self.e = [], None, None, None, 0.0
        self.area_fil = math.pi * (filamento / 2.0) ** 2
    def raw(self, s): self.out.append(s)
    def c(self, s): self.out.append(";" + s)
    def e_por_mm(self, ancho, alto): return ancho * alto / self.area_fil
    def g(self, x=None, y=None, z=None, e_len=0.0, v=None, ancho=0.0, alto=0.0, retraer=None):
        """Movimiento lineal. e_len > 0 = extruye la longitud del recorrido (con ancho/alto de linea)."""
        w = ["G1"]
        if x is not None: w.append("X%.3f" % x)
        if y is not None: w.append("Y%.3f" % y)
        dx = (x - self.x) if (x is not None and self.x is not None) else 0.0
        dy = (y - self.y) if (y is not None and self.y is not None) else 0.0
        d = math.hypot(dx, dy)          # recorrido en XY (la primera posicion absoluta no extruye)
        if z is not None: w.append("Z%.3f" % z)
        if e_len:
            self.e += d * self.e_por_mm(ancho, alto)
            w.append("E%.5f" % self.e)
        if retraer is not None:
            self.e += retraer; w.append("E%.5f" % self.e)
        if v is not None: w.append("F%d" % round(v * 60))
        if x is not None: self.x = x
        if y is not None: self.y = y
        if z is not None: self.z = z
        self.out.append(" ".join(w))
    def texto(self): return "\n".join(self.out) + "\n"

def _cabecera(g, m, tipo, titulo, valores_txt, p):
    g.c("ARQUIS torres de prueba v%d - %s" % (VERSION, titulo))
    g.c("Maquina: %s (cama %dx%d mm). Material: %s. Boquilla %.2f mm, capa %.2f mm, linea %.2f mm." % (m["nombre"], m["cama"][0], m["cama"][1], p["material"], p["boquilla"], p["capa"], p["ancho"]))
    g.c("Temperaturas: boquilla %d C, cama %d C. Ajusta --temp-boquilla / --temp-cama a TU filamento y vuelve a generar." % (p["t_boq"], p["t_cama"]))
    if tipo == "caudal": g.c("Este archivo no cambia ajustes de movimiento; por seguridad termina con M501 (vuelve a lo guardado en la EEPROM).")
    else: g.c("Este archivo cambia valores de movimiento mientras imprime y al final ejecuta M501 (vuelve a lo guardado en la EEPROM).")
    for l in valores_txt: g.c(l)

def _inicio(g, m, p):
    g.raw("G21"); g.raw("G90"); g.raw("M82"); g.raw("G92 E0")
    g.raw("M140 S%d" % p["t_cama"]); g.raw("M104 S%d" % max(150, p["t_boq"] - 50))
    g.raw("M190 S%d" % p["t_cama"]); g.raw("G28")
    g.raw("G1 Z5.000 F600")          # tras G28 la boquilla queda a Z=0: se sube antes de cualquier desplazamiento
    g.raw("M109 S%d" % p["t_boq"])
    g.raw("M220 S100"); g.raw("M221 S100")

def _linea_cebado(g, m, p, y):
    """Linea de cebado en el borde delantero, fuera de la pieza."""
    cx = m["cama"][0] / 2.0
    x0, x1 = cx - 40, cx + 40
    g.g(x=x0, y=y, v=100); g.g(z=0.3, v=10)
    g.g(x=x1, y=y, e_len=1, v=30, ancho=p["ancho"] * 1.2, alto=0.3)
    g.g(retraer=-p["retr"], v=40); g.g(z=2.0, v=10)

def _fin(g, m, z_final, p):
    g.g(retraer=-p["retr"], v=40)
    g.g(z=min(z_final + 10.0, m["z_max"] - 1.0), v=10)
    g.g(x=MARGEN + 5, y=m["cama"][1] - MARGEN - 5, v=150)
    g.raw("M104 S0"); g.raw("M140 S0"); g.raw("M107"); g.raw("M84 X Y E"); g.raw("M501")

def _lazo_cuadrado(g, cx, cy, mitad, z, v, p):
    """Un lazo cuadrado con esquinas vivas, empezando y acabando en la esquina delantera izquierda."""
    xl, xr, yl, yr = cx - mitad, cx + mitad, cy - mitad, cy + mitad
    for (x, y) in ((xr, yl), (xr, yr), (xl, yr), (xl, yl)):
        g.g(x=x, y=y, e_len=1, v=v, ancho=p["ancho"], alto=p["capa"])

# ---------------------------------------------------------------------------------------------------------------------------
def torre_paredes(m, tipo, p):
    """Torre cuadrada de una sola pared. tipo: 'ringing-freq' | 'ringing-accel' | 'k'. Un parametro distinto en cada tramo."""
    h, w = p["capa"], p["ancho"]
    capas_tramo = max(1, round(p["tramo_mm"] / h)); n = len(p["valores"])
    lado = p["lado"]; mitad = lado / 2.0
    cx, cy = m["cama"][0] / 2.0, m["cama"][1] / 2.0
    cebado_y = cy - mitad - 15.0
    nombre = {"ringing-freq": "frecuencia de Input Shaping (M593 F), X e Y a la vez", "ringing-accel": "aceleracion (M204 P/T)", "k": "Linear Advance K (M900 K)"}[tipo]
    def orden(v):
        if tipo == "ringing-freq": return ["M400", "M593 X F%.1f" % v, "M593 Y F%.1f" % v]
        if tipo == "ringing-accel": return ["M204 P%d T%d" % (v, v)]
        return ["M900 K%.3f" % v]
    unidad = {"ringing-freq": "Hz", "ringing-accel": "mm/s2", "k": ""}[tipo]
    g = Gcode(p["filamento"])
    tabla = ["TRAMOS (de abajo hacia arriba; cada tramo mide %.1f mm de alto; una marca (escalon) separa un tramo del siguiente):" % p["tramo_mm"]]
    for i, v in enumerate(p["valores"]):
        fmt = "%.3f" if tipo == "k" else ("%.1f" if tipo == "ringing-freq" else "%d")
        tabla.append("  Tramo %d  (%.1f - %.1f mm): %s %s" % (i + 1, i * p["tramo_mm"], (i + 1) * p["tramo_mm"], fmt % v, unidad))
    _cabecera(g, m, tipo, "torre de " + nombre, tabla, p)
    g.c("Velocidad de la pared: %d mm/s. Lado de la torre: %d mm." % (p["velocidad"], lado))
    if p.get("subir_amax"):
        g.c("ATENCION: este archivo sube la aceleracion MAXIMA de X e Y (M201) para llegar a los valores de la tabla. Se restaura con M501 al terminar.")
    _inicio(g, m, p)
    if p.get("subir_amax"):
        g.raw("M201 X%d Y%d" % (p["subir_amax"], p["subir_amax"]))
    if tipo != "ringing-accel":
        g.raw("M204 P%d T%d" % (p["acc"], p["acc"]))
    _linea_cebado(g, m, p, cebado_y)
    capas = capas_tramo * n
    for c in range(capas):
        z = round((c + 1) * h, 3)
        tramo = c // capas_tramo; primera_tramo = (c % capas_tramo == 0)
        if primera_tramo:
            g.c("TRAMO %d" % (tramo + 1))
            for o in orden(p["valores"][tramo]): g.raw(o)
        if c == 2: g.raw("M106 S%d" % p["ventilador"])
        v = p["v_primera"] if c == 0 else p["velocidad"]
        if c == 0:                      # viaje a la esquina del lazo exterior de la base con la boquilla alta; despues baja y repone la retraccion
            g.g(x=cx - mitad - p["borde"] * w, y=cy - mitad - p["borde"] * w, v=150)
            g.g(z=z, v=10); g.g(retraer=+p["retr"], v=40)
            for k in range(p["borde"], -1, -1):      # base: de fuera hacia dentro; el lazo nominal (desplazamiento 0) va el ultimo
                off = k * w
                if k != p["borde"]: g.g(x=cx - mitad - off, y=cy - mitad - off, e_len=1, v=v, ancho=w, alto=h)
                _lazo_cuadrado(g, cx, cy, mitad + off, z, v, p)
            continue
        g.g(z=z, v=10)
        if primera_tramo:               # marca de tramo: un lazo exterior extra (escalon visible)
            g.g(x=cx - mitad - w, y=cy - mitad - w, e_len=1, v=v, ancho=w, alto=h)
            _lazo_cuadrado(g, cx, cy, mitad + w, z, v, p)
            g.g(x=cx - mitad, y=cy - mitad, e_len=1, v=v, ancho=w, alto=h)
        _lazo_cuadrado(g, cx, cy, mitad, z, v, p)
    _fin(g, m, capas * h, p)
    return g.texto()

def torre_caudal(m, p):
    """Cilindro de una sola pared en espiral (modo vaso) sobre una base solida; la velocidad sube por tramos = caudal creciente."""
    h, w = p["capa"], p["ancho"]
    r = p["diametro"] / 2.0
    cx, cy = m["cama"][0] / 2.0, m["cama"][1] / 2.0
    n = len(p["valores"]); capas_tramo = max(1, round(p["tramo_mm"] / h))
    g = Gcode(p["filamento"])
    tabla = ["TRAMOS (de abajo hacia arriba; cada tramo mide %.1f mm de alto):" % p["tramo_mm"]]
    for i, f in enumerate(p["valores"]):
        v = f / (w * h)
        tabla.append("  Tramo %d  (%.1f - %.1f mm): caudal %.1f mm3/s = velocidad %.0f mm/s" % (i + 1, i * p["tramo_mm"], (i + 1) * p["tramo_mm"], f, v))
    _cabecera(g, m, "caudal", "torre de caudal maximo (cilindro en espiral de una pared, %d mm de diametro)" % p["diametro"], tabla, p)
    g.c("El caudal maximo es el del ULTIMO tramo donde la pared sigue llena y uniforme (sin huecos ni capas finas).")
    _inicio(g, m, p)
    g.raw("M204 P%d T%d" % (p["acc"], p["acc"]))
    cebado_y = cy - r - 15.0
    _linea_cebado(g, m, p, cebado_y)
    def circulo(rad, v):
        k = max(16, int(2 * math.pi * rad / CUERDA))
        for i in range(1, k + 1):
            a = 2 * math.pi * i / k
            g.g(x=cx + rad * math.cos(a), y=cy + rad * math.sin(a), e_len=1, v=v, ancho=w, alto=h)
    radios = []
    rad = r
    while rad > w: radios.append(rad); rad -= w
    g.g(x=cx + r, y=cy, v=150)                       # viaje con la boquilla alta
    g.g(z=h, v=10); g.g(retraer=+p["retr"], v=40)
    for c, orden_radios in enumerate((radios, radios[::-1])):   # capa 1: de fuera hacia dentro; capa 2: de dentro hacia fuera (acaba en el arranque de la espiral)
        if c: g.g(z=round(2 * h, 3), v=10)
        v = p["v_primera"] if c == 0 else 60
        for n_r, rr in enumerate(orden_radios):
            if n_r: g.g(x=cx + rr, y=cy, e_len=1, v=v, ancho=w, alto=h)
            circulo(rr, v)
    g.raw("M106 S%d" % p["ventilador"])
    z0 = 2 * h
    vueltas = capas_tramo * n
    for i in range(vueltas):
        tramo = i // capas_tramo
        if i % capas_tramo == 0: g.c("TRAMO %d" % (tramo + 1))
        v = p["valores"][tramo] / (w * h)
        k = max(16, int(2 * math.pi * r / CUERDA))
        for j in range(1, k + 1):
            a = 2 * math.pi * j / k
            z = z0 + h * (i + j / k)
            g.g(x=cx + r * math.cos(a), y=cy + r * math.sin(a), z=round(z, 4), e_len=1, v=v, ancho=w, alto=h)
    _fin(g, m, z0 + vueltas * h, p)
    return g.texto()

# ---------------------------------------------------------------------------------------------------------------------------
PERMITIDAS = {"G21", "G90", "M82", "G92", "M140", "M104", "M190", "M109", "G28", "M220", "M221", "G1", "M106", "M107", "M84",
              "M204", "M201", "M900", "M593", "M400", "M501"}

def verificar_gcode(texto, m, tipo=None):
    """Lee el TEXTO del G-code y devuelve la lista de problemas (vacia = correcto). Independiente del generador."""
    err = []
    x = y = z = None; e = 0.0; homed = False; caliente = False; amax = list(m["amax"]); ultimo_e = 0.0
    ultimas = []; xy_visto = False
    for n, raw in enumerate(texto.splitlines(), 1):
        l = raw.split(";", 1)[0].strip()
        if not l: continue
        ultimas.append(l)
        w = l.split(); cmd = w[0].upper()
        if cmd not in PERMITIDAS: err.append("linea %d: orden no permitida %s" % (n, cmd)); continue
        val = {t[0].upper(): t[1:] for t in w[1:] if t[0].isalpha() and len(t) > 1}
        f = lambda k: float(val[k]) if k in val else None
        try:
            if cmd == "G28": homed = True
            elif cmd in ("M104", "M109", "M190", "M140"):
                s = f("S")
                if s is None: err.append("linea %d: %s sin S" % (n, cmd)); continue
                lim = m["cama_max"] if cmd in ("M140", "M190") else m["boquilla_max"]
                if not 0 <= s <= lim: err.append("linea %d: temperatura %s fuera de limites (0-%d)" % (n, l, lim))
                if cmd == "M109" and s >= 170: caliente = True
            elif cmd == "G1":
                nx, ny, nz, ne, fv = f("X"), f("Y"), f("Z"), f("E"), f("F")
                if not homed: err.append("linea %d: movimiento antes de G28" % n)
                if (nx is not None or ny is not None) and not xy_visto:
                    xy_visto = True
                    if (nz if nz is not None else (z if z is not None else 0.0)) < 1.0: err.append("linea %d: primer desplazamiento XY con la boquilla a menos de 1 mm de la cama (tras G28 esta a Z=0)" % n)
                for nm, vv, lo, hi in (("X", nx, MARGEN, m["cama"][0] - MARGEN), ("Y", ny, MARGEN, m["cama"][1] - MARGEN), ("Z", nz, 0.0, m["z_max"])):
                    if vv is not None and not (lo - 1e-6 <= vv <= hi + 1e-6): err.append("linea %d: %s=%.3f fuera de [%.1f, %.1f]" % (n, nm, vv, lo, hi))
                if fv is not None:
                    xy = nx is not None or ny is not None
                    if xy and fv > m["vmax_xy"] * 60 + 1e-6: err.append("linea %d: velocidad XY %.0f mm/s > %d" % (n, fv / 60, m["vmax_xy"]))
                    if nz is not None and not xy and fv > m["vmax_z"] * 60 + 1e-6: err.append("linea %d: velocidad Z %.1f mm/s > %d" % (n, fv / 60, m["vmax_z"]))
                if ne is not None:
                    de = ne - e
                    if de > 0 and not caliente: err.append("linea %d: extrusion antes de calentar" % n)
                    if de > 0 and (nx is not None or ny is not None) and x is not None and y is not None:
                        d = math.hypot((nx if nx is not None else x) - x, (ny if ny is not None else y) - y)
                        if d > 0 and de / d > 0.1: err.append("linea %d: extrusion %.3f mm de filamento por mm de recorrido (excesiva)" % (n, de / d))
                    if de < -3.0: err.append("linea %d: retraccion mayor de 3 mm" % n)
                    e = ne
                if nx is not None: x = nx
                if ny is not None: y = ny
                if nz is not None: z = nz
            elif cmd == "G92":
                if "E" in val: e = float(val["E"])
            elif cmd == "M204":
                for k in ("P", "T"):
                    if k in val:
                        a = float(val[k])
                        if not ACC_MIN <= a <= ACC_MAX: err.append("linea %d: aceleracion %s fuera de %d-%d" % (n, l, ACC_MIN, ACC_MAX))
                        if a > min(amax) + 1e-6: err.append("linea %d: %s supera la aceleracion maxima de los ejes (%d/%d): Marlin la recortaria; usa --subir-amax" % (n, l, amax[0], amax[1]))
            elif cmd == "M201":
                for i, k in enumerate(("X", "Y")):
                    if k in val:
                        a = float(val[k])
                        if not ACC_MIN <= a <= ACC_MAX: err.append("linea %d: %s fuera de %d-%d" % (n, l, ACC_MIN, ACC_MAX))
                        amax[i] = a
            elif cmd == "M900":
                k = f("K")
                if k is None or not 0 <= k <= K_MAX: err.append("linea %d: K fuera de 0-%.1f" % (n, K_MAX))
            elif cmd == "M593":
                fr = f("F")
                if fr is None or not ((fr == 0) or (m["shaping_min"] <= fr <= FREC_MAX)): err.append("linea %d: frecuencia fuera de 0 o [%.0f, %.0f] Hz" % (n, m["shaping_min"], FREC_MAX))
            elif cmd == "M106":
                s = f("S")
                if s is None or not 0 <= s <= 255: err.append("linea %d: ventilador fuera de 0-255" % n)
        except ValueError:
            err.append("linea %d: valor no numerico en '%s'" % (n, l))
    if not homed: err.append("falta G28")
    if not caliente: err.append("falta calentar la boquilla (M109)")
    if not ultimas or ultimas[-1] != "M501": err.append("el archivo debe terminar con M501 (restaurar los valores guardados)")
    if not any(c.startswith("M104 S0") for c in ultimas) or not any(c.startswith("M140 S0") for c in ultimas): err.append("falta apagar calentadores al final")
    return err

# ---------------------------------------------------------------------------------------------------------------------------
def _rango(ini, paso, n, redondeo):
    return [round(ini + i * paso, redondeo) for i in range(n)]

def construir(tipo, a):
    """Devuelve (texto del G-code, nombre sugerido). Lanza ValueError con un mensaje claro si algun parametro no es valido."""
    if a.maquina not in MAQUINAS: raise ValueError("maquina desconocida: %s (usa %s)" % (a.maquina, ", ".join(MAQUINAS)))
    m = MAQUINAS[a.maquina]
    if not 150 <= a.temp_boquilla <= m["boquilla_max"]: raise ValueError("--temp-boquilla debe estar entre 150 y %d" % m["boquilla_max"])
    if not 0 <= a.temp_cama <= m["cama_max"]: raise ValueError("--temp-cama debe estar entre 0 y %d" % m["cama_max"])
    if not 0.1 <= a.capa <= 0.3 * a.boquilla / 0.4 + 0.05: raise ValueError("--capa fuera de rango para esa boquilla")
    if not 0.8 * a.boquilla <= a.ancho <= 1.4 * a.boquilla: raise ValueError("--ancho debe estar entre 0.8 y 1.4 veces la boquilla")
    p = {"material": a.material, "boquilla": a.boquilla, "capa": a.capa, "ancho": a.ancho, "filamento": a.filamento, "t_boq": a.temp_boquilla,
         "t_cama": a.temp_cama, "retr": 1.0, "ventilador": a.ventilador, "borde": 3, "v_primera": 30, "tramo_mm": a.tramo, "acc": a.acc if a.acc else m["acc"]}
    if not 1 <= a.tramos <= 20: raise ValueError("--tramos debe estar entre 1 y 20")
    if not 1.0 <= a.tramo <= 20.0: raise ValueError("--tramo debe estar entre 1 y 20 mm")
    if not ACC_MIN <= p["acc"] <= ACC_MAX: raise ValueError("--acc fuera de %d-%d" % (ACC_MIN, ACC_MAX))
    if not 0 <= a.ventilador <= 255: raise ValueError("--ventilador entre 0 y 255")
    if tipo in ("ringing-freq", "ringing-accel", "k"):
        if tipo == "ringing-freq":
            p["valores"] = _rango(a.f_inicio, a.f_paso, a.tramos, 2)
            if a.f_paso <= 0: raise ValueError("--f-paso debe ser > 0")
            if p["valores"][0] < m["shaping_min"] or p["valores"][-1] > FREC_MAX:
                raise ValueError("las frecuencias deben estar entre %.0f Hz (minimo del buffer de Input Shaping compilado) y %.0f Hz; ahora %.1f..%.1f" % (m["shaping_min"], FREC_MAX, p["valores"][0], p["valores"][-1]))
            nombre = "torre_ringing_frecuencia_%s.gcode" % a.maquina
        elif tipo == "ringing-accel":
            tope = min(m["amax"]); paso = a.a_paso if a.a_paso is not None else 500.0
            if a.a_inicio is None:
                ini = tope - paso * (a.tramos - 1)
                if ini < 500 and a.a_paso is None:         # poco margen: pasos mas finos, nunca por debajo de 500 al empezar
                    paso = max(50.0, ((tope - 500) // max(1, a.tramos - 1)) // 50 * 50); ini = tope - paso * (a.tramos - 1)
                a_ini = max(500.0, ini)
            else: a_ini = a.a_inicio
            if paso <= 0: raise ValueError("--a-paso debe ser > 0")
            p["valores"] = [int(v) for v in _rango(a_ini, paso, a.tramos, 0)]
            if p["valores"][0] < ACC_MIN or p["valores"][-1] > ACC_MAX: raise ValueError("las aceleraciones deben estar entre %d y %d" % (ACC_MIN, ACC_MAX))
            if p["valores"][-1] > min(m["amax"]):
                if not a.subir_amax: raise ValueError("el ultimo tramo (%d mm/s2) supera la aceleracion maxima de los ejes (%d/%d): Marlin la recortaria. Reduce --a-inicio/--a-paso/--tramos o usa --subir-amax (sube M201 solo durante esta impresion)" % (p["valores"][-1], m["amax"][0], m["amax"][1]))
                p["subir_amax"] = p["valores"][-1]
            nombre = "torre_ringing_aceleracion_%s.gcode" % a.maquina
        else:
            if a.k_paso <= 0: raise ValueError("--k-paso debe ser > 0")
            p["valores"] = _rango(a.k_inicio, a.k_paso, a.tramos, 4)
            if p["valores"][0] < 0 or p["valores"][-1] > K_MAX: raise ValueError("K debe estar entre 0 y %.1f; ahora %.3f..%.3f" % (K_MAX, p["valores"][0], p["valores"][-1]))
            nombre = "torre_k_%s.gcode" % a.maquina
        p["velocidad"] = a.velocidad; p["lado"] = a.lado
        if not 20 <= a.velocidad <= m["vmax_xy"]: raise ValueError("--velocidad entre 20 y %d mm/s" % m["vmax_xy"])
        if not 20 <= a.lado <= min(m["cama"]) - 2 * MARGEN - 30: raise ValueError("--lado entre 20 y %d mm para esa cama" % (min(m["cama"]) - 2 * MARGEN - 30))
        if p["tramo_mm"] * a.tramos > m["z_max"] - 15: raise ValueError("la torre no cabe en Z")
        texto = torre_paredes(m, tipo, p)
    else:
        if a.c_paso <= 0: raise ValueError("--c-paso debe ser > 0")
        p["valores"] = _rango(a.c_inicio, a.c_paso, a.tramos, 2)
        if p["valores"][0] <= 0: raise ValueError("el caudal inicial debe ser > 0")
        p["diametro"] = a.diametro
        if not 30 <= a.diametro <= min(m["cama"]) - 2 * MARGEN - 30: raise ValueError("--diametro entre 30 y %d mm para esa cama" % (min(m["cama"]) - 2 * MARGEN - 30))
        r = a.diametro / 2.0; chord = CUERDA
        for f in p["valores"]:
            v = f / (a.ancho * a.capa)
            if v > m["vmax_xy"]: raise ValueError("el caudal %.1f mm3/s pide %.0f mm/s, mas que el maximo de la maquina (%d)" % (f, v, m["vmax_xy"]))
            if v * v / r > p["acc"]: raise ValueError("a %.0f mm/s un circulo de %d mm exige %.0f mm/s2 > aceleracion %d: usa un --diametro mayor o menos caudal" % (v, a.diametro, v * v / r, p["acc"]))
            if v > math.sqrt(8 * p["acc"] * m["jd"]) * r / chord: raise ValueError("a %.0f mm/s la desviacion de union limitaria el circulo: usa un --diametro mayor" % v)
        if p["tramo_mm"] * a.tramos > m["z_max"] - 15: raise ValueError("la torre no cabe en Z")
        texto = torre_caudal(m, p)
        nombre = "torre_caudal_%s.gcode" % a.maquina
    problemas = verificar_gcode(texto, m, tipo)
    if problemas: raise ValueError("el G-code generado no pasa la verificacion:\n  " + "\n  ".join(problemas[:10]))
    return texto, nombre

def parser():
    ap = argparse.ArgumentParser(description="Torres de prueba ARQUIS (ringing, Linear Advance, caudal maximo).", formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    ap.add_argument("tipo", choices=["ringing-freq", "ringing-accel", "k", "caudal", "todas"])
    ap.add_argument("--salida", help="archivo .gcode (o carpeta con 'todas'); por defecto, en la carpeta actual")
    ap.add_argument("--maquina", default="xq1", choices=sorted(MAQUINAS))
    ap.add_argument("--material", default="PLA", help="solo para el encabezado")
    ap.add_argument("--temp-boquilla", type=int, default=210, dest="temp_boquilla")
    ap.add_argument("--temp-cama", type=int, default=60, dest="temp_cama")
    ap.add_argument("--boquilla", type=float, default=0.4); ap.add_argument("--capa", type=float, default=0.2)
    ap.add_argument("--ancho", type=float, default=0.45); ap.add_argument("--filamento", type=float, default=1.75)
    ap.add_argument("--ventilador", type=int, default=255, help="0-255, desde la capa 3")
    ap.add_argument("--tramos", type=int, default=8, help="numero de tramos (valores distintos)"); ap.add_argument("--tramo", type=float, default=5.0, help="alto de cada tramo (mm)")
    ap.add_argument("--acc", type=int, default=0, help="aceleracion fija de impresion para las torres de frecuencia, K y caudal (0 = la de la maquina)")
    ap.add_argument("--velocidad", type=float, default=100, help="velocidad de la pared en las torres de ringing y K (mm/s)")
    ap.add_argument("--lado", type=float, default=40, help="lado de la torre cuadrada (mm)")
    ap.add_argument("--f-inicio", type=float, default=25.0, dest="f_inicio"); ap.add_argument("--f-paso", type=float, default=5.0, dest="f_paso")
    ap.add_argument("--a-inicio", type=float, default=None, dest="a_inicio", help="por defecto: lo que haga falta para que el ultimo tramo sea la aceleracion maxima de la maquina")
    ap.add_argument("--a-paso", type=float, default=None, dest="a_paso", help="por defecto 500 (menos si la maquina tiene poco margen)")
    ap.add_argument("--subir-amax", action="store_true", dest="subir_amax", help="permite que la torre de aceleracion suba M201 por encima de la maxima actual (se restaura con M501)")
    ap.add_argument("--k-inicio", type=float, default=0.0, dest="k_inicio"); ap.add_argument("--k-paso", type=float, default=0.01, dest="k_paso")
    ap.add_argument("--c-inicio", type=float, default=6.0, dest="c_inicio", help="caudal inicial (mm3/s)"); ap.add_argument("--c-paso", type=float, default=2.0, dest="c_paso")
    ap.add_argument("--diametro", type=float, default=60, help="diametro del cilindro de la torre de caudal (mm)")
    return ap

def main(argv=None):
    a = parser().parse_args(argv)
    tipos = ["ringing-freq", "ringing-accel", "k", "caudal"] if a.tipo == "todas" else [a.tipo]
    try:
        res = []
        for t in tipos:
            aa = argparse.Namespace(**vars(a))
            if t == "k" and a.tipo == "todas": pass
            res.append(construir(t, aa))
    except ValueError as ex:
        print("ERROR: %s" % ex, file=sys.stderr); return 2
    for (texto, nombre) in res:
        if a.tipo == "todas" or (a.salida and os.path.isdir(a.salida)):
            os.makedirs(a.salida or ".", exist_ok=True); ruta = os.path.join(a.salida or ".", nombre)
        else: ruta = a.salida or nombre
        with open(ruta, "w", encoding="ascii", newline="\n") as f: f.write(texto)
        print("escrito %s (%d lineas)" % (ruta, texto.count("\n")))
    return 0

if __name__ == "__main__":
    sys.exit(main())
