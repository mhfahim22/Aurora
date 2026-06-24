# Memory Model

Aurora provides multiple memory management strategies. Default depends on context; specify with attributes.

## Allocation Attributes

| Attribute  | Strategy | Description                            | Best For            |
|------------|----------|----------------------------------------|---------------------|
| `@stack`   | Stack    | Fast, no deallocation cost             | Small, short-lived  |
| `@arena`   | Arena    | Bulk-free, batch workloads             | Temporary data      |
| `@raii`    | RAII     | Deterministic destructor on scope exit | Resources (files)   |
| `@arc`     | ARC      | Automatic reference counting           | Shared data         |
| `@gc`      | GC       | Tracing garbage collector              | Long-lived data     |

```aura
@stack a = 100                  # stack-allocated integer
@arena data = [1, 2, 3, 4, 5]  # arena-allocated array
@raii file = open("test.txt")  # RAII-managed resource
@arc shared = [1, 2, 3]        # reference-counted array
@gc big_data = load_large()    # garbage-collected data
```

## Choosing a Strategy

| Use Case                          | Recommended |
|-----------------------------------|-------------|
| Local variables, small data       | `@stack`    |
| Temporary allocations in a batch  | `@arena`    |
| File handles, locks, resources    | `@raii`     |
| Shared across threads/modules     | `@arc`      |
| Complex data structures, caches   | `@gc`       |

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
move x                          # x's ownership transferred (x invalid)
copy y                          # explicit copy of y
shared a                        # make a shared/ref-counted
weak w                          # create weak reference
borrow r                        # borrow reference
reference r                     # typed reference
pointer p                       # raw pointer
drop x                          # call destructor
free ptr                        # free memory
delete ptr                      # unsafe deallocation
```

### Expression / Function-call Forms

```aura
dest = move(source)             # transfer ownership
y = copy(x)                     # deep copy
shared a = original             # shared reference
weak w = original               # weak reference
borrow z = x                    # borrow
r = reference(a)                # typed reference
p = pointer(x)                  # raw pointer
```

## Safe / Unsafe Blocks

```aura
safe
    # memory-safe operations only

unsafe
    # low-level operations (pointer arithmetic, manual free)
```

> **Guideline:** Prefer `safe` blocks by default. Use `unsafe` only when interfacing with raw memory or FFI code. Most Aurora code never needs `unsafe`.

---

**Next:** [Error Handling](10-error-handling.md)
