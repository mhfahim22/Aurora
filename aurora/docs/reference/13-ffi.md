# FFI (Foreign Function Interface)

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

# FFI cost annotation
@cost(zero)
extern function fast_add(a: i32, b: i32) -> i32

@cost(alloc)
extern function malloc(size: i64) -> pointer

@cost(indirection)
extern function query_database(query: cstring) -> i32
```

### Supported Calling Conventions

| Convention | Description                |
|------------|----------------------------|
| (default)  | Platform default           |
| `cdecl`    | C declaration (x86)        |
| `stdcall`  | Standard call (Win32)      |

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

Library bindings in `libc/` provide ready-to-use FFI declarations for:
- `kernel32.auf` (Windows API)
- `user32.auf` (Windows GUI)
- `stdio.auf` (C standard I/O)
- `stdlib.auf` (C standard library)
- `string.auf` (C string functions)
- `gui.auf` (cross-platform GUI)
- `libtorch.auf` (PyTorch bindings)
- `pq.auf` (PostgreSQL libpq)
