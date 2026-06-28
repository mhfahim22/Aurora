#include "compiler/typechecker.hpp"
#include "compiler/class_oop.hpp"
#include "compiler/type_registry.hpp"

#include <sstream>
#include <iostream>

/* ── FFI helper: parse type name string → AuroraType ── */
static AuroraType parse_extern_type(const std::string& name) {
    if (name == "int" || name == "i64" || name == "Int" || name == "u64")   return AuroraType::Int;
    if (name == "i32" || name == "u32")    return AuroraType::Int;     /* treat as int */
    if (name == "i16")    return AuroraType::Int;     /* treat as int */
    if (name == "i8" || name == "char") return AuroraType::Int;  /* treat as int */
    if (name == "float" || name == "f64" || name == "Float" || name == "double") return AuroraType::Float;
    if (name == "f32")    return AuroraType::Float;   /* treat as float */
    if (name == "string" || name == "String" || name == "str" || name == "cstring" || name == "char*") return AuroraType::String;
    if (name == "void" || name == "Void")    return AuroraType::Void;
    if (name == "bool" || name == "Bool")    return AuroraType::Bool;
    if (name == "pointer" || name == "Pointer" || name == "ptr" || name == "void*") return AuroraType::Pointer;
    return AuroraType::Unknown;
}

/* ── AstTypeKind → AuroraType conversion (reverse) ── */
static AuroraType ast_kind_to_type(AstTypeKind k) {
    switch (k) {
        case AstTypeKind::Unknown:   return AuroraType::Unknown;
        case AstTypeKind::Int:       return AuroraType::Int;
        case AstTypeKind::Float:     return AuroraType::Float;
        case AstTypeKind::String:    return AuroraType::String;
        case AstTypeKind::Bool:      return AuroraType::Bool;
        case AstTypeKind::Array:     return AuroraType::Array;
        case AstTypeKind::Struct:    return AuroraType::Struct;
        case AstTypeKind::Function:  return AuroraType::Function;
        case AstTypeKind::Class:     return AuroraType::Class;
        case AstTypeKind::Void:      return AuroraType::Void;
        case AstTypeKind::Enum:      return AuroraType::Enum;
        case AstTypeKind::Interface: return AuroraType::Interface;
        case AstTypeKind::Tuple:     return AuroraType::Tuple;
        case AstTypeKind::Pointer:   return AuroraType::Pointer;
        case AstTypeKind::List:      return AuroraType::List;
        case AstTypeKind::Map:       return AuroraType::Map;
        case AstTypeKind::Set:       return AuroraType::Set;
        case AstTypeKind::Vector:    return AuroraType::Vector;
        case AstTypeKind::Stack:     return AuroraType::Stack;
        case AstTypeKind::Queue:     return AuroraType::Queue;
        case AstTypeKind::Json:      return AuroraType::Json;
    }
    return AuroraType::Unknown;
}

/* ── AuroraType → AstTypeKind conversion (H2 Phase B) ── */
static AstTypeKind to_ast_type_kind(AuroraType t) {
    switch (t) {
        case AuroraType::Unknown:  return AstTypeKind::Unknown;
        case AuroraType::Void:     return AstTypeKind::Void;
        case AuroraType::Int:      return AstTypeKind::Int;
        case AuroraType::Float:    return AstTypeKind::Float;
        case AuroraType::String:   return AstTypeKind::String;
        case AuroraType::Bool:     return AstTypeKind::Bool;
        case AuroraType::Array:    return AstTypeKind::Array;
        case AuroraType::Struct:   return AstTypeKind::Struct;
        case AuroraType::Function: return AstTypeKind::Function;
        case AuroraType::Class:    return AstTypeKind::Class;
        case AuroraType::Enum:      return AstTypeKind::Enum;
        case AuroraType::Interface: return AstTypeKind::Interface;
        case AuroraType::Tuple:    return AstTypeKind::Tuple;
        case AuroraType::Pointer:  return AstTypeKind::Pointer;
        case AuroraType::List:     return AstTypeKind::List;
        case AuroraType::Map:       return AstTypeKind::Map;
        case AuroraType::Set:       return AstTypeKind::Set;
        case AuroraType::Vector:   return AstTypeKind::Vector;
        case AuroraType::Stack:    return AstTypeKind::Stack;
        case AuroraType::Queue:    return AstTypeKind::Queue;
        case AuroraType::Json:     return AstTypeKind::Json;
    }
    return AstTypeKind::Unknown;
}

/* ── Write resolved type annotation onto an AST node (H2 Phase B) ── */
/*   element_kind_val: for compound types (Array, List, etc.), the element type.
       Defaults to Unknown, which preserves backwards compatibility.                 */
static void annotate_node(const ASTNode* node, AuroraType type,
                          const std::string& user_type_name = "",
                          AstTypeKind element_kind_val = AstTypeKind::Unknown) {
    if (!node) return;
    auto& ann = const_cast<ASTNode*>(node)->type_annotation;
    ann.kind = to_ast_type_kind(type);
    ann.type_name = user_type_name;
    ann.element_kind = element_kind_val;
}

/* ── Annotate and return helper — used at every return site of infer_* ── */
static AuroraType annotate_ret(const ASTNode* node, AuroraType type,
                               const std::string& user_type_name = "",
                               AstTypeKind element_kind_val = AstTypeKind::Unknown) {
    annotate_node(node, type, user_type_name, element_kind_val);
    return type;
}

/* ── ABI validation: check if a type name is valid for C FFI ── */
bool is_valid_abi_type(const std::string& name) {
    auto pt = parse_extern_type(name);
    if (pt != AuroraType::Unknown) return true;    /* includes int, float, string, void, bool, pointer */
    if (global_type_registry().has_struct(name)) return true;
    if (global_type_registry().has_enum(name)) return true;
    return false;
}

void validate_abi_type(const std::string& name, int line, const std::string& context) {
    if (!is_valid_abi_type(name)) {
        std::ostringstream msg;
        msg << "ABI validation error at line " << line << ": '" << name
            << "' is not a valid C-compatible type in " << context
            << ". Use int, i64, i32, i16, i8, float, f64, f32, double, string, bool, pointer, a struct name, or void.";
        throw TypeError(msg.str(), line);
    }
}

void TypeChecker::analyse(const ASTNode* root) {
    functions_.clear();
    scopes_.clear();
    var_element_types_.clear();
    user_types_.clear();
    current_return_type_ = AuroraType::Unknown;
    inside_function_ = false;
    current_class_name_.clear();
    current_class_depth_ = 0;

    oop_set_current_tc(this);
    oop_clear_object_types();

    register_functions(root);
    push_scope();
    walk_block(root);
    pop_scope();
}

void TypeChecker::push_scope() {
    scopes_.emplace_back();
}

void TypeChecker::pop_scope() {
    if (!scopes_.empty()) scopes_.pop_back();
}

void TypeChecker::define_match_pattern_vars(const ASTNode* pattern) {
    if (!pattern) return;
    switch (pattern->type) {
        case NodeType::Var:
            if (pattern->value != "_") {
                define_var(pattern->value, AuroraType::Int);
                annotate_node(pattern, AuroraType::Int);  /* H2 Phase D2 */
            }
            break;
        case NodeType::Call: {
            const ASTNode* fp = pattern->args.get();
            while (fp) {
                define_match_pattern_vars(fp);
                fp = fp->next.get();
            }
            break;
        }
        case NodeType::Array: {
            const ASTNode* ep = pattern->args.get();
            while (ep) {
                define_match_pattern_vars(ep);
                ep = ep->next.get();
            }
            break;
        }
        default:
            break;
    }
}

void TypeChecker::define_var(const std::string& name, AuroraType type) {
    if (scopes_.empty()) push_scope();

    for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) {
            it->second = type;
            return;
        }
    }
    scopes_.back()[name] = type;
}

/* H2 Phase E-1: define variable with element kind tracking */
void TypeChecker::define_var_elem(const std::string& name, AuroraType type,
                                  AstTypeKind elem_kind) {
    define_var(name, type);
    if (elem_kind != AstTypeKind::Unknown)
        var_element_types_[name] = elem_kind;
    else
        var_element_types_.erase(name);
}

/* H2 Phase E-1: lookup element kind for a variable */
AstTypeKind TypeChecker::lookup_var_elem(const std::string& name) const {
    auto it = var_element_types_.find(name);
    if (it != var_element_types_.end()) return it->second;
    return AstTypeKind::Unknown;
}

AuroraType TypeChecker::lookup_var(const std::string& name, int line) const {
    for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return it->second;
    }

    std::ostringstream msg;
    msg << "unknown variable '" << name << "'";
    throw TypeError(msg.str(), line);
}

AuroraType TypeChecker::get_type(const std::string& var_name) const {
    for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
        auto it = scopes_[i].find(var_name);
        if (it != scopes_[i].end()) return it->second;
    }
    return AuroraType::Unknown;
}

bool TypeChecker::has_type(const std::string& var_name) const {
    for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
        if (scopes_[i].find(var_name) != scopes_[i].end()) return true;
    }
    return false;
}

std::string TypeChecker::type_describe(const std::string& var_name) const {
    AuroraType t = get_type(var_name);
    if (t == AuroraType::Struct || t == AuroraType::Class) {
        /* Look up user type name — for now return the base name */
        return aurora_type_name(t);
    }
    return aurora_type_name(t);
}

void TypeChecker::register_functions(const ASTNode* node) {
    functions_["output"]        = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["outputln"]     = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["outputN"]      = FunctionTypeInfo{{}};
    functions_["outputf"]      = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["input"]         = FunctionTypeInfo{{}};
    functions_["len"]           = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["sum"]           = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["min"]           = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["max"]           = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["range"]         = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["type"]          = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["convert"]       = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["clone"]         = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["debug"]         = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["panic"]         = FunctionTypeInfo{{AuroraType::Unknown}};

    /* ── String builtins ── */
    functions_["upper"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["lower"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["trim"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["replace"]       = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown, AuroraType::Unknown}, AuroraType::String};
    functions_["split"]         = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Array};
    functions_["join"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::String};
    functions_["has"]           = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["starts"]        = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["ends"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["reverse"]       = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["strlen"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["strcat"]        = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::String};
    functions_["substr"]        = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown, AuroraType::Unknown}, AuroraType::String};
    functions_["index"]         = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Int};

    /* ── Math builtins ── */
    functions_["abs"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Float};
    functions_["sqrt"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Float};
    functions_["floor"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Float};
    functions_["ceil"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Float};
    functions_["round"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Float};
    functions_["pow"]           = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Float};
    functions_["clamp"]         = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Float};
    functions_["rand"]          = FunctionTypeInfo{{}, AuroraType::Int};

    /* ── Number conversion builtins ── */
    functions_["str"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["int"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["float"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Float};
    functions_["bool"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* ── Collection builtins ── */
    functions_["push"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}};
    functions_["pop"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["insert"]        = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown, AuroraType::Unknown}};
    functions_["remove"]        = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}};
    functions_["clear"]         = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["sort"]          = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["unique"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Array};
    functions_["map"]           = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Array};
    functions_["filter"]        = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Array};
    functions_["reduce"]        = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Unknown};
    functions_["find"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Int};
    functions_["any"]           = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["all"]           = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};

    /* ── Collection utility functions (runtime-backed) ── */
    functions_["list_get"]      = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Int};
    functions_["list_len"]      = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["list_push"]     = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}};
    functions_["list_free"]     = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["map_get"]       = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Int};
    functions_["map_has"]       = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["map_set"]       = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown, AuroraType::Unknown}};
    functions_["map_copy"]      = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}};
    functions_["map_free"]      = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["set_add"]       = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}};
    functions_["set_has"]       = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["set_free"]      = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["stack_push"]    = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}};
    functions_["stack_pop"]     = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["stack_empty"]   = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["stack_free"]    = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["queue_enqueue"] = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}};
    functions_["queue_dequeue"] = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["queue_empty"]   = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["queue_free"]    = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["vector_x"]      = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Float};
    functions_["vector_y"]      = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Float};
    functions_["vector_z"]      = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Float};
    functions_["json_free"]     = FunctionTypeInfo{{AuroraType::Unknown}};

    /* ── File builtins ── */
    functions_["read"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["write"]         = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["append"]        = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["exists"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["delete"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["copy"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["move"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["download"]      = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Int};

    /* ── Path builtins ── */
    functions_["cwd"]           = FunctionTypeInfo{{}, AuroraType::String};
    functions_["cd"]            = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["path"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["name"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["ext"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};

    /* ── Time builtins ── */
    functions_["now"]           = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["sleep"]         = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["stamp"]         = FunctionTypeInfo{{}, AuroraType::Int};

    /* ── JSON builtins ── */
    functions_["encode"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["decode"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Json};

    /* ── HTTP builtins ── */
    functions_["get"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["post"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::String};

    /* ── OS / Environment builtins ── */
    functions_["os"]            = FunctionTypeInfo{{}, AuroraType::String};
    functions_["cpu"]           = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["mem"]           = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["env"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["run"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["exit"]          = FunctionTypeInfo{{AuroraType::Unknown}};

    /* ── Error builtins ── */
    functions_["error"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};

    /* ── I/O / Confirm ── */
    functions_["ask"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* ── Type conversion ── */
    functions_["char"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};

    /* ── Path aliases ── */
    functions_["dir"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};

    /* ── Async builtins ── */
    functions_["spawn"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["await"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Unknown};
    functions_["chan"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["send"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}};
    functions_["html"]          = FunctionTypeInfo{{AuroraType::Unknown}};
    functions_["content_type"]  = FunctionTypeInfo{{AuroraType::String}};
    functions_["recv"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Unknown};

    /* ── Event bus builtins ── */
    functions_["event_on"]      = FunctionTypeInfo{{AuroraType::String, AuroraType::Unknown}};
    functions_["event_off"]     = FunctionTypeInfo{{AuroraType::String, AuroraType::Unknown}};
    functions_["event_emit"]    = FunctionTypeInfo{{AuroraType::String, AuroraType::Unknown}};

    /* ── Fiber builtins ── */
    functions_["fiber_create"]  = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Int};
    functions_["fiber_resume"]  = FunctionTypeInfo{{AuroraType::Int}};
    functions_["fiber_yield"]   = FunctionTypeInfo{{}};
    functions_["fiber_is_done"] = FunctionTypeInfo{{AuroraType::Int}, AuroraType::Int};
    functions_["fiber_get_result"] = FunctionTypeInfo{{AuroraType::Int}, AuroraType::Int};
    functions_["fiber_destroy"] = FunctionTypeInfo{{AuroraType::Int}};

    /* ── Performance builtins ── */
    functions_["measure"]       = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["bench"]         = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Int};
    functions_["profile"]       = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["trace"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};

    /* ── Reflection builtins ── */
    functions_["fields"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Array};
    functions_["methods"]       = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Array};

    /* ── Package builtins ── */
    functions_["install"]       = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["update"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["search"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Array};


    /* ════════════════════════════════════════════
        Backend / Server builtins
        ════════════════════════════════════════════ */

    /* Route/Middleware */
    functions_["route_group"]      = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["middleware"]       = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["next"]            = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["rate_limit"]      = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["cors"]            = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["csrf"]            = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["request"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["status"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Unknown};

    /* Session */
    functions_["session"]         = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["session_get"]     = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["session_set"]     = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["session_delete"]  = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* Cookie */
    functions_["cookie_get"]      = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["cookie_set"]      = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["cookie_delete"]   = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* Proxy/Streaming */
    functions_["proxy"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["reverse_proxy"]   = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["stream"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["stream_file"]     = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["sse"]             = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["webhook"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* Health/Metrics */
    functions_["health"]          = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["metrics"]         = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["trace_id"]        = FunctionTypeInfo{{}, AuroraType::String};
    functions_["request_id"]      = FunctionTypeInfo{{}, AuroraType::String};
    functions_["audit"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* Lock/Sync */
    functions_["lock"]            = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["unlock"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["atomic"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["retry"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["timeout"]         = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["circuit_breaker"] = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* Pool */
    functions_["pool"]            = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["worker_pool"]     = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["batch"]           = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Array};
    functions_["paginate"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Array};

    /* DB/ORM */
    functions_["index"]           = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["migrate"]         = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["seed"]            = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["model"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["schema"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["validate"]        = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["sanitize"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};

    /* Throttle/Debounce */
    functions_["throttle"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["debounce"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};

    /* Crypto */
    functions_["sign"]            = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["verify"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["secret"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["vault"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};

    /* Compress/Serialize */
    functions_["compress"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["decompress"]      = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["serialize"]       = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["deserialize"]     = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Unknown};

    /* Event/PubSub */
    functions_["event"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["emit"]            = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["listen"]          = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["publish"]         = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};
    functions_["subscribe"]       = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown}, AuroraType::Bool};

    /* RPC/Cluster */
    functions_["rpc"]             = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["discover"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["cluster"]         = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["node_id"]         = FunctionTypeInfo{{}, AuroraType::String};
    functions_["leader"]          = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["shard"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["replica"]         = FunctionTypeInfo{{}, AuroraType::Bool};

    /* Backup */
    functions_["backup"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["restore"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* Monitor/Profile */
    functions_["monitor"]         = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["profile_request"] = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["memory_snapshot"] = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["gc_collect"]      = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["hot_reload"]      = FunctionTypeInfo{{}, AuroraType::Bool};

    /* Plugin/FeatureFlag */
    functions_["plugin"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["feature_flag"]    = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* Tenant */
    functions_["tenant"]          = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["tenant_context"]  = FunctionTypeInfo{{}, AuroraType::String};

    /* Geo/Captcha */
    functions_["geoip"]           = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["captcha_verify"]  = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* Payment/Analytics */
    functions_["payment"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["invoice"]         = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["analytics"]       = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* Search/AI extended */
    functions_["search_engine"]   = FunctionTypeInfo{{}, AuroraType::Bool};
    functions_["vector_search"]   = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Array};
    functions_["semantic_search"] = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Array};
    functions_["embed_store"]     = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["embed_query"]     = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Array};
    functions_["ai_agent"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["tool"]            = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["workflow"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["pipeline"]        = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};
    functions_["step"]            = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Bool};

    /* ── AI/ML functional builtins ── */
    /* Data loading */
    functions_["csv"]   = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["data"]  = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};

    /* Data processing */
    functions_["clean"]      = FunctionTypeInfo{{AuroraType::Int}, AuroraType::Int};
    functions_["normalize"]  = FunctionTypeInfo{{AuroraType::Int}, AuroraType::Int};
    functions_["standard"]   = FunctionTypeInfo{{AuroraType::Int}, AuroraType::Int};
    functions_["shuffle"]    = FunctionTypeInfo{{AuroraType::Int}, AuroraType::Int};
    functions_["split_data"] = FunctionTypeInfo{{AuroraType::Int, AuroraType::Float}, AuroraType::Int};

    /* Model lifecycle */
    functions_["model_create"] = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};
    functions_["model_save"]   = FunctionTypeInfo{{AuroraType::Int, AuroraType::Unknown}, AuroraType::Int};
    functions_["model_load"]   = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::Int};

    /* Model config */
    functions_["set_loss"]             = FunctionTypeInfo{{AuroraType::Int, AuroraType::Unknown}, AuroraType::Int};
    functions_["set_optimizer"]        = FunctionTypeInfo{{AuroraType::Int, AuroraType::Unknown}, AuroraType::Int};
    functions_["set_lr"]               = FunctionTypeInfo{{AuroraType::Int, AuroraType::Float}, AuroraType::Int};
    functions_["set_batch_size"]       = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["set_epochs"]           = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["set_validation_split"] = FunctionTypeInfo{{AuroraType::Int, AuroraType::Float}, AuroraType::Int};
    functions_["set_verbose"]          = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["set_early_stop"]       = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int}, AuroraType::Int};

    /* Layer creation */
    functions_["dense"]       = FunctionTypeInfo{{AuroraType::Int, AuroraType::Unknown}, AuroraType::Int};
    functions_["conv"]        = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["lstm"]        = FunctionTypeInfo{{AuroraType::Int}, AuroraType::Int};
    functions_["gru"]         = FunctionTypeInfo{{AuroraType::Int}, AuroraType::Int};
    functions_["dropout"]     = FunctionTypeInfo{{AuroraType::Float}, AuroraType::Int};
    functions_["batchnorm"]   = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["attention"]   = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["transformer"] = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["embedding"]   = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["layernorm"]   = FunctionTypeInfo{{}, AuroraType::Int};

    /* Model operations */
    functions_["add"]     = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["train"]   = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["fit"]     = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["test"]    = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["predict"] = FunctionTypeInfo{{AuroraType::Int, AuroraType::Int}, AuroraType::Int};
    functions_["retrain"] = FunctionTypeInfo{{AuroraType::Int}, AuroraType::Int};

    /* ── Todo / In-memory Store (backend) ── */
    functions_["aurora_todo_list"]   = FunctionTypeInfo{{}, AuroraType::String};
    functions_["aurora_todo_create"] = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["aurora_todo_get"]    = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};
    functions_["aurora_todo_update"] = FunctionTypeInfo{{AuroraType::Unknown, AuroraType::Unknown, AuroraType::Unknown}, AuroraType::String};
    functions_["aurora_todo_delete"] = FunctionTypeInfo{{AuroraType::Unknown}, AuroraType::String};

    /* ── Connection Pool ── */
    functions_["aurora_db_pool_create"]       = FunctionTypeInfo{{AuroraType::String, AuroraType::Int, AuroraType::Int}, AuroraType::Pointer};
    functions_["aurora_db_pool_acquire"]      = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Int}, AuroraType::Pointer};
    functions_["aurora_db_pool_release"]      = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_db_pool_query"]        = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::String}, AuroraType::String};
    functions_["aurora_db_pool_query_free"]   = FunctionTypeInfo{{AuroraType::String}, AuroraType::Void};
    functions_["aurora_db_pool_destroy"]      = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_db_pool_active_count"] = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_db_pool_idle_count"]   = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};

    /* ── 4x4 Matrix Math ── */
    functions_["aurora_mat4_new"]         = FunctionTypeInfo{{}, AuroraType::Pointer};
    functions_["aurora_mat4_free"]        = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_mat4_identity"]    = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_mat4_copy"]        = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_mat4_mul"]         = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_mat4_translate"]   = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Float, AuroraType::Float, AuroraType::Float}, AuroraType::Void};
    functions_["aurora_mat4_rotate"]      = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Float, AuroraType::Float, AuroraType::Float, AuroraType::Float}, AuroraType::Void};
    functions_["aurora_mat4_scale"]       = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Float, AuroraType::Float, AuroraType::Float}, AuroraType::Void};
    functions_["aurora_mat4_perspective"] = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Float, AuroraType::Float, AuroraType::Float, AuroraType::Float}, AuroraType::Void};
    functions_["aurora_mat4_lookat"]      = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Float, AuroraType::Float, AuroraType::Float,  AuroraType::Float, AuroraType::Float, AuroraType::Float,  AuroraType::Float, AuroraType::Float, AuroraType::Float}, AuroraType::Void};

    /* ── Cube vertex data helpers ── */
    functions_["aurora_gl_cube_vertices"]     = FunctionTypeInfo{{}, AuroraType::Pointer};
    functions_["aurora_gl_cube_vertex_count"]  = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["aurora_gl_cube_vertex_stride"] = FunctionTypeInfo{{}, AuroraType::Int};

    /* ── Lit cube helpers ── */
    functions_["aurora_gl_lit_cube_vertices"]     = FunctionTypeInfo{{}, AuroraType::Pointer};
    functions_["aurora_gl_lit_cube_vertex_count"]  = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["aurora_gl_lit_cube_vertex_stride"] = FunctionTypeInfo{{}, AuroraType::Int};

    /* ── UV cube helpers ── */
    functions_["aurora_gl_uv_cube_vertices"]     = FunctionTypeInfo{{}, AuroraType::Pointer};
    functions_["aurora_gl_uv_cube_vertex_count"]  = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["aurora_gl_uv_cube_vertex_stride"] = FunctionTypeInfo{{}, AuroraType::Int};

    /* ── Component helpers ── */
    functions_["aurora_component_create"]           = FunctionTypeInfo{
        {AuroraType::Pointer, AuroraType::Int, AuroraType::Int, AuroraType::Int, AuroraType::Int},
        AuroraType::Pointer};
    functions_["aurora_component_destroy"]          = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_component_add_child"]        = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_component_set_widget_type"]  = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Int}, AuroraType::Void};
    functions_["aurora_component_set_render_fn"]    = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_component_set_update_fn"]    = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_component_set_state"]        = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_component_mount"]            = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_component_set_pos"]          = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Int, AuroraType::Int}, AuroraType::Void};
    functions_["aurora_component_set_size"]         = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Int, AuroraType::Int}, AuroraType::Void};
    functions_["aurora_component_show"]             = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_component_hide"]             = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_component_render_tree"]      = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_component_update_tree"]      = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Float}, AuroraType::Void};

    /* ── Win32 UI helpers ── */
    functions_["aurora_ui_win32_init"]              = FunctionTypeInfo{
        {AuroraType::Pointer, AuroraType::Int, AuroraType::Int},
        AuroraType::Int};
    functions_["aurora_ui_win32_create_control"]    = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_ui_win32_destroy_control"]   = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_ui_win32_set_text"]          = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_ui_win32_get_text"]          = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Pointer};
    functions_["aurora_ui_win32_listbox_add"]       = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_ui_win32_listbox_clear"]     = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_ui_win32_listbox_selected"]  = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_ui_win32_listbox_count"]     = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_ui_win32_mount"]             = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_ui_win32_sync_tree"]         = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_ui_win32_run"]               = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["aurora_ui_win32_pump"]              = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["aurora_ui_win32_shutdown"]          = FunctionTypeInfo{{}, AuroraType::Void};
    functions_["aurora_ui_win32_event_type"]        = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["aurora_ui_win32_event_source"]      = FunctionTypeInfo{{}, AuroraType::Pointer};
    functions_["aurora_ui_win32_event_data"]        = FunctionTypeInfo{{}, AuroraType::Int};

    /* ── GLFW cursor helpers ── */
    functions_["aurora_glfw_get_cursor_x"] = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Float};
    functions_["aurora_glfw_get_cursor_y"] = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Float};
    /* ── Generic i32-pair helper ── */
    functions_["aurora_glfw_get_i32_pair"] = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Int}, AuroraType::Int};
    /* ── Convenience window helpers ── */
    functions_["aurora_glfw_window_width"]       = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_glfw_window_height"]      = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_glfw_framebuffer_width"]  = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_glfw_framebuffer_height"] = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_glfw_window_pos_x"]       = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_glfw_window_pos_y"]       = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};

    /* ── GL buffer/VAO helpers ── */
    functions_["aurora_gl_gen_buffer"]        = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["aurora_gl_gen_vertex_array"]  = FunctionTypeInfo{{}, AuroraType::Int};

    /* ── Image helpers ── */
    functions_["aurora_image_load"]              = FunctionTypeInfo{{AuroraType::Pointer, AuroraType::Pointer, AuroraType::Pointer, AuroraType::Pointer}, AuroraType::Pointer};
    functions_["aurora_image_free"]              = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};
    functions_["aurora_image_create_gl_texture"] = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};

    /* ── OBJ helpers ── */
    functions_["aurora_obj_load"]          = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Pointer};
    functions_["aurora_obj_vertex_count"]  = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_obj_vertex_data"]   = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Pointer};
    functions_["aurora_obj_free"]          = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Void};

    /* ── Audio helpers ── */
    functions_["aurora_audio_init"]       = FunctionTypeInfo{{}, AuroraType::Int};
    functions_["aurora_audio_play_file"] = FunctionTypeInfo{{AuroraType::Pointer}, AuroraType::Int};
    functions_["aurora_audio_shutdown"]    = FunctionTypeInfo{{}, AuroraType::Void};

    while (node) {
        if (node->type == NodeType::Function || node->type == NodeType::PerformanceFn) {
            FunctionTypeInfo info;
            const ASTNode* param = node->args.get();
            while (param) {
                info.params.push_back(AuroraType::Unknown);
                param = param->next.get();
            }
            /* Detect generic functions (those with type parameters) */
            if (node->template_params) {
                info.is_generic = true;
                const ASTNode* tp = node->template_params.get();
                while (tp) {
                    info.generic_params.push_back(tp->value);
                    tp = tp->next.get();
                }
                info.generic_ast_node = node;
            }
            functions_[node->value] = info;
        }
        if (node->type == NodeType::ExternFn) {
            FunctionTypeInfo info;
            const ASTNode* param = node->args.get();
            while (param) {
                AuroraType ptype = AuroraType::Int;
                if (param->right) {
                    if (param->right->type == NodeType::FunctionType) {
                        ptype = AuroraType::Pointer; /* callback → function pointer */
                        annotate_node(param->right.get(), AuroraType::Function);  /* H2 Phase D */
                        /* Validate callback param types */
                        const ASTNode* cp = param->right->args.get();
                        while (cp) {
                            validate_abi_type(cp->value, node->src_line, "callback parameter type");
                            cp = cp->next.get();
                        }
                        if (param->right->left)
                            validate_abi_type(param->right->left->value, node->src_line,
                                "callback return type");
                    } else {
                        std::string ptype_name = param->right->value;
                        validate_abi_type(ptype_name, node->src_line,
                            "extern function '" + node->value + "' parameter '" + param->value + "'");
                        ptype = parse_extern_type(ptype_name);
                    }
                }
                info.params.push_back(ptype);
                param = param->next.get();
            }
            if (node->left) {
                std::string ret_name = node->left->value;
                validate_abi_type(ret_name, node->src_line,
                    "extern function '" + node->value + "' return type");
                info.result = parse_extern_type(ret_name);
            } else {
                info.result = AuroraType::Void;
            }
            info.is_vararg = node->is_vararg;
            functions_[node->value] = info;
            /* H2 Phase D: annotate ExternFn node with resolved types */
            annotate_node(node, info.result);
            {
                const ASTNode* ap = node->args.get();
                size_t ai = 0;
                while (ap && ai < info.params.size()) {
                    annotate_node(ap, info.params[ai]);
                    ap = ap->next.get();
                    ai++;
                }
            }
        }
        if (node->type == NodeType::Class) {
            oop_register_class(node);
        }
        if (node->type == NodeType::StructDecl) {
            register_struct(node);
        }
        if (node->type == NodeType::ExternStruct) {
            register_struct(node);
        }
        if (node->type == NodeType::ExternUnion) {
            register_struct(node);
        }
        if (node->type == NodeType::EnumDecl) {
            register_enum(node);
        }
        if (node->type == NodeType::InterfaceDecl) {
            register_interface(node);
        }
        if (node->type == NodeType::TypeAlias) {
            register_type_alias(node);
        }

        /* For user-defined functions, analyze return type */
        if (node->type == NodeType::Function && node->body) {
            const ASTNode* s = node->body.get();
            while (s) {
                if (s->type == NodeType::Return && s->left) {
                    const ASTNode* e = s->left.get();
                    bool is_str = false;
                    if (e->type == NodeType::Str) is_str = true;
                    else if (e->type == NodeType::Attribute) is_str = true;
                    else if (e->type == NodeType::Call &&
                             global_string_fns().count(e->value) > 0) is_str = true;
                    else if (e->type == NodeType::BinOp && e->value == "+") {
                        if ((e->left && e->left->type == NodeType::Str) ||
                            (e->right && e->right->type == NodeType::Str))
                            is_str = true;
                    }
                    if (is_str) {
                        global_string_fns().insert(node->value);
                        break;
                    }
                }
                s = s->next.get();
            }
        }

        node = node->next.get();
    }

    /* Register built-in functions that return strings */
    for (auto& [name, info] : functions_) {
        if (info.result == AuroraType::String)
            global_string_fns().insert(name);
    }
}

void TypeChecker::walk_block(const ASTNode* node) {
    while (node) {
        walk_stmt(node);
        node = node->next.get();
    }
}

void TypeChecker::walk_stmt(const ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case NodeType::Class: {
            oop_register_class(node);
            annotate_node(node, AuroraType::Class, node->value);  /* H2 Phase D2 */
            push_scope();
            std::string saved_class = current_class_name_;
            int saved_depth = current_class_depth_;
            current_class_name_ = node->value;
            current_class_depth_ = 1;
            define_var("self", AuroraType::Class);
            walk_block(node->body.get());
            current_class_name_ = saved_class;
            current_class_depth_ = saved_depth;
            pop_scope();
            break;
        }

        case NodeType::StructDecl: {
            register_struct(node);
            break;
        }

        case NodeType::EnumDecl: {
            register_enum(node);
            break;
        }

        case NodeType::InterfaceDecl: {
            register_interface(node);
            break;
        }

        case NodeType::TypeAlias: {
            register_type_alias(node);
            break;
        }

        case NodeType::Async: {
            /* Async function: register callable name + walk body */
            if (node->body && node->body->type == NodeType::Function) {
                const ASTNode* fn = node->body.get();
                std::string fname = fn->value;
                /* Build param types from function args */
                std::vector<AuroraType> param_types;
                const ASTNode* p = fn->args.get();
                while (p) {
                    param_types.push_back(AuroraType::Unknown);
                    p = p->next.get();
                }
                /* Register: async function returns Int (task handle) */
                functions_[fname] = FunctionTypeInfo{param_types, AuroraType::Int};
                /* Walk body for type inference */
                push_scope();
                const ASTNode* param = fn->args.get();
                while (param) {
                    define_var(param->value, AuroraType::Unknown);
                    annotate_node(param, AuroraType::Unknown);  /* H2 Phase D2 */
                    param = param->next.get();
                }
                if (fn->body) walk_block(fn->body.get());
                /* H2 Phase D2: annotate inner function return type (task handle) */
                annotate_node(fn, AuroraType::Int);
                pop_scope();
            } else if (node->body) {
                /* Plain async block: walk body */
                walk_block(node->body.get());
            }
            break;
        }

        case NodeType::UnsafeBlock:
        case NodeType::SafeBlock:
        case NodeType::Parallel:
        case NodeType::Event:
        case NodeType::Component:
        case NodeType::Page:
        case NodeType::Layout:
        case NodeType::Theme:
        case NodeType::Style:
        case NodeType::Route:
        case NodeType::Render:
        case NodeType::Animate:
        case NodeType::Transition:
        case NodeType::State:
        case NodeType::Properties:
        case NodeType::Server:
        case NodeType::Api:
        case NodeType::Middleware:
        case NodeType::Database:
        case NodeType::Model:
        case NodeType::Cache:
        case NodeType::Session:
        case NodeType::Auth:
            if (node->body) walk_block(node->body.get());
            break;

        case NodeType::Request:
        case NodeType::Response:
        case NodeType::Query:
        case NodeType::Token:
        case NodeType::Input:
        case NodeType::Update:
        case NodeType::Tick:
            if (node->left) infer_expr(node->left.get());
            if (node->body) walk_block(node->body.get());
            break;

        case NodeType::Ai:
        case NodeType::Train:
        case NodeType::Predict:
        case NodeType::Tensor:
        case NodeType::Neural:
        case NodeType::Sleep:
        case NodeType::Time:
        case NodeType::Random:
            if (node->left) infer_expr(node->left.get());
            if (node->body) walk_block(node->body.get());
            break;

        case NodeType::Scene:
        case NodeType::Entity:
        case NodeType::Object:
        case NodeType::Sprite:
        case NodeType::Camera:
        case NodeType::Physics:
        case NodeType::Collision:
        case NodeType::Audio:
        case NodeType::Animation:
            if (node->body) walk_block(node->body.get());
            break;

        case NodeType::Spawn:
        case NodeType::Wait:
        case NodeType::Thread:
        case NodeType::Signal:
        case NodeType::Emit:
        case NodeType::Callback:
        case NodeType::Inline:
        case NodeType::NoInline:
        case NodeType::ConstExpr:
            if (node->left) infer_expr(node->left.get());
            break;

        case NodeType::NamespaceDecl: {
            /* namespace — push scope and walk body */
            push_scope();
            if (node->body) walk_block(node->body.get());
            pop_scope();
            break;
        }

        case NodeType::ModuleDecl:
        case NodeType::PackageDecl:
        case NodeType::AliasDecl:
            break;

        case NodeType::Assign: {
            if (!node->left || !node->right) return;

            if (node->left->type == NodeType::Attribute) {
                const std::string& obj_name   = node->left->left ? node->left->left->value : "";
                const std::string& field_name = node->left->value;
                oop_check_field_access(obj_name, field_name, node->src_line);
                AuroraType field_type = infer_expr(node->right.get());
                /* H2 Phase E-1: propagate element kind from RHS */
                annotate_node(node, field_type, "",
                    node->right ? node->right->type_annotation.element_kind : AstTypeKind::Unknown);
                break;
            }

            AuroraType value_type = infer_expr(node->right.get());

            if (node->right->type == NodeType::Call) {
                const std::string& call_name = node->right->value;
                if (global_class_registry().has(call_name)) {
                    oop_check_new_object(call_name, node->right->args.get(), node->right->src_line);
                    oop_register_object(node->left->value, call_name);
                    define_var(node->left->value, AuroraType::Class);
                    annotate_node(node, AuroraType::Class);
                    break;
                }
            }

            /* H2 Phase E-1: store element kind alongside variable type */
            AstTypeKind rhs_elem = node->right
                ? node->right->type_annotation.element_kind
                : AstTypeKind::Unknown;
            define_var_elem(node->left->value, value_type, rhs_elem);
            annotate_node(node, value_type, "", rhs_elem);
            break;
        }

        case NodeType::IndexAssign: {
            AuroraType arr_type = lookup_var(node->value, node->src_line);
            if (arr_type != AuroraType::Array && arr_type != AuroraType::Unknown) {
                std::ostringstream msg;
                msg << "index assignment needs an array";
                throw TypeError(msg.str(), node->src_line);
            }
            AuroraType idx_type = infer_expr(node->left.get());
            if (idx_type != AuroraType::Int && idx_type != AuroraType::Unknown) {
                std::ostringstream msg;
                msg << "array index must be int";
                throw TypeError(msg.str(), node->src_line);
            }
            infer_expr(node->right.get());
            break;
        }

        case NodeType::Output:
        case NodeType::Return:
        case NodeType::Throw: {
            AuroraType value_type = node->left ? infer_expr(node->left.get()) : AuroraType::Void;
            if (node->type == NodeType::Return && inside_function_) {
                if (current_return_type_ == AuroraType::Unknown) {
                    current_return_type_ = value_type;
                    auto it = functions_.find("");
                    (void)it;
                } else {
                    expect_assignable(current_return_type_, value_type, node->src_line);
                }
            }
            /* H2 Phase E-1: propagate element kind from value expression */
            annotate_node(node, value_type, "",
                node->left ? node->left->type_annotation.element_kind : AstTypeKind::Unknown);
            break;
        }

        case NodeType::If: {
            AuroraType cond = infer_expr(node->left.get());
            if (!is_boolish(cond)) {
                std::ostringstream msg;
                msg << "if condition must be numeric or bool";
                throw TypeError(msg.str(), node->src_line);
            }
            push_scope();
            walk_block(node->body.get());
            pop_scope();
            if (node->orelse) walk_stmt(node->orelse.get());
            break;
        }

        case NodeType::Else:
            push_scope();
            walk_block(node->body.get());
            pop_scope();
            break;

        case NodeType::While: {
            AuroraType cond = infer_expr(node->left.get());
            if (!is_boolish(cond)) {
                std::ostringstream msg;
                msg << "while condition must be numeric or bool";
                throw TypeError(msg.str(), node->src_line);
            }
            walk_block(node->body.get());
            break;
        }

        case NodeType::Loop: {
            walk_block(node->body.get());
            break;
        }

        case NodeType::For: {
            AuroraType count_type = infer_expr(node->left.get());
            if (count_type != AuroraType::Int && count_type != AuroraType::Unknown
                && count_type != AuroraType::Array) {
                std::ostringstream msg;
                msg << "for count must be int or array";
                throw TypeError(msg.str(), node->src_line);
            }
            push_scope();
            define_var(node->value, AuroraType::Int);
            /* H2 Phase E-1: propagate element kind from range expression */
            annotate_node(node, AuroraType::Int, "",
                node->left ? node->left->type_annotation.element_kind : AstTypeKind::Unknown);
            walk_block(node->body.get());
            pop_scope();
            break;
        }

        case NodeType::Match: {
            AuroraType match_type = infer_expr(node->left.get());
            (void)match_type; /* Allow any type for pattern matching */
            const ASTNode* case_ptr = node->args.get();
            while (case_ptr) {
                if (case_ptr->value != "default" && case_ptr->args) {
                    push_scope();
                    define_match_pattern_vars(case_ptr->args.get());
                    walk_block(case_ptr->body.get());
                    pop_scope();
                } else {
                    walk_block(case_ptr->body.get());
                }
                case_ptr = case_ptr->next.get();
            }
            break;
        }

        case NodeType::Repeat: {
            walk_block(node->body.get());
            AuroraType cond = infer_expr(node->left.get());
            if (!is_boolish(cond)) {
                std::ostringstream msg;
                msg << "until condition must be numeric or bool";
                throw TypeError(msg.str(), node->src_line);
            }
            break;
        }

        case NodeType::Function:
        case NodeType::Lambda: {
            bool saved_inside = inside_function_;
            AuroraType saved_return = current_return_type_;

            /* Register named lambda as a variable in the enclosing scope before pushing the function's scope */
            if (node->type == NodeType::Lambda && !node->value.empty()) {
                bool starts_with_underscore = node->value.rfind("__lambda_", 0) == 0;
                if (!starts_with_underscore) {
                    define_var(node->value, AuroraType::Unknown);
                }
            }

            inside_function_ = true;
            current_return_type_ = AuroraType::Unknown;
            push_scope();

            const ASTNode* param = node->args.get();
            while (param) {
                define_var(param->value, AuroraType::Unknown);
                annotate_node(param, AuroraType::Unknown);  /* H2 Phase D */
                param = param->next.get();
            }

            bool is_lambda = (node->type == NodeType::Lambda);
            if (is_lambda) {
                tracking_var_refs_ = true;
                var_refs_.clear();
            }

            walk_block(node->body.get());

            if (is_lambda) {
                tracking_var_refs_ = false;
                for (const auto& ref : var_refs_) {
                    bool is_param = false;
                    const ASTNode* p = node->args.get();
                    while (p) {
                        if (p->value == ref) { is_param = true; break; }
                        p = p->next.get();
                    }
                    if (!is_param) {
                        for (int i = (int)scopes_.size() - 2; i >= 0; --i) {
                            auto it = scopes_[i].find(ref);
                            if (it != scopes_[i].end()) {
                                const_cast<ASTNode*>(node)->captures.push_back(ref);
                                break;
                            }
                        }
                    }
                }
            }

            auto& fn = functions_[node->value];
            /* Preserve generic info if set during registration */
            bool saved_is_generic = fn.is_generic;
            auto saved_generic_params = fn.generic_params;
            const ASTNode* saved_generic_ast = fn.generic_ast_node;
            fn.params.clear();
            {
                const ASTNode* pp = node->args.get();
                while (pp) {
                    fn.params.push_back(AuroraType::Unknown);
                    pp = pp->next.get();
                }
            }
            fn.result = current_return_type_ == AuroraType::Unknown
                ? AuroraType::Int
                : current_return_type_;
            fn.is_generic = saved_is_generic;
            fn.generic_params = std::move(saved_generic_params);
            fn.generic_ast_node = saved_generic_ast;
            annotate_node(node, fn.result);  /* H2 Phase D */

            pop_scope();
            current_return_type_ = saved_return;
            inside_function_ = saved_inside;
            break;
        }

        case NodeType::PerformanceFn: {
            bool saved_inside = inside_function_;
            AuroraType saved_return = current_return_type_;

            inside_function_ = true;
            current_return_type_ = AuroraType::Unknown;
            push_scope();

            const ASTNode* param = node->args.get();
            while (param) {
                define_var(param->value, AuroraType::Unknown);
                annotate_node(param, AuroraType::Unknown);  /* H2 Phase D */
                param = param->next.get();
            }

            walk_block(node->body.get());
            auto& fn = functions_[node->value];
            /* Preserve generic info if set during registration */
            bool saved_is_generic = fn.is_generic;
            auto saved_generic_params = fn.generic_params;
            const ASTNode* saved_generic_ast = fn.generic_ast_node;
            fn.result = current_return_type_ == AuroraType::Unknown
                ? AuroraType::Int
                : current_return_type_;
            fn.is_generic = saved_is_generic;
            fn.generic_params = std::move(saved_generic_params);
            fn.generic_ast_node = saved_generic_ast;
            annotate_node(node, fn.result);  /* H2 Phase D */

            pop_scope();
            current_return_type_ = saved_return;
            inside_function_ = saved_inside;
            break;
        }

        case NodeType::Call:
            infer_call(node);
            break;

        case NodeType::Delete:
        case NodeType::Move:
        case NodeType::Drop:
        case NodeType::SharedRef:
        case NodeType::WeakRef:
        case NodeType::Borrow:
        case NodeType::Copy:
        case NodeType::Free:
        case NodeType::Reference:
        case NodeType::Pointer:
            if (!node->value.empty()) {
                /* H2 Phase E-1: propagate stored element kind for compound types */
                auto var_type = lookup_var(node->value, node->src_line);
                auto var_elem = lookup_var_elem(node->value);
                annotate_node(node, var_type, "", var_elem);  /* H2 Phase D2 */
            } else if (node->left) {
                auto expr_type = infer_expr(node->left.get());
                /* H2 Phase E-1: propagate element kind from expression */
                annotate_node(node, expr_type, "",
                    node->left->type_annotation.element_kind);
            }
            break;

        case NodeType::New: {
            /* new ClassName(args) */
            if (node->args) {
                const ASTNode* arg = node->args.get();
                while (arg) {
                    infer_expr(arg);
                    arg = arg->next.get();
                }
            }
            break;
        }

        case NodeType::Import:
        case NodeType::Break:
        case NodeType::Continue:
        case NodeType::Skip:
        case NodeType::Pass:
        case NodeType::Yield:
        case NodeType::ExternFn:
        case NodeType::ExternStruct:
        case NodeType::ExternUnion:
        case NodeType::FunctionType:
            break;

        default:
            infer_expr(node);
            break;
    }
}

AuroraType TypeChecker::infer_expr(const ASTNode* node) {
    if (!node) return AuroraType::Void;

    switch (node->type) {
        case NodeType::Num:   return annotate_ret(node, AuroraType::Int);
        case NodeType::Float: return annotate_ret(node, AuroraType::Float);
        case NodeType::Str:   return annotate_ret(node, AuroraType::String);
        case NodeType::Array: {
            /* Tuple if value == "__tuple__" */
            if (node->value == "__tuple__") {
                AstTypeKind elem_kind = AstTypeKind::Unknown;
                const ASTNode* elem = node->args.get();
                if (elem) {
                    AuroraType first = infer_expr(elem);
                    elem_kind = to_ast_type_kind(first);
                    elem = elem->next.get();
                }
                /* Check homogeneity — if any element differs, reset to Unknown */
                while (elem) {
                    AuroraType et = infer_expr(elem);
                    if (to_ast_type_kind(et) != elem_kind)
                        elem_kind = AstTypeKind::Unknown;
                    elem = elem->next.get();
                }
                return annotate_ret(node, AuroraType::Tuple, "", elem_kind);
            }
            /* H2 Phase E-1: determine element type from first element */
            AstTypeKind elem_kind = AstTypeKind::Unknown;
            const ASTNode* elem = node->args.get();
            if (elem) {
                AuroraType first = infer_expr(elem);
                elem_kind = to_ast_type_kind(first);
                elem = elem->next.get();
            }
            while (elem) {
                infer_expr(elem);
                elem = elem->next.get();
            }
            return annotate_ret(node, AuroraType::Array, "", elem_kind);
        }
        case NodeType::Var: {
            if (tracking_var_refs_)
                var_refs_.insert(node->value);
            try {
                auto r = lookup_var(node->value, node->src_line);
                /* H2 Phase E-1: propagate stored element kind for compound types */
                auto elem = lookup_var_elem(node->value);
                return annotate_ret(node, r, "", elem);
            } catch (const TypeError&) {
                if (functions_.count(node->value))
                    return annotate_ret(node, AuroraType::Unknown);
                throw;
            }
        }
        case NodeType::Index: {
            AuroraType arr_type = lookup_var(node->value, node->src_line);
            if (arr_type != AuroraType::Array && arr_type != AuroraType::Tuple && arr_type != AuroraType::Unknown) {
                std::ostringstream msg;
                msg << "only arrays or tuples can be indexed";
                throw TypeError(msg.str(), node->src_line);
            }
            AuroraType idx_type = infer_expr(node->left.get());
            if (idx_type != AuroraType::Int && idx_type != AuroraType::Unknown) {
                std::ostringstream msg;
                msg << "array index must be int";
                throw TypeError(msg.str(), node->src_line);
            }
            /* Use element type from array variable annotation */
            AstTypeKind elem_kind = lookup_var_elem(node->value);
            if (elem_kind != AstTypeKind::Unknown)
                return annotate_ret(node, ast_kind_to_type(elem_kind), "", elem_kind);
            return annotate_ret(node, AuroraType::Unknown);
        }
        case NodeType::BinOp:
            return infer_binop(node);
        case NodeType::UnaryOp:
            return infer_unary(node);
        case NodeType::Call:
            return infer_call(node);
        case NodeType::Attribute: {
            const std::string& obj_name   = node->left ? node->left->value : "";
            const std::string& field_name = node->value;
            oop_check_field_access(obj_name, field_name, node->src_line);
            /* Look up field type from class registry */
            if (!obj_name.empty()) {
                std::string class_name = oop_class_of_var(obj_name);
                if (!class_name.empty()) {
                    const ClassFieldInfo* field = global_class_registry().find_field(class_name, field_name);
                    if (field && field->type_kind != AstTypeKind::Unknown)
                        return annotate_ret(node, ast_kind_to_type(field->type_kind), "",
                            field->element_kind);
                }
            }
            return annotate_ret(node, AuroraType::Unknown);
        }
        case NodeType::New: {
            /* new ClassName(args) — handle as expression */
            if (node->args) {
                const ASTNode* arg = node->args.get();
                while (arg) {
                    infer_expr(arg);
                    arg = arg->next.get();
                }
            }
            return annotate_ret(node, AuroraType::Class, node->value);
        }
        case NodeType::StructLiteral: {
            if (!global_type_registry().has_struct(node->value))
                throw TypeError("unknown struct '" + node->value + "'", node->src_line);
            const ASTNode* f = node->args.get();
            while (f) {
                if (f->left) infer_expr(f->left.get());
                f = f->next.get();
            }
            return annotate_ret(node, AuroraType::Struct, node->value);
        }
        case NodeType::Move:
        case NodeType::SharedRef:
        case NodeType::WeakRef:
        case NodeType::Borrow:
        case NodeType::Copy:
        case NodeType::Free:
        case NodeType::Reference:
        case NodeType::Pointer:
            if (!node->value.empty()) {
                auto r = lookup_var(node->value, node->src_line);
                /* H2 Phase E-1: propagate stored element kind for compound types */
                auto elem = lookup_var_elem(node->value);
                return annotate_ret(node, r, "", elem);
            }
            return annotate_ret(node, AuroraType::Unknown);
        default:
            return annotate_ret(node, AuroraType::Unknown);
    }
}

AuroraType TypeChecker::infer_binop(const ASTNode* node) {
    AuroraType left = infer_expr(node->left.get());
    AuroraType right = infer_expr(node->right.get());
    const std::string& op = node->value;

    if (op == "+") {
        if (left == AuroraType::String || right == AuroraType::String) {
            if (left == AuroraType::Unknown || right == AuroraType::Unknown)
                return annotate_ret(node, AuroraType::String);
            if (left == AuroraType::String && right == AuroraType::String)
                return annotate_ret(node, AuroraType::String);
            return annotate_ret(node, AuroraType::String);
        }
        return annotate_ret(node, common_numeric(left, right, node->src_line));
    }

    if (op == "-" || op == "*" || op == "/" || op == "%" || op == "**" || op == "//")
        return annotate_ret(node, common_numeric(left, right, node->src_line));

    if (op == "^" || op == "xor") {
        if ((left == AuroraType::Int || left == AuroraType::Bool || left == AuroraType::Unknown) &&
            (right == AuroraType::Int || right == AuroraType::Bool || right == AuroraType::Unknown))
            return annotate_ret(node, AuroraType::Int);
        std::ostringstream msg;
        msg << "xor needs int or bool values";
        throw TypeError(msg.str(), node->src_line);
    }

    if (op == "and" || op == "or" || op == "&&" || op == "||") {
        if (is_boolish(left) && is_boolish(right)) return annotate_ret(node, AuroraType::Bool);
        std::ostringstream msg;
        msg << "'" << op << "' needs numeric or bool values";
        throw TypeError(msg.str(), node->src_line);
    }

    if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
        /* Allow pointer/cstring == 0 / null comparison (check before string check) */
        if ((left == AuroraType::Pointer && right == AuroraType::Int) ||
            (left == AuroraType::Int && right == AuroraType::Pointer) ||
            (left == AuroraType::String && right == AuroraType::Int) ||
            (left == AuroraType::Int && right == AuroraType::String)) {
            if (op == "==" || op == "!=")
                return annotate_ret(node, AuroraType::Bool);
            if (left == AuroraType::String || right == AuroraType::String) {
                std::ostringstream msg;
                msg << "strings only support == and !=";
                throw TypeError(msg.str(), node->src_line);
            }
        }
        if (left == AuroraType::String || right == AuroraType::String) {
            if ((op == "==" || op == "!=") && (left == right || left == AuroraType::Unknown || right == AuroraType::Unknown))
                return annotate_ret(node, AuroraType::Bool);
            std::ostringstream msg;
            msg << "strings only support == and !=";
            throw TypeError(msg.str(), node->src_line);
        }
        common_numeric(left, right, node->src_line);
        return annotate_ret(node, AuroraType::Bool);
    }

    if (op == "&" || op == "|" || op == "<<" || op == ">>") {
        if ((left == AuroraType::Int || left == AuroraType::Unknown) &&
            (right == AuroraType::Int || right == AuroraType::Unknown))
            return annotate_ret(node, AuroraType::Int);
        std::ostringstream msg;
        msg << "'" << op << "' needs int values";
        throw TypeError(msg.str(), node->src_line);
    }

    if (op == "in") {
        if (right == AuroraType::Array || right == AuroraType::Unknown)
            return annotate_ret(node, AuroraType::Bool);
        std::ostringstream msg;
        msg << "'in' needs an array on the right";
        throw TypeError(msg.str(), node->src_line);
    }

    std::ostringstream msg;
    msg << "unsupported operator '" << op << "'";
    throw TypeError(msg.str(), node->src_line);
}

AuroraType TypeChecker::infer_unary(const ASTNode* node) {
    AuroraType value = infer_expr(node->left.get());
    if (node->value == "-") {
        if (is_numeric(value)) return annotate_ret(node, value);
        std::ostringstream msg;
        msg << "unary '-' needs a number";
        throw TypeError(msg.str(), node->src_line);
    }
    if (node->value == "not") {
        if (is_boolish(value)) return annotate_ret(node, AuroraType::Bool);
        std::ostringstream msg;
        msg << "'not' needs numeric or bool value";
        throw TypeError(msg.str(), node->src_line);
    }
    std::ostringstream msg;
    msg << "unsupported unary operator '" << node->value << "'";
    throw TypeError(msg.str(), node->src_line);
}

AuroraType TypeChecker::infer_call(const ASTNode* node) {
    /* ── Generic function call: foo[Int, Float](x, y) ── */
    if (node->template_args) {
        auto it = functions_.find(node->value);
        if (it == functions_.end()) {
            std::ostringstream msg;
            msg << "unknown generic function '" << node->value << "'";
            throw TypeError(msg.str(), node->src_line);
        }
        if (!it->second.is_generic) {
            std::ostringstream msg;
            msg << "function '" << node->value << "' is not generic";
            throw TypeError(msg.str(), node->src_line);
        }
        /* Build mangled name: foo__Int__Float for foo[Int, Float] */
        std::string mangled = node->value;
        const ASTNode* ta = node->template_args.get();
        while (ta) {
            mangled += "__" + ta->value;
            ta = ta->next.get();
        }
        /* Instantiate concrete version if not already */
        if (functions_.count(mangled) == 0) {
            FunctionTypeInfo concrete = it->second;
            concrete.is_generic = false;
            concrete.generic_params.clear();
            concrete.generic_ast_node = nullptr;
            functions_[mangled] = concrete;
        }
        /* Type-check args */
        size_t count = 0;
        const ASTNode* arg = node->args.get();
        while (arg) {
            infer_expr(arg);
            count++;
            arg = arg->next.get();
        }
        return annotate_ret(node, functions_[mangled].result);
    }

    if (node->value == "output") {
        const ASTNode* arg = node->args.get();
        while (arg) {
            infer_expr(arg);
            arg = arg->next.get();
        }
        return annotate_ret(node, AuroraType::Void);
    }

    if (node->value == "array") {
        /* H2 Phase E-1: determine element type from first argument */
        AstTypeKind elem_kind = AstTypeKind::Unknown;
        const ASTNode* arg = node->args.get();
        if (arg) {
            AuroraType first = infer_expr(arg);
            elem_kind = to_ast_type_kind(first);
            arg = arg->next.get();
        }
        while (arg) { infer_expr(arg); arg = arg->next.get(); }
        return annotate_ret(node, AuroraType::Array, "", elem_kind);
    }

    if (node->value == "tuple") {
        /* H2 Phase E-1: determine element type from first argument (if homogeneous) */
        AstTypeKind elem_kind = AstTypeKind::Unknown;
        const ASTNode* arg = node->args.get();
        if (arg) {
            AuroraType first = infer_expr(arg);
            elem_kind = to_ast_type_kind(first);
            arg = arg->next.get();
            while (arg) {
                AuroraType et = infer_expr(arg);
                if (to_ast_type_kind(et) != elem_kind)
                    elem_kind = AstTypeKind::Unknown;
                arg = arg->next.get();
            }
        }
        return annotate_ret(node, AuroraType::Tuple, "", elem_kind);
    }

    /* ── Phase 2: collection constructors ── */
    if (node->value == "list" || node->value == "map" || node->value == "set" ||
        node->value == "vector" || node->value == "stack" || node->value == "queue" ||
        node->value == "json") {
        /* H2 Phase E-1: determine element type from first argument (if any) */
        AstTypeKind elem_kind = AstTypeKind::Unknown;
        const ASTNode* arg = node->args.get();
        if (arg && node->value != "map" && node->value != "json") {
            /* map/json are heterogeneous by nature — leave element_kind Unknown */
            AuroraType first = infer_expr(arg);
            elem_kind = to_ast_type_kind(first);
            arg = arg->next.get();
        }
        while (arg) { infer_expr(arg); arg = arg->next.get(); }
        if (node->value == "list")   return annotate_ret(node, AuroraType::List, "", elem_kind);
        if (node->value == "map")    return annotate_ret(node, AuroraType::Map);
        if (node->value == "set")    return annotate_ret(node, AuroraType::Set, "", elem_kind);
        if (node->value == "vector") return annotate_ret(node, AuroraType::Vector, "", elem_kind);
        if (node->value == "stack")  return annotate_ret(node, AuroraType::Stack, "", elem_kind);
        if (node->value == "queue")  return annotate_ret(node, AuroraType::Queue, "", elem_kind);
        if (node->value == "json")   return annotate_ret(node, AuroraType::Json);
    }

    /* ── typeof / sizeof — built-in inspection ── */
    if (node->value == "typeof" || node->value == "sizeof") {
        if (node->args) infer_expr(node->args.get());
        if (node->value == "typeof") return annotate_ret(node, AuroraType::String);
        return annotate_ret(node, AuroraType::Int);
    }

    /* ── panic / debug / log — built-in calls ── */
    if (node->value == "panic" || node->value == "debug" || node->value == "log") {
        if (node->args) infer_expr(node->args.get());
        return annotate_ret(node, AuroraType::Void);
    }

    {
        auto dot = node->value.find('.');
        if (dot != std::string::npos) {
            std::string obj_name    = node->value.substr(0, dot);
            std::string method_name = node->value.substr(dot + 1);
            oop_check_method_call(obj_name, method_name, node->src_line);
            const ASTNode* arg = node->args.get();
            while (arg) { infer_expr(arg); arg = arg->next.get(); }
            /* Look up method return type from class registry */
            std::string class_name = oop_class_of_var(obj_name);
            if (!class_name.empty()) {
                const ClassMethodInfo* method = global_class_registry().find_method(class_name, method_name);
                if (method && method->return_kind != AstTypeKind::Unknown)
                    return annotate_ret(node, ast_kind_to_type(method->return_kind));
            }
            return annotate_ret(node, AuroraType::Unknown);
        }
    }

    if (global_class_registry().has(node->value)) {
        oop_check_new_object(node->value, node->args.get(), node->src_line);
        return annotate_ret(node, AuroraType::Class, node->value);
    }

    /* Check if the call name is a known variable (function pointer / lambda) */
    if (has_type(node->value)) {
        const ASTNode* arg = node->args.get();
        while (arg) { infer_expr(arg); arg = arg->next.get(); }
        return annotate_ret(node, AuroraType::Unknown);
    }

    auto it = functions_.find(node->value);
    if (it == functions_.end()) {
        std::ostringstream msg;
        msg << "unknown function '" << node->value << "'";
        throw TypeError(msg.str(), node->src_line);
    }

    size_t count = 0;
    const ASTNode* arg = node->args.get();
    while (arg) {
        infer_expr(arg);
        count++;
        arg = arg->next.get();
    }

    if (node->value == "range" && (count == 1 || count == 2)) {
        return annotate_ret(node, it->second.result);
    }
    if (node->value == "outputf" && count >= 1) {
        return annotate_ret(node, it->second.result);
    }
    if ((node->value == "min" || node->value == "max") && count <= 2) {
        return annotate_ret(node, AuroraType::Unknown);
    }
    if (node->value == "replace" && count == 3) {
        return annotate_ret(node, AuroraType::String);
    }
    if (node->value == "split" && count == 2) {
        return annotate_ret(node, AuroraType::Array, "", AstTypeKind::String);
    }
    if (node->value == "has" && count == 2) {
        return annotate_ret(node, AuroraType::Bool);
    }
    if (node->value == "starts" && count == 2) {
        return annotate_ret(node, AuroraType::Bool);
    }
    if (node->value == "ends" && count == 2) {
        return annotate_ret(node, AuroraType::Bool);
    }

    /* H2 Phase E-1: builtins that return compound types with known element kinds */
    if (node->value == "fields" && count == 1) {
        return annotate_ret(node, AuroraType::Array, "", AstTypeKind::String);
    }
    if (node->value == "methods" && count == 1) {
        return annotate_ret(node, AuroraType::Array, "", AstTypeKind::String);
    }
    if ((node->value == "unique" || node->value == "filter") && count >= 1) {
        /* Element kind matches the input array's element kind */
        AstTypeKind input_elem = node->args
            ? node->args->type_annotation.element_kind
            : AstTypeKind::Unknown;
        return annotate_ret(node, AuroraType::Array, "", input_elem);
    }

    if (count < it->second.params.size() ||
        (!it->second.is_vararg && count != it->second.params.size())) {
        std::ostringstream msg;
        msg << "function '" << node->value
            << "' expects at least " << it->second.params.size() << " argument(s), got " << count;
        throw TypeError(msg.str(), node->src_line);
    }

    /* H2 Phase E-1: generic compound-return builtins propagate element kind from first arg */
    if (it->second.result == AuroraType::Array && count >= 1) {
        AstTypeKind input_elem = node->args
            ? node->args->type_annotation.element_kind
            : AstTypeKind::Unknown;
        return annotate_ret(node, AuroraType::Array, "", input_elem);
    }

    return annotate_ret(node, it->second.result);
}

bool TypeChecker::is_numeric(AuroraType type) const {
    return type == AuroraType::Int ||
           type == AuroraType::Float ||
           type == AuroraType::Bool ||
           type == AuroraType::Unknown;
}

bool TypeChecker::is_boolish(AuroraType type) const {
    return type == AuroraType::Int ||
           type == AuroraType::Bool ||
           type == AuroraType::Unknown;
}

AuroraType TypeChecker::common_numeric(AuroraType left, AuroraType right, int line) const {
    if (!is_numeric(left) || !is_numeric(right)) {
        std::ostringstream msg;
        msg << "numeric operation got "
            << aurora_type_name(left) << " and " << aurora_type_name(right);
        throw TypeError(msg.str(), line);
    }
    if (left == AuroraType::Float || right == AuroraType::Float) return AuroraType::Float;
    if (left == AuroraType::Unknown || right == AuroraType::Unknown) return AuroraType::Unknown;
    return AuroraType::Int;
}

void TypeChecker::expect_assignable(AuroraType target, AuroraType value, int line) const {
    if (target == AuroraType::Unknown || value == AuroraType::Unknown) {
        /* Log diagnostic when Unknown propagates through a type check */
        if (target != AuroraType::Unknown && value == AuroraType::Unknown) {
            std::cerr << "[typecheck] warning at line " << line
                      << ": expected '" << aurora_type_name(target)
                      << "' but value type is unknown\n";
        }
        return;
    }
    if (target == value) return;
    if (target == AuroraType::Float && value == AuroraType::Int) return;

    std::ostringstream msg;
    msg << "expected " << aurora_type_name(target)
        << ", got " << aurora_type_name(value);
    throw TypeError(msg.str(), line);
}
