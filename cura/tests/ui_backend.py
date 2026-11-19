# SPDX-License-Identifier: LGPL-3.0-or-later
"""Carga ui/backend.py (necesita PyQt6) con los mismos nombres de paquete que usa el plugin."""
import importlib.util
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("arquis_ui_backend", os.path.join(HERE, "..", "plugins", "ARQUISPlugin", "ui", "backend.py"))
_m = importlib.util.module_from_spec(spec)
sys.modules["arquis_ui_backend"] = _m
spec.loader.exec_module(_m)
ObjectManagerBackend = _m.ObjectManagerBackend
