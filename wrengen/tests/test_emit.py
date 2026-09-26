import pathlib
import struct
import unittest
import sys
import types

from wren.codec import WrenDecodeError
from wrengen.emit_py import emit_python
from wrengen.parser import parse

FIXTURE = pathlib.Path(__file__).parent / "fixtures" / "calc.wren"

def load(src):
    code = emit_python(parse(src))
    module = types.ModuleType("wrengen_generated")
    sys.modules[module.__name__] = module
    try:
        exec(compile(code, "<generated>", "exec"), module.__dict__)
    finally:
        del sys.modules[module.__name__]
    return module.__dict__

class TestEmitStructs(unittest.TestCase):
    def test_fixture_generates_runnable_code(self):
        ns = load(FIXTURE.read_text())
        self.assertIn("Point", ns)
        self.assertIn("Row", ns)

    def test_point_matches_protocol_layout(self):
        Point = load(FIXTURE.read_text())["Point"]
        self.assertEqual(Point(1.5, 2.5).encode(), struct.pack("!dd", 1.5, 2.5))

    def test_row_wire_bytes_and_round_trip(self):
        Row = load(FIXTURE.read_text())["Row"]
        row = Row(1, "alice", "admin")
        data = row.encode()
        self.assertEqual(data,
                         b"\x00\x00\x00\x01"
                         b"\x00\x00\x00\x05alice"
                         b"\x00\x00\x00\x05admin")
        self.assertEqual(Row.decode(data), row)

    def test_array_prefixes(self):
        Arr = load("struct Arr { u32[] v; u8[2] f; }")["Arr"]
        self.assertEqual(Arr([7], [1, 2]).encode(),
                         b"\x00\x00\x00\x01" b"\x00\x00\x00\x07" b"\x01\x02")

    def test_nested_types_round_trip(self):
        ns = load("""
            struct Point { f64 x; f64 y; }
            struct Poly {
                Point[] vertices;
                f32[4] color;
                u32[][] grid;
                string[] tags;
                bytes blob;
                bool closed;
            }
        """)
        Point, Poly = ns["Point"], ns["Poly"]
        poly = Poly([Point(0.0, 0.0), Point(1.5, 2.0)],
                    [1.0, 0.5, 0.25, 0.0],
                    [[1, 2], [], [3]],
                    ["a", "é"],
                    b"\x00\xff",
                    True)
        self.assertEqual(Poly.decode(poly.encode()), poly)

    def test_fixed_array_wrong_length_raises(self):
        Arr = load("struct Arr { f32[4] color; }")["Arr"]
        with self.assertRaises(ValueError):
            Arr([1.0, 2.0, 3.0]).encode()

    def test_empty_struct(self):
        Empty = load("struct Empty {}")["Empty"]
        self.assertEqual(Empty().encode(), b"")
        self.assertEqual(Empty.decode(b""), Empty())

    def test_decode_rejects_trailing_bytes(self):
        Point = load(FIXTURE.read_text())["Point"]
        with self.assertRaises(WrenDecodeError):
            Point.decode(b"\x00" * 17)


if __name__ == "__main__":
    unittest.main()