import unittest

from wrengen.lexer import tokenize, TokenKind, LexError

class TestLexer(unittest.TestCase):
    def test_empty_input(self):
        tokens = tokenize("")
        self.assertEqual(len(tokens), 1)
        self.assertEqual(tokens[0].kind, TokenKind.EOF)

    def test_whitespace_only(self):
        tokens = tokenize("   \n\t\r\n  ")
        self.assertEqual(len(tokens), 1)
        self.assertEqual(tokens[0].kind, TokenKind.EOF)

    def test_single_keyword(self):
        tokens = tokenize("struct")
        self.assertEqual([t.kind for t in tokens],
                         [TokenKind.STRUCT, TokenKind.EOF])

    def test_identifier_vs_keyword(self):
        tokens = tokenize("struct Point u32")
        kinds = [t.kind for t in tokens[:-1]]
        values = [t.value for t in tokens[:-1]]
        self.assertEqual(kinds, [TokenKind.STRUCT, TokenKind.IDENT, TokenKind.IDENT])
        self.assertEqual(values, ["struct", "Point", "u32"])

    def test_punctuation(self):
        tokens = tokenize("{}()[],;->")
        kinds = [t.kind for t in tokens[:-1]]
        self.assertEqual(kinds, [
            TokenKind.LBRACE, TokenKind.RBRACE,
            TokenKind.LPAREN, TokenKind.RPAREN,
            TokenKind.LBRACK, TokenKind.RBRACK,
            TokenKind.COMMA, TokenKind.SEMI,
            TokenKind.ARROW,
        ])

    def test_number(self):
        tokens = tokenize("16 0 999")
        numbers = [t for t in tokens if t.kind == TokenKind.NUMBER]
        self.assertEqual([t.value for t in numbers], ["16", "0", "999"])

    def test_line_comment(self):
        src = "struct // this is a comment\nPoint"
        tokens = tokenize(src)
        kinds = [t.kind for t in tokens[:-1]]
        self.assertEqual(kinds, [TokenKind.STRUCT, TokenKind.IDENT])

    def test_line_col_tracking(self):
        src = "struct\n    Point"
        tokens = tokenize(src)
        struct_tok, point_tok = tokens[0], tokens[1]
        self.assertEqual((struct_tok.line, struct_tok.col), (1, 1))
        self.assertEqual((point_tok.line, point_tok.col), (2, 5))

    def test_full_fixture(self):
        import pathlib
        fixture = pathlib.Path(__file__).parent / "fixtures" / "calc.wren"
        source = fixture.read_text()
        tokens = tokenize(source)
        self.assertEqual(tokens[-1].kind, TokenKind.EOF)
        kinds = {t.kind for t in tokens}
        self.assertIn(TokenKind.STRUCT, kinds)
        self.assertIn(TokenKind.SERVICE, kinds)

    def test_unknown_char(self):
        with self.assertRaises(LexError):
            tokenize("struct @ Point")

if __name__ == "__main__":
    unittest.main()