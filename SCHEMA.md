# wren schema spec (v0.1)

A .wren file/schema describes a service and the types it uses. `wrengen` reads it and generates client and server code for each language.

The schema language is kept small on purpose. It can only describe what the wire protocol can actually carry (see [PROTOCOL.md](PROTOCOL.md)). Nicer features like generics, unions, optionals, defaults and imports aren't in v0.1.

## Lexical stuff

- **Whitespace** (spaces, tabs, newlines) just separates tokens and is otherwise ignored.
- **Comments** start with `//` and go to the end of the line. There are no block comments.
- **Identifiers** start with a letter or underscore and can contain letters, digits and underscores. They're case-sensitive. (Names starting with an underscore are allowed by the lexer, but rejected as reserved; see [Reserved names](#reserved-names).)

## Primitive types

These match the wire types in PROTOCOL.md exactly.

| Type     | Wire format                     |
| -------- | ------------------------------- |
| `bool`   | 1 byte (0 or 1)                 |
| `u8`     | 1 byte                          |
| `u16`    | 2 bytes, big-endian             |
| `u32`    | 4 bytes, big-endian             |
| `u64`    | 8 bytes, big-endian             |
| `i8`     | 1 byte (signed)                 |
| `i16`    | 2 bytes, big-endian             |
| `i32`    | 4 bytes, big-endian             |
| `i64`    | 8 bytes, big-endian             |
| `f32`    | 4 bytes, IEEE 754 binary32      |
| `f64`    | 8 bytes, IEEE 754 binary64      |
| `bytes`  | `[len: u32][data: <len> bytes]` |
| `string` | Same as `bytes`, but UTF-8      |

## Arrays

- `T[]` is a variable-length array. On the wire: `[count: u32][element × count]`.
- `T[N]` is a fixed-length array of exactly `N` elements. On the wire: just `element × N`, with no count in front.

`T` can be any type, including a struct or another array.

## Structs

A `struct` defines a named type. Its fields are encoded in the order they're declared, back to back, with no padding.

```
struct Point {
    f64 x;
    f64 y;
}
```

Fields are written `TYPE NAME;`, and they can be arrays or other structs.

**Structs have to be declared before they're used.** Anywhere you reference a struct, that struct's declaration has to appear earlier in the file. A struct only counts as declared once you hit its closing `}`, so a struct can't refer to itself, either directly or through an array.

This means you can't write recursive types like trees or linked lists. The workaround is to flatten them into an array of nodes that point at each other by index. I might add recursive types in a later version, but its just doing too much, so they're out for now.

## Services

A `service` defines a group of methods. It's what gets turned into a client and a server.

**A schema can have at most one service.** The wire header has a method ID but no service ID, so one server can only host one service. If a program needs several services, it runs one server per service.

```
service Calc {
    add(u32 a, u32 b) -> u32;
    log_message(string msg);
}
```

### Method syntax

`METHOD_NAME ( ARG_LIST ) [-> RETURN_TYPE] ;`

- `METHOD_NAME` is an identifier, and it has to be unique within the service.
- `ARG_LIST` is a comma-separated list of `TYPE NAME` pairs, or nothing.
- `RETURN_TYPE` is either one type or a tuple of typed names in parentheses. Leave off `-> RETURN_TYPE` completely if the method doesn't return anything.

**Method IDs come from declaration order,** starting at 1. The first method is method_id 1, the second is 2, and so on. You don't write IDs in the schema. This means that **reordering methods changes the wire format**, so it's a breaking change.

Method IDs are 16 bits on the wire and 0 is reserved, so a service can have at most 65,535 methods.

### Multiple return values

A method can return a tuple:

`divmod(u32 a, u32 b) -> (u32 quotient, u32 remainder);`

On the wire, that's exactly the same as returning a struct with those two fields in that order.

## Names

Every name lives in a scope, and it has to be unique within that scope:

- **Top level:** struct names and the service name share one scope, so a struct and a service can't have the same name.
- **Inside a struct:** field names have to be unique.
- **Inside a service:** method names have to be unique.
- **Inside a method:** argument names have to be unique, and so do the names in a tuple return.

Names in different scopes never conflict. Two structs can both have a field called `id`, and a field can have the same name as a struct. Names are case-sensitive, so `Point` and `point` are different names.

## Reserved names

Generated code can't be allowed to collide with the target languages, or with the names it uses for itself. So these are rejected anywhere you declare a name (structs, services, fields, methods, arguments and tuple return names):

- **Names starting with an underscore.** Generated code uses these for its own internal names.
- **Names starting with `wren_`.** These are reserved for the wren runtime and the types generated code defines, like the C runtime's `wren_bytes_t`.
- **Keywords in any target language.** That means Python's keywords, and C's keywords (C11, plus `bool`, `true` and `false`).
- **Names of generated members:** `encode` and `decode` as field names, and `close` as a method name.

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

## Things not in v0.1 (on purpose)

- **Imports or multi-file schemas.** One service per file. copy shared types if you need them in more than one place.
- **Enums.** Use a `u32` and document what each value means.
- **Unions or sum types.** Use a struct with a "kind" field plus a `bytes` payload.
- **Generics.** There's no `Map<K, V>` or `Option<T>`. build what you need out of the basic types.
- **Optional fields.** Use a `bool` flag next to the value.
- **Default values.** Handle them in your own code.
- **Annotations or metadata.** There's no decorator syntax for fields or methods.
- **Service inheritance or composition.** Services are flat.
- **Choosing method IDs yourself.** IDs always come from declaration order.
- **Declaring error types.** Errors are just runtime values, so document your error codes somewhere else.
- **Recursive types.** See [Structs](#structs) for the workaround.

## Versioning

This is version 0.1 of the schema language. If a schema uses something not explained here, it gets rejected rather than guessed at, which leaves room to add new primitives or new kinds of declarations later without breaking anything.
