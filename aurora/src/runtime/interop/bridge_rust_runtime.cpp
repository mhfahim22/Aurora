#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* ── Runtime bridge for Rust ecosystem calls ──
 *   Rust cdylibs expose C ABI. This module provides:
 *   1. Library loading with Rust naming conventions
 *   2. Option<T> → T with sentinel (tagged union unwrap)
 *   3. Result<T,E> → T or error code (tagged union unwrap)
 *   4. Slice access (pointer + length pair)
 *   5. Struct field extraction at byte offset
 */

extern "C" {

/* ════════════════════════════════════════════════════════════
   1. Library Loading
   ════════════════════════════════════════════════════════════ */
int aurora_bridge_rust_try_load(const char* crate_name, void** out_handle) {
    if (!crate_name || !out_handle) return -1;
    void* handle = nullptr;

#ifdef _WIN32
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s.dll", crate_name);
    handle = (void*)::LoadLibraryA(buf);
    if (!handle) {
        std::snprintf(buf, sizeof(buf), "lib%s.dll", crate_name);
        handle = (void*)::LoadLibraryA(buf);
    }
    if (!handle) {
        /* Rust adds hash suffix: name-<hash>.dll — use FindFirstFile to locate */
        WIN32_FIND_DATAA ffd;
        std::snprintf(buf, sizeof(buf), "%s-*.dll", crate_name);
        HANDLE hFind = ::FindFirstFileA(buf, &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            std::snprintf(buf, sizeof(buf), "%s", ffd.cFileName);
            handle = (void*)::LoadLibraryA(buf);
            ::FindClose(hFind);
        }
    }
#else
    handle = ::dlopen(crate_name, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        std::string lib_name = std::string("lib") + crate_name + ".so";
        handle = ::dlopen(lib_name.c_str(), RTLD_LAZY | RTLD_LOCAL);
    }
    if (!handle) {
        /* Try with version suffix: lib<name>.so.<version> */
        std::string lib_name = std::string("lib") + crate_name + ".so.1";
        handle = ::dlopen(lib_name.c_str(), RTLD_LAZY | RTLD_LOCAL);
    }
#endif

    *out_handle = handle;
    return handle ? 0 : -1;
}

/* ════════════════════════════════════════════════════════════
   2. Option<T> Unwrapping
      Rust Option<T> ABI:
        - None: discriminant = 0, payload = undefined
        - Some(T): discriminant = 1, payload = T
      For Option<ptr> (niche optimization):
        - None: ptr = null
        - Some(ptr): ptr != null (no discriminant)
   ════════════════════════════════════════════════════════════ */

/* ── Unwrap Option<i64> (tagged union layout) ──
 *   Input:  pair (discriminant, value) packed into a single i64
 *           high 32 bits = discriminant, low 32 bits = value
 *   Output: value if Some, 0 if None
 *   Sets *is_some to 1 if Some, 0 if None */
int64_t aurora_bridge_rust_option_i64_unwrap(int64_t tagged, int* is_some) {
    int32_t disc = (int32_t)(tagged >> 32);
    int64_t val = (int64_t)(int32_t)(tagged & 0xFFFFFFFFLL);
    if (is_some) *is_some = (disc == 1);
    return (disc == 1) ? val : 0;
}

/* ── Unwrap Option<f64> ──
 *   Input:  struct { i64 disc; f64 val; } packed as two i64 slots
 *           slot0 = discriminant, slot1 = value bits
 *   Returns value if Some, 0.0 if None */
double aurora_bridge_rust_option_f64_unwrap(int64_t disc, int64_t val_bits, int* is_some) {
    if (is_some) *is_some = (disc == 1);
    if (disc == 1) {
        double result;
        std::memcpy(&result, &val_bits, sizeof(result));
        return result;
    }
    return 0.0;
}

/* ── Unwrap Option<pointer> (niche optimization) ──
 *   Rust uses null pointer to represent None for Option<&T>/Option<Box<T>> */
void* aurora_bridge_rust_option_ptr_unwrap(void* ptr, int* is_some) {
    if (is_some) *is_some = (ptr != nullptr);
    return ptr;
}

/* ════════════════════════════════════════════════════════════
   3. Result<T,E> Unwrapping
      Rust Result<T,E> ABI:
        - Ok(T):  discriminant = 0, payload = T
        - Err(E): discriminant = 1, payload = E
   ════════════════════════════════════════════════════════════ */

/* ── Unwrap Result<i64, i64> ──
 *   Returns value if Ok, 0 if Err.
 *   *is_ok = 1 if Ok, 0 if Err.
 *   *err_code = error value if Err */
int64_t aurora_bridge_rust_result_i64_unwrap(int64_t tagged, int* is_ok, int64_t* err_code) {
    int32_t disc = (int32_t)(tagged >> 32);
    int64_t val = (int64_t)(int32_t)(tagged & 0xFFFFFFFFLL);
    if (is_ok)   *is_ok = (disc == 0);
    if (err_code && disc != 0) *err_code = val;
    return (disc == 0) ? val : 0;
}

/* ── Unwrap Result<{}, i64> (unit on success, error code on failure) ──
 *   Returns 0 on Ok, error code on Err */
int64_t aurora_bridge_rust_result_unit_unwrap(int64_t tagged, int* is_ok) {
    int32_t disc = (int32_t)(tagged >> 32);
    if (is_ok) *is_ok = (disc == 0);
    return (disc == 0) ? 0 : (int64_t)(int32_t)(tagged & 0xFFFFFFFFLL);
}

/* ════════════════════════════════════════════════════════════
   4. Slice Access
      Rust &[T] ABI: { T* ptr, size_t len }
      Passed as two i64 values (on x64): ptr, len
   ════════════════════════════════════════════════════════════ */

void* aurora_bridge_rust_slice_ptr(int64_t ptr_slot) {
    return (void*)(uintptr_t)ptr_slot;
}

int64_t aurora_bridge_rust_slice_len(int64_t len_slot) {
    return len_slot;
}

/* ════════════════════════════════════════════════════════════
   5. Struct Field Extraction
      Given a pointer to a struct and a byte offset,
      extract the field as i64, f64, or pointer.
   ════════════════════════════════════════════════════════════ */

int64_t aurora_bridge_rust_struct_field_i64(void* struct_ptr, int64_t offset) {
    if (!struct_ptr) return 0;
    return *(int64_t*)((char*)struct_ptr + offset);
}

double aurora_bridge_rust_struct_field_f64(void* struct_ptr, int64_t offset) {
    if (!struct_ptr) return 0.0;
    return *(double*)((char*)struct_ptr + offset);
}

void* aurora_bridge_rust_struct_field_ptr(void* struct_ptr, int64_t offset) {
    if (!struct_ptr) return nullptr;
    return *(void**)((char*)struct_ptr + offset);
}

int32_t aurora_bridge_rust_struct_field_i32(void* struct_ptr, int64_t offset) {
    if (!struct_ptr) return 0;
    return *(int32_t*)((char*)struct_ptr + offset);
}

/* ════════════════════════════════════════════════════════════
   6. String Conversion
      Rust String/CString → Aurora i64 (ptr)
      String is ptr+len+cap triplet; we take ownership of the ptr.
   ════════════════════════════════════════════════════════════ */

/* ── Convert Rust String (ptr, len, cap triplet) to Aurora raw i64 ptr ──
 *   Rust String layout: { ptr: *mut u8, len: usize, cap: usize }
 *   We take ownership of the buffer and return just the pointer.
 *   The caller is responsible for freeing via aurora_str_free. */
void* aurora_bridge_rust_string_to_ptr(void* rust_string_ptr) {
    if (!rust_string_ptr) return nullptr;
    /* Rust String layout at the pointer: [ptr][len][cap] on x64 = 24 bytes */
    void* data_ptr = *(void**)rust_string_ptr;
    return data_ptr;
}

} /* extern "C" */
