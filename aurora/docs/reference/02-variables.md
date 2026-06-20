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

> **Note:** All variables are immutable by default. Use `mutable` only when you need reassignment.

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

See [Memory Model](09-memory.md) for detailed semantics.

```aura
source = [1, 2, 3]
dest = move source         # transfer ownership (source invalid)

y = copy(x)                # explicit deep copy
shared a = original        # shared reference-counted handle
weak w = original          # non-owning weak reference
borrow z = x               # temporary immutable reference
reference r = a            # typed reference
pointer p = x              # raw pointer
drop x                     # explicit destructor
free ptr                   # free memory (low-level)
delete ptr                 # manual deallocation (unsafe)
```

Statement and expression forms:

```aura
move x                     # statement form
y = move(x)                # expression form

copy x                     # statement
y = copy(x)                # expression

reference x                # statement
r = reference(a)           # expression

pointer x                  # statement
p = pointer(x)             # expression
```

> **Tip:** Use `move` to transfer ownership of large data structures without copying. Use `borrow` for temporary read-only access.

---

**Next:** [Types](03-types.md)
