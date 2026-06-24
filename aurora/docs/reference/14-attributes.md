# Attributes

Attributes modify the behavior of functions, variables, and FFI declarations.

## Function Attributes

| Attribute         | Description                              |
|-------------------|------------------------------------------|
| `@performance`    | Optimized function mode                  |
| `@inline`         | Hint to inline function                  |
| `@noinline`       | Hint to not inline function              |
| `@constexpr`      | Compile-time constant evaluation         |

```aura
@performance function fast_sum(n)
    total = 0
    for i in n
        total += i
    return total

@inline function small_helper(x) x + 1

@noinline function rarely_used()
    output("keep separate")

@constexpr function compile_time_val() 42
```

## Memory Allocation Attributes

| Attribute  | Strategy | Description                            |
|------------|----------|----------------------------------------|
| `@stack`   | Stack    | Fast, no deallocation cost             |
| `@arena`   | Arena    | Bulk-free, batch workloads             |
| `@raii`    | RAII     | Deterministic destructor on scope exit |
| `@arc`     | ARC      | Reference counting                     |
| `@gc`      | GC       | Garbage collection                     |

```aura
@stack a = 100
@arena data = [1, 2, 3, 4, 5]
@raii file = open("test.txt")
@arc shared = [1, 2, 3]
@gc big_data = load_large()
```

### Allocation on functions

```aura
@stack function process()
    # function runs with stack allocation

@arena function batch_job()
    # temporary allocations use arena
```

### When to Use Each

| Attribute | Use When                                          |
|-----------|---------------------------------------------------|
| `@stack`  | Local variables with known size, short lifetime   |
| `@arena`  | Many temporary allocations in a loop/scope        |
| `@raii`   | Managing OS resources (files, sockets, locks)      |
| `@arc`    | Shared ownership across multiple owners           |
| `@gc`     | Complex data structures, caches, long-lived data   |

## FFI Cost Attributes

| Attribute               | Description                     |
|-------------------------|---------------------------------|
| `@cost(zero)`           | No cost (pure computation)      |
| `@cost(alloc)`          | Allocates memory                |
| `@cost(indirection)`    | Indirect call (e.g., callback)  |

```aura
@cost(zero)
extern function fast_add(a: i32, b: i32) -> i32

@cost(alloc)
extern function malloc(size: i64) -> pointer
```

## Statement Forms (without `@`)

`inline`, `noinline`, and `constexpr` can be used without `@` as statement prefixes:

```aura
inline fn(x) -> x * 2         # force inline
noinline fn(x) -> x * 2       # prevent inlining
constexpr val = compile_fn()  # compile-time constant
```

## Inline Attribute Forms

Attributes can appear on their own line before the declaration, or inline:

```aura
# Separate line:
@performance
function fast_sum(n)
    return n * (n + 1) / 2

# Inline:
@performance function renderer()
    # body
```

Both forms are equivalent. Use separate line for readability with long attribute lists.

---

**Next:** [Built-in Functions](15-builtins.md)
