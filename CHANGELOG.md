# Changelog

## v1.0.0 (2026-06-26) — The Masterpiece Language

Aurora v1.0.0 is the complete, production-ready release. All 10 phases are fully implemented and battle-tested.

### Core Language
- LLVM-native compilation with OOP, generics, pattern matching, ownership system
- 4 memory management strategies: stack, arena, RAII, ARC, GC
- Async/await with spawn, wait, parallel, channels, fibers, event bus

### Frameworks
- **Backend**: HTTP server, routing, middleware, sessions, auth, CORS, caching, WebSocket, database ORM
- **UI**: Cross-platform GUI (Win32/X11/Cocoa), components, layout, style, animation
- **Game**: 3D OpenGL 3.3+ engine, physics, audio, input, sprite system
- **AI/ML**: Tensor ops, autograd, dense/conv/LSTM/transformer layers, ONNX, CUDA

### Polyglot FFI
- Zero-boilerplate bridges to Python (PyPI), JavaScript (npm), Rust (Cargo), Java, Go, C/C++

### Tooling
- `aurorac` compiler with JIT/AOT, `voss` package manager, `aurora_lsp` language server
- 254+ test files, fuzz testing, memory stress tests, benchmarks

### v0.3.0-h3 (2026-06-26) — Annotation-Aware ABI Migration

- Annotation-first type system and code generation
- Typed ABI generation with `ast_kind_to_abi_type()`
- Typed indirect dispatch for closures, function pointers, OOP vtables
- Legacy boolean flags fully migrated to `AstTypeKind` enum
