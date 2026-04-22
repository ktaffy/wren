from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto

class TokenKind(Enum):
    IDENT   = auto()
    NUMBER  = auto()

    STRUCT  = auto()
    SERVICE = auto()

    LBRACE  = auto()
    RBRACE  = auto()
    LPAREN  = auto()
    RPAREN  = auto()
    LBRACK  = auto()
    RBRACK  = auto()
    COMMA   = auto()
    SEMI    = auto()
    ARROW   = auto()

    EOF     = auto()

@dataclass(frozen=True)
class Token:
    kind: TokenKind
    value: str
    line: int
    col: int

KEYWORDS = {
    "struct": TokenKind.STRUCT,
    "service": TokenKind.SERVICE
}

class LexError(Exception):
    def __init__(self, message: str, line: int, col: int):
        super().__init__(f"line {line}:{col}: {message}")
        self.line = line
        self.col = col

def tokenize(source: str) -> list[Token]:
    tokens: list[Token] = []
    i = 0
    line = 1
    col = 1
    n = len(source)

    def advance(count: int = 1) -> None:
        nonlocal i, col
        i += count
        col += count

    while i < n:
        ch = source[i]

        if ch == "\n":
            i += 1
            line += 1
            col = 1
            continue

        if ch in " \t\r":
            advance()
            continue

        if ch == "/" and i + 1 < n and source[i + 1] == "/":
            while i < n and source[i] != "\n":
                i += 1
            continue

        if ch == "-" and i + 1 < n and source[i + 1] == ">":
            tokens.append(Token(TokenKind.ARROW, "->", line, col))
            advance(2)
            continue

        punct = {
            "{": TokenKind.LBRACE,
            "}": TokenKind.RBRACE,
            "(": TokenKind.LPAREN,
            ")": TokenKind.RPAREN,
            "[": TokenKind.LBRACK,
            "]": TokenKind.RBRACK,
            ",": TokenKind.COMMA,
            ";": TokenKind.SEMI,
        }
        if ch in punct:
            tokens.append(Token(punct[ch], ch, line, col))
            advance()
            continue

        if ch.isdigit():
            start = i
            start_col = col
            while i < n and source[i].isdigit():
                advance()
            tokens.append(Token(TokenKind.NUMBER, source[start:i], line, start_col))
            continue

        if ch.isalpha() or ch == "_":
            start = i
            start_col = col
            while i < n and (source[i].isalnum() or source[i] == "_"):
                advance()
            text = source[start:i]
            kind = KEYWORDS.get(text, TokenKind.IDENT)
            tokens.append(Token(kind, text, line, start_col))
            continue

        raise LexError(f"unexpected character {ch!r}", line, col)
    
    tokens.append(Token(TokenKind.EOF, "", line, col))
    return tokens