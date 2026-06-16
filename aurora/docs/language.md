# Aurora Language Reference

Aurora is a whitespace-sensitive, compiled language with built-in support for polyglot FFI (Python, npm, Rust), OOP, functional patterns, async/concurrency, and domain-specific frameworks for UI, backend, game, and AI/ML workloads. It compiles to native code via LLVM.

**Key characteristics:**
- Indentation-based blocks (no braces required, but brace blocks also supported)
- Dynamic typing with optional type annotations
- Multiple memory management strategies (stack, arena, RAII, ARC, GC)
- First-class async via fibers, channels, and event bus
- Built-in UI, backend, game engine, and AI/ML frameworks

---

## Reference (by topic)

| # | Topic | File |
|---|-------|------|
| 1 | **Syntax Basics** — whitespace, comments, literals, identifiers | [reference/01-syntax-basics.md](reference/01-syntax-basics.md) |
| 2 | **Variables & Assignment** — assignment, compound ops, qualifiers, memory annotations | [reference/02-variables.md](reference/02-variables.md) |
| 3 | **Types** — primitives, compounds, structs, enums, type aliases | [reference/03-types.md](reference/03-types.md) |
| 4 | **Operators** — arithmetic, comparison, logical, bitwise, range, precedence | [reference/04-operators.md](reference/04-operators.md) |
| 5 | **Control Flow** — if/elseif/else, while, for, loop, repeat/until, match/switch | [reference/05-control-flow.md](reference/05-control-flow.md) |
| 6 | **Functions** — function, fn, lambda, callback, return, yield, attributes | [reference/06-functions.md](reference/06-functions.md) |
| 7 | **OOP** — classes, inheritance, abstract/final, interfaces, visibility | [reference/07-oop.md](reference/07-oop.md) |
| 8 | **Structs, Enums, Unions** — declarations, construction, extern unions | [reference/08-structs-enums-unions.md](reference/08-structs-enums-unions.md) |
| 9 | **Memory Model** — stack/arena/RAII/ARC/GC, move/copy/shared/weak/borrow, safe/unsafe | [reference/09-memory.md](reference/09-memory.md) |
| 10 | **Error Handling** — try/catch/finally, throw, ensure, panic | [reference/10-error-handling.md](reference/10-error-handling.md) |
| 11 | **Async & Concurrency** — async/await/spawn, parallel, channels, fibers, event bus | [reference/11-async-concurrency.md](reference/11-async-concurrency.md) |
| 12 | **Modules & Imports** — import, from, module, package, namespace, alias | [reference/12-modules-imports.md](reference/12-modules-imports.md) |
| 13 | **FFI** — extern functions/structs/unions, calling conventions, callbacks | [reference/13-ffi.md](reference/13-ffi.md) |
| 14 | **Attributes** — @performance, @inline, @constexpr, memory attrs, @cost | [reference/14-attributes.md](reference/14-attributes.md) |
| 15 | **Built-in Functions** — I/O, string, math, collections, file, time, HTTP, JSON, etc. | [reference/15-builtins.md](reference/15-builtins.md) |
| 16 | **Standard Library** — libc/ bindings (stdio, stdlib, string, collections, Win32, GUI, Torch) | [reference/16-stdlib.md](reference/16-stdlib.md) |
| 17 | **Domain Frameworks** — UI, Backend, Game Engine, AI/ML keywords | [reference/17-frameworks.md](reference/17-frameworks.md) |

---

## Quick Reference

### Syntax Variants

Most constructs support multiple syntax forms:

| Construct | Forms |
|-----------|-------|
| **if** | `if cond: body()`, `if cond\n    body()`, `if cond do body() end`, `if cond { body() }` |
| **function** | `function name(params) body`, `function name(params):\n    body`, `function name(params) { body }` |
| **lambda** | `lambda(x) expr`, `fn(x) -> expr`, `fn(x) expr end` |
| **match** | `match x / case val`, `match x: / case val:`, `match x / pattern -> expr` |
| **for** | `for i in 5`, `for i in 0..10`, `for i in 0..=10`, `for i in 0, 5`, `for x in array` |
| **try** | `try:\n    body\ncatch err:\n    handler`, `try(expr)` as expression |
| **extern** | `extern function name(params) -> ret`, `extern "lib" function...`, `extern "stdcall" "lib" function...` |

### Examples

```aura
# if/elseif/else
if x > 0
    output("positive")
elseif x == 0
    output("zero")
else
    output("negative")

# loop with break/continue
for i in 100
    if i == 5
        break
    if i % 2 == 0
        continue
    output(i)

# function with lambda
function map(arr, fn)
    result = []
    for item in arr
        result = push(result, fn(item))
    return result

doubled = map([1, 2, 3], lambda(x) x * 2)

# class with inheritance
class Animal
    name = ""
    function speak()
        output(self.name + " makes a sound")

class Dog extends Animal
    function speak()
        output(self.name + " barks")

# async/spawn
task = spawn fetch_data("https://example.com")
result = await task

# memory annotation
@arc shared_data = [1, 2, 3]
@arena batch = load_large_dataset()
```

---

## Grammar Summary

### Keywords

```
and  or  not  xor  in  is  as
if  elseif  else  for  while  loop  repeat  until
break  continue  skip  return  yield  pass  end
function  fn  lambda  callback
class  struct  enum  interface  extends  implements
abstract  final  public  private  protected
self  new  this
try  catch  finally  throw  ensure  panic
async  await  spawn  wait  parallel  signal  emit  event
thread  fiber
import  from  module  package  namespace  alias  global  outer
extern  constant  mutable  union
move  copy  shared  weak  borrow  reference  pointer  drop  free  delete
safe  unsafe
type  using
constexpr  inline  noinline
true  false  null  void
typeof  sizeof  convert  clone
time  random  sleep  debug  log
```

### Operators (by precedence)

| Precedence | Operators |
|------------|-----------|
| 1 (lowest) | `and`, `or` |
| 2 | `==`, `!=`, `<`, `>`, `<=`, `>=`, `equals`, `in` |
| 3 | `|` |
| 4 | `^`, `xor` |
| 5 | `&` |
| 6 | `<<`, `>>` |
| 7 | `..`, `..=` (range) |
| 8 | `+`, `-` (binary) |
| 9 | `*`, `/`, `//`, `%` |
| 10 | unary `-`, `not`, `**` |

### Compound assignment: `+=`, `-=`, `*=`, `/=`

---

## Compiler & Build

```bash
.\aurora.bat file.aura                   # compile and run
.\aurora.bat file.aura --emit-obj        # compile to object file
.\aurora.bat build build                 # cmake build
.\aurora.bat run tests/my_test.aura      # run a specific test
```

Output modes: JIT execution, object file emission, linked executable.
