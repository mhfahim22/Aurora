# Control Flow

All control flow constructs accept both colon (`:`) and no-colon before the body, and support indented blocks or brace blocks.

---

## If / Elseif / Else

### Multi-line (indented)
```aura
if condition
    body()

if condition:
    body()                       # colon optional

if condition:
    body()
elseif other_condition:
    other_body()
else:
    fallback()
```

### Inline (single expression)
```aura
if n <= 1 return n               # inline after condition
if n <= 1 { return n }           # brace block inline
```

### Ruby-style (with `do` / `end`)
```aura
if cond do body() end
if i >= n break end
```

---

## While

```aura
while condition
    body()

while condition:
    body()                       # colon optional

while condition {                # brace block
    body()
}
```

---

## For

```aura
# Count form: iterate 0..n-1
for i in 5
    output(i)

# Explicit range (exclusive)
for i in 0..10
    output(i)

# Inclusive range
for i in 0..=10
    output(i)

# Comma range
for i in 0, 5
    output(i)

# Array iteration
for item in array
    output(item)

# All forms support colon
for i in 5:
    output(i)

# All forms support brace block
for i in 3 {
    output(i)
}
```

---

## Loop (infinite)

```aura
loop
    body()

loop:                           # colon optional
    body()

loop {                          # brace block
    body()
}
```

---

## Repeat / Until

```aura
repeat
    body()
until condition

repeat:                         # colon optional
    body()
until condition
```

---

## Break / Continue / Skip

```aura
for i in 100
    if i == 5
        break                    # exit loop
    if i % 2 == 0
        continue                 # next iteration
    skip                         # alias for continue
    output(i)
```

---

## Match / Switch

`match` and `switch` are exact aliases.

```aura
match value
    case 1
        output("one")
    case 2, 3
        output("two or three")
    case _
        output("other")          # wildcard

# With colons
match value:
    case 1:
        output("one")
    default:
        output("other")

# Arrow syntax
match x
    0 -> output("zero")
    1 -> output("one")
    _ -> output("other")
```

### Pattern types

```aura
match value
    case 42                      # literal match
    case x                       # variable binding
    case _                       # wildcard (ignore)
    case [a, b, c]               # array destructuring
    case Point(0, 0)             # struct/constructor pattern
    default                      # fallback (same as case _)
```

### Switch alias

```aura
switch x
    case 1:
        output("one")
    case 2:
        output("two")
```
