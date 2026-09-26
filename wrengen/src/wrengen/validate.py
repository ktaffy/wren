from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Iterable, Optional, Sequence

from .ast import Schema

@dataclass(frozen=True)
class Diagnostic:
    message: str
    line: int
    col: int

    def __str__(self) -> str:
        return f"line {self.line}:{self.col}: {self.message}"

Rule = Callable[[Schema], Iterable[Diagnostic]]

# will add validation rules later
RULES: list[Rule] = []

def validate(schema: Schema, rules: Optional[Sequence[Rule]] = None) -> list[Diagnostic]:
    if rules is None:
        rules = RULES

    diagnostics: list[Diagnostic] = []
    for rule in rules:
        diagnostics.extend(rule(schema))
    diagnostics.sort(key=lambda d: (d.line, d.col))
    return diagnostics