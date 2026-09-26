from __future__ import annotations

from dataclasses import dataclass

@dataclass(frozen=True)
class Primitive:
    name: str
    size: int | None
    c_type: str
    py_type: str
    py_fmt: str | None

_TABLE = (
    Primitive("bool",    1,    "bool",         "bool",  "?"),
    Primitive("u8",      1,    "uint8_t",      "int",   "B"),
    Primitive("u16",     2,    "uint16_t",     "int",   "H"),
    Primitive("u32",     4,    "uint32_t",     "int",   "I"),
    Primitive("u64",     8,    "uint64_t",     "int",   "Q"),
    Primitive("i8",      1,    "int8_t",       "int",   "b"),
    Primitive("i16",     2,    "int16_t",      "int",   "h"),
    Primitive("i32",     4,    "int32_t",      "int",   "i"),
    Primitive("i64",     8,    "int64_t",      "int",   "q"),
    Primitive("f32",     4,    "float",        "float", "f"),
    Primitive("f64",     8,    "double",       "float", "d"),
    Primitive("bytes",   None, "wren_bytes_t", "bytes", None),
    Primitive("string",  None, "wren_bytes_t", "str",   None),
)

PRIMITIVES: dict[str, Primitive] = {p.name: p for p in _TABLE}