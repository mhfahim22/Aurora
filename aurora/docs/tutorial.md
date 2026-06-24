# Getting Started with Aurora

A step-by-step tutorial for new Aurora users.

---

## Prerequisites

- **C++23 compiler** (MSVC 2022+, Clang 18+, GCC 14+)
- **LLVM/Clang 19.x** (with `llvm-config`)
- **CMake 3.20+**
- **Python 3.8+** (for PyPI bridge)
- **Node.js 18+** (for npm bridge)
- **Rust 1.70+** (for Cargo bridge)

---

## Step 1: Install Aurora

### Option A: Download Release

Download the latest release from GitHub.

### Option B: Build from Source

```bash
git clone https://github.com/your-org/aurora.git
cd aurora
cmake -B build
cmake --build build --config Release
```

Verify the build:

```bash
.\aurora.bat version
```

---

## Step 2: Hello, World!

Create a file `hello.aura`:

```aura
output("Hello, Aurora!")
name = "World"
output("Hello, " + name)
```

Run it:

```bash
.\aurora.bat hello.aura
```

Expected output:

```
Hello, Aurora!
Hello, World
```

---

## Step 3: Variables & Operations

```aura
# Numbers
count = 42
pi = 3.14
result = count + pi * 2

output("count: " + count)
output("pi: " + pi)
output("result: " + result)

# Strings
greeting = "Hello"
target = "Aurora"
message = greeting + ", " + target + "!"
output(message)

# Booleans
is_ready = true
is_done = false

if is_ready and not is_done
    output("Ready to go!")
```

---

## Step 4: Control Flow

```aura
# If / Else
score = 85
if score >= 90
    output("A")
elseif score >= 80
    output("B")
else
    output("C")

# While loop
i = 0
while i < 5
    output("i = " + i)
    i += 1

# For loop
for i in 5
    output("item " + i)

# For over array
fruits = ["apple", "banana", "cherry"]
for fruit in fruits
    output(fruit)
```

---

## Step 5: Functions

```aura
function add(a, b)
    return a + b

function greet(name)
    output("Hello, " + name)

sum = add(3, 4)
output("3 + 4 = " + sum)

greet("Alice")

# Recursion
function factorial(n)
    if n <= 1
        return 1
    return n * factorial(n - 1)

output("5! = " + factorial(5))

# Lambda
double = lambda(x) x * 2
output(double(10))
```

---

## Step 6: Arrays

```aura
numbers = [1, 2, 3, 4, 5]
output(numbers[0])    # 1
output(numbers[4])    # 5

numbers[2] = 99
output(numbers[2])    # 99

total = 0
for n in numbers
    total += n
output("sum: " + total)
```

---

## Step 7: Classes & OOP

```aura
class Animal
    name = "unknown"
    function speak()
        output(self.name + " makes a sound")

class Dog extends Animal
    breed = "unknown"
    function speak()
        output(self.name + " barks")

a = Animal("Generic")
a.speak()

d = Dog("Rex", "German Shepherd")
d.speak()
output(d.breed)
```

---

## Step 8: Memory Management

```aura
# Arena allocation (good for batch work)
@arena large_data = [1, 2, 3, 4, 5]

# Shared reference counting
shared counter = 0
shared copy = counter   # reference count increases

# Move ownership
source = [1, 2, 3]
dest = move source      # dest now owns the data
# output(source[0])     # ERROR: source is invalid

# Explicit copy
a = [1, 2, 3]
b = copy a              # deep copy
```

---

## Step 9: Error Handling

```aura
try
    result = risky_operation()
    output(result)
catch error
    output("Error: " + error)
finally
    output("Cleanup complete")
```

---

## Step 10: Using Imports

```aura
import "stdio"

function main()
    printf("Hello from stdio!\n")
    name = printf("Enter your name: ")
    printf("Hi, %s\n", name)
```

---

## Step 11: Using Bridges

### PyPI (Python)

```aura
import "stdio"

function main()
    md = markdown_import()
    text = markdown_str("# Hello Aurora")
    html = markdown_call1(md, "markdown", text)
    if html != 0
        printf("HTML: %s\n", markdown_to_cstr(html))
        markdown_free(html)
```

### npm (JavaScript)

```aura
import "stdio"

function main()
    mod = moment_import()
    now = moment_call0(mod, "format")
    printf("Current time: %s\n", moment_to_cstr(now))
    moment_free(now)
    moment_free(mod)
```

### Cargo (Rust)

```aura
import "stdio"

function main()
    mod = hello_rust_import()
    greeting = hello_rust_call1(mod, "greet", hello_rust_str("Aurora"))
    printf("Greeting: %s\n", hello_rust_to_cstr(greeting))
    hello_rust_free(greeting)
```

---

## Step 12: Framework Quick Start

### UI Component

```aura
component HelloButton
    state count = 0
    function handle_click()
        self.count += 1
        render()

    function render()
        output("Clicked " + self.count + " times")

HelloButton()
```

### Backend Server

```aura
server my_api
    route "/hello"
        response("Hello, World!")

    route "/user/:name"
        response("Hello, " + request.params.name)

    middleware logger
        output("Request: " + request.method + " " + request.path)
        next()
```

### Game Entity

```aura
entity Player
    sprite = "@"
    speed = 5.0
    health = 100

    function update(dt)
        if input.key_down("left")
            self.x -= self.speed * dt
        if input.key_down("right")
            self.x += self.speed * dt

    function on_collide(other)
        if other.type == "enemy"
            self.health -= 10
            if self.health <= 0
                scene.remove(self)
```

---

## Step 13: What's Next?

- Read the full [Language Reference](language.md) for complete syntax details
- Explore `aurora/examples/` for more code samples
- Check `aurora/libc/` for available standard library modules
- Read `bridge_pypi.md`, `bridge_security.md` for bridge details
- Build a real project with `voss init`
