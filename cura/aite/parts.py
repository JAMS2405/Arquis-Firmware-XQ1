# SPDX-License-Identifier: LGPL-3.0-or-later
"""Piezas de prueba con volumen y angulos conocidos por formula, para validar el nucleo geometrico sin depender de un STL externo.

Todas devuelven mallas estancas con normales hacia fuera. ``make_cross_part`` es la pieza de la primera prueba del informe (F):
un tronco vertical con ramas horizontales opuestas unidas a 90 grados. Se construye por voxeles alineados a ``cell`` (la union de cajas
alineadas queda exacta), con cada cara visible como dos triangulos.
"""
import math
from typing import Iterable, List, Sequence, Set, Tuple

from .mesh import Mesh, MeshBuilder, MeshError

# esquinas de la cara de un cubo unidad, antihorarias vistas desde fuera
_FACES = {
    (0, 0, -1): ((0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0)),
    (0, 0, 1): ((0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)),
    (0, -1, 0): ((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)),
    (0, 1, 0): ((1, 1, 0), (0, 1, 0), (0, 1, 1), (1, 1, 1)),
    (-1, 0, 0): ((0, 1, 0), (0, 0, 0), (0, 0, 1), (0, 1, 1)),
    (1, 0, 0): ((1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)),
}


def _quad(b: MeshBuilder, p0, p1, p2, p3) -> None:
    b.triangle(p0, p1, p2)
    b.triangle(p0, p2, p3)


def make_box(size: Tuple[float, float, float], origin: Tuple[float, float, float] = (0.0, 0.0, 0.0)) -> Mesh:
    sx, sy, sz = size
    if min(size) <= 0:
        raise ValueError("las dimensiones de la caja deben ser positivas")
    b = MeshBuilder()
    ox, oy, oz = origin
    for corners in _FACES.values():
        pts = [(ox + c[0] * sx, oy + c[1] * sy, oz + c[2] * sz) for c in corners]
        _quad(b, *pts)
    return b.mesh


def make_cylinder(radius: float, height: float, segments: int = 96, z0: float = 0.0) -> Mesh:
    """Cilindro vertical centrado en el origen. Volumen teselado = segments/2 * r^2 * sen(2 pi/segments) * h."""
    if radius <= 0 or height <= 0 or segments < 8:
        raise ValueError("radio, altura positivos y al menos 8 segmentos")
    b = MeshBuilder()
    ring = [(radius * math.cos(2 * math.pi * i / segments), radius * math.sin(2 * math.pi * i / segments)) for i in range(segments)]
    bc, tc = (0.0, 0.0, z0), (0.0, 0.0, z0 + height)
    for i in range(segments):
        (x0, y0), (x1, y1) = ring[i], ring[(i + 1) % segments]
        b.triangle(bc, (x1, y1, z0), (x0, y0, z0))                       # tapa inferior, normal -z
        b.triangle(tc, (x0, y0, z0 + height), (x1, y1, z0 + height))     # tapa superior, normal +z
        _quad(b, (x0, y0, z0), (x1, y1, z0), (x1, y1, z0 + height), (x0, y0, z0 + height))
    return b.mesh


def cylinder_volume(radius: float, height: float, segments: int) -> float:
    return segments / 2.0 * radius * radius * math.sin(2 * math.pi / segments) * height


def make_extruded_profile(profile_xz: Sequence[Tuple[float, float]], y0: float, y1: float) -> Mesh:
    """Prisma: poligono CONVEXO en el plano XZ (antihorario con x a la derecha y z arriba) extruido de y0 a y1."""
    n = len(profile_xz)
    if n < 3 or y1 <= y0:
        raise ValueError("el perfil necesita 3 o mas puntos y y1 > y0")
    area2 = sum(profile_xz[i][0] * profile_xz[(i + 1) % n][1] - profile_xz[(i + 1) % n][0] * profile_xz[i][1] for i in range(n))
    if area2 <= 0:
        raise ValueError("el perfil debe ser antihorario (x a la derecha, z arriba)")
    b = MeshBuilder()
    front = [(x, y0, z) for x, z in profile_xz]
    back = [(x, y1, z) for x, z in profile_xz]
    for i in range(1, n - 1):
        b.triangle(front[0], front[i], front[i + 1])                     # cara en y0, normal -y
        b.triangle(back[0], back[i + 1], back[i])                        # cara en y1, normal +y
    for i in range(n):
        j = (i + 1) % n
        _quad(b, front[i], back[i], back[j], front[j])
    return b.mesh


def make_overhang_wedge(angle_deg: float, base: float = 3.0, height: float = 3.0, width: float = 2.0) -> Mesh:
    """Bloque cuya cara derecha se inclina hacia afuera al subir: es un voladizo de ``angle_deg`` respecto a la vertical
    (normal de esa cara con nz = -sen(angle)). La cara izquierda y los lados son verticales. Posado en z = 0."""
    if not 0.0 <= angle_deg < 90.0:
        raise ValueError("el angulo debe estar en [0, 90)")
    run = height * math.tan(math.radians(angle_deg))
    return make_extruded_profile([(0.0, 0.0), (base, 0.0), (base + run, height), (0.0, height)], 0.0, width)


# ---------------------------------------------------------------- union de cajas alineadas a ``cell``
def _idx(v: float, cell: float, what: str) -> int:
    q = v / cell
    if abs(q - round(q)) > 1e-6:
        raise ValueError("%s = %g no es multiplo de cell = %g" % (what, v, cell))
    return int(round(q))


def mesh_from_voxels(occupied: Set[Tuple[int, int, int]], cell: float) -> Mesh:
    b = MeshBuilder()
    for (i, j, k) in occupied:
        for (dx, dy, dz), corners in _FACES.items():
            if (i + dx, j + dy, k + dz) in occupied:
                continue
            pts = [((i + c[0]) * cell, (j + c[1]) * cell, (k + c[2]) * cell) for c in corners]
            _quad(b, *pts)
    return b.mesh


def _box_voxels(x: Tuple[float, float], y: Tuple[float, float], z: Tuple[float, float], cell: float, out: Set[Tuple[int, int, int]]) -> None:
    i0, i1 = _idx(x[0], cell, "x0"), _idx(x[1], cell, "x1")
    j0, j1 = _idx(y[0], cell, "y0"), _idx(y[1], cell, "y1")
    k0, k1 = _idx(z[0], cell, "z0"), _idx(z[1], cell, "z1")
    for i in range(i0, i1):
        for j in range(j0, j1):
            for k in range(k0, k1):
                out.add((i, j, k))


def make_cross_part(trunk: Tuple[float, float, float] = (20.0, 20.0, 40.0), branch_len: float = 15.0,
                    branch_section: Tuple[float, float] = (10.0, 10.0), branch_z0: float = 15.0, axis: str = "x",
                    both: bool = True, cell: float = 0.5) -> Mesh:
    """Tronco (x, y, z) centrado en XY y apoyado en z = 0, con una o dos ramas opuestas de seccion (ancho, alto), de longitud
    ``branch_len`` contada desde la pared del tronco, cuya cara inferior queda en ``branch_z0``. ``axis`` = 'x' o 'y'.
    Todas las cotas deben ser multiplos de ``cell``. Volumen exacto: ``cross_part_volume``."""
    if axis not in ("x", "y"):
        raise ValueError("axis debe ser 'x' o 'y'")
    tx, ty, tz = trunk
    bw, bh = branch_section
    if branch_z0 + bh > tz + 1e-9 or branch_z0 < 0 or branch_len <= 0:
        raise ValueError("la rama debe caber dentro de la altura del tronco y tener longitud positiva")
    vox: Set[Tuple[int, int, int]] = set()
    _box_voxels((-tx / 2, tx / 2), (-ty / 2, ty / 2), (0.0, tz), cell, vox)
    if axis == "x":
        _box_voxels((tx / 2, tx / 2 + branch_len), (-bw / 2, bw / 2), (branch_z0, branch_z0 + bh), cell, vox)
        if both:
            _box_voxels((-tx / 2 - branch_len, -tx / 2), (-bw / 2, bw / 2), (branch_z0, branch_z0 + bh), cell, vox)
    else:
        _box_voxels((-bw / 2, bw / 2), (ty / 2, ty / 2 + branch_len), (branch_z0, branch_z0 + bh), cell, vox)
        if both:
            _box_voxels((-bw / 2, bw / 2), (-ty / 2 - branch_len, -ty / 2), (branch_z0, branch_z0 + bh), cell, vox)
    return mesh_from_voxels(vox, cell)


def cross_part_volume(trunk=(20.0, 20.0, 40.0), branch_len: float = 15.0, branch_section=(10.0, 10.0), both: bool = True) -> float:
    n = 2 if both else 1
    return trunk[0] * trunk[1] * trunk[2] + n * branch_len * branch_section[0] * branch_section[1]
