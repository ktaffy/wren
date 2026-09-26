from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Iterable, Optional, Sequence, Iterator

from .ast import Schema, Struct, StructType, ArrayType, TupleReturn, Type

@dataclass(frozen=True)
class Diagnostic:
    message: str
    line: int
    col: int

    def __str__(self) -> str:
        return f"line {self.line}:{self.col}: {self.message}"

def _struct_refs(t: Type | TupleReturn | None) -> Iterator[StructType]:
    if isinstance(t, StructType):
        yield t
    elif isinstance(t, ArrayType):
        yield from _struct_refs(t.element)
    elif isinstance(t, TupleReturn):
        for arg in t.fields:
            yield from _struct_refs(arg.type)
    # primitive and none have no struct references

def check_struct_refs(schema: Schema) -> Iterator[Diagnostic]:
    first_decl: dict[str, Struct] = {}
    for s in schema.structs:
        first_decl.setdefault(s.name, s)

    decls = sorted([*schema.structs, *schema.services],
                   key=lambda d: (d.line, d.col))

    declared: set[str] = set()
    for decl in decls:
        if isinstance(decl, Struct):
            types = [f.type for f in decl.fields]
            inside = decl.name
        else:
            types = []
            for m in decl.methods:
                types.extend(a.type for a in m.args)
                types.append(m.returns)
            inside = None

        for t in types:
            for ref in _struct_refs(t):
                if ref.name in declared:
                    continue
                if ref.name == inside:
                    msg = f"struct '{ref.name}' cannot refer to itself"
                elif ref.name in first_decl:
                    later = first_decl[ref.name].line
                    msg = f"type '{ref.name}' used before its declaration on line {later}"
                else:
                    msg = f"unknown type '{ref.name}'"
                yield Diagnostic(msg, ref.line, ref.col)

        if isinstance(decl, Struct):
            declared.add(decl.name)

Rule = Callable[[Schema], Iterable[Diagnostic]]

RULES: list[Rule] = [
    check_struct_refs,
]

def validate(schema: Schema, rules: Optional[Sequence[Rule]] = None) -> list[Diagnostic]:
    if rules is None:
        rules = RULES

    diagnostics: list[Diagnostic] = []
    for rule in rules:
        diagnostics.extend(rule(schema))
    diagnostics.sort(key=lambda d: (d.line, d.col))
    return diagnostics