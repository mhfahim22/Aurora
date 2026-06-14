# Contributing to Aurora

## Code of Conduct

Be respectful, constructive, and inclusive. We have zero tolerance for harassment.

## Getting Started

1. **Build the project** — see [README.md](README.md#quick-start)
2. **Run the tests** — `cmake --build build --target test_bridge_e2e --config Release && build\Release\test_bridge_e2e.exe`
3. **Check ASAN** — `.\build_asan.bat && build_asan\Debug\test_ffi_memory_safety.exe`

## Development Workflow

### Branch Strategy

- `main` — stable, release-ready
- `develop` — integration branch for features
- `feature/*` — feature branches, merged to `develop` via PR

### Before Submitting a PR

1. **Run the full test suite:**
   ```bash
   cmake --build build --config Release --target test_bridge_e2e --target test_ffi_memory_safety --target test_pypi_thread_safety --target test_integration_http --target test_unified_type_system --target test_universal_bridge -j2
   build\Release\test_bridge_e2e.exe
   build\Release\test_ffi_memory_safety.exe
   build\Release\test_pypi_thread_safety.exe
   build\Release\test_integration_http.exe
   ```
2. **Run ASAN tests:**
   ```bash
   .\build_asan.bat
   build_asan\Debug\test_ffi_memory_safety.exe
   ```
3. **Rebuild voss** if changing bridge templates, then regenerate at least one bridge DLL

## Code Style

- **C++**: C++23, 4-space indent, `camelCase` for functions, `snake_case` for variables, `g_` prefix for globals
- **C (bridge templates)**: C11/C17 subset, same naming as C++
- **Batch scripts**: `@echo off` + `setlocal enabledelayedexpansion` at top, errorlevel checks after every build command
- **PowerShell scripts**: `$RepoRoot = $PSScriptRoot`, use `Write-Host` for output

## Pull Request Guidelines

1. One feature/fix per PR
2. Include test cases for new functionality
3. Update `PRODUCTION_PLAN.md` if changing phase status
4. Update documentation if changing public API (bridge exports, error codes, etc.)
5. Keep bridge DLLs ASAN-clean

## Bridge Development

### Adding a New Bridge Export

1. Add the C function template in `aurora/tools/voss/bridge_<eco>.cpp`
2. Add the `.auf` binding in the `gen_<eco>_au_binding()` function
3. Add the export declaration (if needed) to the C/JS/Rust codegen
4. Rebuild voss: `cmake --build build --target voss`
5. Regenerate the bridge: `voss bridge <eco> <pkg>`
6. Test: `build\Release\test_bridge_e2e.exe`

### Testing Across Ecosystems

- Bridges are tested via `test_bridge_e2e.cpp` which uses `dl_open`/`LoadLibrary` + `dl_sym`/`GetProcAddress`
- Add new tests as `TEST("name")` / `CHECK(cond, msg)` / `PASS()` / `FAIL(msg)` blocks
- PyPI tests require the package installed: `pip install <pkg>`
- npm tests require the bridge generated: `voss bridge npm <pkg>`

## Release Process

See [RELEASE.md](RELEASE.md) for versioning and release procedures.
[CHANGELOG.md](CHANGELOG.md) for version history.
[VERSION](VERSION) for the current version number.
