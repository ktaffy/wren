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


if __name__ == "__main__":
    unittest.main()