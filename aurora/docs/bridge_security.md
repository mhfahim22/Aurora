# Aurora Bridge: Thread-Safety & Error Handling

## 1. Thread-Safety Model

### 1.1 Overview

Each bridge ecosystem uses a different concurrency model:

| Bridge | Model | Thread-Safe? | Details |
|--------|-------|-------------|---------|
| **PyPI** | GIL-based | ✅ Yes | Python GIL via `PyGILState_Ensure`/`Release` |
| **npm (QuickJS)** | Single-threaded + lock | ✅ Yes | Per-bridge mutex for JS runtime |
| **npm (subprocess)** | Mutex + pipe | ✅ Yes | `CriticalSection` (Win32) / `pthread_mutex_t` (POSIX) |
| **Cargo** | Per-rust bridge | ✅ Yes | Rust `Arc<Mutex<>>` per bridge state |

### 1.2 PyPI Bridge (GIL)

**Protocol:**
```
Thread A:          Thread B:
GIL_E()            GIL_E()
  Py_* calls         Py_* calls
GIL_R()            GIL_R()
```

**Rules:**
- Every exported function starts with `GIL_ENTER()` and ends with `GIL_RETURN(x)`
- `GIL_RETURN(x)` **releases the GIL before evaluating `x`** to avoid deadlocks when `x` calls Python
- Never call `PyEval_SaveThread()` inside a bridge — it causes hangs when interleaved with `PyGILState_Ensure`
- The GIL is acquired at most **once per Python C API call**
- `aurora_py_ensure_initialized()` is guarded by `std::mutex` + `std::atomic<int>` + `std::call_once`
- All `rt_pyapi()` pointers are read-only after init, safe for concurrent read

### 1.3 npm QuickJS Bridge

**Protocol:**
```
Thread A:                          Thread B:
qjs_call()                         qjs_call()
  mutex_lock(&g_qjs_lock)            mutex_lock(&g_qjs_lock)
    JS_Eval(...)                       JS_Eval(...)
  mutex_unlock(&g_qjs_lock)          mutex_unlock(&g_qjs_lock)
```

**Rules:**
- All JS operations (eval, call, getattr) are serialized by a per-bridge mutex
- The QuickJS runtime `g_rt` is single-threaded — concurrent access corrupts the heap
- `g_objs[]` array is protected by the same mutex
- Never hold the mutex across an I/O operation (pipe read, file write)

### 1.4 npm Subprocess Bridge

**Protocol:**
```
Thread A:                          Thread B:
send_rpc(req)                      send_rpc(req)
  EnterCriticalSection(&lock)        EnterCriticalSection(&lock)
    WriteFile(pipe)                    WriteFile(pipe)
    ReadFile(pipe)                     ReadFile(pipe)
  LeaveCriticalSection(&lock)        LeaveCriticalSection(&lock)
```

**Rules:**
- All RPCs (read+write) are serialized by a single critical section / mutex
- The Node.js subprocess processes requests sequentially
- Pipe reads use a shared read buffer (`g_read_buf`) protected by the same lock
- `g_rpc_ok` (volatile int) signals subprocess crash detection across threads
- `g_restarting` (atomic) prevents concurrent restart attempts

### 1.5 Cargo Bridge (Rust)

**Rules:**
- Each bridge DLL has its own Rust `Arc<Mutex<BridgeState>>`
- Rust's type system prevents data races at compile time
- Serde serialization/deserialization is lock-protected

---

## 2. Error Handling

### 2.1 Error Codes

Bridges use a consistent error reporting pattern:

| Code | Meaning | Where |
|------|---------|-------|
| `NULL` / `0` | Function failed | All bridge exports |
| Non-null | Success | All bridge exports |
| `_get_last_error()` | Error message string | PyPI, npm, Cargo bridges |
| `g_last_error` | Last error global | Internal bridge state |

### 2.2 Error Sources

| Error | Source | Recovery |
|-------|--------|----------|
| Python import failed | `PyImport_ImportModule` returns NULL | Check `pip install` |
| Python call exception | `PyObject_Call` returns NULL | Use `capture_py_error()` for traceback |
| npm module not found | `JS_Eval` / `resolve_mod` returns NULL | Auto-install attempted; check npm registry |
| npm subprocess crash | Pipe read returns 0 bytes | Auto-restart via `try_restart_bridge()` |
| npm RPC timeout | 30s timeout on pipe read | Auto-restart triggered |
| Cargo bridge error | Rust `Result::Err` | `_last_error()` returns Rust error string |
| Bridge DLL not found | `LoadLibrary` / `dlopen` fails | Check bridge generation with `voss bridge` |
| Memory allocation failure | `malloc` returns NULL | Returns `NULL`; check `_get_last_error()` |
| GIL acquisition failure | `PyGILState_Ensure` | Fatal; check Python DLL initialization |

### 2.3 Error Recovery Strategies

**PyPI:**
```c
void* result = pypi_call1(mod, "fn", arg);
if (!result) {
    const char* err = pypi_get_last_error();
    printf("Error: %s\n", err);
    return;
}
```

**npm QuickJS:**
```c
void* mod = moment_require();
if (!mod) {
    const char* err = moment_get_last_error();
    printf("Require failed: %s\n", err);
    return;
}
```

**npm Subprocess (auto-restart):**
- All exported calls check `g_rpc_ok` before proceeding
- If subprocess crashed, `try_restart_bridge()` is called automatically
- The bridge resets attr cache, deferred-free queue, and read buffer on restart
- 30-second timeout on each pipe read prevents hangs

### 2.4 Memory Leak Prevention

| Practice | Bridge |
|----------|--------|
| Every `_str()` / `_int()` / `_call*()` result must be `_free()`d | All bridges |
| `_free_cstr()` for `_to_cstr()` results | npm (subprocess) |
| Deferred free queue flushed at threshold (128 items) or on every RPC | npm (subprocess) |
| `JS_FreeValue()` on all temporary JS values | npm (QuickJS) |
| `Py_DECREF()` on all temporary Python objects | PyPI |
| Rust bridge uses `drop` for automatic cleanup | Cargo |

### 2.5 Best Practices

1. **Always check return values** — bridge calls return `NULL` on failure
2. **Always free results** — every `_call*`, `_str`, `_int`, `_float`, `_getattr` result needs `_free()`
3. **Don't double-free** — freeing a null handle is safe; freeing the same handle twice is not
4. **Use `_get_last_error()`** immediately after a failure for diagnostics
5. **Use `_get_perf_stats()`** for performance monitoring (available in npm bridges)
6. **Free in reverse order** — free args before the module handle
7. **Thread safety is automatic** — no user-side locking needed

---

## 3. Bridge Lifecycle

### 3.1 Initialization

```
User code → dl_open("bridge.dll") → dl_sym("bridge_fn") → call bridge_fn()
                                    ↓
                            [bridge DLL_PROCESS_ATTACH]
                                    ↓
                         ┌───────────────────────┐
                         │  cache_pyapi() [PyPI]  │
                         │  init_qjs()    [npm]   │
                         │  spawn_bridge() [sub]  │
                         └───────────────────────┘
                                    ↓
                            Ready for calls
```

### 3.2 Cleanup

```
User code → FreeLibrary(bridge.dll)
                                    ↓
                            [bridge DLL_PROCESS_DETACH]
                                    ↓
                         ┌───────────────────────┐
                         │  Py_Finalize() [PyPI]  │
                         │  JS_FreeRuntime() [QJS]│
                         │  cleanup_bridge() [sub]│
                         └───────────────────────┘
```

### 3.3 Crash Recovery (npm Subprocess)

```
send_rpc() → pipe read fails → g_rpc_ok = 0
                                ↓
next call → try_restart_bridge() → wait for g_restarting (atomic)
                                    ↓
                            Close stale pipe handles
                            Reset: attr_cache, read_buf, deferred_free, next_id
                            spawn_bridge() → new Node.js process
                                    ↓
                            g_rpc_ok = 1
```

---

## 4. Diagnostics

### 4.1 `_get_perf_stats()` (npm bridges only)

Returns JSON string with performance counters:

**Subprocess bridge:**
```json
{
  "rpc_calls": 150,
  "rpc_time_ms": 2340,
  "batch_items": 89,
  "batch_calls": 12,
  "restarts": 0,
  "attr_hits": 45,
  "attr_misses": 12
}
```

**QuickJS bridge:**
```json
{
  "require_calls": 23,
  "bc_hits": 15,
  "bc_misses": 8,
  "exports_lookups": 12,
  "eval_time_ms": 450,
  "resolve_time_ms": 120
}
```

### 4.2 `_get_last_error()`

Returns the most recent error message as a C string. The buffer is overwritten on each error. Always read immediately after a failed call.

### 4.3 ASAN / AppVerif

- AddressSanitizer builds: `build_asan.bat`
- Test all bridges under ASAN before release
- Run `test_ffi_memory_safety.exe` for memory leak detection
- Run `bench_npm_bridge.exe` for QuickJS GC stability

---

## 5. Known Limitations

| Limitation | Bridge | Impact |
|------------|--------|--------|
| No CWD in DLL search path after Python init | PyPI | Workaround: `aurora_dl_open()` with `LOAD_WITH_ALTERED_SEARCH_PATH` |
| Single-threaded QJS runtime | npm (QJS) | All JS calls serialized by mutex |
| Subprocess pipe is half-duplex | npm (sub) | Request/response model only |
| No ES module (`import`) syntax in QuickJS | npm (QJS) | ESM packages transpiled to CJS at bridge-gen time or at runtime |
| Cargo auto-gen fails for generic types | Cargo | Only simple signatures work; real crates need manual wrappers |
| No TLS/SSL in QuickJS `net` module | npm (QJS) | `tls` module is a stub (wraps `net.Socket` with `authorized=true`) |
