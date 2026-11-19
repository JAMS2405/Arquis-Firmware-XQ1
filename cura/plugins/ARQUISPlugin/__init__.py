# ARQUIS Slicer Suite - plugin de Cura 5.11 (API 8). Licencia: ver LICENSE del repositorio.
from . import ARQUISPlugin


def getMetaData():
    return {}


def register(app):
    return {"extension": ARQUISPlugin.ARQUISPlugin()}
