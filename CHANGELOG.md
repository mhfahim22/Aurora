# Changelog

## v1.0.0 (2026-07-03) — Stable Release

Aurora v1.0.0 is the first stable release. All 30 phases complete, 23 build targets, zero errors.

### Phase 30: Stable Release
- **Version 1.0.0**: Bumped from `1.0.0-rc.1` — stable ABI, no breaking changes.
- **Release infrastructure**: Inno Setup installer, Linux/macOS install scripts, GitHub Actions release workflow with auto-packaging for all 3 platforms.
- **Release readiness script**: `scripts/check_release_readiness.ps1` validates build, tests, docs, versions before tagging.
- **Full CI/CD pipeline**: Release builds + packages + uploads artifacts automatically on tag push.

### Phase 29: Cross-Platform Validation
- **CMakePresets.json**: 9 presets covering Windows, Linux x64, macOS x64/arm64, Android NDK, iOS device/simulator.
- **Build scripts**: `build_linux.sh`, `build_macos.sh`, `build_android.sh`, `build_ios.sh` for all 5 targets.
- **Dockerfile**: Reproducible Linux build environment (Ubuntu 22.04 + LLVM 19).
- **Validation suite**: `test_crossplatform` CTest target with 9 tests (platform detection, threading, filesystem, performance). Runs on all 3 CI runners.
- **Android/iOS validation**: `test_android_emulator.sh`, `test_ios_simulator.sh` for device/simulator testing.
- **Cross-platform docs**: `docs/cross_platform_validation.md` with build guide for all platforms.

### Phase 28: Performance Optimization
- **Pool allocator**: Thread-local caches (5 buckets: 8/16/32/64/128 bytes) — no-lock fast path for small objects.
- **Work-stealing scheduler**: Per-thread dequeues (`unique_ptr<WorkerQueue>`) — steal-from-back strategy.
- **Lock-free SPSC channel**: Power-of-2 capacity fast path — bypasses mutex for single-producer/single-consumer.
- **GC lock**: Upgraded to SRWLock shared/exclusive — reduced contention vs plain mutex.
- **Optimizer passes**: CSE, load/store forwarding, algebraic simplification, iteration limit 5→10.
- **ThinLTO pipeline**: GVNPass, MemCpyOptPass, SCCPPass, InferAddressSpacesPass added.
- **Binary size flags**: `/Gy`, `/OPT:REF`, `/OPT:ICF` (MSVC), `-ffunction-sections`, `-fvisibility=hidden` (GCC/Clang).

### Phase 27: Security
- **Sandbox**: Path whitelist validation for file I/O.
- **Permission model**: Grant/revoke/list runtime permissions.
- **Secure storage**: AES-256-CBC encrypted key-value store.
- **Encryption**: Key/IV generation, AES encrypt/decrypt, PBKDF2 key derivation.
- **Certificates**: Load/info/verify/free X.509 certificates.
- **Hashing**: SHA-256, HMAC-SHA256, password hash/verify (bcrypt-compatible).
- **Authentication**: HMAC token gen/verify, Basic/Bearer auth header generation.

### Phase 26: Documentation
- **Stdlib reference**: Updated `reference/16-stdlib.md` covering Phases 14-25.
- **API reference**: Updated `api_reference.md` with all new module entries.
- **Cookbook**: 30 recipes across 15 categories.
- **Migration guide**: v0.x → v1.0 breaking changes and migration paths.
- **Best practices**: Naming, project organization, database, performance, testing, platform conventions.

### Phase 25: Developer Tools
- **Formatter**: Indentation engine with configurable style.
- **Linter**: Basic diagnostic scan (unused variables, type mismatches).
- **LSP stubs**: Language server protocol foundations.
- **Completions**: Word-prefix and context-aware completion.
- **Debugger stubs**: Breakpoint management, stack inspection.
- **Profiler**: Per-function timing counters.
- **Inspector stubs**: AST and symbol table inspection.
- **Memory viewer**: Arena/ARC/GC usage tracking.
- **Performance monitor**: Real-time FPS, memory, CPU metrics.

### Phase 24: Testing Framework
- **Unit tests**: Suite/test case registration, assertions (eq, ne, true, false, gt, lt, approx).
- **Integration tests**: Test server with lifecycle hooks.
- **Widget tests**: UI component rendering with synthetic events.
- **Benchmarks**: High-resolution timing, loops, warmup.
- **Snapshot testing**: File-based golden output comparison.
- **Coverage**: Line/function/branch counters for tracked regions.

### Phase 23: Hot Reload
- **File watcher**: Polling-based with mtime delta detection.
- **UI reload**: Callback registration for UI reconstruction.
- **Code reload**: Module versioning with staleness check.
- **Asset reload**: Dirty-bit tracking for images, audio, data.
- **State preservation**: Key-value string store across reloads.
- **Developer console**: Log buffer, command execution.

### Phase 22: Build System
- **Parallel compilation**: `--jobs N` with thread-pool worker dispatch.
- **Cross-compilation**: `--target triple` for target-aware LLVM IR generation.
- **TokenType → TokenKind**: Renamed to resolve Windows SDK `TOKEN_INFORMATION_CLASS` conflict.

### Phase 21: Package Manager
- **18 C API functions**: install/remove/update/publish/search, registry config, login, lock file, dependency resolution, offline cache.
- **voss CLI integration**: Wraps existing CLI tool via `popen`/`_popen`.

### Phase 20: Plugin System
- **Native plugins**: LoadLibrary/dlopen with standard ABI contract.
- **Plugin registry**: Load/unload/scan/query registered plugins.
- **Reflection API**: Type, field, method enumeration.
- **Version compatibility**: ABI version check prevents incompatible plugins.

### Phase 19: OpenGL & Game Support
- **Lighting**: 10 functions — create/destroy/set_position/direction/color/intensity/range/spot_angle/get_count/get.
- **Tilemap**: Multi-layer grid with solid check, properties.
- **Mesh primitives**: Plane/sphere/cylinder/capsule with interleaved pos3+norm3+tex2 + indices.
- **GL/sprite2d/animation**: Fixed all missing typechecker/codegen entries (79 total).

### Phase 18: Desktop Integration (Win32)
- **System tray**: Persistent icon + context menu + balloon notifications.
- **Notifications**: Win32 balloon tooltip via NOTIFYICONDATAW.
- **Clipboard**: Get/set text via Win32 API.
- **Drag & drop**: WM_DROPFILES with file path callbacks.
- **File associations**: HKCU registry registration.
- **Startup registration**: HKCU Run key.
- **Window effects**: DWM acrylic/mica/blur/dark mode/rounded corners.
- **Global hotkeys**: RegisterHotKey dispatch via hidden window.

### Phase 17: Mobile Widgets
- **Cross-platform widget engine**: 34 functions, 16 widget types.
- **Flexbox layout**: Column/Row/Grid.
- **Hit-testing**: Event dispatch with touch support.
- **Platform-agnostic**: Pure C++ — no `#ifdef` needed.

### Phase 16: Mobile Runtime
- **Android**: JNI bridge, NativeActivity lifecycle, touch/sensors/permissions.
- **iOS**: UIKit bridge, Metal renderer, touch/haptics/bundle paths.
- **Desktop stubs**: 35 mobile symbols resolve with safe defaults on desktop.
- **APK/IPA build**: `build_apk.bat`, `build_ipa.sh`.

### Phase 15: Database (SQLite3)
- **SQLite3 amalgamation**: Bundled in `third_party/sqlite3/`.
- **28 C API functions**: Connection mgmt, prepared statements, transactions, utility.
- **28 exports + 28 typechecker + 28 codegen entries**.

### Phase 14: Serialization
- **JSON**: Native C JSON library with parse/serialize.
- **Binary**: Compact TLV format with 7 type tags.
- **File I/O**: Format auto-detection from extension.
- **9 exports + typechecker + codegen entries**.

### Phases 0–13: Foundation
- **Core language**: OOP, generics, pattern matching, ownership, async/await.
- **Memory management**: Stack, arena, RAII, ARC, GC.
- **GUI framework**: 35 widgets, layout system, reactive state, animation.
- **Graphics**: Canvas 2D, image processing, video playback.
- **Audio**: Playback (WAV/MP3/FLAC/OGG), recording, effects.
- **Networking**: HTTP/HTTPS, WebSocket, TCP/UDP, DNS.
- **Threading**: Fiber engine, thread pool, channels, futures.

## v1.0.0-rc.1 (2026-07-02) — Release Candidate 1

### Phase 4: Release Engineering
- **Regression Test Suite (4.1)**: Created `scripts/regression.ps1` with 7 stages.
- **Performance Profiling (4.2)**: Added `--timing` flag to compiler.
- **Release Packaging (4.3)**: Installers for all platforms, GitHub Actions release workflow.

### Phase 3: Standard Library & Production Web
- **JSON User Bindings (3.1)**: 18/18 extern declarations verified.
- **TLS/SSL & WebSocket Certification (3.2)**: 6/6 server integration tests pass.
- **Std Library Coverage Audit (3.3)**: All 37 `.auf` files matched.
- **Cross-Platform GUI Completion (3.4)**: Win32/X11/Cocoa fully implemented.

### Phase 2: Advanced Features
- **DWARF Debug Info (2.1)**: `--debug`/`-g` flag with LLVM DIBuilder.
- **Generics/Monomorphization (2.2)**: `<T, U>` params, instantiation, mangling.
- **Cross-Ecosystem FFI Bridge (2.3)**: Python/QuickJS/Rust bridges.
- **Incremental Compilation (2.4)**: SHA-256 build cache.

### Phase 1: Core Stability
- **Fiber Engine (1.1)**: Async runtime, event bus, channels.
- **Autograd System (1.2)**: Backward pass, gradient computation.
- **LLVM IR Verification (1.3)**: 49/49 examples pass.
- **ASan Cleanup (1.4)**: Zero errors, `/MD` CRT fix.
- **Code Quality (1.5)**: Parser error recovery, no C-style casts.

### Phase 0: Pre-Flight Audit
- Tensor API redundancy resolved
- Parser error recovery with panic_recover
- Import cycle detection
- Type checker safety

## v0.3.0-h3 (2026-06-26) — Annotation-Aware ABI Migration

- Annotation-first type system and code generation
- Typed ABI generation with `ast_kind_to_abi_type()`
- Typed indirect dispatch for closures, function pointers, OOP vtables
- Legacy boolean flags fully migrated to `AstTypeKind` enum
