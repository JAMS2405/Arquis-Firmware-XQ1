# SPDX-License-Identifier: LGPL-3.0-or-later
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.join(os.path.dirname(HERE), "plugins", "ARQUISPlugin")
sys.path.insert(0, PLUGIN)
sys.path.insert(0, HERE)

import arquis_bootstrap  # noqa: E402

# Los mismos paquetes, con los mismos nombres, que cargan el plugin y los scripts dentro de Cura
arquis_bootstrap.load_engines()
