from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Iterable, Optional, Sequence, Iterator

from .ast import Schema, Struct, StructType, ArrayType, TupleReturn, Type

MAX_METHODS_PER_SERVICE = 65535

# hardcoded keywords for now and not read from keyword module.
# schema shouldnt depend on which python version is running.
# will find a better apprach later, have to move on for now
PYTHON_KEYWORDS = frozenset({
    "False", "None", "True", "and", "as", "assert", "async", "await",
    "break", "class", "continue", "def", "del", "elif", "else", "except",
    "finally", "for", "from", "global", "if", "import", "in", "is",
    "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
    "while", "with", "yield",
})

C_KEYWORDS = frozenset({
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "restrict", "return", "short",
    "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
    "unsigned", "void", "volatile", "while", "bool", "true", "false",
})

GENERATED_MEMBERS = {
    "field": frozenset({"encode", "decode"}),
    "method": frozenset({"close"}),
}

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

def _duplicates(entries) -> Iterator[Diagnostic]:
    first: dict[str, tuple[str, object]] = {}
    for name, what, node in entries:
        if name not in first:
            first[name] = (what, node)
            continue
        first_what, first_node = first[name]
        if what == first_what:
            msg = f"duplicate {what} '{name}' (first declared on line {first_node.line})"
        else:
            msg = f"{what} '{name}' conflicts with {first_what} on line {first_node.line}"
        yield Diagnostic(msg, node.line, node.col)

def check_dup_names(schema:Schema) -> Iterator[Diagnostic]:
    top = [(s.name, "struct", s) for s in schema.structs]
    top += [(v.name, "service", v) for v in schema.services]
    top.sort(key=lambda e: (e[2].line, e[2].col))
    yield from _duplicates(top)

    for s in schema.structs:
        yield from _duplicates((f.name, "field", f) for f in s.fields)

    for svc in schema.services:
        yield from _duplicates((m.name, "method", m) for m in svc.methods)
        for m in svc.methods:
            yield from _duplicates((a.name, "argument", a) for a in m.args)
            if isinstance(m.returns, TupleReturn):
                yield from _duplicates(
                    (a.name, "return value", a) for a in m.returns.fields)

def check_single_service(schema: Schema) -> Iterator[Diagnostic]:
    services = schema.services
    for extra in services[1:]:
        yield Diagnostic(
            f"only one service per schema is allowed "
            f"(first declared on line {services[0].line})",
            extra.line, extra.col)

def check_method_count(schema: Schema) -> Iterator[Diagnostic]:
    for svc in schema.services:
        if len(svc.methods) > MAX_METHODS_PER_SERVICE:
            first_over = svc.methods[MAX_METHODS_PER_SERVICE]
            yield Diagnostic(
                f"service '{svc.name}' declares {len(svc.methods)} methods; "
                f"at most {MAX_METHODS_PER_SERVICE} allowed",
                first_over.line, first_over.col)

def _declared_names(schema: Schema):
    for s in schema.structs:
        yield "struct", s
        for f in s.fields:
            yield "field", f
    for svc in schema.services:
        yield "service", svc
        for m in svc.methods:
            yield "method", m
            for a in m.args:
                yield "argument", a
            if isinstance(m.returns, TupleReturn):
                for a in m.returns.fields:
                    yield "return value", a

def check_reserved_names(schema:Schema) -> Iterator[Diagnostic]:
    for what, node in _declared_names(schema):
        name = node.name
        if name.startswith("_"):
            reason = "names starting with '_' are reserved for generated code"
        elif name in PYTHON_KEYWORDS:
            reason = "it is a Python keyword"
        elif name in C_KEYWORDS:
            reason = "it is a C keyword"
        elif name in GENERATED_MEMBERS.get(what, ()):
            reason = "generated code defines a member with this name"
        else:
            continue
        yield Diagnostic(f"{what} name '{name}' is reserved: {reason}",
                         node.line, node.col)

Rule = Callable[[Schema], Iterable[Diagnostic]]

RULES: list[Rule] = [
    check_struct_refs,
    check_dup_names,
    check_single_service,
    check_method_count,
    check_reserved_names,
]

def validate(schema: Schema, rules: Optional[Sequence[Rule]] = None) -> list[Diagnostic]:
    if rules is None:
        rules = RULES

    diagnostics: list[Diagnostic] = []
    for rule in rules:
        diagnostics.extend(rule(schema))
    diagnostics.sort(key=lambda d: (d.line, d.col))
    return diagnostics