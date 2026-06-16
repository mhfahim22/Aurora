# Functions

## Function Definition

```aura
function add(a, b)
    return a + b

function greet(name):
    output("Hello, " + name)    # colon optional

# Inline body (single expression)
function add(a, b) return a + b
function double(x) x * 2

# Brace block
function fib(n) {
    if n <= 1 return n
    return fib(n - 1) + fib(n - 2)
}
```

Functions return the last expression value implicitly. Use `return` to exit early.

## Anonymous Functions (`fn`)

```aura
add = fn(a, b) -> a + b          # arrow form
double = fn(x) -> x * 2

# Multi-line body
counter = fn()
    start = 0
    return fn()
        start = start + 1
        return start
```

## Lambda

```aura
square = lambda(x) x * x         # single expression
result = square(5)               # 25

doubled = map(array, lambda(x) x * 2)

# Multi-line body
lambda(a, b):
    return a + b
```

## Callback

```aura
callback my_cb(x):
    output(x)
```

## Return / Yield

```aura
return expr                       # return value
return                            # return void (0)
yield expr                        # yield value (generators)
```

## Function Attributes

```aura
@performance function fast_sum(n)
    total = 0
    for i in n
        total += i
    return total

@inline function small_helper(x) x + 1
@constexpr function compile_time_val() 42

# Inline attribute form:
@performance function renderer()
    # body
```

## `inline` / `noinline` / `constexpr` (statement forms)

```aura
inline fn(x) -> x * 2         # force inline
noinline fn(x) -> x * 2       # prevent inline
constexpr val = compile_fn()  # evaluate at compile time
```

## Typed Parameters

```aura
function add(a: int, b: int) -> int
    return a + b
```
