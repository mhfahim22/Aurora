#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

/* ════════════════════════════════════════════════════════════
   Aurora OOP — Class Registry
   ════════════════════════════════════════════════════════════

   ClassFieldInfo  — একটা field এর নাম + default value + position + visibility
   ClassMethodInfo — একটা method এর নাম + parameter list + visibility
   ClassInfo       — একটা class এর সব fields + methods + parent + flags

   ClassRegistry   — সব registered class এর global store
                     Parser → TypeChecker → Codegen সবাই এটা share করে
   ════════════════════════════════════════════════════════════ */

/* ── Visibility levels ── */
enum class Visibility {
    Public,
    Private,
    Protected
};

inline Visibility visibility_from_string(const std::string& s) {
    if (s == "private")   return Visibility::Private;
    if (s == "protected") return Visibility::Protected;
    return Visibility::Public;
}

inline std::string visibility_to_string(Visibility v) {
    switch (v) {
        case Visibility::Private:   return "private";
        case Visibility::Protected: return "protected";
        default:                    return "public";
    }
}

/* ── Field info ── */
struct ClassFieldInfo {
    std::string name;
    std::string default_value;   /* default হিসেবে যা দেওয়া আছে, e.g. "0", "\"unknown\"" */
    int         position { 0 };  /* positional constructor এর জন্য order */
    bool        is_string  { false };
    bool        is_float   { false };
    Visibility  visibility { Visibility::Public };  /* Encapsulation */
};

/* ── Method info ── */
struct ClassMethodInfo {
    std::string              name;
    std::vector<std::string> params;  /* self বাদে parameter names */
    std::string              llvm_name; /* generated LLVM function name, e.g. "Person__greet" */
    Visibility               visibility { Visibility::Public }; /* Encapsulation */
    bool                     is_abstract { false }; /* Abstraction */
    bool                     is_final    { false }; /* Abstraction */
    int                      vtable_index { -1 };   /* Polymorphism: index in vtable */
    bool                     returns_string { false }; /* set by typechecker: true if the method returns a string */
};

/* ── Full class definition ── */
struct ClassInfo {
    std::string name;
    std::string parent_name;   /* extends থাকলে, নাহলে "" */
    bool        is_abstract { false };  /* Abstraction */
    bool        is_final    { false };  /* final class — cannot be extended */
    bool        has_vtable  { false };  /* Polymorphism: has virtual method table */

    std::vector<ClassFieldInfo>  fields;
    std::vector<ClassMethodInfo> methods;
    std::vector<std::string>     interface_names;  /* interfaces this class implements */
    /* interface method -> implementing class method name mapping */
    std::unordered_map<std::string, std::string> interface_method_map;

    /* field নাম দিয়ে index খোঁজা */
    int field_index(const std::string& fname) const {
        for (int i = 0; i < (int)fields.size(); i++)
            if (fields[i].name == fname) return i;
        return -1;
    }

    /* method নাম দিয়ে খোঁজা (inheritance চেক করে না) */
    const ClassMethodInfo* find_method(const std::string& mname) const {
        for (auto& m : methods)
            if (m.name == mname) return &m;
        return nullptr;
    }

    /* method by vtable index */
    const ClassMethodInfo* method_by_vtable_index(int idx) const {
        for (auto& m : methods)
            if (m.vtable_index == idx) return &m;
        return nullptr;
    }

    bool has_field(const std::string& fname) const {
        return field_index(fname) >= 0;
    }

    /* check if this class (or parent) has any virtual methods */
    bool needs_vtable() const {
        if (has_vtable) return true;
        for (auto& m : methods)
            if (m.vtable_index >= 0) return true;
        return false;
    }
};

/* ════════════════════════════════════════════════════════════
   ClassRegistry — সব class store করে, compile-time এ accessible
   ════════════════════════════════════════════════════════════ */
class ClassRegistry {
public:
    /* class register করো */
    void register_class(ClassInfo info) {
        std::string name = info.name;
        classes_[name] = std::move(info);
    }

    /* class আছে কিনা দেখো */
    bool has(const std::string& name) const {
        return classes_.count(name) > 0;
    }

    /* class info নাও — না পেলে nullptr */
    const ClassInfo* get(const std::string& name) const {
        auto it = classes_.find(name);
        return (it != classes_.end()) ? &it->second : nullptr;
    }

    ClassInfo* get_mut(const std::string& name) {
        auto it = classes_.find(name);
        return (it != classes_.end()) ? &it->second : nullptr;
    }

    /* inheritance সহ field খোঁজা */
    const ClassFieldInfo* find_field(const std::string& class_name,
                                     const std::string& field_name) const {
        const ClassInfo* cls = get(class_name);
        while (cls) {
            int idx = cls->field_index(field_name);
            if (idx >= 0) return &cls->fields[idx];
            if (cls->parent_name.empty()) break;
            cls = get(cls->parent_name);
        }
        return nullptr;
    }

    /* inheritance সহ method খোঁজা */
    const ClassMethodInfo* find_method(const std::string& class_name,
                                        const std::string& method_name) const {
        const ClassInfo* cls = get(class_name);
        while (cls) {
            auto* m = cls->find_method(method_name);
            if (m) return m;
            if (cls->parent_name.empty()) break;
            cls = get(cls->parent_name);
        }
        return nullptr;
    }

    /* inheritance + visibility সহ method খোঁজা */
    const ClassMethodInfo* find_method_in_class(
            const std::string& class_name,
            const std::string& method_name) const {
        const ClassInfo* cls = get(class_name);
        if (!cls) return nullptr;
        return cls->find_method(method_name);
    }

    /* inheritance সহ সব fields collect করা (parent fields আগে) */
    std::vector<ClassFieldInfo> all_fields(const std::string& class_name) const {
        std::vector<ClassFieldInfo> result;
        _collect_fields(class_name, result);
        return result;
    }

    /* inheritance সহ সব methods collect করা (parent methods আগে) */
    std::vector<const ClassMethodInfo*> all_methods(const std::string& class_name) const {
        std::vector<const ClassMethodInfo*> result;
        _collect_methods(class_name, result);
        return result;
    }

    /* check if class_name (or parent) implements interface_name */
    bool class_implements_interface(const std::string& class_name,
                                    const std::string& interface_name) const {
        const ClassInfo* cls = get(class_name);
        while (cls) {
            for (auto& iname : cls->interface_names)
                if (iname == interface_name) return true;
            if (cls->parent_name.empty()) break;
            cls = get(cls->parent_name);
        }
        return false;
    }

    /* check if class can be extended (not final) */
    bool can_extend(const std::string& class_name) const {
        const ClassInfo* cls = get(class_name);
        return cls && !cls->is_final;
    }

    /* check if class has abstract methods (make it abstract) */
    bool has_abstract_methods(const std::string& class_name) const {
        const ClassInfo* cls = get(class_name);
        if (!cls) return false;
        for (auto& m : cls->methods)
            if (m.is_abstract) return true;
        return false;
    }

    /* compute vtable for a class (collects all virtual methods including inherited) */
    std::vector<const ClassMethodInfo*> compute_vtable(const std::string& class_name) const {
        std::vector<const ClassMethodInfo*> vtable;
        auto all_m = all_methods(class_name);
        for (auto* m : all_m) {
            if (m->vtable_index >= 0) {
                /* ensure vtable is sized correctly */
                while ((int)vtable.size() <= m->vtable_index)
                    vtable.push_back(nullptr);
                vtable[m->vtable_index] = m;
            }
        }
        return vtable;
    }

    /* Search ALL registered classes for one that has a field with the given name.
       Returns the class name, or empty string if none found (or ambiguous). */
    std::string find_class_by_field(const std::string& field_name) const {
        std::string result;
        for (auto& [name, cls] : classes_) {
            if (cls.field_index(field_name) >= 0) {
                if (!result.empty()) return ""; /* ambiguous */
                result = name;
            }
        }
        return result;
    }

    /* Search ALL registered classes for one that has a method with the given name. */
    std::string find_class_by_method(const std::string& method_name) const {
        std::string result;
        for (auto& [name, cls] : classes_) {
            if (cls.find_method(method_name)) {
                if (!result.empty()) return ""; /* ambiguous */
                result = name;
            }
        }
        return result;
    }

    const std::unordered_map<std::string, ClassInfo>& all() const {
        return classes_;
    }

private:
    std::unordered_map<std::string, ClassInfo> classes_;

    void _collect_fields(const std::string& class_name,
                          std::vector<ClassFieldInfo>& out) const {
        const ClassInfo* cls = get(class_name);
        if (!cls) return;
        /* parent fields আগে */
        if (!cls->parent_name.empty())
            _collect_fields(cls->parent_name, out);
        for (auto& f : cls->fields)
            out.push_back(f);
    }

    void _collect_methods(const std::string& class_name,
                           std::vector<const ClassMethodInfo*>& out) const {
        const ClassInfo* cls = get(class_name);
        if (!cls) return;
        /* parent methods আগে */
        if (!cls->parent_name.empty())
            _collect_methods(cls->parent_name, out);
        for (auto& m : cls->methods)
            out.push_back(&m);
    }
};

/* ── Global singleton — সব stage এ একই registry use করবে ── */
/* TODO: add thread-safety (mutex) for global_class_registry if used from multiple threads.
   Currently safe for single-threaded single-pass compilation only. */
inline ClassRegistry& global_class_registry() {
    static ClassRegistry reg;
    return reg;
}

/* Global set of Aurora functions that return strings.
   Populated during typechecking, checked during codegen. */
/* TODO: add thread-safety for global_string_fns if used from multiple threads. */
inline std::unordered_set<std::string>& global_string_fns() {
    static std::unordered_set<std::string> fns;
    return fns;
}

/* ════════════════════════════════════════════════════════════
   OOP TypeChecker free functions — no LLVM dependency
   (defined in typechecker_oop.cpp)
   ════════════════════════════════════════════════════════════ */
struct ASTNode;

void oop_register_class     (const ASTNode* node);
void oop_check_new_object   (const std::string& class_name,
                              const ASTNode* args_node, int src_line);
void oop_check_method_call  (const std::string& obj_name,
                              const std::string& method_name, int src_line);
void oop_check_field_access (const std::string& obj_name,
                              const std::string& field_name, int src_line);
void oop_register_object    (const std::string& var_name,
                              const std::string& class_name);
void oop_clear_object_types ();

/* ── Visibility helpers ── */
bool oop_is_accessible(Visibility member_vis, Visibility context_vis,
                       bool is_same_class, bool is_subclass);

/* ── Current TypeChecker setter for visibility checks ── */
struct TypeChecker;
void oop_set_current_tc(TypeChecker* tc);

/* ── Utility (LLVM-dependent) — defined in codegen_oop.cpp ── */
bool        oop_is_object   (const std::string& var_name);
std::string oop_class_of    (const std::string& var_name);
void        oop_clear       ();
void        oop_clear_vtable_cache ();
