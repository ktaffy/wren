import struct
import unittest

from wrengen.parser import PRIMITIVES as PARSER_PRIMITIVES
from wrengen.typemap import PRIMITIVES

SPEC_SIZES = {
    "bool": 1,
    "u8": 1, "u16": 2, "u32": 4, "u64": 8,
    "i8": 1, "i16": 2, "i32": 4, "i64": 8,
    "f32": 4, "f64": 8,
    "bytes": None, "string": None,
}

INTEGERS = ["u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64"]

class TestTypeMap(unittest.TestCase):
    def test_covers_exactly_the_parser_primitives(self):
        self.assertEqual(set(PRIMITIVES), PARSER_PRIMITIVES)

    def test_sizes_match_spec(self):
        self.assertEqual({n: p.size for n, p in PRIMITIVES.items()}, SPEC_SIZES)

    def test_fixed_sizes_match_struct_module(self):
        for p in PRIMITIVES.values():
            with self.subTest(p.name):
                if p.size is None:
                    self.assertIsNone(p.py_fmt)
                else:
                    self.assertEqual(struct.calcsize("!" + p.py_fmt), p.size)

    def test_integer_signedness_and_width(self):
        for name in INTEGERS:
            p = PRIMITIVES[name]
            unsigned = name.startswith("u")
            with self.subTest(name):
                self.assertEqual(p.py_fmt.isupper(), unsigned)
                expected_c = f"{'u' if unsigned else ''}int{p.size * 8}_t"
                self.assertEqual(p.c_type, expected_c)