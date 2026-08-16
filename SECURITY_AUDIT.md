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

---

## 4. OWASP Top 10 Checklist (Phase 38.4)

Verification of the web framework against OWASP Top 10 (2021) — each item is
checked against the built-in security APIs (`app_security`, `security`,
server middleware, rate limiter) plus the phase-38 hardening work.

| # | OWASP Top 10 (2021) | Aurora Mitigation | Status |
|---|--------------------|--------------------|--------|
| A01 | Broken Access Control | `aurora_sec_permission_check/request/revoke` + `role_required` middleware + session auth | ✅ Pass |
| A02 | Cryptographic Failures | AES-256-CBC + PBKDF2 (10000 iter) + SHA-256/HMAC + `aurora_sec_encrypt/decrypt` | ✅ Pass |
| A03 | Injection (SQL/XSS/path) | ORM prepared statements; `aurora_app_security_sanitize`; `aurora_sec_sandbox_check_path` | ✅ Pass |
| A04 | Insecure Design | Rate limiter (token bucket, per-IP) + gateway batch guard + request size limits | ✅ Pass |
| A05 | Security Misconfiguration | Secure storage default; sandbox off-by-default; explicit permissions | ✅ Pass |
| A06 | Vulnerable Components | SQLite3 amalgamation pinned; QuickJS/Rust/Python bridges version-noted | ⚠️ Pass w/ note |
| A07 | Identification & Auth Failures | JWT HS256 (`aurora_jwt_encode/decode`) + password hashing (PBKDF2-SHA256) | ✅ Pass |
| A08 | Software & Data Integrity | Package checksums (voss); JWT signature verification enforced | ✅ Pass |
| A09 | Logging & Monitoring | `aurora_server_*` structured request logging; metrics endpoint (/metrics) | ✅ Pass |
| A10 | SSRF | Restricted redirects; URL validation in `aurora_net_http_get/post` | ⚠️ Pass w/ note |

**Test coverage (Phase 38.4):** SQL injection, XSS, and path-traversal
sanitization tests compile and pass via `Workflow/tests/test_sec_input.aura`.
Rate limiter distributed (Redis-compatible) mode available via
`aurora_ratelimit_redis_*` RESTP client — see `security.auf`.
X.509 certificate APIs now perform real DER parsing (subject/issuer/serial,
validity window, SHA-256 fingerprint) — `aurora_sec_cert_load/info/verify/free`.

**Overall:** 8/10 Pass, 2/10 Pass-with-note. No Critical/High findings open.
