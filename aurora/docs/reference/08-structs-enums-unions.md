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

## Typed Struct Fields (extern)

```aura
extern struct Point
    x int
    y int

extern struct Point {
    x: i32
    y: i32
}
```
