from __future__ import annotations

from dataclasses import dataclass, field
from typing import Union

# --- Types
@dataclass(frozen=True)
class PrimitiveType:
    name: str

@dataclass(frozen=True)
class StructType:
    name: str

@dataclass(frozen=True)
class ArrayType:
    element: Type
    size: int | None = None

Type = Union[PrimitiveType, StructType, "ArrayType"]

# --- Structs decls.
@dataclass(frozen=True)
class Field:
    name: str
    type: Type

@dataclass(frozen=True)
class Struct:
    name: str
    fields: tuple[Field, ...]

# --- Service and Method decls.
@dataclass(frozen=True)
class Arg:
    name: str
    type: Type

@dataclass(frozen=True)
class TupleReturn:
    fields: tuple[Arg, ...]

@dataclass(frozen=True)
class Method:
    name: str
    args: tuple[Arg, ...]
    returns: Type | TupleReturn | None
    method_id: int

@dataclass(frozen=True)
class Service:
    name: str
    methods: tuple[Method, ...]

# --- Top Level Schema
@dataclass(frozen=True)
class Schema:
    structs: tuple[Struct, ...] = field(default_factory=tuple)
    services: tuple[Service, ...] = field(default_factory=tuple)