# Types

## Primitive Types

| Type      | Aliases                      | Description                    |
|-----------|------------------------------|--------------------------------|
| `int`     | `i64`, `Int`, `u64`, `i32`   | Integer (various widths)       |
|           | `u32`, `i16`, `i8`, `char`   |                                |
| `float`   | `f64`, `Float`, `double`, `f32` | Floating-point               |
| `string`  | `String`, `str`              | UTF-8 string                   |
| `bool`    | `Bool`                       | Boolean (`true`, `false`)      |
| `void`    | `Void`                       | No value                       |
| `ptr<T>`  | `pointer`, `Pointer`, `void*` | Raw pointer                   |
| `cstring` | `char*`                      | C-style null-terminated string |

## Compound Types

| Type      | Description                         |
|-----------|-------------------------------------|
| `array`   | Dynamic array `[1, 2, 3]`           |
| `struct`  | Named field collection               |
| `class`   | Object with methods and inheritance  |
| `enum`    | Named variant constants              |
| `list`    | Generic list                         |
| `map`     | Key-value dictionary                 |
| `set`     | Unique element collection            |
| `stack`   | LIFO stack                           |
| `queue`   | FIFO queue                           |
| `vector`  | 3D vector (x, y, z)                  |
| `tuple`   | Fixed-size heterogeneous collection  |
| `json`    | JSON value                           |
| `function`| Callable                             |

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

p1 = Point(10, 20)                 # positional args
p2 = Point { x: 10, y: 20 }       # named fields
p3 = Point { x: 10; y: 20 }       # semicolons also work
output(p.x)                        # 10
```

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

## Function Type

```aura
add = fn(a, b) -> a + b        # fn -> arrow form
callback handler = fn(x) output(x) end   # callback type
```

See [Functions](06-functions.md) for full details.
