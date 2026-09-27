import contextlib
import io
import pathlib
import tempfile
import unittest

from wrengen.cli import main
from wrengen.emit_c import emit_c
from wrengen.parser import parse

FIXTURES = pathlib.Path(__file__).parent / "fixtures"
CALC = FIXTURES / "calc.wren"
GOLDEN = FIXTURES / "calc.py.golden"

class TestCli(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = pathlib.Path(tmp.name)

    def run_cli(self, *argv):
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            code = main([str(a) for a in argv])
        return code, err.getvalue()

    def schema(self, text):
        path = self.dir / "s.wren"
        path.write_text(text)
        return path

    def test_python_output_matches_golden(self):
        out = self.dir / "out"
        self.assertEqual(self.run_cli(CALC, "--lang", "py", "--out", out), (0, ""))
        self.assertEqual([p.name for p in out.iterdir()], ["calc.py"])
        self.assertEqual((out / "calc.py").read_text(), GOLDEN.read_text())

    def test_c_output(self):
        self.assertEqual(self.run_cli(CALC, "--lang", "c", "--out", self.dir), (0, ""))
        header, source = emit_c(parse(CALC.read_text()), "calc")
        self.assertEqual((self.dir / "calc.h").read_text(), header)
        self.assertEqual((self.dir / "calc.c").read_text(), source)

    def test_lex_error(self):
        path = self.schema("struct @")
        self.assertEqual(self.run_cli(path, "--lang", "py", "--out", self.dir),
                         (1, f"{path}:1:8: error: unexpected character '@'\n"))

    def test_parse_error(self):
        path = self.schema("struct P { f64 x }")
        self.assertEqual(self.run_cli(path, "--lang", "py", "--out", self.dir),
                         (1, f"{path}:1:18: error: expected SEMI (got RBRACE '}}')\n"))

    def test_all_validation_errors_reported_and_nothing_written(self):
        path = self.schema("struct A { Pointt p; }\nstruct A {}\n")
        out = self.dir / "out"
        code, err = self.run_cli(path, "--lang", "c", "--out", out)
        self.assertEqual(code, 1)
        self.assertEqual(err.splitlines(), [
            f"{path}:1:12: error: unknown type 'Pointt'",
            f"{path}:2:1: error: duplicate struct 'A' (first declared on line 1)",
        ])
        self.assertFalse(out.exists())

    def test_unreadable_schema(self):
        code, err = self.run_cli(self.dir / "missing.wren", "--lang", "py")
        self.assertEqual(code, 1)
        self.assertIn("cannot read", err)

    def test_unknown_language_is_usage_error(self):
        with contextlib.redirect_stderr(io.StringIO()), \
                self.assertRaises(SystemExit) as cm:
            main([str(CALC), "--lang", "rust"])
        self.assertEqual(cm.exception.code, 2)