#pragma once
#include <cstdint>
#include <cstddef>

extern "C" {

/* ── FFI type identifiers ── */
typedef enum {
    FFI_VOID    = 0,
    FFI_INT8    = 1,
    FFI_INT16   = 2,
    FFI_INT32   = 3,
    FFI_INT64   = 4,
    FFI_UINT8   = 5,
    FFI_UINT16  = 6,
    FFI_UINT32  = 7,
    FFI_UINT64  = 8,
    FFI_FLOAT   = 9,
    FFI_DOUBLE  = 10,
    FFI_PTR     = 11,
    FFI_STRUCT  = 12,
} ffi_type_id;

#define FFI_ABI_WIN64 0
#define FFI_ABI_SYSV  1

/* ── Compound type descriptor (for structs) ── */
typedef struct ffi_type {
    ffi_type_id        id;        /* FFI_STRUCT or primitive       */
    int                nelem;     /* 0 = primitive                 */
    size_t             size;      /* total struct size (auto)      */
    struct ffi_type**  elements;  /* NULL = primitive              */
} ffi_type;

/* ── Convenience macros for defining struct types ── */
#define FFI_STRUCT_DEF(sname, ...)                                  \
    static ffi_type* sname##_elems[] = { __VA_ARGS__ };            \
    static ffi_type  sname = {                                      \
        FFI_STRUCT,                                                 \
        (int)(sizeof(sname##_elems) / sizeof(sname##_elems[0])),   \
        0, /* auto-computed at first use */                         \
        sname##_elems                                               \
    }

#define FFI_PRIMITIVE(id) (&ffi_type_##id)
/* Declared as extern in the .c file: */
extern ffi_type ffi_type_void;
extern ffi_type ffi_type_int8;
extern ffi_type ffi_type_int16;
extern ffi_type ffi_type_int32;
extern ffi_type ffi_type_int64;
extern ffi_type ffi_type_uint8;
extern ffi_type ffi_type_uint16;
extern ffi_type ffi_type_uint32;
extern ffi_type ffi_type_uint64;
extern ffi_type ffi_type_float;
extern ffi_type ffi_type_double;
extern ffi_type ffi_type_pointer;

/* ── Extended CIF with full type descriptors ── */
typedef struct ffi_cif2 {
    ffi_type*   ret_type;
    int         arg_count;
    ffi_type*   arg_types[16];
    int         flags;       /* internal flags (hidden ptr etc) */
} ffi_cif2;

/* ── Old-style CIF (kept for backward compat) ── */
typedef struct ffi_cif {
    ffi_type_id ret_type;
    int         arg_count;
    ffi_type_id arg_types[16];
} ffi_cif;

/* ── FFI value (holds any type up to 8 bytes) ── */
typedef union ffi_value {
    int8_t    i8;
    int16_t   i16;
    int32_t   i32;
    int64_t   i64;
    uint8_t   u8;
    uint16_t  u16;
    uint32_t  u32;
    uint64_t  u64;
    float     f32;
    double    f64;
    void*     ptr;
} ffi_value;

/* ── Internal flags ── */
#define FFI_F_HIDDEN_RET 1  /* return via hidden pointer */

/* ── Platform ABI detection ── */
int ffi_abi_platform(void);

/* ── CIF preparation (old style) ── */
int ffi_cif_prep(ffi_cif* cif, ffi_type_id ret_type,
                 int arg_count, const ffi_type_id* arg_types);

/* ── CIF preparation (new style with ffi_type*) ── */
int ffi_cif_prep_type(ffi_cif2* cif, ffi_type* ret_type,
                      int arg_count, ffi_type** arg_types);

/* ── Make an FFI call (old style) ── */
void ffi_call(const ffi_cif* cif, void* fn, ffi_value* ret, ffi_value* args);

/* ── Make an FFI call (new style: supports structs, all types) ── */
/*    ret points to return buffer; args[i] points to i-th argument value */
void ffi_call_type(const ffi_cif2* cif, void* fn, void* ret, void** args);

/* ── Type size/alignment helpers ── */
int  ffi_type_size(ffi_type_id t);
int  ffi_type_size_full(const ffi_type* t);
int  ffi_type_align(const ffi_type* t);

/* ── Error string ── */
const char* ffi_strerror(int err);

/* Error codes */
#define FFI_OK       0
#define FFI_EBADTYPE 1
#define FFI_ETOOMANY 2
#define FFI_ENULL    3
#define FFI_EMEM     4

} /* extern "C" */
