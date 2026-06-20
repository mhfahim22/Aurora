# Structs, Enums, Unions

## Struct

```aura
struct Point
    x
    y

# With default values:
struct Point
    x = 0
    y = 0

# With typed fields:
struct Point
    x: int = 0
    y: int = 0
```

### Construction

```aura
# Positional args (match field order)
p = Point(10, 20)

# Named field syntax (commas)
p = Point { x: 10, y: 20 }

# Named field syntax (semicolons also work)
p = Point { x: 10; y: 20 }

# Field access
output(p.x)                        # 10
```

### Value Semantics

Structs are value types — assignment creates a copy:

```aura
a = Point(1, 2)
b = a                              # b is a copy
b.x = 99
output(a.x)                        # 1 (unchanged)
output(b.x)                        # 99
```

## Enum

```aura
enum Color
    Red
    Green
    Blue

# Access variants
c = Color.Red
d = Color.Green
```

## Union (extern, FFI-only)

```aura
extern union Data
    i int
    f float

extern union Value {
    i: i32
    f: f32
}
```

> **Warning:** Unions are for FFI interop only. Accessing the wrong field is undefined behavior.

## Typed Struct Fields (extern)

For FFI compatibility with C structs:

```aura
extern struct Point
    x int
    y int

extern struct Point {
    x: i32
    y: i32
}
```

## Struct vs Class

| Aspect      | Struct | Class |
|-------------|--------|-------|
| Semantics   | Value  | Reference |
| Inheritance | No     | Yes   |
| Methods     | No     | Yes   |
| Visibility  | N/A    | Public/private/protected |

---

**Next:** [Memory Model](09-memory.md)
