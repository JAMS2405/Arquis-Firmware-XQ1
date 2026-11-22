#!/usr/bin/env python3
"""Pruebas del generador de torres de prueba (herramientas/torres/generar_torres.py). Solo biblioteca estandar: python3 tests/torres/test_torres.py"""
import argparse, math, pathlib, re, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "herramientas" / "torres"))
import generar_torres as gt

def args(tipo, **kw):
    a = gt.parser().parse_args([tipo]); 
    for k, v in kw.items(): setattr(a, k, v)
    return a
def gen(tipo, **kw): return gt.construir(tipo, args(tipo, **kw))[0]
TIPOS = ("ringing-freq", "ringing-accel", "k", "caudal")

def define(src, name):
    m = re.search(r'^\s*#define\s+%s\s+([^\s/]+)' % name, src, re.M); return m.group(1) if m else None
def lista(src, name):
    m = re.search(r'^\s*#define\s+%s\s+\{([^}]*)\}' % name, src, re.M); return [float(x) for x in m.group(1).split(",")] if m else None

class Maquinas(unittest.TestCase):
    """Los limites del generador son los del firmware (no una copia que se desactualice)."""
    def setUp(self):
        self.cfg = (ROOT / "Marlin" / "Configuration.h").read_text(encoding="utf-8")
        self.adv = (ROOT / "Marlin" / "Configuration_adv.h").read_text(encoding="utf-8")
        self.aq = (ROOT / "Marlin" / "machine" / "aquila_d1.h").read_text(encoding="utf-8")
    def val(self, nombre, perfil):   # el perfil de la maquina manda; si no, el valor por defecto de Configuration.h
        v = define(perfil, nombre); return v if v is not None else define(self.cfg, nombre)
    def test_xq1(self):
        m = gt.MAQUINAS["xq1"]
        self.assertEqual((int(define(self.cfg, "X_BED_SIZE") or 0) or 300, ), (m["cama"][0],))
        self.assertIn("#define X_BED_SIZE 300", self.cfg); self.assertIn("#define Y_BED_SIZE 300", self.cfg); self.assertIn("#define Z_MAX_POS 450", self.cfg)
        self.assertEqual(lista(self.cfg, "DEFAULT_MAX_ACCELERATION")[:2], list(map(float, m["amax"])))
        self.assertEqual(lista(self.cfg, "DEFAULT_MAX_FEEDRATE")[:3], [m["vmax_xy"], m["vmax_xy"], m["vmax_z"]])
        self.assertEqual(float(define(self.cfg, "DEFAULT_ACCELERATION")), m["acc"]); self.assertEqual(float(define(self.cfg, "JUNCTION_DEVIATION_MM")), m["jd"])
        self.assertEqual(int(define(self.cfg, "HEATER_0_MAXTEMP")) - int(define(self.cfg, "HOTEND_OVERSHOOT")), m["boquilla_max"])
        self.assertEqual(int(define(self.cfg, "BED_MAXTEMP")) - int(define(self.cfg, "BED_OVERSHOOT")), m["cama_max"])
        self.assertEqual(min(float(define(self.adv, "SHAPING_FREQ_X")), float(define(self.adv, "SHAPING_FREQ_Y"))), m["shaping_min"])
    def test_aquila(self):
        m = gt.MAQUINAS["aquila"]
        self.assertEqual((float(self.val("X_BED_SIZE", self.aq)), float(self.val("Y_BED_SIZE", self.aq)), float(self.val("Z_MAX_POS", self.aq))), (m["cama"][0], m["cama"][1], m["z_max"]))
        self.assertEqual(lista(self.aq, "DEFAULT_MAX_ACCELERATION")[:2], list(map(float, m["amax"])))
        self.assertEqual(lista(self.cfg, "DEFAULT_MAX_FEEDRATE")[:2], [m["vmax_xy"]] * 2)

class Generacion(unittest.TestCase):
    def test_todas_pasan_la_verificacion_en_las_dos_maquinas(self):
        for maq in gt.MAQUINAS:
            for t in TIPOS:
                txt = gen(t, maquina=maq)
                self.assertEqual(gt.verificar_gcode(txt, gt.MAQUINAS[maq]), [], "%s %s" % (maq, t))
                self.assertTrue(txt.rstrip().endswith("M501"), "termina con M501")
    def test_determinista_y_ascii(self):
        for t in TIPOS:
            a, b = gen(t), gen(t)
            self.assertEqual(a, b); a.encode("ascii"); self.assertNotIn("\r", a)
    def test_tablas_coinciden_con_las_ordenes(self):
        txt = gen("k"); ks = [float(x) for x in re.findall(r'^M900 K([\d.]+)$', txt, re.M)]
        self.assertEqual(ks, [round(i * 0.01, 4) for i in range(8)]); self.assertEqual(len(re.findall(r'^;TRAMO \d+', txt, re.M)), 8)
        txt = gen("ringing-freq"); fx = re.findall(r'^M593 X F([\d.]+)$', txt, re.M); fy = re.findall(r'^M593 Y F([\d.]+)$', txt, re.M)
        self.assertEqual(fx, fy); self.assertEqual([float(f) for f in fx], [25 + 5 * i for i in range(8)])
        self.assertEqual(len(re.findall(r'^M400$', txt, re.M)), 8, "M400 antes de cada cambio de frecuencia")
        txt = gen("ringing-accel", maquina="xq1"); acc = [int(x) for x in re.findall(r'^M204 P(\d+) T\d+$', txt, re.M)]
        self.assertEqual(acc, [1000 + 500 * i for i in range(8)]); self.assertEqual(acc[-1], 4500, "el ultimo tramo es la aceleracion maxima de la maquina")
        accq = [int(x) for x in re.findall(r'^M204 P(\d+) T\d+$', gen("ringing-accel", maquina="aquila"), re.M)]
        self.assertLessEqual(max(accq), 3500); self.assertEqual(len(accq), 8)
    def test_cada_tramo_tiene_su_altura(self):
        txt = gen("k"); zs = sorted({float(z) for z in re.findall(r'^G1 Z([\d.]+) F600$', txt, re.M)})
        capas = [z for z in zs if z <= 40.0 + 1e-6 and z >= 0.2]
        self.assertAlmostEqual(max(capas), 40.0, places=3)      # 8 tramos x 5 mm
        # la marca de tramo: en la primera capa de los tramos 2..8 hay un lazo exterior extra (una linea mas)
        lineas = {}
        z = None
        for l in txt.splitlines():
            m = re.match(r'G1 Z([\d.]+) F600', l)
            if m: z = float(m.group(1)); lineas.setdefault(z, 0); continue
            if l.startswith("G1 X") and z is not None and 0.2 < z <= 40.0: lineas[z] += 1
        base = lineas[0.4]
        for t in range(1, 8): self.assertGreater(lineas[round(5.0 * t + 0.2, 3)], base, "marca del tramo %d" % (t + 1))
        self.assertEqual(lineas[0.4], lineas[4.8], "capas normales: igual numero de lineas")
    def test_extrusion_coherente_con_el_ancho_de_linea(self):
        """Releyendo el texto: filamento extruido / (recorrido x capa) = ancho de linea en todos los movimientos de impresion."""
        for t in TIPOS:
            txt = gen(t); x = y = None; e = 0.0; z = 0.0; area = math.pi * (1.75 / 2) ** 2; malos = 0; imp = 0
            for l in txt.splitlines():
                l = l.split(";")[0].strip()
                if not l.startswith("G1"): continue
                v = {k[0]: float(k[1:]) for k in l.split()[1:]}
                nx, ny, ne, nz = v.get("X", x), v.get("Y", y), v.get("E", e), v.get("Z", z)
                if x is not None and "E" in v and ne > e and ("X" in v or "Y" in v):
                    d = math.hypot(nx - x, ny - y)
                    if d > 0.05 and z >= 0.2 and abs(z - 0.3) > 1e-9:       # (la linea de cebado va a Z 0.3 con otro ancho)
                        ancho = (ne - e) * area / (d * 0.2); imp += 1
                        if not (0.40 <= ancho <= 0.50): malos += 1
                x, y, e, z = nx, ny, ne, nz if nz is not None else z
            self.assertGreater(imp, 100); self.assertEqual(malos, 0, t)
    def test_caudal_velocidad_igual_a_caudal_sobre_seccion(self):
        txt = gen("caudal"); vs = sorted({int(f) for f in re.findall(r'F(\d+)', txt)})
        for q in (6.0, 12.0, 20.0): self.assertTrue(any(abs(v / 60 - q / (0.45 * 0.2)) < 1.0 for v in vs), "velocidad de %.0f mm3/s" % q)
        self.assertLessEqual(max(v for v in vs) / 60, 500)
    def test_la_torre_cabe_y_esta_centrada(self):
        for maq, m in gt.MAQUINAS.items():
            txt = gen("k", maquina=maq); xs = [float(v) for v in re.findall(r'X([\d.]+)', txt)]; ys = [float(v) for v in re.findall(r'Y([\d.]+)', txt)]
            self.assertGreaterEqual(min(xs), gt.MARGEN); self.assertLessEqual(max(xs), m["cama"][0] - gt.MARGEN)
            self.assertGreaterEqual(min(ys), gt.MARGEN); self.assertLessEqual(max(ys), m["cama"][1] - gt.MARGEN)

class Rechazos(unittest.TestCase):
    def rechaza(self, tipo, texto, **kw):
        with self.assertRaises(ValueError) as c: gen(tipo, **kw)
        self.assertIn(texto, str(c.exception))
    def test_argumentos(self):
        self.rechaza("ringing-freq", "frecuencias", f_inicio=10)                       # por debajo del buffer de Input Shaping: desbordaria
        self.rechaza("ringing-freq", "frecuencias", f_inicio=150, f_paso=20)
        self.rechaza("ringing-freq", "--f-paso", f_paso=0)
        self.rechaza("ringing-accel", "aceleracion maxima", a_inicio=3000, a_paso=500)   # 6500 > 4500: Marlin la recortaria
        self.rechaza("ringing-accel", "aceleraciones", a_inicio=100)
        self.rechaza("k", "K debe estar", k_inicio=0, k_paso=1.0)
        self.rechaza("k", "--k-paso", k_paso=-0.01)
        self.rechaza("k", "--tramos", tramos=0); self.rechaza("k", "--tramos", tramos=21)
        self.rechaza("k", "--temp-boquilla", temp_boquilla=300); self.rechaza("k", "--temp-cama", temp_cama=200)
        self.rechaza("k", "--velocidad", velocidad=900); self.rechaza("k", "--lado", lado=400)
        self.rechaza("k", "maquina desconocida", maquina="otra")
        self.rechaza("caudal", "circulo", diametro=30, c_inicio=40, c_paso=1)
        self.rechaza("caudal", "maximo de la maquina", diametro=100, c_inicio=60, c_paso=10, tramos=3)
    def test_subir_amax_es_explicito_y_queda_en_el_archivo(self):
        txt = gen("ringing-accel", a_inicio=3000, a_paso=500, subir_amax=True)
        self.assertIn("M201 X6500 Y6500", txt); self.assertIn("ATENCION", txt)
        self.assertEqual(gt.verificar_gcode(txt, gt.MAQUINAS["xq1"]), [])
    def test_el_default_de_aceleracion_nunca_supera_la_maxima(self):
        for maq, m in gt.MAQUINAS.items():
            for n in (2, 5, 8, 12): 
                accs = [int(x) for x in re.findall(r'^M204 P(\d+)', gen("ringing-accel", maquina=maq, tramos=n), re.M)]
                self.assertLessEqual(max(accs), min(m["amax"])); self.assertEqual(len(accs), n)

class Verificador(unittest.TestCase):
    """El verificador lee el TEXTO: detecta G-code manipulado aunque el generador estuviera mal."""
    def setUp(self): self.m = gt.MAQUINAS["xq1"]; self.txt = gen("k")
    def prob(self, txt): return gt.verificar_gcode(txt, self.m)
    def test_original_correcto(self): self.assertEqual(self.prob(self.txt), [])
    def test_detecta_manipulaciones(self):
        casos = {
            "sin M501 final": self.txt.rstrip()[:-4] + "\n",
            "fuera de la cama": self.txt.replace("G1 X128.650 Y128.650 F9000", "G1 X999 Y128.650 F9000", 1),
            "orden no permitida": self.txt.replace("M220 S100", "M999", 1),
            "frecuencia por debajo del buffer": gen("ringing-freq").replace("M593 X F25.0", "M593 X F10.0", 1),
            "K fuera de rango": self.txt.replace("M900 K0.010", "M900 K9.000", 1),
            "temperatura alta": self.txt.replace("M109 S210", "M109 S300", 1),
            "sin calentar": self.txt.replace("M109 S210\n", "", 1),
            "velocidad excesiva": self.txt.replace("F6000", "F90000", 1).replace("F1800", "F90000", 1),
            "mover antes de G28": self.txt.replace("G28\n", "", 1),
            "desplazamiento pegado a la cama": self.txt.replace("G1 Z5.000 F600\n", "", 1),
            "extrusion excesiva": self.txt.replace("E5.38814 F1800", "E55.38814 F1800", 1),
            "aceleracion mayor que la maxima": self.txt.replace("M204 P3000 T3000", "M204 P9000 T9000", 1),
            "ventilador fuera de rango": self.txt.replace("M106 S255", "M106 S999", 1),
        }
        for nombre, t in casos.items():
            self.assertNotEqual(t, self.txt, "el caso '%s' debe cambiar el texto" % nombre)
            self.assertTrue(self.prob(t), "no detecto: " + nombre)
    def test_no_escribe_si_falla(self):
        import tempfile, os
        with tempfile.TemporaryDirectory() as d:
            ruta = os.path.join(d, "x.gcode")
            self.assertEqual(gt.main(["ringing-freq", "--f-inicio", "10", "--salida", ruta]), 2)
            self.assertFalse(os.path.exists(ruta), "con un error no se escribe ningun archivo")
            self.assertEqual(gt.main(["k", "--salida", ruta]), 0); self.assertTrue(os.path.exists(ruta))

class Entregados(unittest.TestCase):
    def test_archivos_listos_para_la_sd_coinciden_con_el_generador(self):
        carpeta = ROOT / "herramientas" / "torres" / "listos"
        if not carpeta.exists(): self.skipTest("sin carpeta herramientas/torres/listos")
        nombres = {"torre_ringing_frecuencia_xq1.gcode": "ringing-freq", "torre_ringing_aceleracion_xq1.gcode": "ringing-accel", "torre_k_xq1.gcode": "k", "torre_caudal_xq1.gcode": "caudal"}
        for n, t in nombres.items():
            self.assertTrue((carpeta / n).exists(), n)
            self.assertEqual((carpeta / n).read_text(encoding="ascii"), gen(t, maquina="xq1"), "%s esta desactualizado: python3 herramientas/torres/generar_torres.py todas --salida herramientas/torres/listos" % n)

if __name__ == "__main__":
    unittest.main(verbosity=1)
