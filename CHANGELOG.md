# Changelog

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

### Fixed
- GIL deadlock in `GIL_RETURN(x)` macro (releases GIL before evaluating `x`)
- `PyUnicode_AsUTF8AndSize` → `PyUnicode_AsUTF8` (hangs under MT)
- `JS_FreeValue` leaks on exception paths (10 categories)
- `SetDllDirectory("")` restriction bypassed
- Pipe/file handle leaks in subprocess bridge
- `Py_ssize_t` / `long` type mismatch in `to_cstr()`
- Memory leaks under ASAN: 1605 allocs → 1605 frees
- `JS_WriteObject` API signature mismatch in bytecode cache
- `freeaddrinfo` null-check for `getaddrinfo`
