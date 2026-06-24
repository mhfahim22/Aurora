# Aurora PyPI Bridge — Compilation & CRT Notes

## 1. MSVC CRT Requirement

PyPI bridge DLLs **must** be compiled with the dynamic CRT (`/MD` on MSVC).

### Why

- Python itself is compiled with `/MD` on Windows.
- CRT state (heap, `errno`, `strtok` position, etc.) is per-DLL when linking statically.
- A bridge compiled with `/MT` (static CRT) creates a separate heap. Memory allocated by the bridge (`malloc`) cannot be safely freed by Python (`free`), and vice versa.
- `PyMem_Malloc` / `Py_DECREF` expect the same CRT as the hosting Python process.

### How

The bridge compiler (`bridge_main.cpp`) invokes `cl.exe` (MSVC) with `/LD` to produce a DLL. MSVC's `/LD` flag defaults to `/MD` (dynamic CRT).

If you are testing or debugging with a custom `cl.exe` invocation, ensure the flags include:

```
cl /LD /MD /Fe:bridge.dll bridge.c
```

Without `/MD`, MSVC may default to `/MT` (static CRT) on some configurations, causing subtle memory corruption.

### Non-MSVC Compilers

- **GCC / Clang / Zig cc**: Not affected — these do not use MSVC CRT. The CRT is always dynamically linked (the system `msvcrt.dll` / `ucrtbase.dll`).
- **Cross-compilation**: If cross-compiling a PyPI bridge with `zig cc` or `clang-cl`, ensure the target CRT uses the dynamic model.

## 2. Python DLL Search Path

On Windows, Python's initialization changes the DLL search path. The bridge loads Python's `python3*.dll` dynamically via `rt_pyapi()` (cached function pointers). To prevent `LoadLibrary` failures:

- Use `LOAD_WITH_ALTERED_SEARCH_PATH` when loading the Python DLL.
- Place the bridge DLL in the same directory as the calling executable, or add the bridge directory to `PATH`.

## 3. GIL Initialization

Every exported bridge function must invoke `GIL_ENTER`/`GIL_RETURN` to acquire the Python GIL:

```c
#define GIL_ENTER() \
    PyGILState_STATE __gstate = PyGILState_Ensure()
#define GIL_RETURN(x) \
    do { PyGILState_Release(__gstate); return (x); } while(0)
```

This is handled automatically by `bridge_pypi.cpp`'s generated wrapper code.

## 4. Verifying CRT Match

To verify that a bridge DLL uses the correct CRT:

```
dumpbin /imports bridge.dll | findstr msvcp
dumpbin /imports bridge.dll | findstr ucrtbase
```

Expected output includes `ucrtbase.dll` (dynamic CRT). If you see `LIBCMT` or no CRT imports, the bridge was linked statically.

## 5. Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `malloc` failure after `PyObject_Call` | CRT heap mismatch (bridge uses `/MT`) | Recompile with `/MD` |
| `LoadLibrary` error 126 or 127 | Python DLL not in search path | Set `PATH` or use `LOAD_WITH_ALTERED_SEARCH_PATH` |
| Crash on `Py_DECREF` after bridge call | Double-free or invalid handle | Check ownership: bridge owns handles returned to Aurora |
| Bridge function returns NULL unexpectedly | Python exception | Call `_get_last_error()` for traceback |
