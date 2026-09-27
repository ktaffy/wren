# wren protocol spec (v0.2)

This is the wire format for wren, a binary RPC protocol for making fast function calls between programs, whether they're in different processes, on different machines, or written in different languages. If an implementation disagrees with this doc, the implementation is wrong.

## The basic idea

A wren server hosts functions, and each function is identified by a 16 bit method id. A client connects over tcp and calls those functions by sending CALL messages. The server sends back either a return value or an error.

Connections are meant to stay open, so many calls per connection. A server can have many of clients connected at the same time.

The protocol itself only knows about primitive types. Structs and arrays are just agreed upon ways of laying primitives out one after another (see [Composite layouts](#composite-layouts)).

## Transport

wren runs over TCP. Every multi-byte integer on the wire is big-endian (network byte order). Servers can listen on whatever port they want; 8080 is just a common default.

## Framing

Every message starts with a 12-byte header, and the first 4 bytes of that header are the total length of the message, header included. To read a message, you read 4 bytes, decode the length, then read `length - 4` more bytes.

Messages go back to back on the connection with nothing in between them.

## Message header

| Offset | Size | Field     | What it is                                     |
| ------ | ---- | --------- | ---------------------------------------------- |
| 0      | 4    | length    | Total message length in bytes, header included |
| 4      | 1    | type      | Message type (below)                           |
| 5      | 1    | flags     | Reserved, must be 0                            |
| 6      | 2    | method_id | Which method is being called                   |
| 8      | 4    | req_id    | Matches replies up with calls                  |

### Message types

| Value | Name     | Direction       | Purpose                   |
| ----- | -------- | --------------- | ------------------------- |
| 1     | CALL     | client → server | Call a method             |
| 2     | RESPONSE | server → client | The method's return value |
| 3     | ERROR    | server → client | The method failed         |

**flags** is reserved for later. Senders must set it to 0 for now, and receivers must ignore whatever it contains.

**method_id** only means something in a CALL. 0 is reserved and must never be used, and a CALL with method_id 0 should be treated as a protocol error. In RESPONSE and ERROR messages its value doesn't matter, and receivers must ignore it.

**req_id** is picked by the client for every CALL, and the server copies it unchanged into the matching RESPONSE or ERROR. That's how a client with several calls in flight knows which reply goes with which call. Clients should just count up per connection. The only real rule is that a req_id must be unique within its connection.

## Payload

Everything after the 12-byte header is the payload, which is `length - 12` bytes. What it contains depends on the message type:

- **CALL:** the method's arguments, encoded in the order they're declared. Empty if the method takes no arguments.
- **RESPONSE:** the method's return value. Empty if the method doesn't return anything.
- **ERROR:** an error code and message (see [Errors](#errors)).

Beyond the encoding rules below, the protocol doesn't say what each method's arguments are. That's part of the method's interface, which lives outside the protocol (in a schema, a README, or wherever).

## Primitive encodings

Every implementation has to be able to encode and decode these. Everything else is built out of them.

### bool

1 byte. `0x00` is false and `0x01` is true. Don't send any other value, but if you receive one, treat any nonzero byte as true.

### u8, i8

1 byte. Signed and unsigned look exactly the same on the wire, so it's up to the receiver to know which one it is.

### u16, i16

2 bytes, big-endian. Signed and unsigned look the same on the wire.

### u32, i32

4 bytes, big-endian. Signed and unsigned look the same on the wire.

### u64, i64

8 bytes, big-endian. Signed and unsigned look the same on the wire.

### f32

4 bytes: the IEEE 754 binary32 encoding, sent most significant byte first.

### f64

8 bytes: the IEEE 754 binary64 encoding, sent most significant byte first.

### bytes

A 4-byte big-endian length, followed by that many raw bytes:

`[length: u32][data: <length> bytes]`

The only limit on the length is the size of the message it's in.

### string

Exactly the same as `bytes` on the wire, except the data is supposed to be UTF-8. Implementations shouldn't check that the UTF-8 is valid at the protocol level. That's up to whatever the method does with the string.

## Composite layouts

Structs and arrays aren't special at the protocol level; they're just rules for laying primitives out. Everyone has to follow the same rules, or the bytes won't line up.

### Structs

A struct is just its fields in declaration order, back to back, with no padding and nothing between them. There's no length, header or metadata for the struct itself.

For example, `Point { f64 x; f64 y; }` is 16 bytes: the 8 bytes of `x` followed by the 8 bytes of `y`.

Nested structs work the same way: they're flattened into the struct they're inside.

### Fixed-size arrays

If an array's length is known ahead of time (from the method's interface), it's just its elements back to back, with no length prefix. So `f64[16]` is exactly 128 bytes.

### Variable-size arrays

If the length isn't known ahead of time, the array starts with a 4-byte big-endian element count, followed by the elements:

`[count: u32][element × count]`

Elements can be variable-length themselves, like an array of strings. They just follow their own encoding rules.

### Multiple return values

A method that returns several values encodes them like the fields of an unnamed struct, in declaration order. On the wire, "returns a struct" and "returns multiple values" look identical.

## Putting a payload together

A CALL payload is each argument encoded in declaration order, using the rules above. A RESPONSE payload is the return value (or values) encoded the same way.

For example, calling `add(u32 a, u32 b) -> u32` with `a = 3, b = 4` sends an 8-byte CALL payload:

`00 00 00 03  00 00 00 04`

and the RESPONSE returning 7 has a 4-byte payload:

`00 00 00 07`

## Errors

When a call fails, the server sends an ERROR whose payload is:

`[code: u32][message: string]`

- **code:** 0 means "unknown error." Every other value is up to the implementation or service.
- **message:** a UTF-8 description of what went wrong, meant for logs or for showing to someone. It can be empty.

A client that gets an ERROR has to treat that call as failed, but the connection is still fine, and it can keep making calls on it.

The C server in this repo uses these codes itself:

| Code | Meaning                                                                           |
| ---- | --------------------------------------------------------------------------------- |
| 1001 | The CALL used method_id 0, which is reserved                                      |
| 1002 | No handler is registered for that method_id                                       |
| 1003 | The arguments don't match the method's schema (sent by wrengen-generated servers) |

## Connections

The server accepts TCP connections on its port. There's no handshake, so a client can start sending CALLs as soon as it connects.

Either side can close the connection whenever it wants, and a normal close is just a TCP FIN. If a connection closes while a call is in flight, the client has no way of knowing whether it went through, so it must treat every pending call as failed.

There's no built-in keepalive or ping. If you need to check that a server is alive, add a method for it.

## Size limits

A message with `length < 12` is malformed (it's smaller than its own header), and implementations must reject it. Implementations can also set a maximum message size, but they should support at least 64 MiB. If a message goes over the limit, the receiver must close the connection.

If you need to send more than the limit, split the data across several calls, or wait for the streaming extension below.

## Versioning

This is version 0.2. Anything that breaks the wire format bumps the major version. Backward-compatible additions, like new flag bits, message types or primitive types, can happen within a major version.

For now (0.x), an implementation that receives a message type it doesn't recognize should treat that as a protocol error and close the connection. Later versions might relax this so old and new implementations can talk to each other.

Things planned for later (not part of 0.x):

- **Streaming:** new message types for streaming responses from the server or requests from the client, with flag bits in the header marking chunked frames.
- **Compression:** a flag bit saying the message is compressed.
- **Authentication:** an optional handshake before any calls, for sending credentials.
