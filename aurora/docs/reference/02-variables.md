# Variables & Assignment

## Simple Assignment

```aura
name = "Alice"          # string
count = 42              # integer
pi = 3.14               # float
flag = true             # boolean
data = [1, 2, 3]        # array
person = Person("Alice") # object construction
```

## Compound Assignment

```aura
x = 10
x += 5                    # x = 15
x -= 3                    # x = 12
x *= 2                    # x = 24
x /= 4                    # x = 6
```

All desugared to `x = x OP expr`.

## Qualifiers

```aura
constant x = 42           # immutable binding
mutable y = 10            # explicitly mutable
y = 20                    # allowed
```

## Field & Index Assignment

```aura
p.name = "alice"          # field assignment
arr[0] = 99               # index assignment
```

## Memory Annotations

See [Memory Model](09-memory.md) for full details.

```aura
@stack a = 100            # stack-allocated
@arena x = 50             # arena-allocated
@raii data = 42           # RAII-managed
@arc shared_data = 99     # reference-counted
@gc data = 999            # garbage-collected
```

## Ownership Keywords

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

Statement and expression forms for ownership keywords:

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
