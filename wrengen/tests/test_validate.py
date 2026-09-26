import pathlib
import unittest

import keyword
from wrengen.ast import Method, Schema, Service
from wrengen.parser import parse
from wrengen.validate import Diagnostic, validate, PYTHON_KEYWORDS

FIXTURE = pathlib.Path(__file__).parent / "fixtures" / "calc.wren"

class TestDiagnostic(unittest.TestCase):

    def test_str_format_matches_parse_errors(self):
        d = Diagnostic("unknown type 'Pointt'", 3, 5)
        self.assertEqual(str(d), "line 3:5: unknown type 'Pointt'")

class TestValidate(unittest.TestCase):

    def test_fixture_is_valid(self):
        self.assertEqual(validate(parse(FIXTURE.read_text())), [])

    def test_empty_schema_is_valid(self):
        self.assertEqual(validate(parse("")), [])

    def test_collects_from_all_rules_sorted_by_position(self):
        def rule_a(schema):  # generator-style rule
            yield Diagnostic("a-late", 9, 1)
            yield Diagnostic("a-early", 2, 7)

        def rule_b(schema):  # list-style rule
            return [Diagnostic("b-mid", 5, 3),
                    Diagnostic("b-same-line", 2, 3)]

        result = validate(parse(""), rules=[rule_a, rule_b])
        self.assertEqual(
            [d.message for d in result],
            ["b-same-line", "a-early", "b-mid", "a-late"],
        )

    def test_explicit_empty_rules_runs_nothing(self):
        self.assertEqual(validate(parse(""), rules=[]), [])

def diags(src):
    return [str(d) for d in validate(parse(src))]

class TestStructRefs(unittest.TestCase):

    def test_declared_before_use_is_valid(self):
        src = ("struct Point { f64 x; }\n"
               "struct Line { Point a; Point b; }\n"
               "service S { f(Line l) -> Point; }\n")
        self.assertEqual(diags(src), [])

    def test_unknown_type_in_field(self):
        self.assertEqual(diags("struct A { Pointt p; }"),
                         ["line 1:12: unknown type 'Pointt'"])

    def test_used_before_declaration(self):
        src = ("struct Line { Point a; }\n"
               "struct Point { f64 x; }\n")
        self.assertEqual(
            diags(src),
            ["line 1:15: type 'Point' used before its declaration on line 2"])

    def test_direct_recursion(self):
        self.assertEqual(diags("struct Node { Node next; }"),
                         ["line 1:15: struct 'Node' cannot refer to itself"])

    def test_direct_recursion_through_array(self):
        self.assertEqual(diags("struct Node { Node[] kids; }"),
                         ["line 1:15: struct 'Node' cannot refer to itself"])

    def test_indirect_recursion(self):
        src = ("struct A { B b; }\n"
               "struct B { A a; }\n")
        self.assertEqual(
            diags(src),
            ["line 1:12: type 'B' used before its declaration on line 2"])

    def test_service_unknown_arg_type(self):
        self.assertEqual(diags("service S { f(Pointt p); }"),
                         ["line 1:15: unknown type 'Pointt'"])

    def test_service_before_struct(self):
        src = ("service S { f(Point p); }\n"
               "struct Point { f64 x; }\n")
        self.assertEqual(
            diags(src),
            ["line 1:15: type 'Point' used before its declaration on line 2"])

    def test_nested_return_types_all_reported(self):
        src = "service S { f() -> (Pointt a, u32 b); g() -> Qq[][]; }"
        self.assertEqual(diags(src), [
            "line 1:21: unknown type 'Pointt'",
            "line 1:46: unknown type 'Qq'",
        ])

class TestDupNames(unittest.TestCase):
    def test_same_name_in_different_scopes_is_valid(self):
        src = ("struct Point { f64 x; }\n"
               "struct point { f64 x; }\n"
               "struct Line { Point Point; }\n"
               "service S { f(u32 a); g(u32 a); }\n")
        self.assertEqual(diags(src), [])

    def test_duplicate_struct(self):
        self.assertEqual(
            diags("struct A {}\nstruct A {}"),
            ["line 2:1: duplicate struct 'A' (first declared on line 1)"])

    def test_struct_and_service_share_scope(self):
        self.assertEqual(
            diags("struct Calc {}\nservice Calc {}"),
            ["line 2:1: service 'Calc' conflicts with struct on line 1"])

    def test_duplicate_field(self):
        src = "struct P {\n    f64 x;\n    f64 x;\n}"
        self.assertEqual(
            diags(src),
            ["line 3:5: duplicate field 'x' (first declared on line 2)"])

    def test_duplicate_method(self):
        src = "service S {\n    f();\n    f();\n}"
        self.assertEqual(
            diags(src),
            ["line 3:5: duplicate method 'f' (first declared on line 2)"])

    def test_duplicate_argument(self):
        self.assertEqual(
            diags("service S { f(u32 a, u32 a); }"),
            ["line 1:22: duplicate argument 'a' (first declared on line 1)"])

    def test_duplicate_tuple_return_name(self):
        self.assertEqual(
            diags("service S { f() -> (u32 q, u32 q); }"),
            ["line 1:28: duplicate return value 'q' (first declared on line 1)"])

    def test_every_repeat_is_reported(self):
        src = "struct P {\n    f64 x;\n    f64 x;\n    f64 x;\n}"
        self.assertEqual(diags(src), [
            "line 3:5: duplicate field 'x' (first declared on line 2)",
            "line 4:5: duplicate field 'x' (first declared on line 2)",
        ])

class TestSingleService(unittest.TestCase):
    def test_structs_only_is_valid(self):
        self.assertEqual(diags("struct P { f64 x; }"), [])

    def test_second_service_rejected(self):
        self.assertEqual(
            diags("service A {}\nservice B {}"),
            ["line 2:1: only one service per schema is allowed (first declared on line 1)"])

    def test_every_extra_service_reported(self):
        self.assertEqual(diags("service A {}\nservice B {}\nservice C {}"), [
            "line 2:1: only one service per schema is allowed (first declared on line 1)",
            "line 3:1: only one service per schema is allowed (first declared on line 1)",
        ])

def service_with(n):
    methods = tuple(
        Method(f"m{i}", (), None, method_id=i+1, line=i+2, col=5)
        for i in range(n))
    return Schema(services=(Service("S", methods, line=1, col=1),))

class TestMethodCount(unittest.TestCase):
    def test_max_methods_is_valid(self):
        self.assertEqual(validate(service_with(65535)), [])

    def test_one_over_max_rejected(self):
        self.assertEqual(
            [str(d) for d in validate(service_with(65536))],
            ["line 65537:5: service 'S' declares 65536 methods; at most 65535 allowed"])

class TestReservedNames(unittest.TestCase):
    def test_near_misses_are_valid(self):
        src = ("struct Row { u32 id; string type; string match; u32 close; }\n"
               "service S { encode(); decode(u32 encoded); }\n")
        self.assertEqual(diags(src), [])

    def test_underscore_prefix(self):
        self.assertEqual(
            diags("struct _P {}"),
            ["line 1:1: struct name '_P' is reserved: "
             "names starting with '_' are reserved for generated code"])

    def test_python_keyword(self):
        self.assertEqual(
            diags("struct P { u32 from; }"),
            ["line 1:12: field name 'from' is reserved: it is a Python keyword"])

    def test_c_keyword(self):
        self.assertEqual(
            diags("service S { f(u32 default); }"),
            ["line 1:15: argument name 'default' is reserved: it is a C keyword"])

    def test_generated_field_member(self):
        self.assertEqual(
            diags("struct P { u32 encode; }"),
            ["line 1:12: field name 'encode' is reserved: "
             "generated code defines a member with this name"])

    def test_generated_method_member(self):
        self.assertEqual(
            diags("service S { close(); }"),
            ["line 1:13: method name 'close' is reserved: "
             "generated code defines a member with this name"])

    def test_tuple_return_name(self):
        self.assertEqual(
            diags("service S { f() -> (u32 _q, u32 r); }"),
            ["line 1:21: return value name '_q' is reserved: "
             "names starting with '_' are reserved for generated code"])

    def test_keyword_list_covers_running_python(self):
        missing = set(keyword.kwlist) - PYTHON_KEYWORDS
        self.assertEqual(missing, set(),
                         "new Python keywords; add them to PYTHON_KEYWORDS")

    def test_wren_prefix(self):
        self.assertEqual(
            diags("struct wren_bytes_t {}"),
            ["line 1:1: struct name 'wren_bytes_t' is reserved: "
             "names starting with 'wren_' are reserved for the wren runtime"])

if __name__ == "__main__":
    unittest.main()