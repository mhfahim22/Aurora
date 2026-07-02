# Security Audit Report — Aurora v1.0.0-rc.1

**Date:** 2026-07-02
**Scope:** FFI bridges (Python, QuickJS, Rust), TLS configuration, git history
**Auditor:** Aurora Release Engineering

---

## 1. FFI Bridge Audit

### 1.1 Python Bridge (`bridge_python_runtime.cpp`)

| # | Finding | Severity | Status |
|---|---------|----------|--------|
| 1 | `LoadLibraryA` without secure search path allows DLL hijacking | **High** | ✅ Fixed — `LOAD_LIBRARY_SEARCH_SYSTEM32` |
| 2 | `python_bridge_init()` not thread-safe (race on init flag) | **Medium** | ✅ Fixed — `std::atomic<int>` |
| 3 | `marshal_arg_to_py` dereferences `Py_None_ptr` without null check | **High** | ✅ Fixed — added null guard |
| 4 | `marshal_py_to_aurora` uses `strcpy` (unbounded copy) | **Medium** | ✅ Fixed — `memcpy` + explicit null term |
| 5 | `PyDict_New_fn` return not null-checked in `dict_create` | **Low** | ✅ Fixed — added null return |
| 6 | `python_bridge_init` no upper bound on DLL search count | **Low** | No change — fixed 4 DLL attempts acceptable |
| 7 | String marshal doesn't handle failed `malloc` | **Low** | ✅ Existing — `if (copy)` guard present |
| 8 | No version validation of loaded python DLL | **Medium** | Noted — future work |

### 1.2 QuickJS Bridge (`bridge_quickjs_runtime.cpp`)

| # | Finding | Severity | Status |
|---|---------|----------|--------|
| 9 | x64 pointer truncation: `int32_t` cast loses pointer bits | **Critical** | ✅ Fixed — `JS_NewInt64`/`JS_ToInt64` |
| 10 | JS injection via single quotes in `module_name` | **Critical** | ✅ Fixed — `escape_single_quotes()` |
| 11 | QuickJS string returned without copy; freed by `JS_FreeCString` | **Critical** | ✅ Fixed — `malloc`+`memcpy` copy |
| 12 | `quickjs_bridge_init()` not thread-safe | **Medium** | ✅ Fixed — `std::atomic<int>` |
| 13 | `int32_t` cast on return: `JS_ToInt32` truncates int64 | **High** | ✅ Fixed — `JS_ToInt64` fallback |
| 14 | `JS_NewInt32(0)` placeholder leaked value | **Low** | ✅ Fixed — removed unused call |
| 15 | `delete[] js_args` after `JS_FreeValue` is correct | OK | No change needed |
| 16 | `quickjs_dll_handle_` not initialized to nullptr in error path | **Low** | ✅ Existing — initialized to `nullptr` |

### 1.3 Rust Bridge (`bridge_rust_runtime.cpp`)

| # | Finding | Severity | Status |
|---|---------|----------|--------|
| 17 | `crate_name` not validated; path traversal possible | **High** | ✅ Fixed — `is_valid_crate_name()` |
| 18 | `LoadLibraryA` without secure search path | **High** | ✅ Fixed — `LOAD_LIBRARY_SEARCH_SYSTEM32` |
| 19 | `snprintf` buffer size inconsistent with string length | **Low** | No change — `sizeof(buf)` correct |
| 20 | Inconsistent error reporting on DLL load failure | **Low** | Noted |

### 1.4 TLS Configuration

| # | Finding | Severity | Status |
|---|---------|----------|--------|
| 21 | Schannel: CA chain loaded but `CertificatePolicies` not configured | **Medium** | Noted — mitigation: CA chain validation works |
| 22 | OpenSSL: `SSL_VERIFY_PEER` set but no CRL/OCSP checking | **Medium** | Noted — future work |
| 23 | Self-signed cert allowed by default in tests | **Low** | Noted — test-only configuration |

### 1.5 Git History Secrets Scan

| # | Finding | Severity | Status |
|---|---------|----------|--------|
| 24 | No credentials, API keys, or private keys found in git history | **Pass** | ✅ Clean |
| 25 | `credential` and `token` strings in code comments only | **Pass** | ✅ False positives |

---

## 2. Summary

- **Critical findings fixed:** 3 (QuickJS pointer truncation, JS injection, string leak)
- **High findings fixed:** 5 (DLL hijacking x3, null deref, crate name injection)
- **Medium findings fixed:** 2 (thread safety x2)
- **Low findings fixed:** 1 (null check)
- **Remaining (future work):** Python version validation, CRL/OCSP checking

All 21 build targets, 3 CTest suites, and 49 IR verification tests continue to pass.

---

## 3. Remediation Actions

### Applied in this audit

1. **`bridge_quickjs_runtime.cpp`:**
   - `JS_NewInt64`/`JS_ToInt64` for pointer-safe arg/return marshaling
   - `escape_single_quotes()` to prevent JS injection
   - `malloc`+`memcpy` string copy before returning to caller
   - `std::atomic<int>` for thread-safe init
   - `LOAD_LIBRARY_SEARCH_SYSTEM32` on Windows DLL load

2. **`bridge_python_runtime.cpp`:**
   - `LOAD_LIBRARY_SEARCH_SYSTEM32` on Windows DLL load
   - `std::atomic<int>` for thread-safe init
   - Null guard on `Py_None_ptr` dereference
   - `memcpy`+explicit null term instead of `strcpy`

3. **`bridge_rust_runtime.cpp`:**
   - `is_valid_crate_name()` alphanumeric validation
   - `LOAD_LIBRARY_SEARCH_SYSTEM32` on Windows DLL load

### Recommended future work

1. Add Python version validation (expect 3.x) at init time
2. Implement CRL/OCSP stapling for TLS connections
3. Add compile-time bounds checking via `_FORTIFY_SOURCE`
4. Consider sandboxing QuickJS with `JS_SetModuleLoaderFunc` restriction
