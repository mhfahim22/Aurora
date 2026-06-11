# Session Summary

## QuickJS NPM Wrapper (`gen_quickjs_npm_wrapper`)
- New code generator that wraps NPM packages for use with QuickJS
- Generates C glue code that registers JS functions calling into NPM/CJS modules
- Handles module resolution via `node_modules/` lookup

## C-Level Bridge Helpers (in generated C code)
Added 9 native C functions registered as `__bridge_*` globals in QuickJS:
- `__bridge_fs_readFile`, `__bridge_fs_writeFile`, `__bridge_fs_exists`
- `__bridge_fs_mkdir`, `__bridge_fs_readdir`, `__bridge_fs_stat`, `__bridge_fs_unlink`
- `__bridge_exec` (child_process.execSync via popen)
- `__bridge_http_get` (HTTP GET via curl/powershell)
- Added proper C headers: `<sys/stat.h>`, `<dirent.h>`, `<direct.h>`

## Node.js Builtin Polyfills (`quickjs/node_builtins.js`)
- **fs**: Sync wrappers for readFile, writeFile, exists, mkdir, readdir, stat, unlink, appendFile, realpath, access + `promises` API
- **child_process**: execSync/exec using `__bridge_exec`
- **http/https**: get/request using `__bridge_http_get`
- **module**: Full Module class with _resolveFilename, _cache, _extensions
- Extended `process` with stderr, stdin, pid, uptime, memoryUsage, cpuUsage, umask, kill, hrtime, getuid, getgid

## Cargo Bridge Improvements (`discover_cargo_functions`)
- Detects `async fn` and generates `futures::executor::block_on()` wrappers
- Generates real serde_json-based argument deserialization (no more stubs)
- `&self` methods: generates informative error (can't auto-wrap without instance)
- Added `futures = "0.3"` dependency to generated Cargo.toml

## Runtime Dynamic Module Loader (major feature)
### C-level changes in generated `js_require()`:
- **Module cache**: `g_cache_names[]`/`g_cache_exports[]` arrays (256 entries) with `strdup` tracking
- **Current directory tracker**: `g_current_dir[]` set per-module for correct relative resolution
- **Require chain**: cache → `__node_builtins` → disk (node_modules + relative paths) → auto-install
- **`bridge_npm_install(name)`**: Fetches npm registry metadata → downloads tarball via `bridge_http_get` → extracts to `node_modules/<name>/` via `tar` → renames `package/` dir
- **`js_rust_load(name)`**: Creates temp Rust cdylib project → writes Cargo.toml + lib.rs (serde_json-based dispatch) → `cargo build --release` → `dlopen`/`LoadLibrary` → returns JS object with `__rust_crate` metadata
- **`resolve_mod`**: Now handles relative paths (`./foo`, `../bar` → try .js, index.js, package.json)
- **cleanup**: Frees cached module names on bridge shutdown
- New includes: `<dlfcn.h>` (POSIX), `<io.h>` (Windows for `_mktemp_s`)
- Registered `__bridge_npm_install` as JS-callable global

### JS-level changes (`node_builtins.js`):
- **`require.cache`**: JS-side cache object (complements C cache)
- **`require.resolve`**: Resolves module names to file paths using `__bridge_fs_exists`
- **`require.main`**: Reference to the "main" module
- **`require.extensions`**: `.js` and `.json` handlers
- **`rust:` prefix**: Intercepted in JS wrapper → calls C `js_require` → `js_rust_load`
- **Module class**: Full Node.js-style constructor with `id`, `exports`, `filename`, `loaded`, `parent`, `children`, `paths`

### What the user can now do at runtime:
```js
// Auto-downloads and loads ANY npm package on first require:
const lodash = require('lodash');

// Loads Rust crates on the fly (downloads, compiles to cdylib, dlopen):
const serde = require('rust:serde');

// Relative requires work correctly within modules:
const helper = require('./helper');

// Require.resolve for path lookup:
const path = require.resolve('some-package');
```

### Known limitations:
- Scoped npm packages (`@scope/pkg`) not supported in auto-install (no scope dir creation)
- Rust cdylib auto-discover only returns basic `__rust_crate` metadata; function registration requires implementing `rust_bridge_get_fns()` in the generated wrapper
- Module resolution walks up from `g_current_dir`; shared node_modules in ancestor dirs work correctly
- Auto-install downloads tarballs via HTTP to temp files; requires `tar` and `curl` on POSIX
