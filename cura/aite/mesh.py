# SPDX-License-Identifier: LGPL-3.0-or-later
"""Geometry Analyzer, parte 1: malla triangular, lectores STL/3MF, comprobacion de estanqueidad y escritura.

Regla de AITE (informe, D.9): el modelo original NO se modifica ni se repara. Si la malla no es estanca, ``analyze_mesh`` lo dice y
quien la use decide; aqui nunca se cierran agujeros ni se reorientan caras.
"""
import math
import os
import struct
import zipfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from typing import Callable, Dict, List, Sequence, Tuple

Vec = Tuple[float, float, float]
Tri = Tuple[int, int, int]


class MeshError(ValueError):
    """Archivo ilegible, truncado, con valores no finitos o con caracteristicas no soportadas."""


@dataclass
class Mesh:
    vertices: List[Vec] = field(default_factory=list)
    triangles: List[Tri] = field(default_factory=list)

    def bbox(self) -> Tuple[Vec, Vec]:
        if not self.vertices:
            raise MeshError("malla vacia")
        xs, ys, zs = zip(*self.vertices)
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))

    def corners(self, t: Tri) -> Tuple[Vec, Vec, Vec]:
        return self.vertices[t[0]], self.vertices[t[1]], self.vertices[t[2]]

    def normal_area(self, t: Tri) -> Tuple[Vec, float]:
        """(normal exterior unitaria, area) segun el orden antihorario visto desde fuera. Triangulo degenerado: ((0,0,0), 0)."""
        a, b, c = self.corners(t)
        ux, uy, uz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
        vx, vy, vz = c[0] - a[0], c[1] - a[1], c[2] - a[2]
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        m = math.sqrt(nx * nx + ny * ny + nz * nz)
        if m < 1e-18:
            return (0.0, 0.0, 0.0), 0.0
        return (nx / m, ny / m, nz / m), m / 2.0

    def signed_volume(self) -> float:
        """Teorema de la divergencia. Positivo si las normales miran hacia fuera."""
        v = 0.0
        for t in self.triangles:
            a, b, c = self.corners(t)
            v += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0
        return v

    def surface_area(self) -> float:
        return sum(self.normal_area(t)[1] for t in self.triangles)

    def transformed(self, fn: Callable[[Vec], Vec]) -> "Mesh":
        return Mesh([fn(v) for v in self.vertices], list(self.triangles))


@dataclass
class MeshReport:
    n_vertices: int
    n_triangles: int
    open_edges: int          # aristas usadas por una sola cara
    nonmanifold_edges: int   # aristas usadas por mas de dos caras
    inconsistent_edges: int  # aristas recorridas dos veces en el mismo sentido (orientacion inconsistente)
    degenerate_triangles: int
    volume: float            # con signo: negativo = normales hacia dentro
    bbox: Tuple[Vec, Vec]

    @property
    def inverted(self) -> bool:
        return self.volume < 0

    @property
    def watertight(self) -> bool:
        return (self.n_triangles > 0 and self.open_edges == 0 and self.nonmanifold_edges == 0 and self.inconsistent_edges == 0
                and self.degenerate_triangles == 0 and self.volume > 0)

    def problems(self) -> List[str]:
        out = []
        if self.n_triangles == 0:
            out.append("malla vacia")
        if self.open_edges:
            out.append("%d aristas abiertas (la malla tiene agujeros)" % self.open_edges)
        if self.nonmanifold_edges:
            out.append("%d aristas compartidas por mas de dos caras" % self.nonmanifold_edges)
        if self.inconsistent_edges:
            out.append("%d aristas con orientacion inconsistente" % self.inconsistent_edges)
        if self.degenerate_triangles:
            out.append("%d triangulos degenerados" % self.degenerate_triangles)
        if self.n_triangles and self.volume <= 0 and not (self.open_edges or self.nonmanifold_edges or self.inconsistent_edges):
            out.append("volumen %s: las normales miran hacia dentro" % ("negativo" if self.volume < 0 else "nulo"))
        return out


def analyze_mesh(mesh: Mesh) -> MeshReport:
    directed: Dict[Tuple[int, int], int] = {}
    degenerate = 0
    for t in mesh.triangles:
        if len({t[0], t[1], t[2]}) < 3 or mesh.normal_area(t)[1] == 0.0:
            degenerate += 1
            continue
        for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            directed[(a, b)] = directed.get((a, b), 0) + 1
    seen = set()
    open_e = nonman = incons = 0
    for (a, b), n in directed.items():
        key = (a, b) if a < b else (b, a)
        if key in seen:
            continue
        seen.add(key)
        fwd, back = directed.get((a, b), 0), directed.get((b, a), 0)
        if fwd + back == 1:
            open_e += 1
        elif fwd + back > 2:
            nonman += 1
        elif fwd == 2 or back == 2:
            incons += 1
    vol = mesh.signed_volume() if mesh.triangles else 0.0
    bb = mesh.bbox() if mesh.vertices else ((0.0,) * 3, (0.0,) * 3)
    return MeshReport(len(mesh.vertices), len(mesh.triangles), open_e, nonman, incons, degenerate, vol, bb)


# ---------------------------------------------------------------- construccion con soldado de vertices
class MeshBuilder:
    """Acumula triangulos soldando vertices iguales (redondeo a 1e-6 mm)."""
    def __init__(self) -> None:
        self.mesh = Mesh()
        self._index: Dict[Tuple[int, int, int], int] = {}

    def vertex(self, v: Sequence[float]) -> int:
        if not all(math.isfinite(c) for c in v):
            raise MeshError("vertice no finito")
        key = (round(v[0] * 1e6), round(v[1] * 1e6), round(v[2] * 1e6))
        i = self._index.get(key)
        if i is None:
            i = len(self.mesh.vertices)
            self._index[key] = i
            self.mesh.vertices.append((float(v[0]), float(v[1]), float(v[2])))
        return i

    def triangle(self, a: Sequence[float], b: Sequence[float], c: Sequence[float]) -> None:
        self.mesh.triangles.append((self.vertex(a), self.vertex(b), self.vertex(c)))


# ---------------------------------------------------------------- STL
def load_stl(path: str) -> Mesh:
    with open(path, "rb") as f:
        data = f.read()
    return parse_stl(data)


def parse_stl(data: bytes) -> Mesh:
    if len(data) >= 84:
        n = struct.unpack_from("<I", data, 80)[0]
        if 84 + 50 * n == len(data):                       # binario, tamano exacto
            return _parse_binary_stl(data, n)
    head = data[:5].lower()
    if head == b"solid" and b"facet" in data[:4096] + data[-4096:]:
        return _parse_ascii_stl(data)
    if len(data) >= 84:
        raise MeshError("STL binario truncado o con tamano incoherente con su cabecera")
    raise MeshError("archivo no reconocido como STL")


def _parse_binary_stl(data: bytes, n: int) -> Mesh:
    b = MeshBuilder()
    for i in range(n):
        vals = struct.unpack_from("<12f", data, 84 + 50 * i)
        if not all(math.isfinite(v) for v in vals):
            raise MeshError("STL con valores no finitos (triangulo %d)" % i)
        b.triangle(vals[3:6], vals[6:9], vals[9:12])
    if not b.mesh.triangles:
        raise MeshError("STL sin triangulos")
    return b.mesh


def _parse_ascii_stl(data: bytes) -> Mesh:
    try:
        text = data.decode("utf-8", errors="strict")
    except UnicodeDecodeError as e:
        raise MeshError("STL ASCII con codificacion invalida") from e
    b = MeshBuilder()
    verts: List[Tuple[float, float, float]] = []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[0].lower() == "vertex":
            try:
                v = (float(parts[1]), float(parts[2]), float(parts[3]))
            except ValueError as e:
                raise MeshError("STL ASCII con un numero ilegible: %r" % line.strip()) from e
            if not all(math.isfinite(c) for c in v):
                raise MeshError("STL con valores no finitos")
            verts.append(v)
            if len(verts) == 3:
                b.triangle(*verts)
                verts = []
    if verts:
        raise MeshError("STL ASCII truncado: faceta incompleta")
    if not b.mesh.triangles:
        raise MeshError("STL sin triangulos")
    return b.mesh


def write_stl_binary(mesh: Mesh, path: str) -> None:
    with open(path, "wb") as f:
        f.write(b"AITE binary STL".ljust(80, b"\0"))
        f.write(struct.pack("<I", len(mesh.triangles)))
        for t in mesh.triangles:
            (nx, ny, nz), _ = mesh.normal_area(t)
            a, b, c = mesh.corners(t)
            f.write(struct.pack("<12fH", nx, ny, nz, *a, *b, *c, 0))


# ---------------------------------------------------------------- 3MF
_NS_CORE = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
_UNIT_MM = {"micron": 0.001, "millimeter": 1.0, "centimeter": 10.0, "inch": 25.4, "foot": 304.8, "meter": 1000.0}
Matrix = Tuple[float, ...]
_IDENTITY: Matrix = (1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0)


def _parse_matrix(s: str) -> Matrix:
    try:
        v = tuple(float(x) for x in s.split())
    except ValueError as e:
        raise MeshError("3MF: matriz de transformacion ilegible") from e
    if len(v) != 12 or not all(math.isfinite(x) for x in v):
        raise MeshError("3MF: la transformacion debe tener 12 valores finitos")
    return v


def _apply(m: Matrix, p: Vec) -> Vec:
    """Convencion 3MF (vector fila): x' = x*m00 + y*m10 + z*m20 + m30, etc., con m = m00 m01 m02 m10 m11 m12 m20 m21 m22 m30 m31 m32."""
    x, y, z = p
    return (x * m[0] + y * m[3] + z * m[6] + m[9], x * m[1] + y * m[4] + z * m[7] + m[10], x * m[2] + y * m[5] + z * m[8] + m[11])


def load_3mf(path: str) -> Mesh:
    """Lee un 3MF a milimetros del mundo: aplica componentes, transformaciones de ``build`` y unidades.
    No soporta rutas externas (extension de produccion), objetos distintos de ``model`` se ignoran y se exige al menos un item."""
    try:
        zf = zipfile.ZipFile(path)
    except (zipfile.BadZipFile, OSError) as e:
        raise MeshError("3MF ilegible: %s" % e) from e
    with zf:
        names = [n for n in zf.namelist() if n.lower().endswith(".model")]
        if not names:
            raise MeshError("3MF sin archivo .model")
        main = next((n for n in names if n.lower() == "3d/3dmodel.model"), names[0])
        try:
            root = ET.fromstring(zf.read(main))
        except ET.ParseError as e:
            raise MeshError("3MF con XML invalido: %s" % e) from e

    def q(tag: str) -> str:
        return "{%s}%s" % (_NS_CORE, tag)

    unit = root.get("unit", "millimeter")
    if unit not in _UNIT_MM:
        raise MeshError("3MF: unidad desconocida %r" % unit)
    scale = _UNIT_MM[unit]
    objects: Dict[str, ET.Element] = {}
    for obj in root.iter(q("object")):
        objects[obj.get("id", "")] = obj
    build = root.find(q("build"))
    items = list(build.findall(q("item"))) if build is not None else []
    if not items:
        raise MeshError("3MF sin elementos en <build>")

    def object_geometry(oid: str, depth: int = 0) -> Tuple[List[Vec], List[Tri]]:
        if depth > 16:
            raise MeshError("3MF: componentes anidados en exceso")
        obj = objects.get(oid)
        if obj is None:
            raise MeshError("3MF: objeto %r inexistente" % oid)
        if obj.get("type", "model") != "model":
            return [], []
        mesh_el = obj.find(q("mesh"))
        if mesh_el is not None:
            vs: List[Vec] = []
            for v in mesh_el.iter(q("vertex")):
                try:
                    p = (float(v.get("x")), float(v.get("y")), float(v.get("z")))
                except (TypeError, ValueError) as e:
                    raise MeshError("3MF: vertice ilegible") from e
                if not all(math.isfinite(c) for c in p):
                    raise MeshError("3MF: vertice no finito")
                vs.append(p)
            ts: List[Tri] = []
            for t in mesh_el.iter(q("triangle")):
                try:
                    tri = (int(t.get("v1")), int(t.get("v2")), int(t.get("v3")))
                except (TypeError, ValueError) as e:
                    raise MeshError("3MF: triangulo ilegible") from e
                if min(tri) < 0 or max(tri) >= len(vs):
                    raise MeshError("3MF: indice de vertice fuera de rango")
                ts.append(tri)
            return vs, ts
        comps = obj.find(q("components"))
        if comps is None:
            return [], []
        vs, ts = [], []
        for c in comps.findall(q("component")):
            if any(k.endswith("}path") for k in c.attrib):
                raise MeshError("3MF: componentes en archivos externos no soportados")
            cv, ct = object_geometry(c.get("objectid", ""), depth + 1)
            m = _parse_matrix(c.get("transform")) if c.get("transform") else _IDENTITY
            base = len(vs)
            vs.extend(_apply(m, p) for p in cv)
            ts.extend((a + base, b + base, c2 + base) for a, b, c2 in ct)
        return vs, ts

    out = MeshBuilder()
    for it in items:
        vs, ts = object_geometry(it.get("objectid", ""))
        m = _parse_matrix(it.get("transform")) if it.get("transform") else _IDENTITY
        world = [tuple(c * scale for c in _apply(m, p)) for p in vs]
        for a, b, c in ts:
            out.triangle(world[a], world[b], world[c])
    if not out.mesh.triangles:
        raise MeshError("3MF sin geometria imprimible")
    return out.mesh


def write_3mf(mesh: Mesh, path: str) -> None:
    """3MF minimo (un objeto, milimetros). Sirve para pruebas y para exportar la pieza de prueba."""
    verts = "".join('<vertex x="%.6f" y="%.6f" z="%.6f"/>' % v for v in mesh.vertices)
    tris = "".join('<triangle v1="%d" v2="%d" v3="%d"/>' % t for t in mesh.triangles)
    model = ('<?xml version="1.0" encoding="UTF-8"?><model unit="millimeter" xmlns="%s"><resources><object id="1" type="model"><mesh>'
             '<vertices>%s</vertices><triangles>%s</triangles></mesh></object></resources><build><item objectid="1"/></build></model>'
             % (_NS_CORE, verts, tris))
    rels = ('<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    ctypes = ('<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
              '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
              '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/></Types>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", ctypes)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)


def load_mesh(path: str) -> Mesh:
    ext = os.path.splitext(path)[1].lower()
    if ext == ".stl":
        return load_stl(path)
    if ext == ".3mf":
        return load_3mf(path)
    raise MeshError("formato no soportado: %s (solo .stl y .3mf)" % ext)
