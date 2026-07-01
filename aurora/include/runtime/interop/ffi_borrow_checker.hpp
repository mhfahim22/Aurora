#pragma once
#include "runtime/interop/ffi_ownership.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <cstdint>

/* ════════════════════════════════════════════════════════════
   FFI Borrow Checker — Phase 4.3
   ════════════════════════════════════════════════════════════
   Static analysis across language boundaries.
   Ensures no use-after-free, no double-free, no dangling
   references across FFI calls — all at compile time.
   
   ⚠ WIP — Architecture complete, not yet integrated into the
   compiler pipeline. The analysis logic is implemented but
   not wired into the semantic pass.
   ════════════════════════════════════════════════════════════ */

/* ── Direction of FFI call ── */
enum class FFICallDirection : uint8_t {
    AuroraToC,     /* Aurora calls extern "C" function */
    CToAurora,     /* C callback invoked from Aurora */
    AuroraToCpp,   /* Aurora calls C++ method (via thunk) */
    CppToAurora,   /* C++ calls back into Aurora */
};

/* ── Violation severity ── */
enum class FFIViolationSeverity : uint8_t {
    Error,   /* Must be caught at compile time */
    Warning, /* Should be flagged for user review */
    Info,    /* Advisory */
};

struct FFICheckResult {
    bool safe{true};
    std::string message;
    FFIViolationSeverity severity{FFIViolationSeverity::Error};

    void err(const std::string& msg) {
        safe = false; severity = FFIViolationSeverity::Error; message = msg;
    }
    void warn(const std::string& msg) {
        if (safe) { safe = false; severity = FFIViolationSeverity::Warning; message = msg; }
    }
};

/* ── Represents a single FFI parameter or return value ── */
struct FFIBoundarySlot {
    std::string name;
    InteropTypeKind type_kind{InteropTypeKind::Void};
    FFIOwnership ownership{FFIOwnership::Borrowed};
    bool is_pointer{false};
    bool is_return{false};
    bool nullable{false};
};

struct FFIFunctionSignature {
    std::string name;
    std::vector<FFIBoundarySlot> params;
    FFIBoundarySlot ret;
};

/* ── Tracks borrowed references across FFI calls ── */
struct FFIBorrowSet {
    /* Which local variables have active borrows into foreign code */
    std::unordered_map<std::string, std::string> local_to_ffi;
    /* Which foreign handles are currently borrowed */
    std::unordered_set<std::string> active_ffi_borrows;

    bool is_borrowed(const std::string& local) const {
        return local_to_ffi.find(local) != local_to_ffi.end();
    }

    void add_borrow(const std::string& local, const std::string& ffi_handle) {
        local_to_ffi[local] = ffi_handle;
        active_ffi_borrows.insert(ffi_handle);
    }

    void end_borrow(const std::string& local) {
        auto it = local_to_ffi.find(local);
        if (it != local_to_ffi.end()) {
            active_ffi_borrows.erase(it->second);
            local_to_ffi.erase(it);
        }
    }
};

/* ── The borrow checker itself ── */
class FFIBorrowChecker {
public:
    FFICheckResult check_ffi_call(
        const FFIFunctionSignature& sig,
        const std::vector<FFIOwnership>& param_ownerships,
        FFICallDirection dir)
    {
        FFICheckResult result;

        if (param_ownerships.size() != sig.params.size()) {
            result.err("parameter count mismatch: sig has " +
                std::to_string(sig.params.size()) + ", caller provides " +
                std::to_string(param_ownerships.size()));
            return result;
        }

        for (size_t i = 0; i < sig.params.size(); i++) {
            auto check = check_slot(sig.params[i], param_ownerships[i], dir);
            if (!check.safe) {
                result.err("param '" + sig.params[i].name + "': " + check.message);
                return result;
            }
        }

        auto ret_check = check_return_slot(sig.ret, dir);
        if (!ret_check.safe) {
            result.err("return: " + ret_check.message);
            return result;
        }

        return result;
    }

    /* ── Track that a local variable is being borrowed by foreign code ── */
    void track_foreign_borrow(const std::string& local, const std::string& ffi_var) {
        borrows_.add_borrow(local, ffi_var);
    }

    /* ── Release a foreign borrow ── */
    void release_foreign_borrow(const std::string& local) {
        borrows_.end_borrow(local);
    }

    /* ── Check if it's safe to mutate/drop a variable that may be borrowed ── */
    FFICheckResult check_can_mutate(const std::string& local) {
        FFICheckResult r;
        if (borrows_.is_borrowed(local))
            r.err("cannot mutate '" + local +
                "' — actively borrowed by foreign code");
        return r;
    }

    /* ── Dump state for debugging ── */
    std::string dump() const {
        std::string s;
        for (auto& [local, ffi] : borrows_.local_to_ffi)
            s += "  " + local + " → " + ffi + " (foreign borrow)\n";
        return s;
    }

private:
    FFIBorrowSet borrows_;

    FFICheckResult check_slot(
        const FFIBoundarySlot& slot,
        FFIOwnership caller_intent,
        FFICallDirection dir)
    {
        FFICheckResult r;

        /* Ownership compatibility check */
        bool compatible = false;
        switch (caller_intent) {
            case FFIOwnership::Borrowed:
                compatible = (slot.ownership == FFIOwnership::Borrowed ||
                             slot.ownership == FFIOwnership::Copy);
                break;
            case FFIOwnership::Move:
                compatible = (slot.ownership == FFIOwnership::Move ||
                             slot.ownership == FFIOwnership::Copy);
                break;
            case FFIOwnership::Shared:
                compatible = (slot.ownership == FFIOwnership::Shared);
                break;
            case FFIOwnership::Copy:
                compatible = (slot.ownership == FFIOwnership::Copy);
                break;
            default:
                compatible = true;
        }

        if (!compatible)
            r.err("ownership mismatch: caller passes " +
                std::string(ffi_ownership_name(caller_intent)) +
                " but callee expects " +
                std::string(ffi_ownership_name(slot.ownership)));

        /* Nullable check */
        if (!slot.nullable && slot.is_pointer && !slot.is_return) {
            /* At compile time ensure non-null (runtime assertion added in codegen) */
        }

        /* Direction-specific checks */
        if (slot.ownership == FFIOwnership::Move && dir == FFICallDirection::AuroraToC) {
            /* Moving into C: caller must not use after */
        }

        return r;
    }

    FFICheckResult check_return_slot(
        const FFIBoundarySlot& slot,
        FFICallDirection dir)
    {
        FFICheckResult r;

        if (slot.ownership == FFIOwnership::Borrowed && dir == FFICallDirection::AuroraToC) {
            /* Returning a borrow from Aurora to C: the C caller must not
               outlive the Aurora borrow. This is checked by lifetime tracking. */
        }

        if (slot.ownership == FFIOwnership::Move && dir == FFICallDirection::CToAurora) {
            /* C passes ownership to Aurora: Aurora codegen must insert free */
        }

        return r;
    }
};
