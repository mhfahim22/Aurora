# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.x (development) | ✅ Active development |
| Pre-release | ⚠️ Not for production use |

## Reporting a Vulnerability

**Do not open public GitHub issues for security vulnerabilities.**

Email: `security@aurora-lang.dev` (expected response: 48 hours)

### What to include

- Description of the vulnerability
- Steps to reproduce
- Affected bridge ecosystem (PyPI / npm / Cargo / Native)
- Suggested fix (if known)

## Security Posture

### Memory Safety

- All bridge DLLs are tested under **AddressSanitizer (ASAN)** with zero errors
- Every `malloc` has a corresponding `free`; every `Py_DECREF` is paired; every `JS_FreeValue` is accounted
- AppVerif heap+leak checks pass on all test executables
- QuickJS custom allocator (`JS_NewRuntime2`) verified: 1605 allocs → 1605 frees

### Thread Safety

- **PyPI bridges**: Python GIL acquired/released per call; never hold GIL across blocking operations
- **npm QuickJS bridges**: single-threaded JS runtime serialized by per-bridge mutex
- **npm subprocess bridges**: pipe reads/writes serialized by critical section; 30-second timeout prevents hangs; auto-restart on crash
- **Cargo bridges**: Rust `Arc<Mutex<>>` per bridge state

### Input Validation

- All JSON-RPC requests validated before parsing (`extract_json_field` bounds-checks buffers)
- `snprintf` used throughout — no unbounded string copies
- File paths validated with `strlen` checks before `strncpy`
- `set_last_error` caps message at 4095 chars

### Supply Chain

- Bridge DLLs are generated locally by `voss` — no remote code execution during bridge generation
- npm packages are fetched from the public npm registry via `npm install` in the subprocess bridge
- PyPI packages are imported via Python's `import` mechanism — no remote code in the bridge DLL itself
- Cargo crates are built from source via `cargo build`

## Known Security Limitations

| Limitation | Impact | Mitigation |
|------------|--------|------------|
| QuickJS has no sandbox | Malicious JS could access host APIs | Only run trusted npm packages |
| Python C API unrestricted | Malicious Python code can call any OS API | Only run trusted PyPI packages |
| No TLS in QuickJS `net` module | Network connections are unencrypted | Use Node.js subprocess bridge for TLS |
| Subprocess bridge runs Node.js | Node.js has full filesystem access | Run untrusted packages in a container |
