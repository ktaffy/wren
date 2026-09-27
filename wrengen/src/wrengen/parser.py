from __future__ import annotations

from .ast import (
    Schema, Struct, Field, Service, Method, Arg, TupleReturn,
    PrimitiveType, StructType, ArrayType, Type,
)
from .lexer import Token, TokenKind
from .typemap import PRIMITIVES

class ParseError(Exception):
    def __init__(self, message: str, token: Token):
        self.message = f"{message} (got {token.kind.name} {token.value!r})"
        super().__init__(f"line {token.line}:{token.col}: {self.message}")
        self.token = token

class Parser:
    def __init__(self, tokens: list[Token]):
        self.tokens = tokens
        self.pos = 0

    def peek(self) -> Token:
        return self.tokens[self.pos]

    def advance(self) -> Token:
        tok = self.tokens[self.pos]
        self.pos += 1
        return tok

    def match(self, kind: TokenKind) -> bool:
        if self.peek().kind == kind:
            self.advance()
            return True
        return False

    def expect(self, kind: TokenKind) -> Token:
        tok = self.peek()
        if tok.kind != kind:
            raise ParseError(f"expected {kind.name}", tok)
        return self.advance()

    def parse_schema(self) -> Schema:
        structs: list[Struct] = []
        services: list[Service] = []

        while self.peek().kind != TokenKind.EOF:
            tok = self.peek()
            if tok.kind == TokenKind.STRUCT:
                structs.append(self.parse_struct())
            elif tok.kind == TokenKind.SERVICE:
                services.append(self.parse_service())
            else:
                raise ParseError("expected 'struct' or 'service'", tok)

        return Schema(structs=tuple(structs), services=tuple(services))

    def parse_struct(self) -> Struct:
        kw = self.expect(TokenKind.STRUCT)
        name_tok = self.expect(TokenKind.IDENT)
        self.expect(TokenKind.LBRACE)

        fields: list[Field] = []
        while self.peek().kind != TokenKind.RBRACE:
            fields.append(self.parse_field())

        self.expect(TokenKind.RBRACE)
        return Struct(name=name_tok.value, fields=tuple(fields),
                      line=kw.line, col=kw.col)

    def parse_field(self) -> Field:
        start = self.peek()
        ftype = self.parse_type()
        name_tok = self.expect(TokenKind.IDENT)
        self.expect(TokenKind.SEMI)
        return Field(name=name_tok.value, type=ftype,
                     line=start.line, col=start.col)

    def parse_service(self) -> Service:
        kw = self.expect(TokenKind.SERVICE)
        name_tok = self.expect(TokenKind.IDENT)
        self.expect(TokenKind.LBRACE)

        methods: list[Method] = []
        next_id = 1
        while self.peek().kind != TokenKind.RBRACE:
            methods.append(self.parse_method(next_id))
            next_id += 1

        self.expect(TokenKind.RBRACE)
        return Service(name=name_tok.value, methods=tuple(methods),
                       line=kw.line, col=kw.col)

    def parse_method(self, method_id: int) -> Method:
        name_tok = self.expect(TokenKind.IDENT)
        self.expect(TokenKind.LPAREN)

        args: list[Arg] = []
        if self.peek().kind != TokenKind.RPAREN:
            args.append(self.parse_arg())
            while self.match(TokenKind.COMMA):
                args.append(self.parse_arg())
        self.expect(TokenKind.RPAREN)

        returns: Type | TupleReturn | None = None
        if self.match(TokenKind.ARROW):
            returns = self.parse_return_type()

        self.expect(TokenKind.SEMI)
        return Method(
            name=name_tok.value,
            args=tuple(args),
            returns=returns,
            method_id=method_id,
            line=name_tok.line,
            col=name_tok.col
        )

    def parse_arg(self) -> Arg:
        start = self.peek()
        atype = self.parse_type()
        name_tok = self.expect(TokenKind.IDENT)
        return Arg(name=name_tok.value, type=atype,
                   line=start.line, col=start.col)

    def parse_return_type(self) -> Type | TupleReturn:
        if self.peek().kind == TokenKind.LPAREN:
            self.advance()  # consume '('
            fields: list[Arg] = []
            fields.append(self.parse_arg())
            while self.match(TokenKind.COMMA):
                fields.append(self.parse_arg())
            self.expect(TokenKind.RPAREN)
            return TupleReturn(fields=tuple(fields))
        return self.parse_type()

    def parse_type(self) -> Type:
        tok = self.expect(TokenKind.IDENT)
        if tok.value in PRIMITIVES:
            base: Type = PrimitiveType(name=tok.value, line=tok.line, col=tok.col)
        else:
            base = StructType(name=tok.value, line=tok.line, col=tok.col)

        while self.peek().kind == TokenKind.LBRACK:
            self.advance()  # '['
            size: int | None = None
            if self.peek().kind == TokenKind.NUMBER:
                size_tok = self.advance()
                size = int(size_tok.value)
            self.expect(TokenKind.RBRACK)
            base = ArrayType(element=base, size=size)

        return base


def parse(source: str) -> Schema:
    from .lexer import tokenize
    tokens = tokenize(source)
    return Parser(tokens).parse_schema()