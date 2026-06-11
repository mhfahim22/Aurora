# Aurora

**Polyglot runtime — call Python, npm, and Rust from a single language.**

Aurora is a programming language and polyglot runtime that lets you import and call functions from **PyPI (Python)**, **npm (JavaScript/Node.js)**, and **Cargo (Rust)** packages as if they were native modules — no glue code, no FFI boilerplate.

```python
import pypi:markdown
import npm:moment
import cargo:hello_rust

mod = markdown_import()
html = markdown_call1(mod, "markdown", markdown_str("# Hello"))
printf("HTML: %s\n", markdown_to_cstr(html))
```

## Features

- **🌉 Polyglot Bridge** — Call Python, npm, and Rust from Aurora scripts and C/C++ via auto-generated bridge DLLs
- **⚡ QuickJS Embedded** — npm packages run via embedded QuickJS engine — no Node.js required for pure JS packages
- **🔒 Thread-Safe** — GIL-based for PyPI, mutex-guarded for QuickJS, pipe-locked for subprocess bridges
- **📦 Auto-Generated Bridges** — `voss bridge npm <pkg>` generates everything needed
- **🔄 Bytecode Cache** — `.qbc` files cache compiled JS bytecode for faster require()
- **🩺 Built-in Diagnostics** — `_get_perf_stats()` and `_get_last_error()` on every bridge
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
cmake -B build
cmake --build build --config Release
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
| LLVM/Clang | 19.x | Required for compiler codegen |
| CMake | 3.20+ | Build system |
| Python | 3.8+ | PyPI bridge support (auto-detected) |
| Node.js | 18+ | npm subprocess bridge (optional) |
| Rust | 1.70+ | Cargo bridge (optional) |

## Project Structure

```
aurora/
├── src/              # Compiler & runtime source
│   ├── compiler/     # Aurora compiler (lexer, parser, codegen, optimizer)
│   └── runtime/      # Core runtime + FFI + AI + std
├── include/          # C++ headers
├── tests/            # Test suite (C++ + .aura files)
├── examples/         # Example .aura programs (hello, fib, bridge demos)
├── docs/             # Language reference, bridge security docs
├── tools/
│   └── voss/         # Aurora Package Manager + bridge generator
│       ├── bridge_main.cpp   # PyPI bridge C template
│       ├── bridge_npm.cpp    # npm bridge C template (QuickJS + subprocess)
│       └── bridge_cargo.cpp  # Cargo bridge Rust template
└── packages/         # Aurora packages
```

## Bridge Ecosystems

| Ecosystem | Bridge Type | Engine | Thread Model |
|-----------|-------------|--------|--------------|
| **PyPI** | C DLL | Python C API | GIL (`PyGILState_Ensure`/`Release`) |
| **npm (QuickJS)** | C DLL | QuickJS | Per-bridge mutex |
| **npm (subprocess)** | C DLL | Node.js child process | Pipe lock + auto-restart |
| **Cargo** | Rust DLL | Native Rust | `Arc<Mutex<>>` |

## Documentation

- [Language Reference](aurora/docs/language.md)
- [Bridge Security & Error Handling](aurora/docs/bridge_security.md)
- [Production Plan](PRODUCTION_PLAN.md)

## License

MIT
