# wrengen

Code generator for the wren RPC framework. Reads `.wren` schema files and
emits per-language client/server stubs.

## Setup

From this directory:

    python3 -m venv .venv
    source .venv/bin/activate
    pip install -e .

## Usage

    wrengen calc.wren --lang py --out gen/    # writes gen/calc.py
    wrengen calc.wren --lang c  --out gen/    # writes gen/calc.h, gen/calc.c

Errors are reported as `file:line:col: error: message`, and nothing is
written unless the schema is valid.

## Schema language

See `../SCHEMA.md` for the full specification.
