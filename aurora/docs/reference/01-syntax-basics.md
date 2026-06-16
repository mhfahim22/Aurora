# Syntax Basics

## Whitespace & Block Structure

Blocks are defined by indentation (4 spaces per level). No braces, no `end` keyword.

```aura
function greet(name)
    output("Hello, " + name)    # body is indented
    return 0                    # still in function body

output("done")                  # back to top-level

# Brace blocks also work (alternative):
if true {
    output("brace block")
}
```

Tokens: `Indent` (increased), `Dedent` (decreased), `Newline` (end of line).

## Comments

```aura
# Line comment (hash style)

/* Block comment
   spanning multiple lines */
```

## Literals

```aura
42          # integer
0xFF        # hex integer
3.14        # float
"hello"     # double-quoted string
'c'         # single-quoted character
true        # boolean true
false       # boolean false
null        # null value
[1, 2, 3]   # array literal
```

String escapes: `\n`, `\t`, `\r`, `\\`, `\"`, `\'`.

## Identifiers

Start with letter or underscore, then letters, digits, underscores. Case-sensitive.

```aura
myVar  _private  fib_42  ClassName  ALL_CAPS
```

## Multi-line Bracket

`<< ... >>` acts as parentheses spanning multiple lines:

```aura
x = <<
    1
    2
    3
>>
```

## Semicolons

Not supported. The compiler will hint: "Aurora uses indentation for blocks, not semicolons."

## `end` Keyword

`end` is a no-op (pass statement), not a block terminator. Occasionally used in library code for familiarity:

```aura
end     # does nothing
```
