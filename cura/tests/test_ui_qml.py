# SPDX-License-Identifier: LGPL-3.0-or-later
"""Gestor de objetos: el QML se carga SIN avisos con Qt real (PyQt6, sin pantalla), lista y mapa estan sincronizados y se genera una imagen.

No prueba ObjectManager.qml (UM.Dialog necesita Cura) ni el menu de la extension: ver docs/slicer_suite/05_etapa2_manifiesto.md.
"""
import os
import sys

import pytest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
os.environ.setdefault("QT_QUICK_BACKEND", "software")

try:
    from PyQt6.QtCore import QUrl
    from PyQt6.QtGui import QGuiApplication
    from PyQt6.QtQuick import QQuickView
    HAVE_QT = True
except Exception:                                                   # sin PyQt6 o sin libEGL
    HAVE_QT = False

pytestmark = pytest.mark.skipif(not HAVE_QT, reason="necesita PyQt6 (pip install PyQt6) y las bibliotecas graficas del sistema")

import arquis_bootstrap
from arquis_engines import bed_map_engine as bm
from fixtures import Obj, grid_objects, instances_from

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UI = os.path.join(ROOT, "plugins", "ARQUISPlugin", "ui")
IMG = os.path.join(ROOT, "..", "docs", "slicer_suite", "img")


@pytest.fixture(scope="module")
def app():
    a = QGuiApplication.instance() or QGuiApplication(sys.argv[:1])
    arquis_bootstrap.load_engines()
    yield a


def make_view(backend):
    v = QQuickView()
    warnings = []
    v.engine().warnings.connect(lambda ws: warnings.extend(w.toString() for w in ws))
    v.rootContext().setContextProperty("manager", backend)
    v.setResizeMode(QQuickView.ResizeMode.SizeRootObjectToView)
    v.setSource(QUrl.fromLocalFile(os.path.join(ROOT, "tests", "harness_object_manager.qml")))
    v.resize(790, 420)
    v.show()
    QGuiApplication.processEvents()
    return v, warnings


def demo_backend():
    from arquis_engines.protocol_limits import DESIGN_CAPACITY
    from ui_backend import ObjectManagerBackend
    objs = [Obj("Llavero", 30, 30, 50, 50, 2), Obj("Llavero", 110, 30, 130, 50, 2), Obj("Soporte", 170, 100, 230, 130, 3),
            Obj("Disco", 40, 180, 80, 220, 3), Obj("Llavero", 114, 36, 134, 56, 2)]
    inst = instances_from(objs)
    inst[3] = type(inst[3])(inst[3].scene_index, "Disco", 40, 180, 80, 220, shape="C")
    b = ObjectManagerBackend()
    b.setData(inst, (0.0, 0.0, 300.0, 300.0))
    return b


def test_qml_loads_without_warnings_and_list_and_map_are_synchronized(app):
    b = demo_backend()
    v, warnings = make_view(b)
    assert v.status() == QQuickView.Status.Ready and not warnings, warnings
    root = v.rootObject()
    assert root is not None
    assert len(b.markers) == len(b.rows) == 5
    # seleccionar desde la LISTA resalta el marcador del MAPA y viceversa (un solo indice)
    b.select(2)
    assert [r["selected"] for r in b.rows] == [False, False, True, False, False]
    assert [m["selected"] for m in b.markers] == [False, False, True, False, False]
    m = b.markers[3]
    b.pickAt((m["x0"] + m["x1"]) // 2, (m["y0"] + m["y1"]) // 2)                      # toque en el marcador 4 (circulo)
    assert b.selectedKey == 3 and [r["selected"] for r in b.rows][3] and "Disco" in b.selectedInfo
    b.pickAt(1, 1)                                                                    # fuera de cualquier marcador: sin seleccion
    assert b.selectedKey == -1
    QGuiApplication.processEvents()
    assert not warnings
    v.close()


def test_every_state_has_text_and_badge_and_the_image_is_rendered(app):
    b = demo_backend()
    b.setData(b._instances, b._bed, states={0: bm.PENDING, 1: bm.PRINTING, 2: bm.DONE, 3: bm.CANCELED, 4: bm.UNAVAILABLE})
    b.select(1)
    v, warnings = make_view(b)
    assert not warnings
    assert {m["stateText"] for m in b.markers} == set(bm.STATE_TEXT.values())
    assert {m["badge"] for m in b.markers} == {"", ">", "+", "x", "?"}
    img = v.grabWindow()
    assert not img.isNull() and img.width() >= 700
    os.makedirs(IMG, exist_ok=True)
    img.save(os.path.join(IMG, "gestor_objetos_estados.png"))
    v.close()


def test_thirty_identical_copies_render_without_warnings(app):
    from ui_backend import ObjectManagerBackend
    b = ObjectManagerBackend()
    b.setData(instances_from(grid_objects(30, "Llavero", w=20, d=20, gap=6)), (0.0, 0.0, 300.0, 300.0))
    v, warnings = make_view(b)
    assert not warnings and len(b.rows) == 30 and [r["label"] for r in b.rows][:3] == ["1", "2", "3"]
    assert [r["name"] for r in b.rows][:2] == ["Llavero 1", "Llavero 2"]
    img = v.grabWindow()
    img.save(os.path.join(IMG, "gestor_objetos_30_iguales.png"))
    v.close()
    b.setData(instances_from(grid_objects(34, "Llavero", w=20, d=20, gap=6)), (0.0, 0.0, 300.0, 300.0))
    assert sum(1 for r in b.rows if not r["cancelable"]) == 4 and any("superan la capacidad" in w for w in b.warnings)


def test_incompatible_bed_shows_a_red_banner_and_no_markers(app):
    from ui_backend import ObjectManagerBackend
    b = ObjectManagerBackend()
    b.setData(instances_from(grid_objects(8, "Llavero", w=20, d=20, gap=6, bed=450.0)), (0.0, 0.0, 450.0, 450.0))      # perfil CR-10 Max
    v, warnings = make_view(b)
    assert not warnings and not b.mapValid and b.markers == [] and len(b.rows) == 8
    assert "450 x 450" in b.bedMismatch and "sin posicion valida" in (b.select(0) or b.selectedInfo)
    img = v.grabWindow()
    assert not img.isNull()
    img.save(os.path.join(IMG, "gestor_objetos_cama_incompatible.png"))
    v.close()
