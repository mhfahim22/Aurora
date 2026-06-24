# Syntax Basics

## Case Insensitivity

All Aurora keywords are **case-insensitive**:

```aura
true   True   TRUE    # all valid
false  False  FALSE   # all valid
null   Null   NULL    # all valid
output  OUTPUT  OutPut  # all valid
```

Function names, variable names, and identifiers **are case-sensitive**:

```aura
myVar = 42
myvar = 10            # different variable!
output(myVar)         # 42
output(myvar)         # 10
```

## Whitespace & Block Structure

Aurora uses indentation to define blocks (like Python). **4 spaces** per level is the standard.

```aura
function greet(name)
    output("Hello, " + name)    # ← body is indented 4 spaces
    return 0                    # ← still in function body

output("done")                  # ← back to top-level (dedented)
```

### Three Block Styles

Every control flow / declaration construct supports **all three** styles:

| Style | Example | Notes |
|-------|---------|-------|
| Indented (no colon) | `if cond\n    body` | Clean, Python-like |
| Colon + Indented | `if cond:\n    body` | More explicit |
| Brace `{ }` | `if cond { body }` | C/JS-like, inline friendly |

```aura
# Style 1: Indented (bare)
if true
    output("bare indent")

# Style 2: Colon + Indented
if true:
    output("colon indent")

# Style 3: Brace block
if true {
    output("brace block")
}
```

**Nested blocks** use increasing indentation:

```aura
if condition
    if nested
        output("deeply nested")
```

Tokens: `Indent` (increased), `Dedent` (decreased), `Newline` (end of line).

> **Tip:** Configure your editor to use 4 spaces per indent. Tabs are converted to 4 spaces internally, but 4 spaces is the convention.

## Comments

```aura
# Line comment (hash style)

/* Block comment
   spanning multiple lines */

output("hello")  # inline comment
```

## Literals

```aura
42          # integer (decimal)
0xFF        # hexadecimal
0o77        # octal
0b1010      # binary
3.14        # float
1.5e-3      # scientific notation
1_000_000   # numeric separator (underscore)
"hello"     # double-quoted string
'c'         # single-quoted character
true        # boolean true
false       # boolean false
null        # null value
[1, 2, 3]   # array literal
(1, "hi")   # tuple literal
```

### String Escape Sequences

| Escape | Description  |
|--------|--------------|
| `\n`   | Newline      |
| `\t`   | Tab          |
| `\r`   | Carriage return |
| `\\`   | Backslash    |
| `\"`   | Double quote |
| `\'`   | Single quote |

## Identifiers

Start with letter or underscore, then letters, digits, underscores. Case-sensitive.

```aura
myVar      # camelCase (common for variables)
_private   # underscore prefix (convention for internal)
fib_42     # numbers allowed mid-name
ClassName  # PascalCase (convention for types)
ALL_CAPS   # screaming snake case (convention for constants)
```

## Multi-line Bracket

`<< ... >>` acts as parentheses or array brackets spanning multiple lines:

```aura
x = <<
    1
    2
    3
>>

# Practical use: multi-line function arguments
result = my_function(<<
    arg1,
    arg2,
    arg3
>>)
```

## Semicolons

Not supported. The compiler will hint: "Aurora uses indentation for blocks, not semicolons."

> **Migration tip:** If you're coming from C/Java/JS, omit all semicolons. Use indentation to define block structure.

## `pass` Keyword (No-op)

`pass` does nothing. Use it as a placeholder:

```aura
if condition
    pass         # placeholder — does nothing
```

## `end` Keyword (No-op / Familiarity)

`end` is a no-op (treated as `pass`), not a block terminator. Occasionally used in library code for familiarity:

```aura
end     # does nothing — acts as a placeholder
```

## `self` and `super`

`self` refers to the current class instance (implicitly available inside methods).
`super` refers to the parent class (for calling overridden methods).

```aura
class Animal
    function speak()
        output("generic")

class Dog extends Animal
    function speak()
        super.speak()           # call parent method
        output("woof")
```

---

**Next:** [Variables & Assignment](02-variables.md)
