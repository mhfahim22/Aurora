# Aurora

**A polyglot language with LLVM-native compilation — call Python, npm, Rust, C, C++, OpenGL, and more from a single language.**

Aurora is a general-purpose programming language and polyglot runtime. Write once, use everything: from PyPI packages and npm modules to Cargo crates, native C/C++ libraries, OpenGL graphics, database drivers, HTTP servers, and AI/ML models — all without glue code or FFI boilerplate.

```python
import libc:database
import pypi:requests

db = db_connect("postgresql://user:pass@localhost/mydb")
rows = db_query(db, "SELECT * FROM users")
for row in rows
    resp = requests_get("https://api.example.com/users/{row_get(row, "id")}")
    output(resp)
db_close(db)
```

---

## Features at a Glance

### Core Language
- **LLVM-native compilation** — Aurora IR → LLVM IR → optimized machine code (O3, znver3)
- **Memory management** — 4 strategies: arena, RAII, ARC, GC (per-variable `@arena`/`@raii`/`@rc`/`@gc`)
- **Async/await** — spawn, wait, parallel blocks, channels, fibers, event bus
- **OOP** — classes with inheritance, abstract, interfaces, encapsulation, polymorphism, vtables
- **Pattern matching** — match/switch with struct destructuring, array patterns, wildcards
- **Generics** — type parameters on functions, structs, and classes
- **Ownership system** — compile-time lifetime analysis with `@move`/`@copy` annotations

### Polyglot FFI
- **PyPI (Python)** — import Python packages via C bridge DLL (GIL-thread-safe)
- **npm (JavaScript/Node.js)** — QuickJS embedded engine for pure JS; subprocess bridge for native addons
- **Cargo (Rust)** — auto-generate Rust FFI bridges with cdylib discovery
- **Native C/C++** — `extern "library"` with lazy DLL loading; supports Win64/x64 calling convention

### Frameworks (built-in)
| Framework | What it does |
|-----------|-------------|
| `libc:backend` | HTTP server with router, middleware, sessions, auth, CORS, caching, WebSocket, file serving, hot-reload |
| `libc:ui` | Cross-platform GUI (Win32/X11/Cocoa) with components, layout, style, animation |
| `libc:game` | Game engine with entities, physics, sprites, audio, input, camera, animation |
| `libc:ai` | Tensor operations, model training/prediction, layers (dense, conv, LSTM, transformer, attention) |
| `libc:gl` | 3D graphics via OpenGL + GLFW — window creation, shaders, VAO/VBO, textures, immediate mode |

### Database & ORM
- **PostgreSQL** (`libc:pq`) — 85+ FFI externs via libpq; connection pooling, parameterized queries
- **MySQL** (`libc:mysql`) — 50+ FFI externs via libmysqlclient
- **Unified DB** (`libc:database`) — URL-based auto-detect (`postgresql://` / `mysql://`)
- **ORM** (`libc:orm`) — query builder with SELECT/INSERT/UPDATE/DELETE, joins, group_by
- **Model** (`libc:model`) — lightweight ActiveRecord-style `find`/`save`/`delete` with validation
- **Migrations** (`libc:migration`) — `apply`/`rollback`/`list`/`pending` with `_migrations` table

### Testing & Quality
- **`libc:test`** — 12 assertion functions, test runner with setup/teardown, filtering
- **`libc:mock`** — spy/mock with call tracking and history
- **Coverage** — `--coverage` flag with codegen instrumentation + text report
- **Memory safety** — leak detector with backtrace capture, smart pointers, ASAN-clean builds
- **Fuzz testing** — automated parser fuzzer, 0 crashes across 100+ random inputs

### Package Manager (voss)
- `voss new` — scaffold projects (web-api, library, desktop-app templates)
- `voss bridge` — auto-generate FFI bridges for PyPI/npm/Cargo/native packages
- `voss doc` — generate HTML docs from `##` comments
- `voss publish` — package, sign, and publish to registries
- `voss test` / `voss bench` — run tests and benchmarks
- `voss sandbox` — isolated package execution with policy files

### Tooling
- **LSP server** (`aurora_lsp`) — completion, hover, signature help, diagnostics, go-to-definition
- **REPL** — `aurorac --repl` for interactive experimentation
- **JIT execution** — `aurorac hello.aura --run` for instant feedback
- **AOT compilation** — `aurorac hello.aura -o hello.exe` for standalone binaries

---

## Zero to Hero: See Aurora in Action

### Hello World
```python
output("Hello, World!")
```
```bash
aurorac hello.aura --run
```

### Web Server with Auth
```python
import libc:backend

server = server_new(8080)
server_get(server, "/", fn(request, response)
    response_send(response, 200, "<h1>Hello from Aurora!</h1>")
end)
server_get(server, "/api/data", fn(request, response)
    response_json(response, 200, {"status": "ok", "data": [1, 2, 3]})
end)
server_start(server)
```

### 3D Graphics — Rotating Cube
```python
import libc:gl

window = gl_window_new("Aurora 3D", 800, 600)
gl_window_make_current(window)

while not glfwWindowShouldClose(window)
    gl_poll_events()
    gl_clear(0.1, 0.1, 0.1, 1.0)

    gl_matrix_mode(0x1701)          # MODELVIEW
    gl_load_identity()
    gl_rotate(glfwGetTime(window) * 50, 0, 1, 0)

    gl_begin(0x0004)                # GL_TRIANGLES
    gl_color3f(1, 0, 0); gl_vertex3f(-1, -1,  1)
    gl_color3f(0, 1, 0); gl_vertex3f( 1, -1,  1)
    gl_color3f(0, 0, 1); gl_vertex3f( 0,  1,  1)
    gl_end()

    glfwSwapBuffers(window)
end
```
See `examples/3d/triangle.aura` and `examples/3d/cube.aura`.

### Call Python from Aurora
```python
import pypi:markdown

mod = markdown_import()
html = markdown_call1(mod, "markdown", markdown_str("# Hello"))
printf("HTML: %s\n", markdown_to_cstr(html))
```

### PostgreSQL + ORM
```python
import libc:database
import libc:orm

db = db_connect("postgresql://user:pass@localhost/mydb")

users = table_new(db, "users")
q = query_select(users, ["id", "name"])
q = query_where(q, "age > ?", [18])
rows = query_execute(q)

for row in rows
    output("User: {row_get(row, "name")}")
end
```

### AI/ML — Train a Model
```python
import libc:ai

data = csv("dataset.csv")
X, y = split_data(data, 0.8)

m = model_create()
model_set_loss(m, "categorical_crossentropy")
model_set_optimizer(m, "adam")
add(m, dense(128, "relu", input_shape=784))
add(m, dropout(0.5))
add(m, dense(10, "softmax"))

fit(m, X_train, y_train, epochs=10)
accuracy = test(m, X_test, y_test)
output("Accuracy: {accuracy}")
```

---

## Installation

### Windows (PowerShell)
```powershell
iwr -useb https://raw.githubusercontent.com/mhfahim22/Aurora/main/scripts/install.ps1 | iex
```

### Linux / macOS
```bash
curl -fsSL https://raw.githubusercontent.com/mhfahim22/Aurora/main/scripts/install.sh | bash
```

### From GitHub Releases
1. Go to [Releases](https://github.com/mhfahim22/Aurora/releases)
2. Download `Aurora-<version>-windows-x64.zip`
3. Extract and add to `PATH`

### From Source
```bash
git clone https://github.com/mhfahim22/Aurora.git
cd Aurora
cmake -B build
cmake --build build --config Release
```

**Requirements:** C++23 compiler (MSVC 2022+, GCC 13+, Clang 16+), LLVM 18+, CMake 3.20+. Optional: Python 3.8+ (PyPI bridge), Node.js 18+ (npm subprocess bridge), Rust 1.70+ (Cargo bridge).

---

## Project Structure

```
aurora/              # Compiler + runtime source
├── src/compiler/    # Lexer, parser, IR, typechecker, codegen, optimizer
├── src/runtime/     # Core runtime, FFI, UI, backend, game, AI
├── tools/voss/      # Package manager (voss)
├── tests/           # C++ test suite + .aura test files
libc/                # Standard library (.auf files)
├── glfw.auf         # GLFW 3 bindings
├── opengl.auf       # OpenGL 3.3 bindings
├── gl.auf           # Aurora 3D helpers
├── pq.auf           # PostgreSQL FFI
├── mysql.auf        # MySQL FFI
├── orm.auf          # Query builder
├── model.auf        # ActiveRecord-style model
├── migration.auf    # Database migrations
├── template.auf     # Server-side templates
├── test.auf         # Test framework
├── mock.auf         # Mock library
├── string.auf       # String utilities
├── static.auf       # Static file server
└── dev.auf          # Dev server with hot-reload
examples/            # Example programs
├── 3d/              # triangle.aura, cube.aura
├── demo/            # Demo programs
packages/            # Aurora packages + bridges
scripts/             # Build scripts, test runner
deps/                # External dependencies
```

---

## Running Aurora Programs

```bash
# JIT compile and run (no files on disk)
aurorac hello.aura --run

# Compile to standalone executable
aurorac hello.aura -o hello.exe
./hello.exe

# REPL (interactive mode)
aurorac --repl

# With coverage
aurorac test.aura --run --coverage

# With voss package manager
voss init my-project
voss run
```

---

## Documentation

| Resource | Description |
|----------|-------------|
| [Language Reference](aurora/docs/language.md) | Full language syntax, types, memory model, FFI |
| [Tutorial](aurora/docs/tutorial.md) | Step-by-step from Hello World to bridges |
| [API Reference](aurora/docs/api_reference.md) | 250+ built-in functions |
| [UI Framework](aurora/docs/ui_framework.md) | Components, layout, style, animation |
| [Backend Framework](aurora/docs/backend_framework.md) | HTTP, middleware, auth, caching, WebSocket |
| [Game Engine](aurora/docs/game_engine.md) | Entities, physics, sprites, audio, input |
| [Bridge Developer Guide](aurora/docs/bridge_developer_guide.md) | FFI bridge patterns, threading, packaging |
| [Security](SECURITY.md) | Memory safety, thread safety, supply chain |
| [Contributing](CONTRIBUTING.md) | Development workflow, code style, PR guidelines |

---

## Test Status

All 13 test suites pass (170+ tests, 0 failures):

| Test Suite | Status |
|-----------|--------|
| Optimizer (IR, mem2reg, lowering) | ✅ 23/23 |
| Leak Detector | ✅ 20/20 |
| Smart Pointers | ✅ 27/27 |
| FFI ABI | ✅ 21/21 |
| FFI ABI Extra | ✅ 28/28 |
| FFI ABI Edge | ✅ 63/63 |
| FFI ABI Struct | ✅ 32/32 |
| FFI Memory Safety | ✅ 18/18 |
| Unified Type System | ✅ 52/52 |
| PyPI Thread Safety | ✅ 4/4 |
| Universal Bridge | ✅ 25/25 |
| Bridge E2E | ✅ 15/15 |
| Integration HTTP | ✅ 12/12 |
| Fuzz Parser | ✅ 100 inputs, 0 crashes |

---

## License

MIT
