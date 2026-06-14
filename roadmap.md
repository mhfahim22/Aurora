# Aurora Universal Interop Roadmap

## Vision

Build a Universal Interop system that can reuse C, C++, Rust, Python and
JavaScript ecosystems with near-native performance.

------------------------------------------------------------------------

# Phase 0 - Foundation (COMPLETED)

-   [x] C ABI FFI → `aurora/src/runtime/core/ffi.cpp` + `aurora/include/runtime/interop/ffi_dispatch.hpp`
-   [x] Polyglot Bridge → `aurora/tools/voss/bridge_*.cpp` + `quickjs/moment_quickjs_bridge.c`
-   [x] Voss Package Manager → `aurora/tools/voss/voss.cpp` + `aurora/tools/voss/voss.h`

**Goal:** Stable foundation.

**Build:** `cmake --build build --config Release`

**Test results:** 109/109 passed across 6 test suites.

------------------------------------------------------------------------

# Phase 1 - Universal FFI Core

## Tasks

-   Unified FFI API
-   Dynamic library loader
-   Symbol resolver
-   ABI abstraction

## Exit

Stable cross-platform FFI.

------------------------------------------------------------------------

# Phase 2 - Automatic Binding Generator

Example:

``` bash
voss bind opencv
```

Generated:

``` aurora
import opencv
```

## Tasks

-   Header parser
-   AST
-   Code generator
-   Binding cache

------------------------------------------------------------------------

# Phase 3 - Unified Type System

  C        Aurora
  -------- -----------------
  int      int
  double   float
  char\*   string
  void\*   ptr`<T>`{=html}

------------------------------------------------------------------------

# Phase 4 - C++ Interop

-   Wrapper generator
-   STL mapping
-   Exception translation

------------------------------------------------------------------------

# Phase 5 - Rust Interop (COMPLETED)

-   [x] Cargo bridge → `aurora/tools/voss/bridge_cargo.cpp`
-   [x] ABI adapter → `aurora/tools/voss/commands_bind.cpp`
-   [x] Ownership compatibility → `aurora/include/runtime/interop/ref_count_bridge.hpp`

-----------------------------------------------------------------------

# Phase 6 - Native Python Bridge (COMPLETED)

Principle:

Interpreter Last, Native First.

Flow:

Aurora -\> Binding -\> Native Backend -\> Machine Code

Tasks:
-   [x] PyPI resolver → `aurora/tools/voss/bridge_pypi.cpp` + `aurora/include/runtime/interop/universal_resolver.hpp`
-   [x] Native backend detector → `aurora/src/runtime/core/ffi.cpp` (aurora_py_ensure_initialized) + `aurora/tools/voss/tool_detection.cpp`
-   [x] Compatibility layer → `aurora/tools/voss/bridge_main.cpp` (C wrapper DLL) + `aurora/src/runtime/interop/type_mapping.cpp` (PythonMapper)
-   [x] Interpreter fallback → Cross-ecosystem chain (pypi → npm → cargo), thread-safe GIL management, shared runtime

-----------------------------------------------------------------------

# Phase 7 - JavaScript Bridge (COMPLETED)

-   [x] npm resolver → `aurora/include/runtime/interop/universal_resolver.hpp` (npm_url)
-   [x] Native addon detection → `npm_has_native_addon()` in `bridge_npm.cpp`
-   [x] Runtime fallback → `gen_quickjs_npm_wrapper()` (QuickJS embedded interpreter) + `gen_npm_cpp_wrapper()` (Node.js child-process JSON-RPC)

-----------------------------------------------------------------------

# Phase 8 - Memory Safety

-   [x] Ownership → `aurora/include/compiler/ownership.hpp` (OwnershipTracker, NLL borrow checking)
-   [x] RAII → `aurora/include/runtime/interop/raii_guard_gen.hpp` (RAIIGuardGenerator) + `aurora/src/runtime/core/memory.cpp` (aurora_drop_glue)
-   [x] Smart pointers → `aurora/include/runtime/smart_ptr.hpp` (`AuroraSharedPtr<T>`, `AuroraWeakPtr<T>`, `CppSharedPtrBridge` VTable, `to_std_shared`/`from_std_shared`, `aurora_make_shared`)
-   [x] Leak detection → `aurora/include/runtime/leak_detector.hpp` + `aurora/src/runtime/core/leak_detector.cpp` (per-allocation tracking, backtrace capture, JSON/text reports, atexit auto-report)

------------------------------------------------------------------------

# Phase 9 - Performance

-   [x] Aurora IR → `aurora/include/compiler/ir/ir.hpp` + `aurora/src/compiler/ir/ir.cpp` (standalone SSA-based IR), `aurora/src/compiler/ir/ir_lowering.cpp` (LLVM IR lowering), optimizer tests → `aurora/tests/test_optimizer.cpp` (16 tests: const_fold, DCE, strength_reduce)
-   [x] Runtime type reflection → `aurora/include/runtime/reflection.hpp` + `aurora/src/runtime/core/reflection.cpp` (global type registry, field/method introspection)
-   [x] Coroutine/generator yield → `aurora/src/runtime/core/util.cpp` (aurora_yield with fiber integration, thread-local yield value)
-   [x] OptimizedCodegen function generation → `aurora/src/compiler/codegen/optimized_codegen.cpp` (gen_function no longer a placeholder)
-   [x] Package builtins → `aurora/src/runtime/builtins/std_builtins.cpp` (voss CLI integration for install/update/search)
-   LTO
-   Zero-copy
-   Cross-language optimization

------------------------------------------------------------------------

# Phase 10 - Universal Package Bridge

``` bash
voss add numpy
voss add serde
voss add opencv
```

Aurora resolves everything automatically.

------------------------------------------------------------------------

# Final Checklist

-   C
-   C++
-   Rust
-   Python native bridge
-   JS bridge
-   Auto binding
-   Memory safe
-   Near-native speed
-   Cross platform

------------------------------------------------------------------------

# Recommended Development Order

1.  FFI Core
2.  Binding Generator
3.  Type System
4.  C++
5.  Rust
6.  Python Native
7.  JavaScript
8.  Memory Safety
9.  Performance
10. Universal Package Bridge
