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

> **Note:** `+` on strings performs concatenation: `"hello" + " world"` → `"hello world"`.

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

> **Tip:** Use `==` for value comparison, `equals` for deep structural comparison of complex types.

## Logical

| Op    | Description      | Short-circuits? |
|-------|------------------|-----------------|
| `and` | Logical AND      | Yes             |
| `or`  | Logical OR       | Yes             |
| `not` | Logical NOT      | N/A             |
| `xor` | Logical XOR      | No              |
| `in`  | Membership test  | No              |

```aura
if x > 0 and x < 10       # short-circuit: if first is false, second not evaluated
if x > 0 or cleanup()     # short-circuit: if first is true, second not evaluated
if not flag                # negation
if "hello" in ["hi", "hello", "hey"]  # membership test
```

## Bitwise

| Op   | Description     |
|------|-----------------|
| `&`  | Bitwise AND     |
| `|`  | Bitwise OR      |
| `^`  | Bitwise XOR     |
| `~`  | Bitwise NOT     |
| `<<` | Left shift      |
| `>>` | Right shift     |

```aura
flags = 0b0011
mask  = 0b0101
result = flags & mask       # 0b0001
result = flags | mask       # 0b0111
result = flags ^ mask       # 0b0110
result = ~flags             # bitwise NOT
result = flags << 2         # 0b1100
```

## Range & Spread

| Op    | Description                      |
|-------|----------------------------------|
| `..`  | Range (exclusive upper bound)    |
| `..=` | Range (inclusive upper bound)    |
| `...` | Spread / varargs (FFI)           |

```aura
0..10         # 0, 1, 2, ..., 9 (exclusive)
0..=10        # 0, 1, 2, ..., 10 (inclusive)
```

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
x //= 2   # x = x // 2
x %= 3    # x = x % 3
x **= 2   # x = x ** 2
x &= mask # x = x & mask
x |= mask # x = x | mask
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
u = 0 < x < 10             # chained comparison (true if x between 0 and 10)
```

---

**Next:** [Control Flow](05-control-flow.md)
