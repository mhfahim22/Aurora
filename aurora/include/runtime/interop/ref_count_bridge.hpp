#pragma once
#include "runtime/interop/ffi_ownership.hpp"
#include <string>
#include <cstdint>
#include <atomic>
#include <functional>

/* ════════════════════════════════════════════════════════════
   Ref-Count Bridge — Phase 4.2
   ════════════════════════════════════════════════════════════
   Bridges Aurora ARC with Python/JS/Swift/C++ reference
   counting. Enables shared ownership across language
   boundaries without leaks.
   ════════════════════════════════════════════════════════════ */

/* ── Foreign reference counting protocols ── */
enum class RefCountProtocol : uint8_t {
    AuroraARC,       /* Aurora's own atomic ref-count */
    Python,          /* Py_INCREF / Py_DECREF */
    JavaScript,      /* napi_ref / napi_delete_reference */
    Swift,           /* swift_retain / swift_release */
    CppSharedPtr,    /* std::shared_ptr ABI */
    COM,             /* IUnknown::AddRef / Release */
    Unknown,
};

inline const char* protocol_name(RefCountProtocol p) {
    switch (p) {
        case RefCountProtocol::AuroraARC:    return "aurora_arc";
        case RefCountProtocol::Python:       return "python";
        case RefCountProtocol::JavaScript:   return "javascript";
        case RefCountProtocol::Swift:        return "swift";
        case RefCountProtocol::CppSharedPtr: return "cpp_shared_ptr";
        case RefCountProtocol::COM:          return "com";
        default:                             return "unknown";
    }
}

/* ── Opaque ref-counted handle ── */
struct RefCountedHandle {
    void*       ptr{nullptr};
    void*       allocator{nullptr};
    uint32_t    type_id{0};
    RefCountProtocol protocol{RefCountProtocol::Unknown};
};

/* ── Bridge function pointers (populated by each language bridge) ── */
struct RefCountBridgeVTable {
    void* (*retain)(void* ptr) = nullptr;
    void  (*release)(void* ptr) = nullptr;
    bool  (*is_valid)(void* ptr) = nullptr;
    void* (*copy)(void* ptr) = nullptr;
};

/* ════════════════════════════════════════════════════════════
   Aurora ARC — atomic reference counting (zero-cost in Aurora)
   ════════════════════════════════════════════════════════════ */

class AuroraARC {
public:
    explicit AuroraARC(int initial = 1) : refcount_(initial) {}

    /* ── Thread-safe retain/release ── */
    int retain()  { return ++refcount_; }
    int release() { return --refcount_; }

    /* ── Get current count (debug/assert) ── */
    int count() const { return refcount_.load(std::memory_order_relaxed); }

    /* ── Is this object owned by exactly one reference? ── */
    bool is_unique() const { return refcount_.load(std::memory_order_acquire) == 1; }

private:
    std::atomic<int> refcount_;
};

/* ════════════════════════════════════════════════════════════
   Cross-language ref-count adapter
   ════════════════════════════════════════════════════════════ */

class RefCountAdapter {
public:
    RefCountAdapter() = default;

    /* ── Register a VTable for a foreign protocol ── */
    void register_protocol(RefCountProtocol proto, const RefCountBridgeVTable& vtable) {
        vtables_[static_cast<size_t>(proto)] = vtable;
        active_protocols_ |= (1u << static_cast<size_t>(proto));
    }

    /* ── Retain a handle (protocol-agnostic) ── */
    void* retain(const RefCountedHandle& h) {
        if (!h.ptr) return nullptr;
        auto* vtable = get_vtable(h.protocol);
        if (vtable && vtable->retain)
            return vtable->retain(h.ptr);

        /* Fallback: AuroraARC embedded at ptr offset 0 */
        auto* arc = static_cast<AuroraARC*>(h.ptr);
        if (arc) arc->retain();
        return h.ptr;
    }

    /* ── Release a handle ── */
    void release(const RefCountedHandle& h) {
        if (!h.ptr) return;
        auto* vtable = get_vtable(h.protocol);
        if (vtable && vtable->release) {
            vtable->release(h.ptr);
            return;
        }

        /* Fallback: AuroraARC */
        auto* arc = static_cast<AuroraARC*>(h.ptr);
        if (arc && arc->release() <= 0) {
            /* Codegen emits actual destructor call */
        }
    }

    /* ── Copy a handle ── */
    RefCountedHandle clone(const RefCountedHandle& h) {
        RefCountedHandle copy = h;
        copy.ptr = retain(h);
        return copy;
    }

    /* ── Check if a handle is still valid ── */
    bool is_valid(const RefCountedHandle& h) {
        if (!h.ptr) return false;
        auto* vtable = get_vtable(h.protocol);
        if (vtable && vtable->is_valid)
            return vtable->is_valid(h.ptr);
        return true;
    }

    /* ── Query registered protocols ── */
    bool is_registered(RefCountProtocol p) const {
        return (active_protocols_ & (1u << static_cast<size_t>(p))) != 0;
    }

private:
    RefCountBridgeVTable vtables_[8]{};
    uint32_t active_protocols_{0};

    const RefCountBridgeVTable* get_vtable(RefCountProtocol p) const {
        size_t idx = static_cast<size_t>(p);
        if (idx < 8 && (active_protocols_ & (1u << idx)))
            return &vtables_[idx];
        return nullptr;
    }
};

/* ════════════════════════════════════════════════════════════
   Ownership delegation — decides protocol per FFI boundary
   ════════════════════════════════════════════════════════════ */

struct OwnershipDelegation {
    RefCountProtocol protocol{RefCountProtocol::AuroraARC};
    bool need_retain_on_import{true};
    bool need_release_on_export{true};
    bool zero_cost_path{false}; /* Same protocol both sides → zero cost */
};

static OwnershipDelegation resolve_delegation(
    RefCountProtocol source_protocol,
    RefCountProtocol target_protocol)
{
    OwnershipDelegation d;

    if (source_protocol == target_protocol) {
        /* Same protocol: zero-cost path */
        d.protocol = source_protocol;
        d.need_retain_on_import = false;
        d.need_release_on_export = false;
        d.zero_cost_path = true;
    } else {
        /* Cross-protocol: adapter needed */
        d.protocol = target_protocol;
        d.need_retain_on_import = true;
        d.need_release_on_export = true;
        d.zero_cost_path = false;
    }

    return d;
}
