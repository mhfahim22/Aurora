## Goal
All 30 phases complete — Aurora v1.0.0 stable release.

## Constraints & Preferences
- Follow established module pattern where applicable: header → impl → `.auf` bindings → exports → typechecker entries → codegen entries → CMakeLists.txt update
- Build must succeed with zero errors on Windows (desktop)

## Progress
### Done
- **Phase 14 — Serialization**: JSON/Binary serialize/deserialize, file I/O, format auto-detection, `serial.auf` bindings. 9 exports + typechecker + codegen entries. Build verified.
- **Phase 15 — Database**: SQLite3 amalgamation bundled. `db.hpp` + `db.cpp` C API (28 functions). 28 exports + 28 typechecker entries + 28 codegen entries. Build verified.
- **Phase 16 — Mobile Runtime**: Android & iOS runtime infrastructure. 35 exports + 34 typechecker entries + 35 codegen declarations. Build verified.
- **Phase 17 — Mobile Widgets**: Cross-platform widget engine. 34 exports + typechecker + codegen entries. Build verified.
- **Phase 18 — Desktop Integration**: Win32 desktop module (system tray, notifications, clipboard, DND, file assoc, startup, DWM effects, hotkeys). 27 exports + typechecker + codegen entries. Build verified.
- **Phase 19 — OpenGL & Game Support**: Lighting, tilemap, mesh primitives. 29 exports + fixed missing GL/sprite2d/animation entries. Build verified.
- **Phase 20 — Plugin System**: Native plugin loading with ABI contract, reflection API. 20 exports + typechecker + codegen entries. Build verified.
- **Phase 21 — Package Manager**: C API wrapping voss CLI + direct file I/O. 18 exports + typechecker + codegen entries. Build verified.
- **Phase 22 — Build System**: Parallel compilation (`--jobs N`), cross-compilation (`--target triple`). TokenType → TokenKind rename. Build verified.
- **Phase 23 — Hot Reload**: File watcher, code/asset reload, state preservation, console. 22 exports + typechecker + codegen entries. Build verified.
- **Phase 24 — Testing Framework**: Unit/Integration/Widget/Benchmark/Snapshot/Coverage. 30 exports + typechecker + codegen entries. Build verified.
- **Phase 25 — Developer Tools**: Formatting, linting, LSP stubs, completions, debugger, profiler, inspector, memory, perf monitor. `dev.auf`. 34 exports + 34 typechecker + 34 codegen entries. CMakeLists.txt. Build verified.
- **Phase 26 — Documentation**: Updated stdlib/api ref, created cookbook/migration/best-practices. Build verified.
- **Phase 27 — Security**: Sandbox, permissions, secure storage, encryption, certificates, hashing, auth helpers. `security.hpp/.cpp/.auf`. 30 exports + 30 typechecker + 30 codegen entries. CMakeLists.txt. Build verified.
- **Phase 28 — Performance Optimization**: Pool allocator with thread-local caches (5 buckets: 8/16/32/64/128 bytes). Work-stealing scheduler with per-thread dequeues. Lock-free SPSC channel fast path for power-of-2 capacities. GC lock upgraded to SRWLock shared/exclusive. Optimizer: CSE, load/store forwarding, algebraic simplification, iteration limit 5→10. ThinLTO pipeline (GVN, MemCpyOpt, SCCP, InferAddressSpaces). CMakeLists.txt size flags (`/Gy`, `/OPT:REF`, `/OPT:ICF`, `-ffunction-sections`, `-fvisibility=hidden`). Build verified — zero errors.
- **Phase 29 — Cross-Platform Validation**: CMakePresets.json (9 presets: Windows, Linux x64, macOS x64/arm64, Android NDK, iOS device/simulator). Linux/macOS build scripts (`build_linux.sh`, `build_macos.sh`). Android/iOS build scripts (`build_android.sh`, `build_ios.sh`). Dockerfile for reproducible Linux builds. CI updated with `test_crossplatform` validation step. Android emulator (`test_android_emulator.sh`) and iOS simulator (`test_ios_simulator.sh`) test scripts. `test_crossplatform` CTest target (9 tests: platform detection, threading, filesystem, performance). `docs/cross_platform_validation.md` build guide. All 23 targets build verified — zero errors.
- **Phase 30 — Stable Release**: Version bumped to 1.0.0. CHANGELOG.md restructured with all 30 phases documented. RELEASE.md checklist updated. `aurora_version.hpp` and `setup.iss` version consistent. `check_release_readiness.ps1` validates build/tests/docs/versions before tagging. GitHub Actions release workflow packages + uploads artifacts for all platforms. All 23 targets build verified — zero errors.

## Key Decisions
- Binary serialization uses TLV (Type-Length-Value) with 1-byte tag + variable payload
- SQLite amalgamation compiled as C source directly into `aurora_runtime`
- Mobile functions are platform-conditional; desktop stubs guarded by `AURORA_PLATFORM_*` macros
- `.auf` bindings expose both low-level `extern function` and high-level Aurora wrapper functions
- Mobile Widgets are purely cross-platform C++ with platform-agnostic layout/hit-testing
- Desktop Integration is Win32-native (NOTIFYICONDATAW, WM_DROPFILES, DWM, etc.)
- TokenType → TokenKind rename resolves Windows SDK `TOKEN_INFORMATION_CLASS` conflict
- Security module leverages existing `crypto.cpp` for SHA-256, AES, HMAC, Base64
- Pool allocator uses thread-local caches per bucket (8/16/32/64/128 bytes) with no-lock fast path
- GC lock upgraded to SRWLock shared/exclusive (RWLock) for reduced contention
- Scheduler uses work-stealing with per-thread dequeues (via `std::unique_ptr<WorkerQueue>`)  
- Channels use lock-free SPSC fast path for power-of-2 capacities
- CMakeLists.txt size flags: MSVC `/Gy` `/OPT:REF` `/OPT:ICF`; GCC/Clang `-ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--icf=safe -fvisibility=hidden`
- Build system parallel compilation uses `std::thread` with atomic work-stealing across source files; each worker thread owns its own `LLVMContext` for thread safety
- Cross-platform validation uses a single C++ test binary (`test_crossplatform`) that runs on all 5 target platforms with no external dependencies
- CMakePresets.json provides standardized one-command configure/build for every supported platform
- Release version single-sourced from `VERSION` file; `aurora_version.hpp`, `setup.iss` must match

## CI / Build Status
- **✅ CI is fully green across all 3 platforms** (macOS, Ubuntu, Windows) — run #134 (commit `4042178`).
- **CI Repair History**: 15 fixes across 10 CI runs (#125–#134) after Phase 25–30 additions broke compilation:
  - **Run #125**: 5 source-level fixes (`bridge_quickjs.cpp` dangling delete, `bridge_rust.cpp` lib_handle, `websocket.cpp` m_ prefix, `tls.cpp` LOAD macro, `fiber.cpp` unreachable static_assert)
  - **Run #126**: `tls.cpp` void* member access + `engine.cpp` missing `<cstdlib>`
  - **Run #128**: `gui_mac.mm` missing `@class` forward decl + `build_cache.hpp` `__int128` MSVC streaming
  - **Run #129**: Windows LNK error → `CMakeLists.txt` added `audio.cpp`
  - **Run #131**: `audio.cpp` `miniaudio.h` include path fix
  - **Run #132**: `aurora/include/std/audio.hpp` missing from git
  - **Run #133**: `regression.ps1` / `test_ir_verify.ps1` — `opt.exe` PATH lookup (`Test-Path` doesn't search PATH)
  - **Run #134**: ✅ **All 3 platforms green** (macOS + Ubuntu + Windows)
- Windows regression tests now pass via `Get-Command` fallback for `opt.exe` in PATH.
- **Local Build Fix (commit 895797a)**: 80 new files, 273K insertions — CMakeLists.txt was missing `aurora/src/std/*.cpp` (19 files), `aurora/src/mobile/widgets.cpp`, `third_party/sqlite3/sqlite3.c`, `aurora/src/compiler/build_system.cpp`, plus `test_crossplatform` target. `runtime_exports.hpp` had 799 locally-added exports for Phase 14-30 that weren't in the repo. Build verified 0 errors.

## Next Steps
- *(all 30 phases complete — Aurora v1.0.0 released, CI green on all platforms)*

## Critical Context
- `memory.hpp`: Pool API defined with `#define POOL_BUCKET_COUNT 5` + `extern "C"` declarations for `aurora_pool_alloc/free/cleanup`
- `memory.cpp`: Pool allocator code must be placed BEFORE `aurora_alloc` (which calls `pool_bucket_index`). Pool structs, TLS arrays, and helper functions precede the heap allocator section
- `scheduler.cpp`: `std::vector<WorkerQueue>` fails because `WorkerQueue` has `std::mutex` (non-copyable). Fixed by using `std::vector<std::unique_ptr<WorkerQueue>>` with per-thread allocation
- `main.cpp` Phase 28: `MemCpyOpt.h` renamed to `MemCpyOptimizer.h` in LLVM 18; `cfg->use_lto` was undeclared (fixed to `use_lto`); `LoopUnrollPass` requires `LoopNest` level (removed — already in default pipeline)
- `f32` was already defined at line 792 in `codegen_runtime.cpp`; adding a second `auto* f32` at line 1381 caused C2374 redefinition error (fixed by removing the duplicate)
- `desktop.cpp` uses `DWM_SYSTEMBACKDROP_TYPE` (Win10 20H1+) — older Windows versions silently skip; fallback DWMWA constants defined with `#ifndef` guards
- Package manager uses `popen`/`_popen` to call the existing `voss` CLI tool
- All 23+ targets build with zero errors
- 1218 runtime exports via `runtime_exports.hpp` (799 added locally for Phase 14-30)
- `runtime_exports.hpp` must match the files compiled in CMakeLists.txt or MSVC linker errors (LNK2001) occur
- `db.cpp` uses `third_party/sqlite3/sqlite3.h` — `CMakeLists.txt` must include `third_party/` include path
- `gui.cpp` on Win32 uses `#include "gui_win32.cpp"` — do NOT add `gui_win32.cpp` separately to CMakeLists.txt
- Release readiness verified via `scripts/check_release_readiness.ps1`

## Relevant Files
- **Phase 28 — Performance Optimization**:
  - `aurora/include/runtime/memory.hpp`: Pool allocator API (`POOL_BUCKET_COUNT`) and function declarations
  - `aurora/src/runtime/core/memory.cpp`: Pool allocator implementation, RWLock GC, updated `aurora_alloc`
  - `aurora/src/compiler/aurora_optimizer.cpp`: CSE, load/store forwarding, algebraic simplify, iter 5→10
  - `aurora/src/runtime/async/scheduler.cpp`: Work-stealing thread pool with `unique_ptr<WorkerQueue>`
  - `aurora/src/runtime/async/channel.cpp`: Lock-free SPSC fast path for power-of-2 capacities
  - `aurora/src/compiler/main.cpp`: ThinLTO pipeline, GVNPass, MemCpyOptPass, SCCPPass, InferAddressSpacesPass
  - `CMakeLists.txt`: Size reduction flags (`/Gy`, `/OPT:REF`, `/OPT:ICF`, `-ffunction-sections`, `-fvisibility=hidden`)
- **Phase 29 — Cross-Platform Validation**:
  - `CMakePresets.json`: 9 cross-platform build presets
  - `scripts/build_linux.sh`: Linux x64 desktop build script
  - `scripts/build_macos.sh`: macOS desktop build script (Intel + Apple Silicon)
  - `scripts/build_android.sh`: Android NDK cross-compile + Gradle APK
  - `scripts/build_ios.sh`: iOS device/simulator build
  - `scripts/test_android_emulator.sh`: Android emulator validation
  - `scripts/test_ios_simulator.sh`: iOS simulator validation
  - `aurora/tests/crossplatform/validate.cpp`: Cross-platform validation suite (9 tests)
  - `docs/cross_platform_validation.md`: Cross-platform build and test guide
  - `Dockerfile`: Reproducible Linux build environment
  - `.github/workflows/build.yml`: CI updated with `test_crossplatform` step on all 3 runners
- **Phase 30 — Stable Release**:
  - `VERSION`: `1.0.0`
  - `CHANGELOG.md`: Full changelog for all 30 phases
  - `RELEASE.md`: Release process and checklist
  - `release/setup.iss`: Inno Setup installer (v1.0.0)
  - `aurora/include/common/aurora_version.hpp`: Version constants (`AURORA_VERSION_STRING "1.0.0"`)
  - `scripts/check_release_readiness.ps1`: Pre-tag validation script
  - `.github/workflows/release.yml`: Automated release packaging workflow
  - `release/install.sh`, `release/install.ps1`: One-command installers
- **Previous phases**: serial, db, mobile, widgets, desktop, game, plugin, pkg, hotreload, test, dev, security — as listed in Progress above
