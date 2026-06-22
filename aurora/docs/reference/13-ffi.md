# FFI (Foreign Function Interface)

FFI allows calling C/C++ libraries, OS APIs, and other languages directly from Aurora.

## External Functions

```aura
# Basic extern
extern function MessageBoxA(hwnd, text, caption, type)

# With return type
extern function malloc(size: i64) -> pointer
extern function printf(fmt: cstring, ...) -> i32     # varargs

# From specific library
extern "kernel32" function GetTickCount() -> i32

# With calling convention + library
extern "stdcall" "kernel32" function Sleep(ms: u32)

# C calling convention
extern "c" function add(p0: i32, p1: i32) -> i32

# Win64 calling convention
extern "win64" function win_api(param: i64) -> i32

# System V AMD64 ABI (Linux/macOS)
extern "sysv64" function posix_api(param: i64) -> i32
```

### FFI cost annotation

```aura
@cost(zero)
extern function fast_add(a: i32, b: i32) -> i32

@cost(alloc)
extern function malloc(size: i64) -> pointer

@cost(indirection)
extern function query_database(query: cstring) -> i32
```

## Supported Calling Conventions

| Convention | Description                |
|------------|----------------------------|
| (default)  | Platform default (Win64)   |
| `cdecl`    | C declaration (x86 only)   |
| `stdcall`  | Standard call (Win32)      |
| `win64`    | Windows x64 convention     |
| `sysv64`   | System V AMD64 ABI         |

> **Note:** On x86-64, Windows uses a single calling convention. `cdecl` and `stdcall` only differ on x86 (32-bit).

## External Structs

```aura
extern struct Point
    x int
    y int

# Brace syntax also works:
extern struct Point {
    x: i32
    y: i32
}

# Practical usage:
extern struct RECT
    left int
    top int
    right int
    bottom int

extern "user32" function GetWindowRect(hwnd: pointer, rect: pointer) -> i32
```

## External Unions

```aura
extern union Data
    i int
    f float

extern union Value {
    i: i32
    f: f32
}
```

## Callback Parameters

```aura
extern function gui_set_callback(
    widget: pointer,
    cb: callback(int, int, int, int)
)
```

## Typed FFI Parameters

```aura
function vec_push(v: pointer, val: f64) -> void
function vec_len(v: pointer) -> i64
```

## Library Bindings

Ready-to-use FFI declarations in `libc/`:

| File            | Library             |
|-----------------|---------------------|
| `kernel32.auf`  | Windows API         |
| `user32.auf`    | Windows GUI         |
| `stdio.auf`     | C standard I/O      |
| `stdlib.auf`    | C standard library  |
| `string.auf`    | C string functions  |
| `gui.auf`       | Cross-platform GUI  |
| `libtorch.auf`  | PyTorch bindings    |
| `pq.auf`        | PostgreSQL libpq    |

---

**Next:** [Attributes](14-attributes.md)
