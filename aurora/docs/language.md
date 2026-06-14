# Aurora Language Reference

## 1. Overview

Aurora is a whitespace-sensitive, compiled language with built-in support for polyglot FFI (Python, npm, Rust), OOP, functional patterns, async/concurrency, and domain-specific frameworks for UI, backend, game, and AI/ML workloads. It compiles to native code via LLVM.

Key characteristics:
- Indentation-based blocks (no braces)
- Dynamic typing with optional type annotations
- Multiple memory management strategies (stack, arena, RAII, ARC, GC)
- First-class async via fibers, channels, and event bus
- Built-in UI, backend, game engine, and AI/ML frameworks

---

## 2. Syntax

### 2.1 Whitespace & Block Structure

Blocks are defined by indentation. Use 4 spaces per level.

```aura
function greet(name)
    output("Hello, " + name)    # body is indented
    return 0                    # still in function body

output("done")                  # back to top-level
```

Valid tokens: `Indent` (increased indent), `Dedent` (decreased indent), `Newline` (end of line).

### 2.2 Comments

```aura
# Line comment
```

### 2.3 Literals

```aura
42          # integer
3.14        # float
"hello"     # string
'c'         # character literal
true        # boolean
false       # boolean
null        # null value
[1, 2, 3]   # array literal
```

### 2.4 Identifiers

Names must start with a letter or underscore, followed by letters, digits, or underscores. Case-sensitive.

```
myVar  _private  fib_42  ClassName
```

---

## 3. Variables & Assignment

```aura
name = "Alice"            # simple assignment
count = 42                # integer
pi = 3.14                 # float
flag = true               # boolean
data = [1, 2, 3]          # array
person = Person("Alice")  # object construction
```

### Qualifiers

```aura
constant x = 42           # immutable binding
mutable y = 10            # explicitly mutable
y = 20                    # allowed
```

### Compound Assignment

```
+=  -=  *=  /=
```

```aura
x = 10
x += 5                    # x = 15
x *= 2                    # x = 30
```

---

## 4. Types

### 4.1 Primitive Types

| Type | Aliases | Description |
|------|---------|-------------|
| `int`  | `i64`, `Int`, `u64`, `i32`, `u32`, `i16`, `i8`, `char` | Integer |
| `float` | `f64`, `Float`, `double`, `f32` | Floating-point |
| `string` | `String`, `str` | UTF-8 string |
| `bool` | `Bool` | Boolean (`true`, `false`) |
| `void` | `Void` | No value |
| `ptr<T>` | `pointer`, `Pointer`, `void*` | Raw pointer |
| `cstring` | `char*` | C-style null-terminated string |

### 4.2 Compound Types

| Type | Description |
|------|-------------|
| `array` | Dynamic array `[1, 2, 3]` |
| `struct` | Named field collection |
| `class` | Object with methods and inheritance |
| `enum` | Named variant constants |
| `list` | Generic list |
| `map` | Key-value dictionary |
| `set` | Unique element collection |
| `stack` | LIFO stack |
| `queue` | FIFO queue |
| `vector` | 3D vector (x, y, z) |
| `tuple` | Fixed-size heterogeneous collection |
| `json` | JSON value |
| `function` | Callable |

### 4.3 Type Aliases

```aura
type Age = int
type Name = string
type User = struct { name: Name, age: Age }
```

---

## 5. Operators

### 5.1 Arithmetic

| Op | Description |
|----|-------------|
| `+` | Addition / string concatenation |
| `-` | Subtraction (binary) / Negate (unary) |
| `*` | Multiplication |
| `/` | Float division |
| `//` | Integer division |
| `%` | Modulo |
| `**` | Power |

### 5.2 Comparison

| Op | Description |
|----|-------------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |
| `equals` | Structural equality |

### 5.3 Logical

| Op | Description |
|----|-------------|
| `and` | Logical AND |
| `or` | Logical OR |
| `not` | Logical NOT |
| `xor` | Logical XOR |
| `in` | Membership test |

### 5.4 Bitwise

| Op | Description |
|----|-------------|
| `&` | Bitwise AND |
| `|` | Bitwise OR |
| `^` | Bitwise XOR |
| `~` | Bitwise NOT |
| `<<` | Left shift |
| `>>` | Right shift |

### 5.5 Other

| Op | Description |
|----|-------------|
| `.` | Member access / method call |
| `..` | Range (exclusive) |
| `..=` | Range (inclusive) |
| `...` | Spread / varargs |
| `->` | Pointer member access |
| `:` | Type annotation separator |
| `@` | Attribute prefix |

### 5.6 Precedence (lowest to highest)

1. `==`, `!=`, `<`, `>`, `<=`, `>=`, `equals`, `and`, `or`, `in`
2. `^`, `&`, `|`, `xor`
3. `..`, `..=`
4. `+`, `-`
5. `*`, `/`, `//`, `%`, `**`
6. Unary: `-`, `not`

---

## 6. Control Flow

### 6.1 If / Elseif / Else

```aura
if condition
    body()
elseif other_condition
    other_body()
else
    fallback()
```

### 6.2 While

```aura
while condition
    body()
```

### 6.3 For

```aura
for i in 10              # 0..9 (exclusive upper bound)
    output(i)

for i in 0..10           # explicit range
    output(i)

for i in 0..=10          # inclusive range
    output(i)

for item in array
    output(item)
```

### 6.4 Loop / Repeat / Until

```aura
loop                      # infinite loop
    if condition
        break

repeat                    # do-while style
    body()
until condition
```

### 6.5 Break / Continue / Skip

```aura
for i in 100
    if i == 5
        break             # exit loop
    if i % 2 == 0
        continue          # next iteration
    output(i)
```

### 6.6 Match

```aura
match value
    case 1
        output("one")
    case 2, 3
        output("two or three")
    case _
        output("other")
```

---

## 7. Functions

### 7.1 Function Definition

```aura
function add(a, b)
    return a + b

function greet(name)
    output("Hello, " + name)
    # implicit return of last expression value
```

Functions can be recursive:

```aura
function fib(n)
    if n <= 1
        return n
    return fib(n - 1) + fib(n - 2)
```

### 7.2 Lambda

```aura
square = lambda(x) x * x
result = square(5)      # 25

doubled = map(array, lambda(x) x * 2)
```

### 7.3 Attributes on Functions

```aura
@performance function fast_sum(n)
    total = 0
    for i in n
        total += i
    return total

@inline function small_helper(x)
    return x + 1

@constexpr function compile_time_val()
    return 42
```

---

## 8. Object-Oriented Programming

### 8.1 Class Definition

```aura
class Person
    name = "unknown"
    age = 0

    function greet()
        output("Hi, I'm " + self.name)

    function have_birthday()
        self.age += 1
```

### 8.2 Construction

```aura
p = Person("Alice", 30)     # positional args match field order
p.greet()
```

### 8.3 Inheritance

```aura
class Student extends Person
    school = "unknown"

    function study()
        output(self.name + " is studying")

s = Student("Bob", 20, "Aurora High")
s.greet()                    # inherited method
s.study()
```

### 8.4 Visibility

```aura
class Account
    private balance = 0      # only accessible inside class
    public owner = ""        # accessible externally

    function deposit(amount)
        if amount > 0
            self.balance += amount
```

### 8.5 Abstract & Final

```aura
abstract class Shape
    function area()          # abstract method (no body)

    function describe()      # concrete method
        output("This is a shape")

final class Circle extends Shape
    radius = 0
    function area()
        return 3.14159 * self.radius * self.radius
```

### 8.6 Interfaces

```aura
interface Drawable
    function draw()

class Sprite implements Drawable
    function draw()
        output("Drawing sprite")
```

---

## 9. Structs, Enums, Unions

### 9.1 Struct

```aura
struct Point
    x
    y

p = Point(10, 20)
output(p.x)         # 10
```

### 9.2 Enum

```aura
enum Color
    Red
    Green
    Blue

c = Color.Red
```

### 9.3 Union (extern)

```aura
extern union Data
    i int
    f float
```

---

## 10. Memory Model

Aurora provides multiple memory management strategies. The default depends on context; you can specify with attributes.

### 10.1 Allocation Attributes

| Attribute | Strategy | Description |
|-----------|----------|-------------|
| `@stack` | Stack allocation | Fast, no deallocation cost, limited lifetime |
| `@arena` | Arena allocation | Bulk-free, good for batch workloads |
| `@raii` | RAII | Deterministic destructor on scope exit |
| `@arc` | Reference counting | Automatic shared ownership |
| `@gc` | Garbage collection | Tracing collector, no manual management |

```aura
@arena data = [1, 2, 3, 4, 5]
@arc shared_data = [1, 2, 3]
```

### 10.2 Ownership Keywords

| Keyword | Description |
|---------|-------------|
| `move` | Transfer ownership; source becomes invalid |
| `copy` | Explicit deep copy |
| `shared` | Create reference-counted handle |
| `weak` | Non-owning weak reference |
| `borrow` | Temporary immutable reference |
| `reference` | Create a typed reference |
| `pointer` | Create a raw pointer |
| `drop` | Explicit destructor call |
| `free` | Free memory (low-level) |
| `delete` | Manual deallocation (unsafe) |

```aura
source = [1, 2, 3]
dest = move source          # dest owns, source invalid

shared x = y                # ref-counted handle
borrow z = x                # temporary reference
```

### 10.3 Safe / Unsafe

```aura
safe
    # memory-safe operations

unsafe
    # low-level operations (pointer arithmetic, manual free)
```

---

## 11. Exception Handling

```aura
try
    result = risky_operation()
    output(result)
catch error
    output("Caught: " + error)
finally
    cleanup()
```

```aura
throw "something went wrong"
ensure resource             # run cleanup on scope exit
panic "fatal error"         # abort execution
```

---

## 12. Async & Concurrency

### 12.1 Async / Await

```aura
async function fetch_data(url)
    result = http_get(url)
    return result

data = await fetch_data("https://example.com")
```

### 12.2 Spawn / Parallel

```aura
task = spawn long_running()
result = wait task       # block until done

parallel
    task1()
    task2()
    task3()
```

### 12.3 Channels

```aura
ch = chan()               # create channel
send(ch, "hello")
msg = recv(ch)            # blocking receive
```

### 12.4 Fibers

```aura
f = fiber_create(my_function)
fiber_resume(f)
result = fiber_get_result(f)
fiber_destroy(f)
```

### 12.5 Event Bus

```aura
event_on("user.login", handler)
event_emit("user.login", data)
event_off("user.login", handler)
```

---

## 13. Modules & Imports

### 13.1 Import Forms

```aura
import "path/to/file.aufra"    # file path
import stdio                   # library module
import kernel32                # FFI module
import MrCode                  # third-party package
from "utils" import helper     # selective import
```

### 13.2 Module Resolution

Search order:
1. Exact file path
2. Path + `.aufra` extension
3. Search path / name
4. Search path / name.aufra
5. Search path / lib / name
6. Search path / lib / name.aufra

### 13.3 Module / Package Declarations

```aura
module com.example.myapp
package myapp
namespace Graphics:
    function render()
        output("rendering")
alias math = Aurora.Math
```

---

## 14. FFI (Foreign Function Interface)

### 14.1 External Functions

```aura
extern function MessageBoxA(hwnd, text, caption, type)
extern function printf(format, ...) : cdecl
extern function malloc(size) from "libc"
```

Supported calling conventions: default (platform), `cdecl`, `stdcall`.

### 14.2 External Structs / Unions

```aura
extern struct Point
    x int
    y int

extern union Data
    i int
    f float
```

---

## 15. Packages

### 15.1 Package Manifest (`aurora.pkg`)

```json
{
    "name": "myapp",
    "version": "1.0.0",
    "author": "Alice",
    "description": "My Aurora application",
    "entry": "main.aufra",
    "depends": ["MrCode >= 2.0"]
}
```

### 15.2 Package Commands

```aura
import package_manager
install("my-package")
update("my-package")
results = search("keyword")
```

---

## 16. Attributes (Full Reference)

| Attribute | Scope | Description |
|-----------|-------|-------------|
| `@performance` | function | Optimized function mode |
| `@inline` | function | Inline hint |
| `@noinline` | function | No-inline hint |
| `@constexpr` | function/variable | Compile-time constant |
| `@stack` | variable | Force stack allocation |
| `@arena` | variable | Force arena allocation |
| `@raii` | variable | Force RAII allocation |
| `@arc` | variable | Force ARC allocation |
| `@gc` | variable | Force GC allocation |
| `@cost(zero\|alloc\|indirection)` | function | FFI cost annotation |

---

## 17. Domain-Specific Frameworks Overview

### 17.1 UI Framework

| Keyword | Description |
|---------|-------------|
| `component` | Declare a UI component |
| `state` | Component local state |
| `properties` | Component input properties |
| `render` | Define render function |
| `style` | Declare style rules |
| `theme` | Declare theme |
| `route` | Register a route |
| `page` | Declare a page |
| `layout` | Declare a layout |
| `animate` | Declare an animation |
| `transition` | Declare a transition |

### 17.2 Backend Framework

| Keyword | Description |
|---------|-------------|
| `server` | HTTP server |
| `request` | HTTP request |
| `response` | HTTP response |
| `api` | API endpoint |
| `middleware` | Middleware handler |
| `database` | Database connection |
| `query` | Database query |
| `model` | Data model |
| `cache` | Cache entry |
| `session` | User session |
| `token` | Auth token |
| `auth` | Authentication |

### 17.3 Game Engine

| Keyword | Description |
|---------|-------------|
| `scene` | Game scene |
| `entity` | Game entity |
| `object` | Game object |
| `sprite` | Sprite definition |
| `camera` | Camera |
| `physics` | Physics body |
| `collision` | Collision event |
| `audio` | Audio source |
| `animation` | Animation clip |
| `input` | Input handler |
| `update` | Update callback |
| `tick` | Tick callback |

### 17.4 AI/ML

| Keyword | Description |
|---------|-------------|
| `ai` | AI context |
| `tensor` | Tensor literal |
| `train` | Train model |
| `predict` | Make prediction |
| `neural` | Neural network |

---

## 18. Built-in Functions Summary

### I/O
`output`, `outputln`, `outputN`, `outputf`, `input`, `ask`, `debug`, `panic`

### String
`len`, `upper`, `lower`, `trim`, `replace`, `split`, `join`, `has`, `starts`, `ends`, `reverse`, `strlen`, `strcat`, `substr`, `index`

### Math
`abs`, `sqrt`, `floor`, `ceil`, `round`, `pow`, `clamp`, `rand`, `sum`, `min`, `max`

### Type Conversion
`str`, `int`, `float`, `bool`, `char`, `convert`, `clone`, `typeof`, `sizeof`

### Collections
`push`, `pop`, `insert`, `remove`, `clear`, `sort`, `unique`, `map`, `filter`, `reduce`, `find`, `any`, `all`, `range`

### Runtime Collections
`list_get`, `list_len`, `list_push`, `list_free`, `map_get`, `map_has`, `map_set`, `map_free`, `set_add`, `set_has`, `set_free`, `stack_push`, `stack_pop`, `stack_empty`, `stack_free`, `queue_enqueue`, `queue_dequeue`, `queue_empty`, `queue_free`, `vector_x`, `vector_y`, `vector_z`, `json_free`

### File
`read`, `write`, `append`, `exists`, `delete`, `copy`, `move`, `download`

### Path
`cwd`, `cd`, `path`, `name`, `ext`, `dir`

### Time
`now`, `sleep`, `stamp`

### JSON
`encode`, `decode`

### HTTP
`get`, `post`

### OS/Environment
`os`, `cpu`, `mem`, `env`, `run`, `exit`

### Async
`spawn`, `await`, `chan`, `send`, `recv`

### Event
`event_on`, `event_off`, `event_emit`

### Fiber
`fiber_create`, `fiber_resume`, `fiber_yield`, `fiber_is_done`, `fiber_get_result`, `fiber_destroy`

### Performance
`measure`, `bench`, `profile`, `trace`

### Reflection
`fields`, `methods`

### AI/ML
`csv`, `data`, `clean`, `normalize`, `standard`, `shuffle`, `split_data`, `model_create`, `model_save`, `model_load`, `set_loss`, `set_optimizer`, `set_lr`, `set_batch_size`, `set_epochs`, `set_validation_split`, `set_verbose`, `set_early_stop`, `dense`, `conv`, `lstm`, `gru`, `dropout`, `batchnorm`, `attention`, `transformer`, `embedding`, `layernorm`, `add`, `train`, `fit`, `test`, `predict`, `retrain`

### Backend
`route_group`, `middleware`, `next`, `rate_limit`, `cors`, `csrf`, `session`, `session_get`, `session_set`, `session_delete`, `cookie_get`, `cookie_set`, `cookie_delete`, `proxy`, `reverse_proxy`, `stream`, `stream_file`, `sse`, `webhook`, `health`, `metrics`, `trace_id`, `request_id`, `audit`, `lock`, `unlock`, `atomic`, `retry`, `timeout`, `circuit_breaker`, `pool`, `worker_pool`, `batch`, `paginate`, `index`, `migrate`, `seed`, `model`, `schema`, `validate`, `sanitize`, `throttle`, `debounce`, `sign`, `verify`, `secret`, `vault`, `compress`, `decompress`, `serialize`, `deserialize`, `event`, `emit`, `listen`, `publish`, `subscribe`, `rpc`, `discover`, `cluster`, `node_id`, `leader`, `shard`, `replica`, `backup`, `restore`, `monitor`, `profile_request`, `memory_snapshot`, `gc_collect`, `hot_reload`, `plugin`, `feature_flag`, `tenant`, `tenant_context`, `geoip`, `captcha_verify`, `payment`, `invoice`, `analytics`

### Search / AI Agents
`search_engine`, `vector_search`, `semantic_search`, `embed_store`, `embed_query`, `ai_agent`, `tool`, `workflow`, `pipeline`, `step`

### Language AI
`ai`, `chat`, `embed`, `classify`, `translate`, `summarize`, `code`

---

## 19. Standard Library (`libc/`)

### stdio.auf
`printf`, `fprintf`, `fopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fputs`, `fflush`, `fseek`, `ftell`, `remove`, `rename`, `puts`, `putchar`, `getchar`

### stdlib.auf
`malloc`, `calloc`, `realloc`, `free`, `atoi`, `atol`, `atoll`, `atof`, `strtol`, `rand`, `srand`, `exit`, `abort`, `atexit`, `abs`, `qsort`, `bsearch`, `getenv`, `system`

### string.auf
`len`, `strlen`, `strcat`, `substr`, `index`, `has`, `starts`, `ends`, `upper`, `lower`, `trim`, `replace`, `split`, `join`, `reverse`

### collections.auf
`list`, `map`, `set`, `stack`, `queue`, `vector`, `json`, `list_last`, `list_first`

### kernel32.auf (Windows)
`GetTickCount`, `Sleep`, `ExitProcess`, `LoadLibraryA/W`, `GetProcAddress`, file I/O, console, `VirtualAlloc/Free`, `HeapAlloc/Free`, environment, system info, timing, error handling, fibers

### user32.auf (Windows)
Message boxes, window management, window class registration, message loop, drawing, resources, timers, clipboard, dialogs

### gui.auf (Cross-platform)
`gui_window_new`, `gui_button_new`, `gui_label_new`, `gui_textbox_new`, `gui_listbox_new`, `gui_run`, `gui_quit`

### libtorch.auf (Cross-platform)
`tensor_1d/2d/3d`, `tensor_add/sub/mul/div/matmul`, `tensor_relu/sigmoid/tanh/softmax`, `module_load/forward/destroy`, `optimizer_sgd`

---

## 20. Compiler & Build

```bash
.\aurora.bat file.aufra                   # compile and run
.\aurora.bat file.aufra --emit-obj        # compile to object file
.\aurora.bat build build                 # cmake build
.\aurora.bat run tests/my_test.aufra      # run a specific test
```

Output modes: JIT execution, object file emission, linked executable.
