# Operators

## Arithmetic

| Op   | Description                           |
|------|---------------------------------------|
| `+`  | Addition / string concatenation       |
| `-`  | Subtraction (binary) / Negate (unary) |
| `*`  | Multiplication                        |
| `/`  | Float division                        |
| `//` | Integer division                      |
| `%`  | Modulo                                |
| `**` | Power                                 |

## Comparison

| Op       | Description              |
|----------|--------------------------|
| `==`     | Equal                    |
| `!=`     | Not equal                |
| `<`      | Less than                |
| `>`      | Greater than             |
| `<=`     | Less than or equal       |
| `>=`     | Greater than or equal    |
| `equals` | Structural equality      |

## Logical

| Op    | Description      |
|-------|------------------|
| `and` | Logical AND      |
| `or`  | Logical OR       |
| `not` | Logical NOT      |
| `xor` | Logical XOR      |
| `in`  | Membership test  |

## Bitwise

| Op   | Description     |
|------|-----------------|
| `&`  | Bitwise AND     |
| `|`  | Bitwise OR      |
| `^`  | Bitwise XOR     |
| `~`  | Bitwise NOT     |
| `<<` | Left shift      |
| `>>` | Right shift     |

## Range & Spread

| Op    | Description                      |
|-------|----------------------------------|
| `..`  | Range (exclusive upper bound)    |
| `..=` | Range (inclusive upper bound)    |
| `...` | Spread / varargs (FFI)           |

## Member Access & Arrow

| Op   | Description                 |
|------|-----------------------------|
| `.`  | Member access / method call |
| `->` | Pointer member access       |
| `:`  | Type annotation separator   |
| `@`  | Attribute prefix            |

## Compound Assignment

```aura
x += 5    # x = x + 5
x -= 3    # x = x - 3
x *= 2    # x = x * 2
x /= 4    # x = x / 4
```

## Operator Precedence (lowest to highest)

| Level | Operators                                    |
|-------|----------------------------------------------|
| 1     | `and`, `or`                                  |
| 2     | `==`, `!=`, `<`, `>`, `<=`, `>=`, `equals`, `in` |
| 3     | `|`                                          |
| 4     | `^`, `xor`                                   |
| 5     | `&`                                          |
| 6     | `<<`, `>>`                                   |
| 7     | `..`, `..=`                                  |
| 8     | `+`, `-` (binary)                            |
| 9     | `*`, `/`, `//`, `%`                          |
| 10    | unary `-`, `not`, `**` (power)               |

## Examples

```aura
x = 10 + 5 * 2             # 20 (multiplication first)
y = (10 + 5) * 2           # 30 (parens override)
z = 2 ** 3                 # 8 (power)
q = 10 // 3                # 3 (integer division)
r = 0..10                  # range 0-9 (exclusive)
s = 0..=10                 # range 0-10 (inclusive)
t = "hello" + " world"     # concatenation
```
