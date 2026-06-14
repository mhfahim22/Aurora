# Changelog

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
