# Functions

## Function Definition

`function` and `fn` are interchangeable keywords for defining functions.

```aura
function add(a, b)
    return a + b

fn add(a, b)                     # fn works exactly like function
    return a + b

function greet(name):
    output("Hello, " + name)    # colon optional
```

### Inline body (single expression)

```aura
function add(a, b) return a + b
function double(x) x * 2
function add(a, b) { return a + b }    # brace inline
function double(x) { x * 2 }           # brace with bare expression
```

### Brace block

```aura
function fib(n) {
    if n <= 1 return n
    return fib(n - 1) + fib(n - 2)
}
```

Functions return the **last expression value** implicitly. Use `return` to exit early.

```aura
function max(a, b)
    if a > b
        a                        # implicit return
    else
        b                        # implicit return
```

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

# Named lambda
lambda myLambda(a, b):
    return a + b

# Multi-line body
lambda(a, b):
    return a + b
```

**`fn` vs `lambda`:** Both create anonymous functions. `fn` supports the arrow syntax (`->`), while `lambda` uses a more traditional form. Choose based on style preference.

## Callback

```aura
callback my_cb(x):
    output(x)

# For FFI callback parameters:
extern function set_handler(cb: callback(int, int))
```

## Return / Yield

```aura
return expr                       # return value
return                            # return void (0)
yield expr                        # yield value (generators)
```

## Return Type Annotation

```aura
function add(a: int, b: int) -> int
    return a + b

function greet(name: string) -> string
    return "Hello, " + name
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

@noinline function rarely_used()
    output("keep separate")
```

See [Attributes](14-attributes.md) for full details.

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

function greet(name: string) -> string
    return "Hello, " + name
```

---

**Next:** [OOP](07-oop.md)
