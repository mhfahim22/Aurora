# NPM Bridge MSVC Build Fix — root cause & permanent strategy

## Root cause
QuickJS targets GCC/Clang and uses `__attribute__`, computed gotos (`&&label`),
GCC builtins (`__builtin_clz`, `__builtin_ctz`, `__builtin_expect`,
`__builtin_frame_address`), and struct identity casts `(JSValue)v`.  None of
these are valid in MSVC C89/C17 mode.  Files that lacked `#include
"quickjs_config.h"` (the compatibility shim designed for this) hit compiler
errors first, masking deeper issues like DLL-imported CRT function pointers in
`static const` initializers (C2099).

## Strategy — upstream-intent approach
`quickjs_config.h` was already present and documented as "force-included first
via -include /FI" but the CMake build system never actually forced it.  The fix
enables the force-include systemically in `add_npm_bridge()`, then patches the
compatibility header to handle all remaining MSVC gaps, and applies four
targeted source changes for issues the header cannot paper over.

## Changes made

### 1. `deps/quickjs/quickjs_config.h` (shim — three gaps fixed)
- **Added include guard** (`#ifndef QUICKJS_CONFIG_H` / `#define`) so the
  force-include doesn't clash with explicit `#include` in bridge C files.
- **Added `WIN32_LEAN_AND_MEAN` + `#include <winsock2.h>`** before our
  `struct timeval` definition, so the system provides the type first,
  eliminating the redefinition when other headers pull in `winsock2.h`.
- **Extended MSVC builtin coverage**: `__builtin_clz`, `__builtin_clzll`,
  `__builtin_expect`, `__builtin_frame_address` (already had `__builtin_ctz`
  and `__builtin_ctzll`).
- **Added `#define __attribute(x)`** (without trailing underscores) — QuickJS
  uses both `__attribute__` and `__attribute` (the shorter form).
- **Added `#define JS_NAN_BOXING 1`** so JSValue becomes `uint64_t` on MSVC,
  avoiding C2440 "cannot convert from 'JSValue' to 'JSValue'" on struct
  identity casts spread across 12+ sites in quickjs.c.
- **Added `#define __JS_INF`** to compute `+Inf` via `1.0e308 * 1.0e308`
  instead of `1.0 / 0.0` (MSVC C2124 on compile-time division by zero).

### 2. `CMakeLists.txt` — `add_npm_bridge()` (systemic force-include)
- Added `/FI"quickjs_config.h"` and `/std:c11` for MSVC.
  This ensures every bridge target gets the shim automatically.

### 3. `deps/quickjs/quickjs.c` — three targeted patches
- **Replaced `1.0 / 0.0` with `__JS_INF`** (4 occurrences) to avoid C2124.
- **Added static math wrappers** (`js__fabs`, `js__floor`, …) for CRT
  functions used in `js_math_funcs[]`.  MSVC+/MD marks CRT functions
  `__declspec(dllimport)`, whose addresses are not compile-time constants,
  triggering C2099.
- **Set `DIRECT_DISPATCH 0` for `_MSC_VER`** to avoid GCC computed-goto
  extensions (`&&label`).

### 4. `deps/quickjs/quickjs.h` — struct identity cast
- Guarded `(JSValue)v` casts in `JS_DupValue` / `JS_DupValueRT` with
  `#if !defined(_MSC_VER)` / `#else return v` (redundant after the
  `JS_NAN_BOXING` change but kept as defensive belt).

### 5. `/deps/quickjs/dtoa.c` — local `__attribute__` define removed
- Reverted the ad-hoc `#ifdef _MSC_VER` / `#define __attribute__` that was
  added earlier; now covered by the systemic force-include.

## Verification
```
cmake --build . --config Release --target bridge_uuid
  → bridge_uuid.dll  (0 errors, 0 warnings)
```
The fix applies to all 8 npm bridge targets through `add_npm_bridge()`.
