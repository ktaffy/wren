# wrengen

Code generator for the wren RPC framework. Reads `.wren` schema files and
emits per-language client/server stubs.

## Setup

From this directory:

    python3 -m venv .venv
    source .venv/bin/activate
    pip install -e .

## Usage

    wrengen --lang=python-client calc.wren -o calc_client.py

## Schema language

See `../SCHEMA.md` for the full specification.