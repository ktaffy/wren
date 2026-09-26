# wren schema specification v0.1

A wren schema file (conventionally `*.wren`) describes one or more services
and any named types they use. The schema is consumed by `wrengen` to
produce per-language client and server code.

The schema language is deliberately minimal — it describes only what the
wire protocol can carry (see PROTOCOL.md). Rich features like generics,
unions, optionals, defaults, and imports are intentionally absent from v1.

## Lexical structure

- **Whitespace** (spaces, tabs, newlines) separates tokens and is otherwise
  ignored.
- **Comments** start with `//` and run to the end of the line. There are no
  block comments.
- **Identifiers** start with a letter or underscore and contain letters,
  digits, and underscores. They are case-sensitive.

## Primitive types

These match the wire types defined in PROTOCOL.md exactly.

| Type     | Wire format                        |
| -------- | ---------------------------------- |
| `bool`   | 1 byte (0 or 1)                    |
| `u8`     | 1 byte                             |
| `u16`    | 2 bytes, network byte order        |
| `u32`    | 4 bytes, network byte order        |
| `u64`    | 8 bytes, network byte order        |
| `i8`     | 1 byte (signed)                    |
| `i16`    | 2 bytes, network byte order        |
| `i32`    | 4 bytes, network byte order        |
| `i64`    | 8 bytes, network byte order        |
| `f32`    | 4 bytes, IEEE 754 binary32         |
| `f64`    | 8 bytes, IEEE 754 binary64         |
| `bytes`  | `[len: u32][data: <len> bytes]`    |
| `string` | Same wire format as `bytes`; UTF-8 |

## Composite types

### Arrays

- `T[]` — variable-length array. Wire: `[count: u32][element × count]`.
- `T[N]` — fixed-length array of exactly `N` elements. Wire: `element × N`,
  no length prefix.

`T` may be any type: primitive, struct, or another array.

### Structs

A `struct` declaration defines a named composite type. Fields are encoded
in declaration order, back-to-back, with no padding.

```
struct Point {
    f64 x;
    f64 y;
}
```

Fields follow the syntax `TYPE NAME;`. Arrays and nested structs are
allowed.

A struct must be declared before it is referenced. Every reference to a
struct type, whether in a field, a method argument, or a return type,
must name a struct whose declaration appears earlier in the file. A
struct counts as declared once its closing `}` is reached, so a struct
cannot refer to itself, directly or through an array. As a result,
recursive types cannot be expressed like Trees and linked lists.
Flatten them into an array of nodes that refer toe achother by index.
Will probably add this later ins a schema update, but too compicated
for now.

## Services

A `service` declaration defines a group of methods. Each service becomes
one family of generated stubs (client and/or server).

A schema declares at most one service. The wire header carries a method
ID but no service identifier, so one server hosts exactly one service's
methods. A program that hosts several services runs one server per
service.

```
service Calc {
    add(u32 a, u32 b) -> u32;
    log_message(string msg);
}
```

## Names

Every name belongs to a scope, and must be unique within that scope:

- **Top level:** struct names and service names share one scope, so a
  struct and a service cannot have the same name.
- **Struct:** field names are unique within their struct.
- **Service:** method names are unique within their service.
- **Method:** argument names are unique within their method. Names in a
  tuple return are unique within that tuple.

Names in different scopes never conflict. Two structs may each have a
field named `id`, and a field may share its name with a struct. Names
are case-sensitive, so `Point` and `point` are distinct.

### Method syntax

`METHOD_NAME ( ARG_LIST ) [-> RETURN_TYPE] ;`

- `METHOD_NAME` is an identifier, unique within the service.
- `ARG_LIST` is a comma-separated list of `TYPE NAME` pairs, or empty.
- `RETURN_TYPE` is a single type or a parenthesized tuple of typed names.
  Omit `-> RETURN_TYPE` entirely for methods that return no value.

Method IDs are assigned implicitly by declaration order, starting at 1.
The first method declared in a service is method_id 1, the second is 2,
and so on. Method IDs must not be written in the schema. Reordering
methods is a breaking change to the wire format.

Method IDs are 16-bit on the wire and 0 is reserved (see PROTOCOL.md),
so a service declares at most 65,535 methods.

## Reserved names

Generated code must never collide with the target languages or with the
names it defines for itself. The following are rejected anywhere a name
is declared (structs, services, fields, methods, arguments, and tuple
return names):

- **Identifiers beginning with an underscore.** These are reserved for
  names generated code uses internally.
- **Keywords of any target language:** Python's keywords, and C's
  keywords (C11, plus `bool`, `true`, and `false`).
- **Names of generated members:** `encode` and `decode` as field names,
  and `close` as a method name.

### Multiple return values

A method may return a tuple, treated on the wire as an anonymous struct:
`divmod(u32 a, u32 b) -> (u32 quotient, u32 remainder);`

Equivalent on the wire to returning a struct with those two fields in
that order.

## Example

```
// calc.wren
struct Point {
    f64 x;
    f64 y;
}
struct Row {
    u32 id;
    string name;
    string role;
}
service Calc {
    add(u32 a, u32 b) -> u32;
    multiply(u32 a, u32 b) -> u32;
    distance_between(Point p1, Point p2) -> f64;

    sum_array(u32[] values) -> u64;

    divmod(u32 a, u32 b) -> (u32 quotient, u32 remainder);

    list_admins() -> Row[];

    log_message(string msg);       // no return value
}
```

## Grammar (EBNF)

```ebnf
schema         = { declaration } ;
declaration    = struct_decl | service_decl ;

struct_decl    = "struct" IDENT "{" { field } "}" ;
field          = type IDENT ";" ;

service_decl   = "service" IDENT "{" { method } "}" ;
method         = IDENT "(" [ arg_list ] ")" [ "->" return_type ] ";" ;
arg_list       = arg { "," arg } ;
arg            = type IDENT ;

return_type    = type | tuple_type ;
tuple_type     = "(" arg_list ")" ;

type           = base_type { array_suffix } ;
base_type      = primitive | IDENT ;                (* IDENT references a struct *)
array_suffix   = "[" [ NUMBER ] "]" ;               (* "[]" = variable, "[N]" = fixed *)

primitive      = "bool" | "u8" | "u16" | "u32" | "u64"
               | "i8" | "i16" | "i32" | "i64"
               | "f32" | "f64" | "bytes" | "string" ;

IDENT          = ( letter | "_" ) { letter | digit | "_" } ;
NUMBER         = digit { digit } ;
```

## What's intentionally not in v1

- **Imports / multi-file schemas.** One service per file; copy shared types
  if needed.
- **Enums.** Use `u32` constants with documented values.
- **Unions / sum types.** Use a struct with a discriminator field and
  payload bytes.
- **Generics.** No `Map<K, V>`, `Option<T>`, etc. Compose out of primitives.
- **Optional fields.** Use a `bool` flag + the value.
- **Default values.** Handle in application code.
- **Annotations / metadata.** No decorator syntax on fields or methods.
- **Service inheritance or composition.** Flat services only.
- **Explicit method ID numbering.** IDs are by declaration order.
- **Error type declarations.** Errors are runtime values; document codes
  externally.

## Versioning

This is schema language v0.1. The parser and codegen will tolerate
future additive extensions (new primitives, new declaration kinds) by
rejecting schemas that use unknown constructs. Breaking changes to
existing syntax will increment the major version.
