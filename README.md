<p align="center">
  <img src="assets/logo.png" alt="Aurora" width="200"/>
</p>

<h1 align="center">Aurora — The Masterpiece Language</h1>

<p align="center">
  <b>Not just a polyglot language — a unified ecosystem where anything is possible.</b><br>
  <b>Web apps, desktop GUI, 3D games, AI/ML, backend servers, scripting — all from a single language.</b><br>
  <b>Call Python, JavaScript, Rust, C, C++, Java, Go, OpenGL seamlessly — zero boilerplate.</b>
</p>

<p align="center">
  <a href="https://github.com/mhfahim22/Aurora/releases"><img src="https://img.shields.io/github/v/release/mhfahim22/Aurora?style=flat-square&label=version" alt="Version"/></a>
  <a href="https://github.com/mhfahim22/Aurora/actions"><img src="https://img.shields.io/github/actions/workflow/status/mhfahim22/Aurora/build.yml?style=flat-square&label=build" alt="Build"/></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square" alt="License"/></a>
  <img src="https://img.shields.io/badge/platform-windows%20|%20linux%20|%20macOS-lightgrey?style=flat-square" alt="Platform"/>
  <img src="https://img.shields.io/badge/standard-C%2B%2B23-purple?style=flat-square" alt="C++23"/>
  <img src="https://img.shields.io/badge/LLVM-18-yellow?style=flat-square" alt="LLVM 18"/>
</p>

---

## 📋 Table of Contents

- [Why Aurora?](#why-aurora)
- [Quick Start](#quick-start)
- [Features at a Glance](#features-at-a-glance)
- [See Aurora in Action](#see-aurora-in-action)
- [Installation](#installation)
- [Project Structure](#project-structure)
- [Running Aurora Programs](#running-aurora-programs)
- [Documentation](#documentation)
- [Community & Contributing](#community--contributing)
- [License](#license)

---

## 🎯 Why Aurora?

Aurora was built with one vision: **a single language that can do everything.**

Not a toy, not a niche experiment — a **masterpiece** engineered from the ground up to eliminate boundaries between ecosystems, frameworks, and platforms.

| What others make you choose | Aurora gives you **all** |
|----------------------------|--------------------------|
| **One language ecosystem** | Call Python, JavaScript, Rust, Java, Go, C/C++ from one codebase |
| **FFI boilerplate** | Zero-copy, auto-generated bridges — no glue code |
| **Slow iteration** | JIT mode for instant feedback, AOT for production |
| **Complex build systems** | Single `aurorac` command — compile, run, or REPL |
| **Fragmented frameworks** | Built-in UI, backend, game, and AI frameworks |
| **Memory management tax** | 4 strategies — stack, arena, RAII, ARC, GC — per variable |
| **Cross-ecosystem packages** | Import PyPI, npm, Cargo packages like native modules |

---

## ⚡ Quick Start

```bash
# Install (Windows PowerShell)
iwr -useb https://raw.githubusercontent.com/mhfahim22/Aurora/main/release/install.ps1 | iex

# Install (Linux / macOS)
curl -fsSL https://raw.githubusercontent.com/mhfahim22/Aurora/main/release/install.sh | bash
```

**Hello World in 5 seconds:**

```python
output("Hello, World!")
```

```bash
aurorac hello.aura --run    # JIT compile & run
```

Under the hood: `aurorac` parses Aurora IR → generates LLVM IR → optimizes (O3, znver3) → executes via JIT or produces a standalone `.exe`.

---

## 🧠 Features at a Glance

Aurora is a **masterpiece language** — every feature is designed to work together seamlessly. From systems programming to AI, from web backends to 3D games, from native desktop GUIs to cross-ecosystem scripting — everything is built-in, nothing is an afterthought.

### 🧩 Core Language
- **LLVM-native compilation** — Aurora IR → LLVM IR → optimized machine code (O3, znver3)
- **Memory management** — 4 strategies per variable: `@stack` / `@arena` / `@raii` / `@arc` / `@gc`
- **Async/await** — `spawn`, `wait`, `parallel` blocks, channels, fibers, event bus
- **OOP** — classes with inheritance, abstract, interfaces, encapsulation, polymorphism, vtables
- **Pattern matching** — `match`/`switch` with struct destructuring, array patterns, wildcards
- **Generics** — type parameters on functions, structs, and classes
- **Ownership system** — compile-time lifetime analysis with `move`/`copy`/`borrow` annotations
- **Type-checking** — full semantic analysis with type inference, safety checks, detailed diagnostics

### 🌐 Polyglot FFI — Unify Every Ecosystem
Aurora doesn't just "support" other languages — it **absorbs** them. Import any PyPI, npm, or Cargo package as if it were native Aurora code. No glue code, no wrappers, no context switching.

| Ecosystem | Method | Thread Safety |
|-----------|--------|---------------|
| **PyPI (Python)** | C bridge DLL | GIL-locked, zero-copy |
| **npm (JavaScript)** | QuickJS embedded / subprocess | Mutex-guarded |
| **Cargo (Rust)** | Auto-generated cdylib bridge | `Arc<Mutex<T>>` |
| **Java/JVM** | 17+ extern function declarations | JVM-guarded |
| **Go** | dlopen plugin system | Platform-dependent |
| **Native C/C++** | `extern "library"` with lazy DLL loading | Manual |

### 🏗️ Built-in Frameworks — Everything Included
No need to stitch together a dozen libraries. Aurora ships with production-ready frameworks for every domain:

| Framework | Purpose |
|-----------|---------|
| `libc:backend` | HTTP server — router, middleware, sessions, auth, CORS, caching, WebSocket, hot-reload |
| `libc:ui` | Cross-platform GUI — components, layout, style, animation, Win32/X11/Cocoa |
| `libc:game` | 2D/3D game engine — entities, physics, sprites, audio, input, camera |
| `libc:ai` | ML pipeline — tensor ops, autograd, layers (dense, conv, LSTM, transformer), ONNX |
| `libc:sprite2d` | GPU-accelerated 2D sprite batcher |
| `libc:gl` | OpenGL 3.3+ graphics — shaders, VAO/VBO, textures, window management |
| `libc:audio` | Audio playback via SDL |
| `libc:image` | Image loading — PNG/JPG/BMP via stb_image |

### 🗄️ Database & ORM — Full-Stack Ready
| Library | Description |
|---------|-------------|
| `libc:pq` | PostgreSQL via libpq — 85+ externs, connection pooling, parameterized queries |
| `libc:mysql` | MySQL via libmysqlclient — 50+ externs |
| `libc:database` | Unified URL-based auto-detect (`postgresql://` / `mysql://`) |
| `libc:orm` | Query builder — SELECT/INSERT/UPDATE/DELETE, joins, group_by |
| `libc:model` | ActiveRecord-style — `find`/`save`/`delete` with validation |
| `libc:migration` | Schema migrations — `apply`/`rollback`/`list`/`pending` |

### 🤖 AI / ML
- **Tensor operations** — add, sub, mul, matmul, relu, sigmoid, tanh
- **Autograd** — automatic differentiation with DAG-based backward pass
- **SGD Optimizer** — gradient descent with configurable learning rate
- **INT8 Quantization** — symmetric scale-based quantization for model compression
- **ONNX Runtime** — Gemm, Conv, Relu, Softmax, BatchNorm operators
- **CUDA Kernels** — GPU-accelerated operations (add, sub, mul, div, relu, sigmoid)
- **Model pipeline** — create, train, test, predict, save/load, export ONNX
- **Layers** — Dense, Conv2D, LSTM, GRU, Dropout, BatchNorm, Attention, Transformer, Embedding

### 🎮 Game Development
- **3D Engine** — OpenGL 3.3 core profile, shaders, camera, lighting, textures, OBJ model loading
- **2D Engine** — GPU sprite batcher, texture atlases, rotation, color tint, orthographic projection
- **Input** — keyboard, mouse, joystick via GLFW
- **Audio** — sound effects and music via SDL

| Example Game | Features | Source |
|-------------|----------|--------|
| **Flappy Bird** | 2D physics, AABB collision, pipe spawning, score, game-over, restart | `examples/2d/flappy_bird.aura` |
| **FPS Shooter** | 3D WASD+QE movement, mouse look, enemies | `examples/3d/shooter.aura` |

### ✅ Testing & Quality
- **`libc:test`** — 12 assertion functions, test runner with setup/teardown, filtering
- **`libc:mock`** — spy/mock with call tracking and history
- **Coverage** — `--coverage` flag with codegen instrumentation + text report
- **Memory safety** — leak detector with backtrace capture, smart pointers, ASAN-clean builds
- **Fuzz testing** — automated parser fuzzer, 0 crashes across 100+ random inputs

### 📦 Package Manager (voss)
| Command | Description |
|---------|-------------|
| `voss new` | Scaffold projects (web-api, library, desktop-app) |
| `voss bridge` | Auto-generate FFI bridges for PyPI/npm/Cargo/native |
| `voss doc` | Generate HTML docs from `##` comments |
| `voss publish` | Package, sign, and publish to registries |
| `voss test` / `voss bench` | Run tests and benchmarks |
| `voss sandbox` | Isolated package execution with policy files |

### 🔧 Tooling
- **LSP server** (`aurora_lsp`) — completion, hover, signature help, diagnostics, go-to-definition
- **REPL** — `aurorac --repl` for interactive experimentation
- **JIT execution** — `aurorac hello.aura --run` for instant feedback
- **AOT compilation** — `aurorac hello.aura -o hello.exe` for standalone binaries

---

## 🚀 See Aurora in Action

### Hello World
```python
output("Hello, World!")
```
```bash
aurorac hello.aura --run
```

---

### Web Server with Routing
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

---

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
Full examples: `examples/3d/triangle.aura` | `examples/3d/cube.aura`

---

### 2D Game — Flappy Bird
```python
import "sprite2d"
import "glfw"

glfwInit()
window = glfwCreateWindow(800, 600, "Flappy Bird", null, null)
glfwMakeContextCurrent(window)
sprite2d_init(2048, 800, 600)

while not glfwWindowShouldClose(window)
    glfwPollEvents()
    sprite2d_draw(0, bird_x, bird_y, 30, 30)  # draw bird sprite
    sprite2d_flush()
    glfwSwapBuffers(window)
end
```
Full game: `examples/2d/flappy_bird.aura`

---

### AI/ML — Train a Neural Network
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

### Win32 Native GUI
```python
import libc:ui

ui_win32_init()
window = ui_win32_create_control("window", 0, 100, 100, 800, 600)
btn = ui_win32_create_control("button", window, 10, 10, 100, 30)
ui_win32_set_text(btn, "Click me")
ui_win32_run()
```
Full app: `examples/chat.aura` (5-widget chat application)

---

### Call Python from Aurora
```python
import pypi:markdown

mod = markdown_import()
html = markdown_call1(mod, "markdown", markdown_str("# Hello"))
printf("HTML: %s\n", markdown_to_cstr(html))
```

---

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

---

## 📦 Installation

### Windows (PowerShell)
```powershell
iwr -useb https://raw.githubusercontent.com/mhfahim22/Aurora/main/release/install.ps1 | iex
```

### Linux / macOS
```bash
curl -fsSL https://raw.githubusercontent.com/mhfahim22/Aurora/main/release/install.sh | bash
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

### Requirements
| Dependency | Required | Version |
|------------|----------|---------|
| C++ Compiler | ✓ | MSVC 2022+ / GCC 13+ / Clang 16+ |
| LLVM | ✓ | 18+ |
| CMake | ✓ | 3.20+ |
| Python | Optional (PyPI bridge) | 3.8+ |
| Node.js | Optional (npm bridge) | 18+ |
| Rust | Optional (Cargo bridge) | 1.70+ |

---

## 📁 Project Structure

```
aurora/                  # Compiler + runtime source
├── src/compiler/        # Lexer, parser, IR, typechecker, codegen, optimizer
├── src/runtime/         # Core runtime, FFI, UI, backend, game, AI, graphics
│   └── gfx/             # gl_helper, sprite2d, audio_helper, image_helper, matrix, obj_helper
├── tools/voss/          # Package manager
├── tests/               # 230+ test files
├── docs/                # Language reference, API reference, tutorials
└── include/             # Public headers
libc/                    # Standard library (.auf bindings)
├── glfw.auf             # GLFW 3 bindings
├── opengl.auf           # OpenGL 3.3 bindings
├── gl.auf               # Aurora 3D helpers
├── sprite2d.auf         # 2D sprite batcher
├── audio.auf            # Audio (SDL)
├── image.auf            # Image loading (stb_image)
├── ui.auf               # Win32 GUI
├── input.auf            # Input helpers
├── pq.auf               # PostgreSQL FFI
├── mysql.auf            # MySQL FFI
├── orm.auf              # Query builder
├── model.auf            # ActiveRecord-style model
├── migration.auf        # Database migrations
├── template.auf         # Server-side templates
├── test.auf             # Test framework
├── mock.auf             # Mock library
├── string.auf           # String utilities
├── static.auf           # Static file server
└── dev.auf              # Dev server with hot-reload
examples/                # Example programs
├── 2d/                  # flappy_bird.aura
├── 3d/                  # triangle, cube, lighting, texture, model, shooter
├── chat.aura            # Win32 GUI chat
├── todo_full.aura       # Full-stack todo with REST + frontend
├── ai_classifier.aura   # MrCode image classifier
├── ai_mnist.aura        # MNIST benchmark
└── poly_pipeline.aura   # Data pipeline
packages/                # Aurora packages + bridges
scripts/                 # Build/test scripts
deps/                    # External dependencies (stb_image.h, etc.)
release/                 # Installers and release assets
```

---

## ▶️ Running Aurora Programs

```bash
# JIT compile and run (no files on disk)
aurorac hello.aura --run

# Compile to standalone executable
aurorac hello.aura -o hello.exe
./hello.exe

# REPL (interactive mode)
aurorac --repl

# With coverage instrumentation
aurorac test.aura --run --coverage

# Using the package manager
voss init my-project
voss run
```

---

## 📚 Documentation

| Resource | Description |
|----------|-------------|
| [Documentation Index](docs/index.md) | Entry point to all docs — tutorials, reference, frameworks |
| [Language Reference](aurora/docs/reference/01-syntax-basics.md) | Complete language specification (17 chapters) |
| [Tutorial](aurora/docs/tutorial.md) | Step-by-step from Hello World to bridges |
| [API Reference](aurora/docs/api_reference.md) | 250+ built-in functions |
| [UI Framework](aurora/docs/ui_framework.md) | Components, layout, style, animation |
| [Backend Framework](aurora/docs/backend_framework.md) | HTTP, middleware, auth, caching, WebSocket |
| [Game Engine](aurora/docs/game_engine.md) | Entities, physics, sprites, audio, input |
| [Bridge Developer Guide](aurora/docs/bridge_developer_guide.md) | FFI bridge patterns, threading, packaging |
| [Package Ecosystem](aurora/docs/package_ecosystem.md) | voss CLI, registries, sandboxing |
| [Security](SECURITY.md) | Memory safety, thread safety, supply chain |
| [Contributing](CONTRIBUTING.md) | Development workflow, code style, PR guidelines |
| [Changelog](CHANGELOG.md) | Release history and version notes |

---

## 👥 Community & Contributing

Aurora is an open-source project under the MIT license. We welcome contributions of all kinds:

- **🐛 Found a bug?** Open an [issue](https://github.com/mhfahim22/Aurora/issues)
- **💡 Have an idea?** Start a [discussion](https://github.com/mhfahim22/Aurora/discussions)
- **🔧 Want to contribute?** See [CONTRIBUTING.md](CONTRIBUTING.md) for:
  - Build and test setup
  - Branch strategy and PR process  
  - Code style guidelines
  - Bridge development workflows

### Development Setup
```bash
git clone https://github.com/mhfahim22/Aurora.git
cd Aurora
cmake -B build
cmake --build build --config Release
ctest --test-dir build --config Release
```

### Project Status
Current version: **1.0.0-rc.1** — Release Candidate 1. All features complete, regression-tested, and packaged.

---

## 📄 License

MIT — see [LICENSE](LICENSE) for details.

---

<p align="center">
  Made with ❤️ by the Aurora community — <b>One language to rule them all.</b>
</p>
