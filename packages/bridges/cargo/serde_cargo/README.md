# serde — Manual Cargo Bridge

This bridge was generated with `--manual` because serde uses generic types, trait bounds, or conditional compilation
that the auto-generator cannot handle.

## To complete

1. Open `src/lib.rs` and replace the `add_user` example stub with real wrappers.
2. Update the `_call()` dispatcher to map function names to your handlers.
3. If the crate's types do not implement `serde::Serialize`/`serde::Deserialize`,
   add manual conversion helpers (e.g. via `.to_string()` / `.parse()`).
4. Run `cargo build --release` in this directory.
5. The resulting cdylib will be loaded by Aurora at runtime.

## Exported C API

All `#[no_mangle] pub extern "C" fn` symbols in `src/lib.rs` are FFI entry points.
The Aurora runtime calls:
- `serde_import()` — initialize
- `serde_call(mod, "fn_name", args)` — invoke a function
- `serde_free(ptr)` — free a returned value
- `serde_last_error()` — retrieve last error string

## Dependencies

Edit `Cargo.toml` to add any additional crate dependencies your wrappers need.
Keep `serde` and `serde_json` — they are required by the bridge runtime.
