#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Comprueba que cada simbolo de la API de Cura/Uranium que usa el plugin EXISTE en el codigo fuente publicado de Cura 5.11.0 / Uranium 5.11.0.

Es una LECTURA del codigo fuente (descarga archivos de texto de raw.githubusercontent.com y busca definiciones): NO ejecuta codigo de Cura ni
prueba que el plugin funcione. Demuestra que los nombres existen; que se comporten como el plugin espera solo lo prueba ejecutarlo en Cura real.
Uso: python3 cura/tools/check_cura_api_source.py [tag]     (por defecto 5.11.0)"""
import re
import sys
import urllib.request

TAG = sys.argv[1] if len(sys.argv) > 1 else "5.11.0"
BASE = {"Uranium": "https://raw.githubusercontent.com/Ultimaker/Uranium/%s/" % TAG, "Cura": "https://raw.githubusercontent.com/Ultimaker/Cura/%s/" % TAG}
# (repositorio, archivo, patron que debe aparecer, para que lo usa el plugin)
CHECKS = [
    ("Uranium", "UM/Scene/Iterator/DepthFirstIterator.py", r"class DepthFirstIterator", "recorrido de la escena (cura_scene_adapter.capture_scene)"),
    ("Uranium", "UM/Scene/SceneNode.py", r"def getName\(", "nombre del nodo = ;MESH:<nombre>"),
    ("Uranium", "UM/Scene/SceneNode.py", r"def getMeshData\(", "malla del nodo"),
    ("Uranium", "UM/Scene/SceneNode.py", r"def getBoundingBox\(", "caja de la escena"),
    ("Uranium", "UM/Scene/SceneNode.py", r"def getWorldOrientation\(", "giro del objeto"),
    ("Uranium", "UM/Scene/SceneNode.py", r"def getWorldPosition\(", "posicion (diagnostico)"),
    ("Uranium", "UM/Scene/SceneNode.py", r"def getParent\(", "grupo padre"),
    ("Uranium", "UM/Scene/SceneNode.py", r"def callDecoration\(", "isSliceable / isGroup / getConvexHull..."),
    ("Uranium", "UM/Math/AxisAlignedBox.py", r"def left\(", "AxisAlignedBox.left"),
    ("Uranium", "UM/Math/AxisAlignedBox.py", r"def right\(", "AxisAlignedBox.right"),
    ("Uranium", "UM/Math/AxisAlignedBox.py", r"def front\(", "AxisAlignedBox.front (mayor Z = hacia el frente)"),
    ("Uranium", "UM/Math/AxisAlignedBox.py", r"def back\(", "AxisAlignedBox.back"),
    ("Uranium", "UM/Math/AxisAlignedBox.py", r"def top\(", "AxisAlignedBox.top (altura del objeto)"),
    ("Uranium", "UM/Math/Quaternion.py", r"def w\(", "Quaternion.w (giro)"),
    ("Uranium", "UM/Math/Quaternion.py", r"def y\(", "Quaternion.y (giro)"),
    ("Uranium", "UM/Math/Polygon.py", r"def getPoints\(", "puntos de la huella convexa"),
    ("Uranium", "UM/Extension.py", r"def setMenuName\(", "menu ARQUIS"),
    ("Uranium", "UM/Extension.py", r"def addMenuItem\(", "entradas del menu"),
    ("Uranium", "UM/Message.py", r"class Message", "avisos"),
    ("Uranium", "UM/Message.py", r"class MessageType", "Message.MessageType.ERROR / WARNING"),
    ("Cura", "cura/CuraApplication.py", r"def getMultiBuildPlateModel\(", "placa activa"),
    ("Uranium", "UM/Qt/QtApplication.py", r"def createQmlComponent\(", "ventana del gestor (CuraApplication hereda de QtApplication)"),
    ("Cura", "cura/CuraApplication.py", r"class CuraApplication\(QtApplication\)", "herencia usada arriba"),
    ("Cura", "cura/Scene/ConvexHullDecorator.py", r"def getConvexHull\(", "huella convexa -> forma del marcador"),
    ("Cura", "cura/Scene/SliceableObjectDecorator.py", r"def isSliceable\(", "filtro de mallas"),
    ("Uranium", "UM/Scene/GroupDecorator.py", r"def isGroup\(", "grupos"),
    ("Cura", "cura/Scene/BuildPlateDecorator.py", r"def getBuildPlateNumber\(", "placa del nodo"),
    ("Cura", "cura/Settings/SettingOverrideDecorator.py", r"def isNonPrintingMesh\(", "mallas modificadoras"),
    ("Cura", "cura/BuildVolume.py", r"_outside_buildarea", "nodo fuera del area de impresion (mismo filtro que StartSliceJob)"),
    ("Cura", "cura/UI/ObjectsModel.py", r"_renameNodes", "Cura renombra las copias: nombre(n)"),
    ("Cura", "plugins/PostProcessingPlugin/Script.py", r"def getSettingDataString\(", "script de post-procesado"),
    ("Cura", "plugins/PostProcessingPlugin/Script.py", r"def execute\(", "script de post-procesado"),
    ("Cura", "plugins/PostProcessingPlugin/Script.py", r"def getSettingValueByKey\(", "ajustes del script"),
]
cache = {}


def fetch(repo, path):
    k = (repo, path)
    if k not in cache:
        try:
            cache[k] = urllib.request.urlopen(BASE[repo] + path, timeout=30).read().decode("utf-8", "replace")
        except Exception as e:                                              # noqa: BLE001
            cache[k] = None
            print("  (no se pudo descargar %s/%s: %s)" % (repo, path, e))
    return cache[k]


def main():
    bad = 0
    for repo, path, pat, why in CHECKS:
        t = fetch(repo, path)
        ok = t is not None and re.search(pat, t) is not None
        bad += 0 if ok else 1
        print("%-5s %s:%s  %s" % ("OK" if ok else "FALTA", repo, path, why))
    print("\n%d simbolos comprobados en %s: %d no encontrados." % (len(CHECKS), TAG, bad))
    print("Esto es una lectura del codigo fuente: NO valida el comportamiento del plugin dentro de Cura.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
