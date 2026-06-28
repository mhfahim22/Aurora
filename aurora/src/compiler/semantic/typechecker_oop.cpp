#include "compiler/typechecker.hpp"
#include "compiler/class_oop.hpp"
#include "compiler/ast.hpp"
#include "compiler/type_registry.hpp"

#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <set>

/* ════════════════════════════════════════════════════════════
   Aurora OOP — TypeChecker Extension
   ════════════════════════════════════════════════════════════ */

/* ── object type tracking: variable name → class name ── */
static std::unordered_map<std::string, std::string> tc_object_types_;

/* ── thread-local current typechecker for visibility queries ── */
static TypeChecker* current_tc_ = nullptr;

void oop_set_current_tc(TypeChecker* tc) { current_tc_ = tc; }

/* ════════════════════════════════════════════════════════════
   Visibility helper — check if member_vis is accessible from context_vis
   ════════════════════════════════════════════════════════════ */
bool oop_is_accessible(Visibility member_vis, Visibility /*context_vis*/,
                       bool is_same_class, bool is_subclass) {
    switch (member_vis) {
        case Visibility::Public:
            return true;
        case Visibility::Protected:
            return is_same_class || is_subclass;
        case Visibility::Private:
            return is_same_class;
    }
    return false;
}

/* ── Get the visibility context for the current scope ── */
static Visibility get_current_context_vis() {
    return Visibility::Public; /* top-level code = public context */
}

/* ── Check if we are inside the given class (or a subclass) ── */
static void check_class_context(const std::string& class_name,
                                 int src_line,
                                 bool& is_same_class,
                                 bool& is_subclass) {
    is_same_class = false;
    is_subclass = false;
    if (!current_tc_) return;
    const std::string& current = current_tc_->current_class_name();
    if (current.empty()) return;
    if (current == class_name) {
        is_same_class = true;
        return;
    }
    /* check if current is a subclass of class_name */
    const ClassInfo* cls = global_class_registry().get(current);
    while (cls) {
        if (cls->parent_name == class_name) {
            is_subclass = true;
            return;
        }
        if (cls->parent_name.empty()) break;
        cls = global_class_registry().get(cls->parent_name);
    }
}

/* ════════════════════════════════════════
   Interface implementation validation
   ════════════════════════════════════════ */
static void validate_interface_impl(const ClassInfo& cls) {
    for (auto& iname : cls.interface_names) {
        if (!global_type_registry().has_interface(iname)) {
            throw std::runtime_error(
                "class '" + cls.name + "' implements unknown interface '" + iname + "'");
        }
        const InterfaceInfo* iface = global_type_registry().get_interface(iname);
        if (!iface) continue;

        for (auto& imethod : iface->methods) {
            /* Check against cls.methods directly (not yet registered in global registry) */
            bool found = false;
            for (auto& m : cls.methods) {
                if (m.name == imethod.name) { found = true; break; }
            }
            if (!found) {
                /* Also check parent methods */
                if (!cls.parent_name.empty()) {
                    const ClassMethodInfo* parent_method =
                        global_class_registry().find_method(cls.parent_name, imethod.name);
                    if (parent_method) found = true;
                }
            }
            if (!found) {
                throw std::runtime_error(
                    "class '" + cls.name + "' does not implement interface method '" +
                    imethod.name + "' from interface '" + iname + "'");
            }
        }
    }
}

/* ════════════════════════════════════════
   Class definition register করা
   ════════════════════════════════════════ */
void oop_register_class(const ASTNode* node) {
    if (!node || node->type != NodeType::Class) return;

    ClassInfo info;
    info.name = node->value;
    info.is_abstract = node->is_abstract;
    info.is_final = node->is_final;

    if (node->left) {
        info.parent_name = node->left->value;
        if (!global_class_registry().has(info.parent_name)) {
            throw std::runtime_error(
                "Line " + std::to_string(node->src_line) +
                ": parent class '" + info.parent_name + "' not defined");
        }
        /* Check: cannot extend final class */
        if (!global_class_registry().can_extend(info.parent_name)) {
            throw std::runtime_error(
                "Line " + std::to_string(node->src_line) +
                ": cannot extend final class '" + info.parent_name + "'");
        }
    }

    /* Helper: check if a method returns a string based on its body */
    auto method_returns_string = [&](const ASTNode* fn_node) -> bool {
        const ASTNode* s = fn_node->body.get();
        while (s) {
            if (s->type == NodeType::Return && s->left) {
                if (s->left->type == NodeType::Attribute && s->left->left &&
                    s->left->left->value == "self") {
                    const std::string& fname = s->left->value;
                    for (auto& f : info.fields)
                        if (f.name == fname && f.type_kind == AstTypeKind::String) return true;
                }
            }
            s = s->next.get();
        }
        return false;
    };

    /* Parse implements */
    if (node->right) {
        info.interface_names.push_back(node->right->value);
    }

    /* track vtable index across methods */
    int next_vtable_idx = 0;
    /* inherit parent's vtable size */
    if (!info.parent_name.empty()) {
        const ClassInfo* parent = global_class_registry().get(info.parent_name);
        if (parent) {
            /* Do NOT auto-inherit abstract flag. Only the abstract keyword makes a class abstract.
               But check if the class is abstract because it has abstract methods that aren't overridden */
            /* Count existing virtual methods in parent for vtable continuity */
            auto parent_vtable = global_class_registry().compute_vtable(info.parent_name);
            next_vtable_idx = (int)parent_vtable.size();
        }
    }

    /* Also reserve vtable slots for interface methods */
    for (auto& iname : info.interface_names) {
        const InterfaceInfo* iface = global_type_registry().get_interface(iname);
        if (iface) {
            for (auto& imethod : iface->methods) {
                /* check if already mapped by a parent */
                const ClassMethodInfo* existing = nullptr;
                if (!info.parent_name.empty())
                    existing = global_class_registry().find_method(info.parent_name, imethod.name);
                if (!existing) {
                    /* will be assigned when we see the implementing method */
                }
            }
        }
    }

    int field_pos = 0;
    const ASTNode* stmt = node->body.get();
    while (stmt) {
        if (stmt->type == NodeType::Assign && stmt->left) {
            ClassFieldInfo field;
            field.name     = stmt->left->value;
            field.position = field_pos++;
            field.visibility = visibility_from_string(stmt->value);

            if (stmt->right) {
                if (stmt->right->type == NodeType::Str) {
                    field.default_value = "\"" + stmt->right->value + "\"";
                    field.type_kind = AstTypeKind::String;
                } else if (stmt->right->type == NodeType::Float) {
                    field.default_value = stmt->right->value;
                    field.type_kind = AstTypeKind::Float;
                } else if (stmt->right->type == NodeType::Num) {
                    field.default_value = stmt->right->value;
                    field.type_kind = AstTypeKind::Int;
                } else {
                    field.default_value = stmt->right->value;
                }
            } else {
                field.default_value = "0";
            }

            info.fields.push_back(field);
        }

        if (stmt->type == NodeType::Function) {
            ClassMethodInfo method;
            method.name      = stmt->value;
            method.llvm_name = info.name + "__" + stmt->value;
            method.visibility = visibility_from_string(stmt->visibility);
            method.is_abstract = stmt->is_abstract;
            method.is_final = stmt->is_final;

            const ASTNode* param = stmt->args.get();
            while (param) {
                if (param->value != "self")
                    method.params.push_back(param->value);
                param = param->next.get();
            }

            /* Assign vtable index for virtual/interface methods */
            bool needs_virtual = false;

            /* 1. Method is abstract -> virtual */
            if (method.is_abstract) {
                needs_virtual = true;
            }

            /* 2. Method overrides a parent virtual method */
            if (!info.parent_name.empty()) {
                const ClassMethodInfo* parent_method =
                    global_class_registry().find_method(info.parent_name, method.name);
                if (parent_method && parent_method->vtable_index >= 0) {
                    needs_virtual = true;
                    method.vtable_index = parent_method->vtable_index;
                }
            }

            /* 3. Method implements an interface method */
            for (auto& iname : info.interface_names) {
                const InterfaceInfo* iface = global_type_registry().get_interface(iname);
                if (iface) {
                    for (auto& imethod : iface->methods) {
                        if (imethod.name == method.name) {
                            needs_virtual = true;
                            info.interface_method_map[imethod.name] = method.name;
                            break;
                        }
                    }
                }
            }

            if (needs_virtual && method.vtable_index < 0) {
                method.vtable_index = next_vtable_idx++;
            }

            /* H2 Phase D2: prefer annotation on function node for return type */
            if (stmt->type_annotation.kind != AstTypeKind::Unknown) {
                method.return_kind = stmt->type_annotation.kind;
            } else {
                method.return_kind = method_returns_string(stmt) ? AstTypeKind::String : AstTypeKind::Unknown;
            }

            info.methods.push_back(method);
        }

        stmt = stmt->next.get();
    }

    info.has_vtable = (next_vtable_idx > 0);

    /* Validate interface implementation */
    validate_interface_impl(info);

    global_class_registry().register_class(std::move(info));
}

/* ════════════════════════════════════════
   Object creation type check — also prevents instantiation of abstract classes
   ════════════════════════════════════════ */
void oop_check_new_object(const std::string& class_name,
                           const ASTNode*     args_node,
                           int                src_line) {
    const ClassInfo* cls = global_class_registry().get(class_name);
    if (!cls) return;

    /* Abstract class check */
    if (cls->is_abstract || global_class_registry().has_abstract_methods(class_name)) {
        throw std::runtime_error(
            "Line " + std::to_string(src_line) +
            ": cannot instantiate abstract class '" + class_name + "'");
    }

    auto all_fields = global_class_registry().all_fields(class_name);
    int  field_count = (int)all_fields.size();

    int arg_count = 0;
    const ASTNode* arg = args_node;
    while (arg) { arg_count++; arg = arg->next.get(); }

    if (arg_count > field_count) {
        std::ostringstream msg;
        msg << "Line " << src_line << ": class '" << class_name
            << "' has " << field_count << " fields but "
            << arg_count << " arguments given";
        throw std::runtime_error(msg.str());
    }
}

/* ════════════════════════════════════════
   Method call type check — with visibility enforcement
   ════════════════════════════════════════ */
void oop_check_method_call(const std::string& obj_name,
                            const std::string& method_name,
                            int                src_line) {
    auto it = tc_object_types_.find(obj_name);
    if (it == tc_object_types_.end()) return;

    const std::string& class_name = it->second;
    const ClassMethodInfo* method =
        global_class_registry().find_method(class_name, method_name);

    if (!method) {
        throw std::runtime_error(
            "Line " + std::to_string(src_line) +
            ": class '" + class_name +
            "' has no method '" + method_name + "'");
    }

    /* Visibility check */
    if (method->visibility != Visibility::Public) {
        bool same = false, sub = false;
        check_class_context(class_name, src_line, same, sub);
        if (!oop_is_accessible(method->visibility, get_current_context_vis(), same, sub)) {
            throw std::runtime_error(
                "Line " + std::to_string(src_line) +
                ": method '" + method_name + "' is " +
                visibility_to_string(method->visibility) +
                " in class '" + class_name + "'");
        }
    }
}

/* ════════════════════════════════════════
   Field access type check — with visibility enforcement
   ════════════════════════════════════════ */
void oop_check_field_access(const std::string& obj_name,
                             const std::string& field_name,
                             int                src_line) {
    auto it = tc_object_types_.find(obj_name);
    if (it == tc_object_types_.end()) return;

    const std::string& class_name = it->second;
    const ClassFieldInfo* field =
        global_class_registry().find_field(class_name, field_name);

    if (!field) {
        throw std::runtime_error(
            "Line " + std::to_string(src_line) +
            ": class '" + class_name +
            "' has no field '" + field_name + "'");
    }

    /* Visibility check */
    if (field->visibility != Visibility::Public) {
        bool same = false, sub = false;
        check_class_context(class_name, src_line, same, sub);
        if (!oop_is_accessible(field->visibility, get_current_context_vis(), same, sub)) {
            throw std::runtime_error(
                "Line " + std::to_string(src_line) +
                ": field '" + field_name + "' is " +
                visibility_to_string(field->visibility) +
                " in class '" + class_name + "'");
        }
    }
}

/* ════════════════════════════════════════
   Object type register করা
   ════════════════════════════════════════ */
void oop_register_object(const std::string& var_name,
                          const std::string& class_name) {
    tc_object_types_[var_name] = class_name;
}

void oop_clear_object_types() {
    tc_object_types_.clear();
}

/* ── Getter: class name for a registered object variable ── */
std::string oop_class_of_var(const std::string& var_name) {
    auto it = tc_object_types_.find(var_name);
    if (it != tc_object_types_.end()) return it->second;
    return "";
}