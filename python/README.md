# wren-python

Python client library for the wren RPC protocol.

## Setup

From this directory (`python/`):

    python3 -m venv .venv
    source .venv/bin/activate
    pip install -e .

## Usage

    from wren import Client
    import struct

    client = Client("localhost", 8080)
    payload = struct.pack("!II", 3, 4)
    response = client.call(method_id=1, payload=payload)
    result = struct.unpack("!I", response)[0]
    print(result)
    client.close()

## Protocol

Implements the wren protocol, specified in `../PROTOCOL.md`.