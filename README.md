# Aurora

**Polyglot runtime — call Python, npm, Rust, C, C++ from a single language with LLVM-native compilation.**

Aurora is a programming language and polyglot runtime that lets you import and call functions from **PyPI (Python)**, **npm (JavaScript/Node.js)**, **Cargo (Rust)**, and native **C/C++** packages as if they were native modules — no glue code, no FFI boilerplate.

```python
import pypi:markdown
import npm:moment
import cargo:hello_rust

mod = markdown_import()
html = markdown_call1(mod, "markdown", markdown_str("# Hello"))
printf("HTML: %s\n", markdown_to_cstr(html))
```

## Features

- **🌉 Polyglot Bridge** — Call Python, npm, Rust, C, C++ from Aurora scripts and C/C++ via auto-generated bridge DLLs
- **⚡ QuickJS Embedded** — npm packages run via embedded QuickJS engine — no Node.js required for pure JS packages
- **🔧 LLVM Compiler** — Aurora IR → LLVM IR codegen with optimization passes (mem2reg, constant folding, DCE, strength reduction)
- **⚡ FFI ABI Abstraction** — Win64/x64 calling convention support via `ffi_abi` subsystem with MASM trampolines
- **🔒 Thread-Safe** — GIL-based for PyPI, mutex-guarded for QuickJS, pipe-locked for subprocess bridges
- **📦 Auto-Generated Bridges** — `voss bridge npm <pkg>` generates everything needed
- **🔄 Bytecode Cache** — `.qbc` files cache compiled JS bytecode for faster require()
- **🩺 Built-in Diagnostics** — `_get_perf_stats()` and `_get_last_error()` on every bridge
- **🛡️ Memory Safety** — Leak detector with per-allocation tracking + backtrace capture, smart pointers (AuroraSharedPtr, CppSharedPtrBridge), RAII guards
- **🪞 Runtime Reflection** — Global type registry with field/method introspection
- **🖥️ LSP Server** — Full language server with completion, hover, signature help, diagnostics, go-to-definition
- **🛡️ ASAN-Clean** — AddressSanitizer builds with zero memory errors

## Installation

### Windows (PowerShell)
```powershell
iwr -useb https://raw.githubusercontent.com/mhfahim22/Aurora/main/scripts/install.ps1 | iex
```

### Linux / macOS
```bash
curl -fsSL https://raw.githubusercontent.com/mhfahim22/Aurora/main/scripts/install.sh | bash
```

### Manual (from source)
```bash
git clone https://github.com/mhfahim22/Aurora.git
cd Aurora
cmake -B _build
cmake --build _build --config Release
```

### From GitHub Releases
1. Go to [Releases](https://github.com/mhfahim22/Aurora/releases)
2. Download `Aurora-<version>-windows-x64.zip`
3. Extract and add to PATH

## Quick Start

```bash
# Create a new Aurora project
voss init my-project
cd my-project

# Add a bridge to a Python package
voss bridge pypi requests

# Add a bridge to an npm package
voss bridge npm lodash

# Add a bridge to a Rust crate
voss bridge cargo serde

# Run your Aurora script
voss run
```

## Requirements (for building from source)

| Dependency | Version | Notes |
|-----------|---------|-------|
| C++ compiler | C++23 | MSVC 2022+, GCC 13+, Clang 16+ |
| LLVM/Clang | 18.x+ | Required for compiler codegen |
| CMake | 3.20+ | Build system |
| Python | 3.8+ | PyPI bridge support (auto-detected) |
| Node.js | 18+ | npm subprocess bridge (optional) |
| Rust | 1.70+ | Cargo bridge (optional) |

## Project Structure

```
aurora/
├── src/compiler/      # Aurora compiler
│   ├── lexer/         # Tokenizer
│   ├── parser/        # AST parser
│   ├── ir/            # SSA-based intermediate representation
│   ├── semantic/      # Type checking, ownership, lifetime analysis
│   ├── codegen/       # LLVM IR codegen + runtime exports
│   └── optimizer/     # const_fold, DCE, strength_reduce
├── src/runtime/       # Core runtime
│   ├── core/          # Memory, FFI, leak detector, reflection, strings
│   ├── interop/       # Type mapping, serialization, FFI dispatch
│   ├── builtins/      # Built-in function implementations
│   ├── async/         # Tasks, scheduler, channels, event bus, fibers
│   ├── ui/            # Component framework, renderer
│   ├── game/          # Engine, physics, audio, input
│   ├── backend/       # HTTP server, middleware, sessions, auth
│   └── ai/            # Tensor, layers, training, models
├── include/           # C++ headers
├── tests/             # Test suite (C++ + .aura files)
├── tools/
│   └── voss/          # Aurora Package Manager + bridge generator
└── packages/          # Aurora packages
```

## Bridge Ecosystems

| Ecosystem | Bridge Type | Engine | Thread Model |
|-----------|-------------|--------|--------------|
| **PyPI** | C DLL | Python C API | GIL (`PyGILState_Ensure`/`Release`) |
| **npm (QuickJS)** | C DLL | QuickJS | Per-bridge mutex |
| **npm (subprocess)** | C DLL | Node.js child process | Pipe lock + auto-restart |
| **Cargo** | Rust DLL | Native Rust | `Arc<Mutex<>>` |

## Test Status

| Test Suite | Tests | Status |
|-----------|-------|--------|
| Optimizer (IR, mem2reg, lowering) | 23 | ✅ All pass |
| Leak Detector | 20 | ✅ All pass |
| Smart Pointers | 27 | ✅ All pass |

## Documentation

- [Language Reference](aurora/docs/language.md)
- [Bridge Security & Error Handling](aurora/docs/bridge_security.md)
- [Production Plan](PRODUCTION_PLAN.md)

## License

MIT
