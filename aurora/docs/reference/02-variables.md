# Variables & Assignment

## Simple Assignment

Variables are **immutable by default** — reassignment is not allowed unless declared `mutable`.

```aura
name = "Alice"           # immutable string
count = 42               # immutable integer
pi = 3.14                # immutable float
flag = true              # immutable boolean
data = [1, 2, 3]         # immutable array
person = Person("Alice") # object construction

# This would ERROR:
# name = "Bob"           # cannot reassign immutable variable
```

## Compound Assignment

```aura
x = 10
x += 5                    # x = 15
x -= 3                    # x = 12
x *= 2                    # x = 24
x /= 4                    # x = 6
```

All desugared to `x = x OP expr`. Requires `x` to be `mutable`.

## Qualifiers

```aura
constant x = 42           # immutable binding (explicit)
mutable y = 10            # explicitly mutable
y = 20                    # allowed — y is mutable
```

> **Note:** All variables are immutable by default. Use `mutable` only when you need reassignment. `constant` is explicit but redundant.

## Field & Index Assignment

```aura
p.name = "alice"          # field assignment (requires mutable object)
arr[0] = 99               # index assignment (requires mutable array)
```

## Memory Annotations

Controls allocation strategy per variable. See [Memory Model](09-memory.md) for full details.

| Annotation  | Strategy | Best For             |
|-------------|----------|----------------------|
| `@stack`    | Stack    | Small, short-lived   |
| `@arena`    | Arena    | Batch workloads      |
| `@raii`     | RAII     | Resources (files)    |
| `@arc`      | ARC      | Shared data          |
| `@gc`       | GC       | Long-lived data      |

```aura
@stack a = 100            # stack-allocated
@arena x = 50             # arena-allocated
@raii data = 42           # RAII-managed
@arc shared_data = 99     # reference-counted
@gc data = 999            # garbage-collected
```

## Ownership Keywords

| Keyword     | Description                                       |
|-------------|---------------------------------------------------|
| `move`      | Transfer ownership; source becomes invalid         |
| `copy`      | Explicit deep copy                                 |
| `shared`    | Create reference-counted handle                    |
| `weak`      | Non-owning weak reference                          |
| `borrow`    | Temporary immutable reference                      |
| `reference` | Create a typed reference                           |
| `pointer`   | Create a raw pointer                               |
| `drop`      | Explicit destructor call                           |
| `free`      | Free memory (low-level)                            |
| `delete`    | Manual deallocation (unsafe)                       |

### Statement Forms

```aura
move x                     # transfer ownership
copy y                     # explicit copy
shared a                   # make shared/ref-counted
weak w                     # create weak reference
borrow r                   # borrow reference
reference r                # typed reference
pointer p                  # raw pointer
drop x                     # call destructor
free ptr                   # free memory
delete ptr                 # unsafe deallocation
```

### Expression / Function-call Forms

```aura
dest = move(source)        # transfer ownership
y = copy(x)                # deep copy
shared a = original        # shared reference
weak w = original          # weak reference
borrow z = x               # borrow
r = reference(a)           # typed reference
p = pointer(x)             # raw pointer
```

> **Tip:** Use `move` to transfer ownership of large data structures without copying. Use `borrow` for temporary read-only access.

### Assignment Form (shared / weak)

```aura
shared y = x               # y gets a shared (ref-counted) reference to x
weak w = x                 # w gets a weak (non-owning) reference to x
```

## `inline` / `noinline` / `constexpr` (statement forms)

```aura
inline fn(x) -> x * 2      # force inline
noinline fn(x) -> x * 2    # prevent inline
constexpr val = compile_fn()  # evaluate at compile time
```

These can also appear as `@` attributes: `@inline`, `@noinline`, `@constexpr`.

---

**Next:** [Types](03-types.md)
