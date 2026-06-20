# Syntax Basics

## Whitespace & Block Structure

Blocks are defined by indentation (4 spaces per level). No braces, no `end` keyword required.

```aura
function greet(name)
    output("Hello, " + name)    # ← body is indented 4 spaces
    return 0                    # ← still in function body

output("done")                  # ← back to top-level (dedented)

# Brace blocks also work (alternative syntax):
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

> **Tip:** Configure your editor to use 4 spaces per indent. Aurora does not support tabs for indentation.

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

`<< ... >>` acts as parentheses spanning multiple lines:

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

## `end` Keyword

`end` is a no-op (pass statement), not a block terminator. Occasionally used in library code for familiarity:

```aura
end     # does nothing — acts as a placeholder
```

---

**Next:** [Variables & Assignment](02-variables.md)
