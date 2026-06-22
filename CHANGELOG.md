# Changelog

## 0.3.2 (2026-06-22)

### Fixed
- **Bug 1 - Try/catch heap corruption**: Changed `Owned` to `Borrowed` ownership for live variables in outlined catch functions to prevent double-free crashes.
- **Bug 2 - Async no-execution**: Added missing `gen_block(node->body->body.get())` in async wrapper body; changed async callee signature from `i8*(i8*)` to `i8*()`.
- **Bug 3 - HOF + arrays + lambdas crash**: Fixed `gen_fnptr_call` to load `i64` then `IntToPtr` to `i8*` instead of loading `i8ptr_ty()` directly from an `i64_ty()` parameter alloca.
- **Bug 4 - String concat + array index access crash**: Added `case NodeType::Index: return false;` to `expr_is_string_type()` so integer array elements are not treated as strings during concatenation.
- **Bug 5 - HOF with regular function definitions**: `gen_var` now falls back to `module_->getFunction(node->value)` when `lookup_var` returns nullptr, so function names referenced as values resolve correctly.
- **Bug 6 - Allocation scope leakage**: `walk_vars` in `allocation_strategy.cpp` now walks function node children individually instead of following `next` pointers from the Function node, preventing phantom variables from being attributed to wrong functions.
- **Bug 7 - Function definitions after class usage cause JIT crash**: `gen_function` now saves/restores the scope stack (`saved_scopes = std::move(scopes_)`) like `gen_class_oop` does, preventing `emit_all_scope_cleanup()` in `gen_return` from generating cross-function IR references to module-level variables.

### Added
- Inline lambda expression codegen and parsing (lambda(params) body_expr).
- `push()` now returns `int64_t` instead of `void`.

### Changed
- Module verification now runs before JIT execution for earlier error detection.

## 0.2.0 (2026-06-14)

### Added
- **Aurora IR subsystem** — standalone SSA-based intermediate representation (`ir.hpp`, `ir.cpp`)
  - IR lowering to LLVM IR (`ir_lowering.cpp`)
  - Optimizer passes: constant folding, dead code elimination, strength reduction
  - Mem2Reg with multi-block phi insertion, load-only (undef) promotion, single-store fast path
  - `gen_for` implementation for loop codegen (was a stub)
- **FFI ABI abstraction** — `ffi_abi.hpp`, `ffi_abi.cpp`, `ffi_call_win64.asm`
  - Win64/x64 calling convention support
  - Automated test coverage (`test_ffi_abi*.cpp`)
- **Leak detector** — `leak_detector.hpp/.cpp` with per-allocation tracking, backtrace capture, JSON/text reports, atexit auto-report (20 tests)
- **Runtime type reflection** — `reflection.hpp/.cpp` with global type registry, field/method introspection
- **Smart pointers** — `smart_ptr.hpp` with `AuroraSharedPtr<T>`, `AuroraWeakPtr<T>`, `CppSharedPtrBridge` VTable, `to_std_shared`/`from_std_shared` (27 tests)
- **LSP enhancements** — `lsp_analysis.cpp`, `lsp_json.cpp`; recursive-descent JSON parser, signatureHelp, improved hover with definition lookup
- **Bridge refactoring** — `bridge_cargo_gen.cpp`, `bridge_cargo_impl.cpp`, `bridge_npm_gen.cpp`, `bridge_npm_impl.cpp` split from monolithic templates; `type_mapping_langs.cpp` extracted lang-specific mappers
- **Codegen improvements** — `codegen_runtime.cpp`, `codegen_ffi.cpp` added to build; `optimized_codegen.cpp` function generation no longer a placeholder
- **Package builtins** — voss CLI integration for install/update/search via `std_builtins.cpp`

### Changed
- CMakeLists.txt updated with new source files and ASM_MASM language support
- `.gitignore` updated for `_build/` and `CMakeFiles/` directories
- Cleaned up duplicate lang-specific mapper code from `type_mapping.cpp`

### Removed
- `ANCHORED_SUMMARY.md` — internal session summary file
- Various stderr/stdout log files from project root
- Orphaned `reg.add_mapping` calls from `type_mapping.cpp` (moved to `type_mapping_langs.cpp`)

## 0.1.0 (2026-06-11)

### Added
- PyPI bridge (markdown, requests, flask, numpy, Pillow) with GIL thread safety
- npm QuickJS bridge (moment, lodash, chalk, execa, got) with embedded JS engine
- npm subprocess bridge for native-addon packages
- Cargo bridge (hello_rust)
- Cross-ecosystem resolver (PyPI ↔ npm ↔ Cargo ↔ Native)
- `voss` package manager with `bridge` command for auto-generating bridge DLLs
- `_get_perf_stats()` diagnostics on all bridges
- `_get_last_error()` error reporting on all bridges
- QuickJS bytecode cache (`.qbc` files)
- JSON-RPC batch request support in subprocess bridge
- `rt_pyapi` pointer cache (PyPI, ~25 cached APIs)
- Node.js builtin polyfills (path, fs, buffer, crypto, net, dns, http, etc.)
- ESM→CJS transpilation (esbuild pre-transpile + C fallback at runtime)
- Windows CNG crypto (BCrypt) + CryptGenRandom for QuickJS
- Full exports map resolver with wildcard subpath support
- `.mjs` / `.cjs` extension support in module resolution
- Stress test: 50 require calls, 1000 JS calls, 1000 JSON conversions
- Thread-local error buffers
- Subprocess crash detection + auto-restart with 30s timeout
- `aurora_dl_open()` with `LOAD_WITH_ALTERED_SEARCH_PATH` fallback (fixes Python DLL search)
- ASAN build support (`build_asan.bat`)
- AppVerif memory validation
- GitHub Actions CI (Windows + Ubuntu)
- Example projects for all 3 bridges
