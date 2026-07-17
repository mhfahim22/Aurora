#pragma once
#include <string>
#include <unordered_map>

/* ════════════════════════════════════════════════════════════
   ast_memory.hpp — Memory Analysis Types
   ════════════════════════════════════════════════════════════ */

/* ── Allocation Strategy ── */
enum class AllocStrategy {
    Unknown,
    Stack,
    Heap,
    Arena,
    RAII,
    ARC,
    GC,
    /* Forced by attribute */
    ForcedStack,
    ForcedArena,
    ForcedRAII,
    ForcedARC,
    ForcedGC,
};

inline const char* alloc_strategy_name(AllocStrategy s) {
    switch (s) {
        case AllocStrategy::Unknown:     return "Unknown";
        case AllocStrategy::Stack:       return "Stack";
        case AllocStrategy::Heap:        return "Heap";
        case AllocStrategy::Arena:       return "Arena";
        case AllocStrategy::RAII:        return "RAII";
        case AllocStrategy::ARC:         return "ARC";
        case AllocStrategy::GC:          return "GC";
        case AllocStrategy::ForcedStack: return "@stack";
        case AllocStrategy::ForcedArena: return "@arena";
        case AllocStrategy::ForcedRAII:  return "@raii";
        case AllocStrategy::ForcedARC:   return "@arc";
        case AllocStrategy::ForcedGC:    return "@gc";
    }
    return "Unknown";
}

inline bool is_forced_strategy(AllocStrategy s) {
    return s == AllocStrategy::ForcedStack ||
           s == AllocStrategy::ForcedArena ||
           s == AllocStrategy::ForcedRAII  ||
           s == AllocStrategy::ForcedARC   ||
           s == AllocStrategy::ForcedGC;
}

inline AllocStrategy forced_to_base(AllocStrategy s) {
    switch (s) {
        case AllocStrategy::ForcedStack: return AllocStrategy::Stack;
        case AllocStrategy::ForcedArena: return AllocStrategy::Arena;
        case AllocStrategy::ForcedRAII:  return AllocStrategy::RAII;
        case AllocStrategy::ForcedARC:   return AllocStrategy::ARC;
        case AllocStrategy::ForcedGC:    return AllocStrategy::GC;
        default: return s;
    }
}

/* ── Escape Status ── */
enum class EscapeStatus {
    Unknown,
    NoEscape,
    ArgEscape,
    ReturnEscape,
    GlobalEscape,
    ClosureEscape,
};

inline const char* escape_status_name(EscapeStatus s) {
    switch (s) {
        case EscapeStatus::Unknown:       return "Unknown";
        case EscapeStatus::NoEscape:      return "NoEscape";
        case EscapeStatus::ArgEscape:     return "ArgEscape";
        case EscapeStatus::ReturnEscape:  return "ReturnEscape";
        case EscapeStatus::GlobalEscape:  return "GlobalEscape";
        case EscapeStatus::ClosureEscape: return "ClosureEscape";
    }
    return "Unknown";
}

/* ── Lifetime Scope ── */
enum class LifetimeScope {
    Unknown,
    Function,
    Block,
    Loop,
    Temporary,
    Global,
};

inline const char* lifetime_scope_name(LifetimeScope s) {
    switch (s) {
        case LifetimeScope::Unknown:   return "Unknown";
        case LifetimeScope::Function:  return "Function";
        case LifetimeScope::Block:     return "Block";
        case LifetimeScope::Loop:      return "Loop";
        case LifetimeScope::Temporary: return "Temporary";
        case LifetimeScope::Global:    return "Global";
    }
    return "Unknown";
}

/* ── Ownership Type ── */
enum class OwnershipType {
    Unknown,
    Single,
    Shared,
    Weak,
    Borrowed,
};

inline const char* ownership_type_name(OwnershipType s) {
    switch (s) {
        case OwnershipType::Unknown:  return "Unknown";
        case OwnershipType::Single:   return "Single";
        case OwnershipType::Shared:   return "Shared";
        case OwnershipType::Weak:     return "Weak";
        case OwnershipType::Borrowed: return "Borrowed";
    }
    return "Unknown";
}

/* ── Memory Metadata ── */
/* NOTE: decl_line in this struct duplicates ASTNode::src_line when MemoryMetadata
   is embedded in an ASTNode. When stored in MemoryAnalysisResult::variables there
   is no associated ASTNode, so decl_line is kept for profiler diagnostics. */
struct MemoryMetadata {
    AllocStrategy  alloc_strategy  { AllocStrategy::Unknown };
    AllocStrategy  forced_strategy { AllocStrategy::Unknown };
    EscapeStatus   escape_status   { EscapeStatus::Unknown };
    LifetimeScope  lifetime_scope  { LifetimeScope::Unknown };
    OwnershipType  ownership_type  { OwnershipType::Unknown };

    int  decl_line       { 0 };
    int  size_estimate   { 0 };
    bool is_mutable      { true };
    bool has_borrows     { false };
    int  ref_count       { 0 };
    int  arena_group_id  { -1 };
    bool is_gc_root      { false };
};

/* ── Memory Analysis Result ── */
struct MemoryAnalysisResult {
    bool is_performance_mode { false };
    AllocStrategy func_forced_strategy { AllocStrategy::Unknown };
    bool gc_disabled         { false };

    std::unordered_map<std::string, MemoryMetadata> variables;

    int stack_count  { 0 };
    int heap_count   { 0 };
    int arena_count  { 0 };
    int raii_count   { 0 };
    int arc_count    { 0 };
    int gc_count     { 0 };

    void update_stats() {
        stack_count = 0; heap_count = 0; arena_count = 0;
        raii_count = 0; arc_count = 0; gc_count = 0;
        for (auto& [name, meta] : variables) {
            AllocStrategy effective = is_forced_strategy(meta.alloc_strategy)
                ? forced_to_base(meta.alloc_strategy) : meta.alloc_strategy;
            switch (effective) {
                case AllocStrategy::Stack:  stack_count++;  break;
                case AllocStrategy::Heap:   heap_count++;   break;
                case AllocStrategy::Arena:  arena_count++;  break;
                case AllocStrategy::RAII:   raii_count++;   break;
                case AllocStrategy::ARC:    arc_count++;    break;
                case AllocStrategy::GC:     gc_count++;     break;
                default: break;
            }
        }
    }
};
