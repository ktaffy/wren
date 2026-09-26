import pathlib
import unittest

from wrengen.parser import parse, ParseError
from wrengen.ast import (
    Schema, Struct, Field, Service, Method, Arg, TupleReturn,
    PrimitiveType, StructType, ArrayType,
)

class TestParser(unittest.TestCase):

    def test_empty_schema(self):
        schema = parse("")
        self.assertEqual(schema.structs, ())
        self.assertEqual(schema.services, ())

    def test_empty_struct(self):
        schema = parse("struct Empty {}")
        self.assertEqual(len(schema.structs), 1)
        self.assertEqual(schema.structs[0].name, "Empty")
        self.assertEqual(schema.structs[0].fields, ())

    def test_simple_struct(self):
        schema = parse("struct Point { f64 x; f64 y; }")
        s = schema.structs[0]
        self.assertEqual(s.name, "Point")
        self.assertEqual(len(s.fields), 2)
        self.assertEqual(s.fields[0], Field("x", PrimitiveType("f64")))
        self.assertEqual(s.fields[1], Field("y", PrimitiveType("f64")))

    def test_struct_with_array_field(self):
        schema = parse("struct Poly { Point[] vertices; f32[16] matrix; }")
        s = schema.structs[0]
        self.assertEqual(s.fields[0].type, ArrayType(StructType("Point"), None))
        self.assertEqual(s.fields[1].type, ArrayType(PrimitiveType("f32"), 16))

    def test_simple_service(self):
        schema = parse("""
            service Calc {
                add(u32 a, u32 b) -> u32;
            }
        """)
        svc = schema.services[0]
        self.assertEqual(svc.name, "Calc")
        self.assertEqual(len(svc.methods), 1)
        m = svc.methods[0]
        self.assertEqual(m.name, "add")
        self.assertEqual(m.method_id, 1)
        self.assertEqual(m.args, (Arg("a", PrimitiveType("u32")),
                                  Arg("b", PrimitiveType("u32"))))
        self.assertEqual(m.returns, PrimitiveType("u32"))

    def test_method_ids_assigned_in_order(self):
        schema = parse("""
            service S {
                first();
                second();
                third();
            }
        """)
        ids = [m.method_id for m in schema.services[0].methods]
        self.assertEqual(ids, [1, 2, 3])

    def test_method_no_return(self):
        schema = parse("service S { log(string msg); }")
        m = schema.services[0].methods[0]
        self.assertIsNone(m.returns)

    def test_method_no_args(self):
        schema = parse("service S { noop(); }")
        m = schema.services[0].methods[0]
        self.assertEqual(m.args, ())

    def test_tuple_return(self):
        schema = parse("service S { divmod(u32 a, u32 b) -> (u32 q, u32 r); }")
        m = schema.services[0].methods[0]
        self.assertIsInstance(m.returns, TupleReturn)
        self.assertEqual(m.returns.fields,
                         (Arg("q", PrimitiveType("u32")),
                          Arg("r", PrimitiveType("u32"))))

    def test_array_return(self):
        schema = parse("service S { rows() -> Row[]; }")
        m = schema.services[0].methods[0]
        self.assertEqual(m.returns, ArrayType(StructType("Row"), None))

    def test_fixture(self):
        fixture = pathlib.Path(__file__).parent / "fixtures" / "calc.wren"
        schema = parse(fixture.read_text())
        self.assertEqual(len(schema.structs), 2)
        self.assertEqual(schema.structs[0].name, "Point")
        self.assertEqual(schema.structs[1].name, "Row")
        self.assertEqual(len(schema.services), 1)
        svc = schema.services[0]
        self.assertEqual(svc.name, "Calc")
        method_names = [m.name for m in svc.methods]
        self.assertEqual(method_names, [
            "add", "multiply", "distance_between", "sum_array",
            "divmod", "list_admins", "log_message",
        ])
        # Verify method IDs
        self.assertEqual([m.method_id for m in svc.methods],
                         [1, 2, 3, 4, 5, 6, 7])

    def test_error_missing_semicolon(self):
        with self.assertRaises(ParseError):
            parse("struct Point { f64 x f64 y; }")

    def test_error_missing_brace(self):
        with self.assertRaises(ParseError):
            parse("struct Point { f64 x;")

    def test_error_unknown_top_level(self):
        with self.assertRaises(ParseError):
            parse("enum Color { red, green }")

def pos(node):
    return (node.line, node.col)

class TestPositions(unittest.TestCase):
    SRC = (
        "struct Point {\n"
        "    f64 x;\n"
        "}\n"
        "service Calc {\n"
        "    dist(Point a) -> f64;\n"
        "}\n"
    )

    def setUp(self):
        self.schema = parse(self.SRC)
        self.struct = self.schema.structs[0]
        self.service = self.schema.services[0]
        self.method = self.service.methods[0]

    def test_struct_position_is_keyword(self):
        self.assertEqual(pos(self.struct), (1, 1))

    def test_field_and_its_type(self):
        f = self.struct.fields[0]
        self.assertEqual(pos(f), (2, 5))
        self.assertEqual(pos(f.type), (2, 5))

    def test_service_position_is_keyword(self):
        self.assertEqual(pos(self.service), (4, 1))

    def test_method_position_is_name(self):
        self.assertEqual(pos(self.method), (5, 5))

    def test_arg_and_struct_reference(self):
        a = self.method.args[0]
        self.assertEqual(pos(a), (5, 10))
        self.assertEqual(pos(a.type), (5, 10))

    def test_return_type(self):
        self.assertEqual(pos(self.method.returns), (5, 22))

    def test_positions_ignored_by_equality_and_hash(self):
        a = PrimitiveType("u32", line=1, col=1)
        b = PrimitiveType("u32", line=9, col=9)
        self.assertEqual(a, b)
        self.assertEqual(hash(a), hash(b))
        self.assertEqual(self.struct.fields[0],
                         Field("x", PrimitiveType("f64")))

if __name__ == "__main__":
    unittest.main()