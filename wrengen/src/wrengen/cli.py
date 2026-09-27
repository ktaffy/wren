from __future__ import annotations

import argparse
import pathlib
import sys

from .emit_c import emit_c
from .emit_py import emit_python
from .lexer import LexError
from .parser import ParseError, parse
from .validate import validate

def _error(path, line: int, col: int, message: str) -> None:
    print(f"{path}:{line}:{col}: error: {message}", file=sys.stderr)

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="wrengen",
        description="Generate wren client and server code from a .wren schema")
    ap.add_argument("schema", type=pathlib.Path, help="path to a .wren schema")
    ap.add_argument("--lang", required=True, choices=["py", "c"], help="target language")
    ap.add_argument("--out", type=pathlib.Path, default=pathlib.Path("."), help="output directory (default: current directory)")
    args = ap.parse_args(argv)

    try:
        text = args.schema.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as e:
        print(f"wrengen: cannot read {args.schema}: {e}", file=sys.stderr)
        return 1

    try:
        schema = parse(text)
    except LexError as e:
        _error(args.schema, e.line, e.col, e.message)
        return 1
    except ParseError as e:
        _error(args.schema, e.token.line, e.token.col, e.message)
        return 1

    diagnostics = validate(schema)
    for d in diagnostics:
        _error(args.schema, d.line, d.col, d.message)
    if diagnostics:
        return 1

    # everything in mem first so failure doesnt create partial files
    name = args.schema.stem
    if args.lang == "py":
        outputs = {f"{name}.py": emit_python(schema)}
    else:
        header, source = emit_c(schema, name)
        outputs = {f"{name}.h": header, f"{name}.c": source}

    try:
        args.out.mkdir(parents=True, exist_ok=True)
        for filename, content in outputs.items():
            (args.out / filename).write_text(content, encoding="utf-8")
    except OSError as e:
        print(f"wrengen: cannot write output: {e}", file=sys.stderr)
        return 1
    return 0
