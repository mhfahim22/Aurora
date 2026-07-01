#include "compiler/codegen.hpp"
#include "bridge_codegen.hpp"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <iostream>
#include <unordered_map>

/* Lazy-load registry: maps (ecosystem, module) → cached symbols */
static std::unordered_map<std::string, void*> s_lazy_registry_;

bool Codegen::is_ecosystem_extern(const ASTNode* node) const {
    if (!node) return false;
    return !node->ecosystem.empty();
}

void Codegen::gen_bridge_fn(const ASTNode* node) {
    if (!node) return;
    BridgeEcosystem eco = bridge_ecosystem_from_string(node->ecosystem);
    switch (eco) {
        case BridgeEcosystem::Python:
            gen_bridge_python_fn(node);
            break;
        case BridgeEcosystem::QuickJS:
            gen_bridge_quickjs_fn(node);
            break;
        case BridgeEcosystem::Rust:
            gen_bridge_rust_fn(node);
            break;
        default:
            std::cerr << "[bridge] ERROR: unknown ecosystem '" << node->ecosystem
                      << "' for extern function '" << node->value << "'\n";
            break;
    }
}

/* ── Register a lazily-loaded bridge module for fast re-lookup ──
 *   After a bridge function has been resolved once, this stores
 *   the handle so subsequent calls skip the resolve step.
 *   The `name` is <ecosystem>_<module>, e.g. "python_numpy". */
void Codegen::gen_bridge_register_lazy(void* handle,
                                        const std::string& name,
                                        const std::string& /*mod_name*/) {
    if (handle && !name.empty()) {
        s_lazy_registry_[name] = handle;
    }
}

/* ── Look up a previously registered lazy handle ──
 *   Returns the cached handle or nullptr if not yet resolved. */
void* Codegen::gen_bridge_lookup_lazy(const std::string& name) {
    auto it = s_lazy_registry_.find(name);
    if (it != s_lazy_registry_.end())
        return it->second;
    return nullptr;
}
