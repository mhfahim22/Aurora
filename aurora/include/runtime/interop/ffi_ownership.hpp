#pragma once
#include "runtime/interop/type_ir.hpp"
#include <string>
#include <cstdint>

/* ════════════════════════════════════════════════════════════
   FFI Ownership Protocol — Phase 4
   ════════════════════════════════════════════════════════════
   Defines how memory ownership transfers across FFI boundaries.
   All analysis is compile-time — zero runtime cost.
   ════════════════════════════════════════════════════════════ */

/* ── Ownership semantics for FFI parameters/returns ────── */
enum class FFIOwnership : uint8_t {
    /* Caller owns the data — callee must not free it */
    Borrowed,
    /* Caller transfers ownership to callee — callee must free */
    Move,
    /* Shared ownership via reference counting */
    Shared,
    /* Weak reference — no ownership, must lock before use */
    Weak,
    /* Copy — trivially copyable, caller retains ownership */
    Copy,
    /* Opaque — no ownership tracking (raw pointer pass-through) */
    Opaque,
};

inline const char* ffi_ownership_name(FFIOwnership o) {
    switch (o) {
        case FFIOwnership::Borrowed: return "borrow";
        case FFIOwnership::Move:     return "move";
        case FFIOwnership::Shared:   return "shared";
        case FFIOwnership::Weak:     return "weak";
        case FFIOwnership::Copy:     return "copy";
        case FFIOwnership::Opaque:   return "opaque";
    }
    return "unknown";
}

/* ── Safety rules for each ownership mode ──────────────── */
struct FFISafetyRule {
    FFIOwnership ownership;
    bool callee_must_not_free;     /* Borrowed */
    bool caller_must_not_use_after; /* Move */
    bool refcount_needed;           /* Shared */
    bool lock_before_use;           /* Weak */
    bool zero_cost;                 /* Copy, Opaque */
};

static constexpr FFISafetyRule ffi_safety_rules[] = {
    /* Borrowed */  { FFIOwnership::Borrowed, true,  false, false, false, true  },
    /* Move */      { FFIOwnership::Move,     false, true,  false, false, true  },
    /* Shared */    { FFIOwnership::Shared,   false, false, true,  false, false },
    /* Weak */      { FFIOwnership::Weak,     false, false, false, true,  false },
    /* Copy */      { FFIOwnership::Copy,     false, false, false, false, true  },
    /* Opaque */    { FFIOwnership::Opaque,   false, false, false, false, true  },
};

/* ── Determine ownership mode for a type at FFI boundary ── */
struct FFIOwnershipResolver {
    FFIOwnership resolve(InteropTypeKind kind, bool is_pointer, bool is_return) const {
        /* Void returns are trivially safe */
        if (kind == InteropTypeKind::Void && is_return)
            return FFIOwnership::Copy;

        /* Primitives: always copy (zero-cost) */
        if (kind == InteropTypeKind::Bool ||
            kind == InteropTypeKind::Int8  || kind == InteropTypeKind::Int16 ||
            kind == InteropTypeKind::Int32 || kind == InteropTypeKind::Int64 ||
            kind == InteropTypeKind::UInt8 || kind == InteropTypeKind::UInt16 ||
            kind == InteropTypeKind::UInt32|| kind == InteropTypeKind::UInt64 ||
            kind == InteropTypeKind::Float16||kind == InteropTypeKind::Float32||
            kind == InteropTypeKind::Float64||kind == InteropTypeKind::Char)
            return FFIOwnership::Copy;

        /* Pointers: borrowed by default (caller retains ownership) */
        if (kind == InteropTypeKind::Pointer || kind == InteropTypeKind::Reference)
            return is_return ? FFIOwnership::Move : FFIOwnership::Borrowed;

        /* C strings: borrowed (caller owns the string buffer) */
        if (kind == InteropTypeKind::CString)
            return FFIOwnership::Borrowed;

        /* Strings: copy on FFI boundary (alloc-cost) */
        if (kind == InteropTypeKind::String)
            return FFIOwnership::Move;

        /* Structs by value: copy (zero-cost if trivially copyable) */
        if (kind == InteropTypeKind::Struct || kind == InteropTypeKind::Union)
            return FFIOwnership::Copy;

        /* Opaque class handles: move by default */
        if (kind == InteropTypeKind::Class || kind == InteropTypeKind::Object)
            return is_return ? FFIOwnership::Move : FFIOwnership::Borrowed;

        /* Collections: move (transfer ownership of heap data) */
        if (kind == InteropTypeKind::List || kind == InteropTypeKind::Map ||
            kind == InteropTypeKind::Set  || kind == InteropTypeKind::Array)
            return FFIOwnership::Move;

        /* Default: borrow (safe default) */
        return FFIOwnership::Borrowed;
    }
};

/* ── Compile-time ownership annotation for FFI declarations ── */
struct FFIOwnershipAnnotation {
    std::string param_name;
    FFIOwnership ownership{FFIOwnership::Borrowed};
    bool nullable{false};
};
