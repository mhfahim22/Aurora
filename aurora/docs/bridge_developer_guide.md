# Bridge Developer Guide

How to create custom bridges for new ecosystems in Aurora.

---

## 1. Overview

A bridge is a shared library (DLL/SO/dylib) that exposes Aurora's bridge API, allowing Aurora code to call functions from another language ecosystem. Aurora supports four bridge types:

| Type | Mechanism | Thread Safety |
|------|-----------|---------------|
| PyPI | Python C API + GIL | GIL-protected |
| npm (QuickJS) | QuickJS embedded + mutex | Mutex-protected |
| npm (subprocess) | Node.js subprocess + pipe lock | Process-isolated |
| Cargo | Native Rust DLL + Arc<Mutex<>> | Arc<Mutex<>> |

---

## 2. Bridge API

Every bridge must export these C functions:

### 2.1 Required Exports

```c
// Initialize the bridge
void __bridge_init(void);

// Shut down the bridge, free all resources
void __bridge_shutdown(void);

// Import a module/library. Returns a module handle (int64_t).
int64_t __bridge_import(const char* name);

// Free a module handle
void __bridge_free_module(int64_t handle);
```

### 2.2 Calling Functions

```c
// Call a function (no arguments)
int64_t __bridge_call_0(int64_t module, const char* func_name);

// Call with one integer argument
int64_t __bridge_call_1(int64_t module, const char* func_name, int64_t arg1);

// Call with two integer arguments
int64_t __bridge_call_2(int64_t module, const char* func_name, int64_t arg1, int64_t arg2);

// Call with three integer arguments
int64_t __bridge_call_3(int64_t module, const char* func_name, int64_t arg1, int64_t arg2, int64_t arg3);
```

### 2.3 String Conversion

```c
// Create a bridge string from a C string
int64_t __bridge_str(const char* s);

// Convert a bridge string to C string
const char* __bridge_to_cstr(int64_t handle);

// Free a bridge string
void __bridge_free_string(int64_t handle);
```

### 2.4 Callback Support

```c
// Set a callback that Aurora code can invoke
void __bridge_set_callback(int64_t module, const char* name, int64_t callback_id);

// Callback invocation (called from bridge)
int64_t __bridge_invoke_callback(int64_t callback_id, int64_t arg);
```

### 2.5 Diagnostic Exports

```c
// Get performance statistics as JSON string
const char* __bridge_get_perf_stats(void);

// Get last error message
const char* __bridge_get_last_error(void);
```

### 2.6 Error Handling

| Error Code | Description |
|-----------|-------------|
| `0` | Success |
| `-1` | General error |
| `-2` | Module not found |
| `-3` | Function not found |
| `-4` | Type conversion error |
| `-5` | Timeout |
| `-6` | Memory allocation failure |
| `-7` | Thread safety violation |

---

## 3. Creating a New Bridge Type

### Step 1: Define the Bridge Header

Create a header in `aurora/include/runtime/bridge/`:

```c
// bridge_myeco.h
#pragma once
#include <stdint.h>

#ifdef _WIN32
#define BRIDGE_EXPORT __declspec(dllexport)
#else
#define BRIDGE_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
    BRIDGE_EXPORT void __bridge_init(void);
    BRIDGE_EXPORT void __bridge_shutdown(void);
    BRIDGE_EXPORT int64_t __bridge_import(const char* name);
    BRIDGE_EXPORT void __bridge_free_module(int64_t handle);
    BRIDGE_EXPORT int64_t __bridge_call_0(int64_t module, const char* func_name);
    BRIDGE_EXPORT int64_t __bridge_call_1(int64_t module, const char* func_name, int64_t arg1);
    BRIDGE_EXPORT int64_t __bridge_str(const char* s);
    BRIDGE_EXPORT const char* __bridge_to_cstr(int64_t handle);
    BRIDGE_EXPORT void __bridge_free_string(int64_t handle);
    BRIDGE_EXPORT const char* __bridge_get_perf_stats(void);
    BRIDGE_EXPORT const char* __bridge_get_last_error(void);
}
```

### Step 2: Implement the Bridge

Create a source file in `aurora/src/runtime/bridge/`:

```cpp
// bridge_myeco.cpp
#include "bridge/bridge_myeco.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <cstring>

static std::mutex g_mutex;
static std::string g_last_error;
static bool g_initialized = false;
static std::unordered_map<int64_t, void*> g_modules;
static int64_t g_next_handle = 1;

extern "C" {

BRIDGE_EXPORT void __bridge_init(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized) {
        // Initialize your ecosystem
        g_initialized = true;
    }
}

BRIDGE_EXPORT void __bridge_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_mutex);
    // Free all modules
    g_modules.clear();
    g_initialized = false;
}

BRIDGE_EXPORT int64_t __bridge_import(const char* name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    // Load module from ecosystem
    void* module = load_my_eco_module(name);
    if (!module) {
        g_last_error = "Module not found: " + std::string(name);
        return -2;
    }
    int64_t handle = g_next_handle++;
    g_modules[handle] = module;
    return handle;
}

BRIDGE_EXPORT void __bridge_free_module(int64_t handle) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_modules.find(handle);
    if (it != g_modules.end()) {
        free_my_eco_module(it->second);
        g_modules.erase(it);
    }
}

// ... implement call_0 through call_3, str, to_cstr, free_string, etc.

}
```

### Step 3: Thread Safety Pattern

Aurora supports three thread safety models. Choose one:

**Option A: Mutex (npm QuickJS style)**
```cpp
static std::recursive_mutex g_mutex;
// Wrap every exported function with lock_guard
```

**Option B: GIL (PyPI style)**
```cpp
PyGILState_STATE gstate = PyGILState_Ensure();
// ... do work ...
PyGILState_Release(gstate);
```

**Option C: Arc<Mutex<>> (Cargo style)**
```rust
use std::sync::{Arc, Mutex};
// Wrap all state in Arc<Mutex<>>
```

### Step 4: Register in Aurora's Bridge Discovery

Edit `aurora/src/runtime/bridge_discovery.cpp`:

```cpp
enum class BridgeType {
    Pypi,
    NpmQuickJS,
    NpmSubprocess,
    Cargo,
    MyEco   // Add your bridge type
};

BridgeType detect_bridge(const std::string& module_name) {
    // Add detection logic
    if (module_name.ends_with(".myeco")) return BridgeType::MyEco;
    // ...
}
```

---

## 4. Testing a Bridge

### 4.1 Unit Test in Aurora

```aura
import "stdio"

function test_bridge()
    mod = myeco_import()
    if mod == 0
        output("FAIL: import failed")
        return

    result = myeco_call0(mod, "hello")
    if result == 0
        output("FAIL: call failed")
        return

    output("PASS: bridge works")
    myeco_free(mod)

test_bridge()
```

### 4.2 Memory Safety Verification

- Run with AddressSanitizer (ASAN): `cmake -DCMAKE_BUILD_TYPE=ASAN`
- Run with Application Verifier (Windows): `appverif /verify myeco_bridge.dll`
- Check for leaks after repeated import/call/free cycles

### 4.3 Performance Benchmarks

```aura
@performance function bench_bridge_calls(n)
    mod = myeco_import()
    start = stamp()
    for i in n
        myeco_call0(mod, "noop")
    elapsed = stamp() - start
    output("ns/call: " + elapsed / n)
    myeco_free(mod)
```

---

## 5. Packaging a Bridge

### 5.1 Bridge Directory Structure

```
packages/bridges/myeco/
    bridge_myeco.dll         # compiled bridge library
    bridge_myeco.manifest    # metadata
    info.json                # version, dependencies
```

### 5.2 Manifest Format (`info.json`)

```json
{
    "name": "myeco",
    "version": "1.0.0",
    "type": "myeco",
    "description": "MyEcosystem bridge for Aurora",
    "dependencies": {
        "myeco-runtime": ">=3.0"
    },
    "thread_safety": "mutex",
    "platforms": ["win32", "linux", "macos"]
}
```

### 5.3 Installation

```bash
voss bridge myeco --install
```

Or from Aurora code:

```aura
install("myeco_bridge")
```

---

## 6. Existing Bridge Reference

### 6.1 PyPI Bridge Files

- `aurora/src/runtime/bridge_pypi.cpp` — 1100+ lines, Python C API, GIL management
- `aurora/include/runtime/bridge_pypi.h` — exports
- `packages/bridges/pypi/` — packaging

### 6.2 npm (QuickJS) Bridge Files

- `aurora/src/runtime/bridge_npm.cpp` — QuickJS embedded, mutex-protected
- `aurora/src/runtime/qjs_wrapper.cpp` — QuickJS wrapper generator
- `aurora/include/runtime/bridge_npm.h`

### 6.3 npm (subprocess) Bridge Files

- `aurora/src/runtime/bridge_npm_subprocess.cpp` — Node.js subprocess, pipe lock
- `packages/bridges/npm/` — JavaScript wrapper

### 6.4 Cargo Bridge Files

- `aurora/src/runtime/bridge_cargo.cpp` — Rust FFI, Arc<Mutex<>>
- `aurora/src/bindings/cargo/` — auto-generator
- `packages/bridges/cargo/`

---

## 7. Best Practices

1. **Always lock**: All bridge entry points must be thread-safe
2. **Never leak handles**: Match every `_import` with `_free_module`
3. **Never leak strings**: Match every `_str` with `_free_string`
4. **Use error codes**: Return negative codes on failure, set last error
5. **ASAN-clean**: Verify with AddressSanitizer before release
6. **Bound arguments**: Limit call arity; extend with call_N variants if needed
7. **Log diagnostics**: Populate `__bridge_get_perf_stats()` with useful data
8. **Test in isolation**: Test bridge standalone before integration

---

## 8. Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| LoadLibrary fails | CRT mismatch | Use `/MD` dynamic CRT |
| Random crashes | Thread safety violation | Add mutex/GIL |
| Memory leak | Missing `_free_string` | Track allocations |
| Wrong function called | Ordinal mismatch | Rebuild bridge |
| Timeout | Deadlock | Check lock ordering |
