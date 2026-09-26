import pathlib
import unittest

from wrengen.parser import parse
from wrengen.validate import Diagnostic, validate

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

if __name__ == "__main__":
    unittest.main()