# Aurora Language Reference

Aurora is a memory-safe compiled programming language with intelligent automatic memory management, built on LLVM.

## Syntax

### Variables
```
name = "Alice"
age = 30
pi = 3.14
```

### Control Flow
```
if x > 10
    output("big")
elseif x > 5
    output("medium")
else
    output("small")

while i < 10
    i = i + 1

for i in 10
    output(i)
```

### Functions
```
function add(a, b)
    return a + b
```

### Classes & OOP
```
class Person
    name = "unknown"
    function greet()
        output("Hello, " + self.name)

p = Person("Alice")
p.greet()
```

### Inheritance
```
class Student extends Person
    school = "unknown"
```

### Arrays
```
arr = [1, 2, 3]
output(arr[0])
arr[0] = 99
```

### Memory Management
| Keyword | Description |
|---------|-------------|
| `move x` | Transfer ownership |
| `copy x` | Deep copy |
| `shared x` | Ref-counted handle |
| `weak x` | Weak non-owning reference |
| `borrow x` | Immutable borrow |
| `drop x` | Explicit destructor |
| `free x` | Raw memory release |

### Allocation Attributes
| Attribute | Strategy |
|-----------|----------|
| `@stack` | Force stack allocation |
| `@arena` | Arena (bump) allocator |
| `@raii` | RAII with destructor |
| `@arc` | Reference counting |
| `@gc` | Garbage collection |
| `@performance` | Optimized mode (no GC) |

### Exceptions
```
try
    result = risky_operation()
catch(err)
    output("caught: " + err)
finally
    cleanup()
```

## Building

```
.\aurora.bat file.aura
```
