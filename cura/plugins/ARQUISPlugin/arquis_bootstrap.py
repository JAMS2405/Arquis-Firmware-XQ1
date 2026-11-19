# SPDX-License-Identifier: LGPL-3.0-or-later
"""Carga los paquetes internos con nombres unicos (``arquis_engines``, ``arquis_adapters``) tanto desde el plugin de Cura como
desde un script de post-procesado o desde las pruebas. Asi todos usan exactamente el mismo codigo y ninguna ruta relativa depende
de como cargue Cura el plugin."""
import importlib.util
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))


def _load(pkg_name: str, folder: str):
    if pkg_name in sys.modules:
        return sys.modules[pkg_name]
    pkg_dir = os.path.join(_HERE, folder)
    spec = importlib.util.spec_from_file_location(pkg_name, os.path.join(pkg_dir, "__init__.py"), submodule_search_locations=[pkg_dir])
    mod = importlib.util.module_from_spec(spec)
    sys.modules[pkg_name] = mod
    spec.loader.exec_module(mod)
    return mod


def load_engines():
    return _load("arquis_engines", "engines")


def load_adapters():
    load_engines()
    return _load("arquis_adapters", "adapters")
