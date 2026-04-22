# wren protocol specification v0.2

Wren is a binary RPC protocol for low-latency function calls between
programs, across processes or machines, in any language. This document
defines the wire format. It is the single source of truth for any
compliant wren implementation.

## Overview

A wren server hosts functions, identified by 16-bit method IDs. A wren
client connects to a server over TCP and invokes those functions by
sending call messages. The server responds with return values or errors.

Connections are long-lived. Many calls may be made on a single connection.
Multiple clients may connect to one server concurrently.

The protocol carries only primitive types. Composite data structures
(structs, arrays) are built by composing primitives in agreed-upon
layouts, described below.

## Transport

Wren runs over TCP. All multi-byte integers on the wire are in network
byte order (big-endian). Servers listen on a port of their choosing; 8080
is a common default but not required.

## Framing

Every message begins with a 12-byte header. The first 4 bytes of the
header give the total length of the message including the header itself.
Receivers read 4 bytes, decode the length, then read `length - 4` more
bytes to complete the message.

A connection carries a stream of such messages back-to-back, with no
delimiters between them.

## Message Header

12 bytes, laid out as follows:

| Offset | Size | Field      | Description                                 |
|--------|------|------------|---------------------------------------------|
| 0      | 4    | length     | Total message length in bytes, incl. header |
| 4      | 1    | type       | Message type (see below)                    |
| 5      | 1    | flags      | Reserved, must be 0                         |
| 6      | 2    | method_id  | Which method is being invoked               |
| 8      | 4    | req_id     | Correlates calls with responses             |

### Message types

| Value | Name     | Direction       | Purpose                             |
|-------|----------|-----------------|-------------------------------------|
| 1     | CALL     | client → server | Invoke a method                     |
| 2     | RESPONSE | server → client | Successful return value             |
| 3     | ERROR    | server → client | Method failed                       |

The `flags` byte is reserved for future use and must be 0 in all messages
sent by current implementations. Receivers must ignore its value.

The `method_id` field is only meaningful in CALL messages. Value 0 is
reserved and must not be used; implementations should treat a CALL with
method_id 0 as a protocol error. In RESPONSE and ERROR messages, the
method_id value is unspecified and receivers must ignore it.

The `req_id` field is chosen by the client for each CALL and echoed
unchanged by the server in the corresponding RESPONSE or ERROR. It allows
the client to match responses to calls when multiple calls are in flight.
Clients should use monotonically increasing `req_id` values per
connection. The protocol does not restrict how they are chosen beyond
uniqueness within the connection.

## Payload

The bytes following the 12-byte header are the message payload. Its
length is `length - 12`. Its meaning depends on message type:

- CALL payload: the encoded arguments to the method, in declaration
  order. May be empty if the method takes no arguments.
- RESPONSE payload: the encoded return value of the method. May be empty
  if the method returns no value.
- ERROR payload: see "Errors" below.

The protocol does not define how arguments or return values are laid out
within the payload beyond the primitive and composite rules described
below. The specific fields of each method are part of its interface,
documented externally (in a README, schema, or similar).

## Primitive Encodings

Wren implementations must support encoding and decoding the following
primitive types. These are the building blocks from which payloads are
composed.

### bool

1 byte. Value `0x00` represents false; `0x01` represents true. Other
values are reserved and must not be produced; receivers should treat any
nonzero value as true for forward compatibility.

### u8, i8

1 byte. Signed and unsigned variants have identical wire format;
interpretation is the caller's responsibility.

### u16, i16

2 bytes, network byte order. Signed and unsigned variants have identical
wire format.

### u32, i32

4 bytes, network byte order. Signed and unsigned variants have identical
wire format.

### u64, i64

8 bytes, network byte order. Signed and unsigned variants have identical
wire format.

### f32

4 bytes. IEEE 754 binary32 representation. The 4 bytes of the IEEE
encoding are transmitted in network byte order (most significant byte
first).

### f64

8 bytes. IEEE 754 binary64 representation. The 8 bytes of the IEEE
encoding are transmitted in network byte order.

### bytes

Variable-length byte array. Encoded as a 4-byte length prefix in network
byte order, followed by that many raw bytes.

`[length: u32][data: <length> bytes]`

Maximum length is bounded only by the enclosing message's total size.

### string

Same wire format as `bytes`. The payload bytes are interpreted as UTF-8
text. Implementations should not enforce UTF-8 validity at the protocol
layer; that is a method-level concern.

## Composite Layouts

Structs and arrays are not distinct protocol-level entities; they are
conventions for composing primitives. All implementations must follow
these conventions so that emitted and received bytes are interoperable.

### Structs

A struct is encoded as its fields in declaration order, back-to-back,
with no padding or separators between fields. The struct itself has no
length prefix, header, or metadata on the wire — only its fields.

Example: a struct `Point { f64 x; f64 y }` is encoded as 16 bytes: the
IEEE encoding of `x` followed by the IEEE encoding of `y`.

Nested structs follow the same rule — they are their fields in order,
flattened into the enclosing structure.

### Fixed-size arrays

An array whose length is known statically (from the method's interface)
is encoded as its elements back-to-back, with no length prefix.

Example: `f64[16]` is 128 bytes.

### Variable-size arrays

An array whose length is not known statically is encoded as a 4-byte
length prefix (`u32`, network byte order) giving the number of elements,
followed by the elements back-to-back.

`[count: u32][element × count]`

The encoding of each element follows the element type's rules, which may
themselves be variable-length (e.g., an array of strings).

### Multiple return values

A method that returns multiple values encodes them as if they were the
fields of an anonymous struct, in declaration order. At the wire level
there is no distinction between "a method returning a struct" and "a
method returning multiple values."

## Payload Composition

A method's CALL payload is built by encoding each argument in declaration
order, using the rules above. A method's RESPONSE payload is built by
encoding the return value (or values) the same way.

Example: a method `add(u32 a, u32 b) -> u32` with arguments `a=3`, `b=4`
has a CALL payload of 8 bytes:

`00 00 00 03  00 00 00 04`

Its RESPONSE payload, returning 7, is 4 bytes:

`00 00 00 07`

Methods with no arguments have an empty CALL payload. Methods that
return no value have an empty RESPONSE payload.

## Errors

ERROR messages are sent by the server when a call fails. The ERROR
payload is structured as:

`[code: u32][message: string]`

- `code`: an implementation-defined error code. Value 0 is reserved for
  "unknown error." Other values are defined by the service.
- `message`: a UTF-8 string describing the error, suitable for logging
  or displaying. May be empty.

A client that receives an ERROR must treat the call as failed. The
connection remains valid; subsequent calls may be made.

## Connection Lifecycle

A wren server accepts TCP connections on its listen port. No handshake
is performed; a connected client may immediately send CALL messages.

A connection may be closed at any time by either side. A clean close is
a normal TCP FIN. If the connection is closed mid-call, the outcome of
any in-flight call is undefined from the client's perspective; the client
must treat pending requests as failed.

There is no keepalive or ping mechanism at the protocol level. If an
application requires liveness detection, it should define a method for
that purpose.

## Message Size Limits

Implementations must reject messages with `length < 12` (a length smaller
than the header itself is malformed). Implementations may reject messages
larger than an implementation-defined maximum. Well-behaved
implementations should support at least 64 MiB per message.

Applications that need to transfer payloads larger than the maximum
should design their interface to chunk the data across multiple calls,
or wait for a future version of the protocol that defines a streaming
extension.

If an implementation receives a message exceeding its configured limit,
it must close the connection.

## Versioning

This specification is version 0.2. Breaking changes to the wire format
will increment the major version. Additions that remain backward
compatible (such as new flag bits, new message types, or new primitive
types) may be made within a major version.

Implementations of version 0.x should treat unknown message types and
unrecognized flag bits as protocol errors and close the connection.
Future versions may relax this to enable forward compatibility.

Planned future extensions (not part of v0.x):

- **Streaming**: new message types for server-streaming and
  client-streaming responses. Flag bits in the existing header will mark
  chunked frames.
- **Compression**: per-message compression indicated by a flag bit.
- **Authentication**: an optional pre-call handshake for credentials.