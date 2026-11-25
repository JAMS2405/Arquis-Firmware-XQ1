# SPDX-License-Identifier: LGPL-3.0-or-later
import os
import sys

CURA = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # .../cura
if CURA not in sys.path:
    sys.path.insert(0, CURA)
