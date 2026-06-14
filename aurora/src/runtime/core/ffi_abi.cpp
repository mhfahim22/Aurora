#include "runtime/interop/ffi_abi.hpp"
#include <cstring>
#include <cstdlib>

extern "C" {

/* ═════════════════════════════════════════════════════════════
   Primitive type singletons
   ═════════════════════════════════════════════════════════════ */
ffi_type ffi_type_void    = {FFI_VOID,   0, 0, NULL};
ffi_type ffi_type_int8    = {FFI_INT8,   0, 1, NULL};
ffi_type ffi_type_int16   = {FFI_INT16,  0, 2, NULL};
ffi_type ffi_type_int32   = {FFI_INT32,  0, 4, NULL};
ffi_type ffi_type_int64   = {FFI_INT64,  0, 8, NULL};
ffi_type ffi_type_uint8   = {FFI_UINT8,  0, 1, NULL};
ffi_type ffi_type_uint16  = {FFI_UINT16, 0, 2, NULL};
ffi_type ffi_type_uint32  = {FFI_UINT32, 0, 4, NULL};
ffi_type ffi_type_uint64  = {FFI_UINT64, 0, 8, NULL};
ffi_type ffi_type_float   = {FFI_FLOAT,  0, 4, NULL};
ffi_type ffi_type_double  = {FFI_DOUBLE, 0, 8, NULL};
ffi_type ffi_type_pointer = {FFI_PTR,    0, 8, NULL};

/* ═════════════════════════════════════════════════════════════
   Type size/alignment helpers
   ═════════════════════════════════════════════════════════════ */
int ffi_type_size(ffi_type_id t) {
    switch (t) {
    case FFI_VOID:   return 0;
    case FFI_INT8:
    case FFI_UINT8:  return 1;
    case FFI_INT16:
    case FFI_UINT16: return 2;
    case FFI_INT32:
    case FFI_UINT32:
    case FFI_FLOAT:  return 4;
    case FFI_INT64:
    case FFI_UINT64:
    case FFI_DOUBLE:
    case FFI_PTR:    return 8;
    case FFI_STRUCT: return 0;
    default:         return -1;
    }
}

static int compute_struct_size(const ffi_type* t) {
    if (!t || t->nelem == 0 || !t->elements) return 0;
    int total = 0;
    int max_align = 1;
    for (int i = 0; i < t->nelem; i++) {
        int elem_sz = ffi_type_size_full(t->elements[i]);
        if (elem_sz < 0) return -1;
        int elem_align = ffi_type_align(t->elements[i]);
        total = (total + elem_align - 1) & ~(elem_align - 1);
        total += elem_sz;
        if (elem_align > max_align) max_align = elem_align;
    }
    total = (total + max_align - 1) & ~(max_align - 1);
    return total;
}

int ffi_type_size_full(const ffi_type* t) {
    if (!t) return 0;
    if (t->id != FFI_STRUCT) return ffi_type_size(t->id);
    if ((int)t->size > 0) return (int)t->size;
    /* auto-compute and cache (cast away const — size is mutable by convention) */
    int sz = compute_struct_size(t);
    ((ffi_type*)t)->size = (size_t)sz;
    return sz;
}

int ffi_type_align(const ffi_type* t) {
    if (!t) return 1;
    if (t->id != FFI_STRUCT) {
        int sz = ffi_type_size(t->id);
        return sz > 0 ? sz : 1;
    }
    int max_align = 1;
    for (int i = 0; i < t->nelem; i++) {
        int a = ffi_type_align(t->elements[i]);
        if (a > max_align) max_align = a;
    }
    return max_align;
}

/* ═════════════════════════════════════════════════════════════
   Platform ABI detection
   ═════════════════════════════════════════════════════════════ */
int ffi_abi_platform(void) {
#if defined(_WIN64) || defined(__x86_64__)
    return FFI_ABI_WIN64;
#else
    return FFI_ABI_SYSV;
#endif
}

/* ═════════════════════════════════════════════════════════════
   Error strings
   ═════════════════════════════════════════════════════════════ */
const char* ffi_strerror(int err) {
    switch (err) {
    case FFI_OK:       return "Success";
    case FFI_EBADTYPE: return "Invalid type in FFI CIF";
    case FFI_ETOOMANY: return "Too many arguments (max 16)";
    case FFI_ENULL:    return "Null pointer in FFI call";
    case FFI_EMEM:     return "Memory allocation failure";
    default:           return "Unknown FFI error";
    }
}

/* ═════════════════════════════════════════════════════════════
   Old-style CIF prep
   ═════════════════════════════════════════════════════════════ */
int ffi_cif_prep(ffi_cif* cif, ffi_type_id ret_type,
                 int arg_count, const ffi_type_id* arg_types) {
    if (!cif) return FFI_ENULL;
    if (ret_type < 0 || ret_type > FFI_STRUCT) return FFI_EBADTYPE;
    if (arg_count < 0 || arg_count > 16) return FFI_ETOOMANY;

    cif->ret_type = ret_type;
    cif->arg_count = arg_count;

    if (arg_count > 0) {
        if (!arg_types) return FFI_ENULL;
        for (int i = 0; i < arg_count; i++) {
            if (arg_types[i] < 0 || arg_types[i] > FFI_STRUCT)
                return FFI_EBADTYPE;
            cif->arg_types[i] = arg_types[i];
        }
    }
    return FFI_OK;
}

/* ═════════════════════════════════════════════════════════════
   New-style CIF prep (with ffi_type*)
   ═════════════════════════════════════════════════════════════ */
int ffi_cif_prep_type(ffi_cif2* cif, ffi_type* ret_type,
                      int arg_count, ffi_type** arg_types) {
    if (!cif) return FFI_ENULL;
    if (!ret_type) return FFI_EBADTYPE;
    if (arg_count < 0 || arg_count > 16) return FFI_ETOOMANY;

    cif->ret_type = ret_type;
    cif->arg_count = arg_count;
    cif->flags = 0;

    /* Validate + compute struct sizes */
    if (ret_type->id < 0 || ret_type->id > FFI_STRUCT)
        return FFI_EBADTYPE;
    int rsz = ffi_type_size_full(ret_type);
    if (rsz < 0) return FFI_EBADTYPE;
    if (ret_type->id == FFI_STRUCT && rsz > 8)
        cif->flags |= FFI_F_HIDDEN_RET;

    if (arg_count > 0) {
        if (!arg_types) return FFI_ENULL;
        for (int i = 0; i < arg_count; i++) {
            if (!arg_types[i]) return FFI_ENULL;
            if (arg_types[i]->id < 0 || arg_types[i]->id > FFI_STRUCT)
                return FFI_EBADTYPE;
            int asz = ffi_type_size_full(arg_types[i]);
            if (asz < 0) return FFI_EBADTYPE;
            cif->arg_types[i] = arg_types[i];
        }
    }
    return FFI_OK;
}

/* ═════════════════════════════════════════════════════════════
   Assembly dispatcher (MASM)
   ═════════════════════════════════════════════════════════════ */
extern void ffi_call_impl(const ffi_cif* cif, void* fn,
                          ffi_value* ret, ffi_value* args);

/* ═════════════════════════════════════════════════════════════
   Old-style FFI call (simple types only, no struct support)
   ═════════════════════════════════════════════════════════════ */
void ffi_call(const ffi_cif* cif, void* fn, ffi_value* ret, ffi_value* args) {
    if (!cif || !fn || !ret) return;

    int n = cif->arg_count;
    if (n < 0) n = 0;
    if (n > 16) n = 16;

    ffi_value packed[20];
    memset(packed, 0, sizeof(packed));

    if (args && n > 0) {
        int reg_idx = 0;
        int stack_idx = 4;
        for (int i = 0; i < n; i++) {
            int type_sz = ffi_type_size(cif->arg_types[i]);
            if (type_sz > 8) type_sz = 8;
            if (type_sz <= 0) type_sz = 1;
            int idx = reg_idx < 4 ? (reg_idx++) : (stack_idx++);
            memcpy(&packed[idx], &args[i], (size_t)type_sz);
        }
    }

    ffi_call_impl(cif, fn, ret, packed);
}

/* ═════════════════════════════════════════════════════════════
   New-style FFI call (supports structs)
   ═════════════════════════════════════════════════════════════ */
void ffi_call_type(const ffi_cif2* cif, void* fn, void* ret, void** args) {
    if (!cif || !fn || !ret) return;

    int n = cif->arg_count;
    if (n < 0) n = 0;
    if (n > 16) n = 16;

    int hidden = (cif->flags & FFI_F_HIDDEN_RET) != 0;

    /* ── Allocate hidden pointer buffer if needed ── */
    void* hidden_buf = NULL;
    if (hidden) {
        int rsz = ffi_type_size_full(cif->ret_type);
        hidden_buf = malloc((size_t)rsz);
        if (!hidden_buf) return;
    }

    /* ── Build packed array ──
       Layout (hidden=0): [arg0..arg3] regs, [arg4..] stack
       Layout (hidden=1): [hidden_ptr, arg0..arg2] regs, [arg3..] stack
    */
    ffi_value packed[24];
    memset(packed, 0, sizeof(packed));

    /* Place hidden pointer at packed[0] if needed */
    int reg_base = 0;
    if (hidden) {
        packed[0].ptr = hidden_buf;
        reg_base = 1;
    }

    int reg_max = hidden ? 3 : 4;
    int reg_used = 0;
    int stack_slot = 4;

    for (int i = 0; i < n; i++) {
        ffi_type* at = cif->arg_types[i];
        int sz = ffi_type_size_full(at);

        if (reg_used < reg_max) {
            /* Pass in register */
            int idx = reg_base + reg_used;
            if (sz <= 8)
                memcpy(&packed[idx], args[i], (size_t)sz);
            else
                packed[idx].ptr = args[i];
            reg_used++;
        } else {
            /* Pass on stack */
            if (sz <= 8)
                memcpy(&packed[stack_slot], args[i], (size_t)sz);
            else
                packed[stack_slot].ptr = args[i];
            stack_slot++;
        }
    }

    int total_slots = stack_slot > (reg_base + reg_used) ? stack_slot : (reg_base + reg_used);
    if (total_slots > 20) total_slots = 20;

    /* ── Create temp old-style CIF for assembly call ── */
    ffi_cif tmp_cif;
    tmp_cif.ret_type = hidden ? FFI_VOID : (ffi_type_id)cif->ret_type->id;
    tmp_cif.arg_count = total_slots;

    /* ── Call assembly dispatcher ── */
    ffi_value tmp_ret;
    memset(&tmp_ret, 0, sizeof(tmp_ret));
    ffi_call_impl(&tmp_cif, fn, &tmp_ret, packed);

    /* ── Copy result to user buffer ── */
    if (hidden && hidden_buf) {
        int rsz = ffi_type_size_full(cif->ret_type);
        memcpy(ret, hidden_buf, (size_t)rsz);
        free(hidden_buf);
    } else if (!hidden) {
        int rsz = ffi_type_size_full(cif->ret_type);
        if (rsz > 0) {
            if (rsz > 8) rsz = 8;
            memcpy(ret, &tmp_ret, (size_t)rsz);
        }
    }
}

} /* extern "C" */
