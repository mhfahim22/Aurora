#pragma once
#include <string>

/* ── Cross-Ecosystem Bridge Types ── */
enum class BridgeEcosystem : uint8_t {
    None,     /* Not a bridge ecosystem call (regular extern) */
    Python,   /* CPython bridge — extern "python" */
    QuickJS,  /* QuickJS bridge — extern "quickjs" */
    Rust      /* Rust cdylib bridge — extern "rust" */
};

inline const char* bridge_ecosystem_name(BridgeEcosystem e) {
    switch (e) {
        case BridgeEcosystem::Python:  return "python";
        case BridgeEcosystem::QuickJS: return "quickjs";
        case BridgeEcosystem::Rust:    return "rust";
        default:                       return "";
    }
}

inline BridgeEcosystem bridge_ecosystem_from_string(const std::string& s) {
    if (s == "python")  return BridgeEcosystem::Python;
    if (s == "quickjs") return BridgeEcosystem::QuickJS;
    if (s == "rust")    return BridgeEcosystem::Rust;
    return BridgeEcosystem::None;
}
