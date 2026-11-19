# SPDX-License-Identifier: LGPL-3.0-or-later
"""Hallazgos (errores y avisos) comunes a todos los motores. Cada codigo es estable y esta documentado en docs/slicer_suite."""
from dataclasses import dataclass
from typing import List

ERROR = "error"
WARNING = "warning"


@dataclass(frozen=True)
class Problem:
    code: str
    severity: str
    message: str

    def __str__(self) -> str:
        return "[%s] %s: %s" % (self.severity, self.code, self.message)


def has_errors(problems: List[Problem]) -> bool:
    return any(p.severity == ERROR for p in problems)
