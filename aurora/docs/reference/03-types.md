# Types

## Primitive Types

| Type      | Aliases                      | Description                    | Default |
|-----------|------------------------------|--------------------------------|---------|
| `int`     | `i64`, `Int`, `u64`, `i32`   | Integer (various widths)       | `0`     |
|           | `u32`, `i16`, `i8`, `char`   |                                |         |
| `float`   | `f64`, `Float`, `double`, `f32` | Floating-point               | `0.0`   |
| `string`  | `String`, `str`              | UTF-8 string                   | `""`    |
| `bool`    | `Bool`                       | Boolean (`true`, `false`)      | `false` |
| `void`    | `Void`                       | No value                       | —       |
| `ptr<T>`  | `pointer`, `Pointer`, `void*` | Raw pointer                   | `null`  |
| `cstring` | `char*`                      | C-style null-terminated string | `null`  |

## Compound Types

| Type      | Description                         | Example                     |
|-----------|-------------------------------------|-----------------------------|
| `array`   | Dynamic array                       | `[1, 2, 3]`                |
| `struct`  | Named field collection               | `struct Point { x, y }`    |
| `class`   | Object with methods and inheritance  | `class Person { ... }`     |
| `enum`    | Named variant constants              | `enum Color { Red }`       |
| `list`    | Generic list                         | `list_new()`               |
| `map`     | Key-value dictionary                 | `map_new()`                |
| `set`     | Unique element collection            | `set_new()`                |
| `stack`   | LIFO stack                           | `stack_new()`              |
| `queue`   | FIFO queue                           | `queue_new()`              |
| `vector`  | 3D vector (x, y, z)                  | `vector_new(1, 2, 3)`     |
| `tuple`   | Fixed-size heterogeneous collection  | `(1, "hello", true)`       |
| `json`    | JSON value                           | `json_parse(str)`          |
| `function`| Callable                             | `fn(x) -> x * 2`          |

## Type Aliases

```aura
type MyInt = int
type UserId = int
type User = struct { name: string, age: int }

using Name = string    # alternative syntax
```

## Struct Declaration

```aura
struct Point
    x
    y

# With defaults:
struct Point
    x = 0
    y = 0

# With field types:
struct Point
    x: int = 0
    y: int = 0

p1 = Point(10, 20)                 # positional args
p2 = Point { x: 10, y: 20 }       # named fields
p3 = Point { x: 10; y: 20 }       # semicolons also work
output(p.x)                        # 10
```

See [Structs, Enums, Unions](08-structs-enums-unions.md) for more details.

## Enum Declaration

```aura
enum Color
    Red
    Green
    Blue

c = Color.Red
```

## Class Declaration

```aura
class Person
    name = ""
    age = 0

    function greet()
        output("Hi, I'm " + self.name)

p = Person("Alice", 30)
p.greet()
```

See [OOP](07-oop.md) for classes, inheritance, interfaces, visibility.

## Union Declaration (extern)

```aura
extern union Data
    i int
    f float
```

> **Warning:** Unions are for FFI only. Accessing the wrong field is undefined behavior.

## Function Type

```aura
add = fn(a, b) -> a + b        # fn -> arrow form
callback handler = fn(x) output(x) end   # callback type
```

See [Functions](06-functions.md) for full details.

---

**Next:** [Operators](04-operators.md)
