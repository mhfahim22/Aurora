#include <clang-c/Index.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

/* ════════════════════════════════════════════════════════════
   aurora-cppwrap — C++ header → Aurora binding + C thunk generator
   ════════════════════════════════════════════════════════════
   Parses C++ headers using libclang and generates:
     1. A C wrapper (.h + .cpp) with extern "C" thunks
     2. An Aurora .au FFI binding file
   ════════════════════════════════════════════════════════════ */

/* ── Config ──────────────────────────────────────────────── */
static std::string g_output_dir = ".";
static std::string g_lib_name = "cppwrap";
static std::string g_module_name = "cpplib";
static bool g_verbose = false;
static bool g_no_cpp = false;
static bool g_no_au = false;
static bool g_with_exceptions = false;

static std::ofstream g_hdr_os;
static std::ofstream g_src_os;
static std::ofstream g_au_os;
static std::vector<std::string> g_header_paths;
static std::vector<const char*> g_clang_args;
static std::vector<std::string> g_clang_args_storage;

/* Track wrapped classes to avoid duplicates */
static std::set<std::string> g_wrapped_classes;
static std::map<std::string, std::vector<std::string>> g_class_methods;
static std::map<std::string, std::vector<std::string>> g_class_fields;
static std::map<std::string, CXCursor> g_class_cursors;
static std::set<std::string> g_tc_classes; /* trivially copyable classes */

/* ── Helpers ─────────────────────────────────────────────── */
static std::string cxstr(CXString s) {
    std::string r = clang_getCString(s);
    clang_disposeString(s);
    return r;
}

static std::string sanitize(const std::string& name) {
    std::string r = name;
    for (auto& c : r) {
        if (!isalnum((unsigned char)c) && c != '_')
            c = '_';
    }
    return r;
}

static std::string to_upper(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)toupper((unsigned char)c);
    return r;
}

static std::string cpp_type_to_c_type(CXType ct) {
    ct = clang_getUnqualifiedType(ct);
    CXTypeKind k = ct.kind;
    switch (k) {
        case CXType_Void:       return "void";
        case CXType_Bool:       return "int8_t";
        case CXType_Char_S:
        case CXType_SChar:      return "int8_t";
        case CXType_Char_U:
        case CXType_UChar:      return "uint8_t";
        case CXType_Short:      return "int16_t";
        case CXType_UShort:     return "uint16_t";
        case CXType_Int:        return "int32_t";
        case CXType_UInt:       return "uint32_t";
        case CXType_Long:
        case CXType_LongLong:   return "int64_t";
        case CXType_ULong:
        case CXType_ULongLong:  return "uint64_t";
        case CXType_Float:      return "float";
        case CXType_Double:     return "double";
        case CXType_Pointer: {
            CXType pointee = clang_getPointeeType(ct);
            if (pointee.kind == CXType_Char_S || pointee.kind == CXType_SChar)
                return "const char*";
            if (pointee.kind == CXType_Void)
                return "void*";
            return "void*";
        }
        case CXType_Record: {
            std::string n = cxstr(clang_getTypeSpelling(ct));
            if (n.find("class ") == 0 || n.find("struct ") == 0) {
                n = n.substr(n.find(' ') + 1);
            }
            /* Template types: keep but strip to base */
            auto tpl = n.find('<');
            if (tpl != std::string::npos) n = n.substr(0, tpl);
            return sanitize(n);
        }
        case CXType_LValueReference:
        case CXType_RValueReference:
            return "void*";
        default: {
            std::string n = cxstr(clang_getTypeSpelling(ct));
            if (n == "size_t") return "uint64_t";
            if (n == "std::string") return "void*";
            if (n.find("std::") == 0) return "void*";
            return sanitize(n);
        }
    }
}

/* ── Check if a type is a trivially copyable user class ── */
static bool is_tc_user_class(CXType ct, const std::set<std::string>& wrapped) {
    CXTypeKind k = ct.kind;
    if (k == CXType_Record) {
        std::string n = cxstr(clang_getTypeSpelling(ct));
        if (n.find("class ") == 0 || n.find("struct ") == 0)
            n = n.substr(n.find(' ') + 1);
        auto tpl = n.find('<');
        if (tpl != std::string::npos) n = n.substr(0, tpl);
        return wrapped.count(n) > 0;
    }
    return false;
}

static std::string cpp_type_to_au_type(CXType ct, const std::set<std::string>& tc_classes = {}) {
    ct = clang_getUnqualifiedType(ct);
    CXTypeKind k = ct.kind;
    switch (k) {
        case CXType_Void:       return "void";
        case CXType_Bool:       return "bool";
        case CXType_Char_S:
        case CXType_SChar:      return "i8";
        case CXType_Char_U:
        case CXType_UChar:      return "u8";
        case CXType_Short:      return "i16";
        case CXType_UShort:     return "u16";
        case CXType_Int:        return "i32";
        case CXType_UInt:       return "u32";
        case CXType_Long:
        case CXType_LongLong:   return "i64";
        case CXType_ULong:
        case CXType_ULongLong:  return "u64";
        case CXType_Float:      return "f32";
        case CXType_Double:     return "f64";
        case CXType_Pointer: {
            CXType pointee = clang_getPointeeType(ct);
            if (pointee.kind == CXType_Char_S || pointee.kind == CXType_SChar)
                return "cstring";
            return "pointer";
        }
        case CXType_Record: {
            std::string n = cxstr(clang_getTypeSpelling(ct));
            if (n.find("class ") == 0 || n.find("struct ") == 0)
                n = n.substr(n.find(' ') + 1);
            auto tpl = n.find('<');
            if (tpl != std::string::npos) n = n.substr(0, tpl);
            /* If the class is trivially copyable, pass by value (struct) */
            if (tc_classes.count(n))
                return n; /* struct type name */
            return "pointer"; /* class instances → opaque pointer */
        }
        case CXType_LValueReference: {
            /* Check if the referent type is a trivially copyable class */
            CXType pointee = clang_getPointeeType(ct);
            if (is_tc_user_class(pointee, tc_classes)) {
                std::string n = cxstr(clang_getTypeSpelling(pointee));
                if (n.find("class ") == 0 || n.find("struct ") == 0)
                    n = n.substr(n.find(' ') + 1);
                auto tpl = n.find('<');
                if (tpl != std::string::npos) n = n.substr(0, tpl);
                return n; /* pass trivially copyable by value */
            }
            return "pointer";
        }
        case CXType_RValueReference:
            return "pointer";
        default: {
            /* Handle Typedef (e.g., int64_t → i64, size_t → u64) */
            if (k == CXType_Typedef) {
                CXType underlying = clang_getTypedefDeclUnderlyingType(
                    clang_getTypeDeclaration(ct));
                return cpp_type_to_au_type(underlying, tc_classes);
            }
            std::string n = cxstr(clang_getTypeSpelling(ct));
            if (n == "size_t" || n == "std::size_t") return "u64";
            if (n == "int64_t")  return "i64";
            if (n == "uint64_t") return "u64";
            if (n == "int32_t")  return "i32";
            if (n == "uint32_t") return "u32";
            if (n == "int16_t")  return "i16";
            if (n == "uint16_t") return "u16";
            if (n == "int8_t")   return "i8";
            if (n == "uint8_t")  return "u8";
            if (n == "std::string") return "pointer";
            return "pointer";
        }
    }
}

static bool is_trivially_copyable(CXCursor cursor) {
    /* Check if class has virtual methods, user destructor, or user constructors */
    bool flags[3] = {false, false, false};

    clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) {
        auto* f = (bool*)d;
        CXCursorKind k = clang_getCursorKind(c);
        if (k == CXCursor_CXXMethod) {
            if (clang_CXXMethod_isVirtual(c))
                f[0] = true;
        }
        if (k == CXCursor_Destructor) {
            f[1] = true;
        }
        if (k == CXCursor_Constructor) {
            f[2] = true;
        }
        return CXChildVisit_Continue;
    }, flags);

    return !flags[0] && !flags[1] && !flags[2];
}

/* ── Generate C wrapper header ───────────────────────────── */
static void gen_c_header_begin() {
    std::string guard = "CPPWRAP_" + to_upper(g_module_name) + "_H";
    g_hdr_os << "#ifndef " << guard << "\n";
    g_hdr_os << "#define " << guard << "\n\n";
    g_hdr_os << "#include <stdint.h>\n";
    g_hdr_os << "#include <stddef.h>\n\n";
    g_hdr_os << "#ifdef __cplusplus\n";
    g_hdr_os << "extern \"C\" {\n";
    g_hdr_os << "#endif\n\n";
}

static void gen_c_header_end() {
    g_hdr_os << "#ifdef __cplusplus\n";
    g_hdr_os << "}\n";
    g_hdr_os << "#endif\n\n";
    g_hdr_os << "#endif /* guard */\n";
}

/* ── Generate Aurora binding file ────────────────────────── */
static void gen_au_begin() {
    g_au_os << "/* ════════════════════════════════════════════════════════════\n";
    g_au_os << "   Auto-generated Aurora FFI bindings from C++ library: "
            << g_lib_name << "\n";
    g_au_os << "   Module: " << g_module_name << "\n";
    g_au_os << "   Generated by aurora-cppwrap\n";
    g_au_os << "   ════════════════════════════════════════════════════════════ */\n\n";
    g_au_os << "extern \"" << g_lib_name << "\"\n\n";
}

/* ── Wrap a single C++ class ─────────────────────────────── */
static void wrap_class(CXCursor cursor, const std::string& class_name) {
    if (g_wrapped_classes.count(class_name)) return;
    g_wrapped_classes.insert(class_name);

    std::string sn = sanitize(class_name);
    if (g_verbose) std::cerr << "[cppwrap] wrapping class: " << class_name << "\n";

    /* Determine if trivially copyable (zero-cost pass-through) */
    bool tc = is_trivially_copyable(cursor);
    if (tc) g_tc_classes.insert(class_name);

    /* Header: typedef for opaque handle */
    g_hdr_os << "/* ── " << class_name << " ── */\n";
    if (tc) {
        g_hdr_os << "/* ZERO-COST: trivially copyable — passed by value */\n";
        g_hdr_os << "typedef struct " << sn << " {\n";
        /* Emit fields */
        clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) {
            if (clang_getCursorKind(c) == CXCursor_FieldDecl) {
                auto& os = *(std::ostream*)d;
                CXType ft = clang_getCursorType(c);
                std::string fn = cxstr(clang_getCursorSpelling(c));
                os << "    " << cpp_type_to_c_type(ft) << " " << sanitize(fn) << ";\n";
            }
            return CXChildVisit_Continue;
        }, &g_hdr_os);
        g_hdr_os << "} " << sn << ";\n\n";
    } else {
        g_hdr_os << "/* NON-TRIVIAL: opaque handle — indirection cost */\n";
        g_hdr_os << "typedef void* " << sn << ";\n\n";
    }

    /* Generate thunks */
    struct WrapData { std::string name; bool has_default_ctor; };
    WrapData wd{ class_name, false };

    clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) {
        auto* data = (WrapData*)d;
        CXCursorKind k = clang_getCursorKind(c);
        if (k == CXCursor_Constructor && clang_Cursor_getNumArguments(c) == 0)
            data->has_default_ctor = true;
        return CXChildVisit_Continue;
    }, &wd);

    clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) {
        auto* wdp = (WrapData*)d;
        auto* cls_name = &wdp->name;
        CXCursorKind k = clang_getCursorKind(c);

        if (k == CXCursor_Constructor) {
            std::string cname = *cls_name;
            /* Generate constructor wrapper */
            int num_params = clang_Cursor_getNumArguments(c);
            std::string fn_name = cname + "_create";
            if (num_params == 0) fn_name = cname + "_new";

            /* Header declaration */
            g_hdr_os << "/* constructor */\n";
            g_hdr_os << "void* " << fn_name << "(";
            for (int i = 0; i < num_params; i++) {
                if (i > 0) g_hdr_os << ", ";
                CXCursor param = clang_Cursor_getArgument(c, i);
                CXType pt = clang_getCursorType(param);
                std::string pname = cxstr(clang_getCursorSpelling(param));
                if (pname.empty()) pname = "p" + std::to_string(i);
                g_hdr_os << cpp_type_to_c_type(pt) << " " << sanitize(pname);
            }
            g_hdr_os << ");\n";

            /* Source implementation */
            g_src_os << "void* " << fn_name << "(";
            for (int i = 0; i < num_params; i++) {
                if (i > 0) g_src_os << ", ";
                CXCursor param = clang_Cursor_getArgument(c, i);
                CXType pt = clang_getCursorType(param);
                std::string pname = cxstr(clang_getCursorSpelling(param));
                if (pname.empty()) pname = "p" + std::to_string(i);
                g_src_os << cpp_type_to_c_type(pt) << " " << sanitize(pname);
            }
            g_src_os << ") {\n";
            if (g_with_exceptions) g_src_os << "    try {\n";
            g_src_os << "    return static_cast<void*>(new " << cname << "(";
            for (int i = 0; i < num_params; i++) {
                if (i > 0) g_src_os << ", ";
                CXCursor param = clang_Cursor_getArgument(c, i);
                std::string pname = cxstr(clang_getCursorSpelling(param));
                if (pname.empty()) pname = "p" + std::to_string(i);
                g_src_os << sanitize(pname);
            }
            g_src_os << "));\n";
            if (g_with_exceptions) {
                g_src_os << "    } catch (const std::exception& _e) {\n";
                g_src_os << "        std::fprintf(stderr, \"[cppwrap] %s: %s\\n\", \"" << fn_name << "\", _e.what());\n";
                g_src_os << "        return nullptr;\n";
                g_src_os << "    } catch (...) {\n";
                g_src_os << "        std::fprintf(stderr, \"[cppwrap] %s: unknown exception\\n\", \"" << fn_name << "\");\n";
                g_src_os << "        return nullptr;\n";
                g_src_os << "    }\n";
            }
            g_src_os << "}\n\n";

            /* Aurora binding */
            g_au_os << "function " << fn_name << "(";
            for (int i = 0; i < num_params; i++) {
                if (i > 0) g_au_os << ", ";
                CXCursor param = clang_Cursor_getArgument(c, i);
                CXType pt = clang_getCursorType(param);
                std::string pname = cxstr(clang_getCursorSpelling(param));
                if (pname.empty()) pname = "p" + std::to_string(i);
                g_au_os << sanitize(pname) << ": " << cpp_type_to_au_type(pt, g_tc_classes);
            }
            g_au_os << ") -> pointer\n";

            if (num_params == 0) wdp->has_default_ctor = true;
        }

        if (k == CXCursor_Destructor) {
            /* Destructor wrapper */
            std::string fn_name = *cls_name + "_delete";
            g_hdr_os << "/* destructor */\n";
            g_hdr_os << "void " << fn_name << "(void* self);\n";

            g_src_os << "void " << fn_name << "(void* self) {\n";
            if (g_with_exceptions) g_src_os << "    try {\n";
            g_src_os << "    if (self) delete static_cast<" << *cls_name << "*>(self);\n";
            if (g_with_exceptions) {
                g_src_os << "    } catch (const std::exception& _e) {\n";
                g_src_os << "        std::fprintf(stderr, \"[cppwrap] %s: %s\\n\", \"" << fn_name << "\", _e.what());\n";
                g_src_os << "    } catch (...) {\n";
                g_src_os << "        std::fprintf(stderr, \"[cppwrap] %s: unknown exception\\n\", \"" << fn_name << "\");\n";
                g_src_os << "    }\n";
            }
            g_src_os << "}\n\n";

            g_au_os << "function " << fn_name << "(self: pointer)\n";
        }

        if (k == CXCursor_CXXMethod) {
            std::string mname = cxstr(clang_getCursorSpelling(c));
            bool is_static = clang_CXXMethod_isStatic(c);
            bool is_virtual = clang_CXXMethod_isVirtual(c);
            bool is_const = clang_CXXMethod_isConst(c);
            CXType ret_type = clang_getCursorResultType(c);
            int num_params = clang_Cursor_getNumArguments(c);

            std::string fn_name = *cls_name + "_" + mname;

            /* Header */
            if (is_virtual) {
                g_hdr_os << "/* virtual method: " << mname << " (vtable dispatch) */\n";
            } else if (is_static) {
                g_hdr_os << "/* static method */\n";
                g_hdr_os << cpp_type_to_c_type(ret_type) << " " << fn_name << "(";
                for (int i = 0; i < num_params; i++) {
                    if (i > 0) g_hdr_os << ", ";
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    CXType pt = clang_getCursorType(param);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_hdr_os << cpp_type_to_c_type(pt) << " " << sanitize(pname);
                }
                g_hdr_os << ");\n";
            } else {
                g_hdr_os << cpp_type_to_c_type(ret_type) << " " << fn_name << "(void* self";
                for (int i = 0; i < num_params; i++) {
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    CXType pt = clang_getCursorType(param);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_hdr_os << ", " << cpp_type_to_c_type(pt) << " " << sanitize(pname);
                }
                g_hdr_os << ");\n";
            }

            /* Source implementation */
            if (is_static) {
                g_src_os << cpp_type_to_c_type(ret_type) << " " << fn_name << "(";
                for (int i = 0; i < num_params; i++) {
                    if (i > 0) g_src_os << ", ";
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    CXType pt = clang_getCursorType(param);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_src_os << cpp_type_to_c_type(pt) << " " << sanitize(pname);
                }
                g_src_os << ") {\n";
                if (g_with_exceptions) g_src_os << "    try {\n";
                g_src_os << "    return " << *cls_name << "::" << mname << "(";
                for (int i = 0; i < num_params; i++) {
                    if (i > 0) g_src_os << ", ";
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_src_os << sanitize(pname);
                }
                g_src_os << ");\n";
                if (g_with_exceptions) {
                    std::string cret = cpp_type_to_c_type(ret_type);
                    g_src_os << "    } catch (const std::exception& _e) {\n";
                    g_src_os << "        std::fprintf(stderr, \"[cppwrap] " << fn_name << ": %s\\n\", _e.what());\n";
                    if (cret != "void") g_src_os << "        return 0;\n";
                    g_src_os << "    } catch (...) {\n";
                    g_src_os << "        std::fprintf(stderr, \"[cppwrap] " << fn_name << ": unknown exception\\n\");\n";
                    if (cret != "void") g_src_os << "        return 0;\n";
                    g_src_os << "    }\n";
                }
                g_src_os << "}\n\n";
            } else if (is_virtual && !is_const) {
                /* Virtual: get vtable, call through it */
                g_src_os << cpp_type_to_c_type(ret_type) << " " << fn_name << "(void* self";
                for (int i = 0; i < num_params; i++) {
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    CXType pt = clang_getCursorType(param);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_src_os << ", " << cpp_type_to_c_type(pt) << " " << sanitize(pname);
                }
                g_src_os << ") {\n";
                if (g_with_exceptions) g_src_os << "    try {\n";
                g_src_os << "    auto* obj = static_cast<" << *cls_name << "*>(self);\n";
                g_src_os << "    return obj->" << mname << "(";
                for (int i = 0; i < num_params; i++) {
                    if (i > 0) g_src_os << ", ";
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_src_os << sanitize(pname);
                }
                g_src_os << ");\n";
                if (g_with_exceptions) {
                    std::string cret = cpp_type_to_c_type(ret_type);
                    g_src_os << "    } catch (const std::exception& _e) {\n";
                    g_src_os << "        std::fprintf(stderr, \"[cppwrap] " << fn_name << ": %s\\n\", _e.what());\n";
                    if (cret != "void") g_src_os << "        return 0;\n";
                    g_src_os << "    } catch (...) {\n";
                    g_src_os << "        std::fprintf(stderr, \"[cppwrap] " << fn_name << ": unknown exception\\n\");\n";
                    if (cret != "void") g_src_os << "        return 0;\n";
                    g_src_os << "    }\n";
                }
                g_src_os << "}\n\n";
            } else {
                g_src_os << cpp_type_to_c_type(ret_type) << " " << fn_name << "(void* self";
                for (int i = 0; i < num_params; i++) {
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    CXType pt = clang_getCursorType(param);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_src_os << ", " << cpp_type_to_c_type(pt) << " " << sanitize(pname);
                }
                g_src_os << ") {\n";
                if (g_with_exceptions) g_src_os << "    try {\n";
                g_src_os << "    auto* obj = static_cast<" << *cls_name << "*>(self);\n";
                g_src_os << "    return obj->" << mname << "(";
                for (int i = 0; i < num_params; i++) {
                    if (i > 0) g_src_os << ", ";
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_src_os << sanitize(pname);
                }
                g_src_os << ");\n";
                if (g_with_exceptions) {
                    std::string cret = cpp_type_to_c_type(ret_type);
                    g_src_os << "    } catch (const std::exception& _e) {\n";
                    g_src_os << "        std::fprintf(stderr, \"[cppwrap] " << fn_name << ": %s\\n\", _e.what());\n";
                    if (cret != "void") g_src_os << "        return 0;\n";
                    g_src_os << "    } catch (...) {\n";
                    g_src_os << "        std::fprintf(stderr, \"[cppwrap] " << fn_name << ": unknown exception\\n\");\n";
                    if (cret != "void") g_src_os << "        return 0;\n";
                    g_src_os << "    }\n";
                }
                g_src_os << "}\n\n";
            }

            /* Aurora binding */
            if (is_static) {
                g_au_os << "function " << fn_name << "(";
                for (int i = 0; i < num_params; i++) {
                    if (i > 0) g_au_os << ", ";
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    CXType pt = clang_getCursorType(param);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_au_os << sanitize(pname) << ": " << cpp_type_to_au_type(pt, g_tc_classes);
                }
                g_au_os << ")";
                if (ret_type.kind != CXType_Void)
                    g_au_os << " -> " << cpp_type_to_au_type(ret_type, g_tc_classes);
                g_au_os << "\n";
            } else if (is_virtual) {
                g_au_os << "/* virtual (vtable dispatch): " << fn_name << " */\n";
                g_au_os << "function " << fn_name << "(self: pointer";
                for (int i = 0; i < num_params; i++) {
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    CXType pt = clang_getCursorType(param);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_au_os << ", " << sanitize(pname) << ": " << cpp_type_to_au_type(pt, g_tc_classes);
                }
                g_au_os << ")";
                if (ret_type.kind != CXType_Void)
                    g_au_os << " -> " << cpp_type_to_au_type(ret_type, g_tc_classes);
                g_au_os << "\n";
            } else {
                g_au_os << "function " << fn_name << "(self: pointer";
                for (int i = 0; i < num_params; i++) {
                    CXCursor param = clang_Cursor_getArgument(c, i);
                    CXType pt = clang_getCursorType(param);
                    std::string pname = cxstr(clang_getCursorSpelling(param));
                    if (pname.empty()) pname = "p" + std::to_string(i);
                    g_au_os << ", " << sanitize(pname) << ": " << cpp_type_to_au_type(pt, g_tc_classes);
                }
                g_au_os << ")";
                if (ret_type.kind != CXType_Void)
                    g_au_os << " -> " << cpp_type_to_au_type(ret_type, g_tc_classes);
                g_au_os << "\n";
            }
        }

        return CXChildVisit_Continue;
    }, (CXClientData)&wd);

    /* If no default constructor was generated, add one anyway */
    if (!wd.has_default_ctor) {
        std::string fn_name = class_name + "_new";
        g_hdr_os << "void* " << fn_name << "();\n";
        g_src_os << "void* " << fn_name << "() {\n";
        if (g_with_exceptions) g_src_os << "    try {\n";
        g_src_os << "    return static_cast<void*>(new " << class_name << "());\n";
        if (g_with_exceptions) {
            g_src_os << "    } catch (const std::exception& _e) {\n";
            g_src_os << "        std::fprintf(stderr, \"[cppwrap] " << fn_name << ": %s\\n\", _e.what());\n";
            g_src_os << "        return nullptr;\n";
            g_src_os << "    } catch (...) {\n";
            g_src_os << "        std::fprintf(stderr, \"[cppwrap] " << fn_name << ": unknown exception\\n\");\n";
            g_src_os << "        return nullptr;\n";
            g_src_os << "    }\n";
        }
        g_src_os << "}\n\n";
        g_au_os << "function " << fn_name << "() -> pointer\n";
    }

    g_hdr_os << "\n";
}

/* ── Check if cursor is from one of the input headers ───── */
static bool is_from_input_header(CXCursor cursor) {
    CXSourceLocation loc = clang_getCursorLocation(cursor);
    if (clang_Location_isFromMainFile(loc)) return true;
    /* Also check via file path */
    CXFile file;
    unsigned line, col, offset;
    clang_getSpellingLocation(loc, &file, &line, &col, &offset);
    if (!file) return false;
    std::string fname = cxstr(clang_getFileName(file));
    for (auto& h : g_header_paths) {
        /* Check if fname ends with the header path */
        if (fname.size() >= h.size() &&
            fname.compare(fname.size() - h.size(), h.size(), h) == 0)
            return true;
        /* Check normalized path */
        std::string normalized = fname;
        for (auto& c : normalized) if (c == '\\') c = '/';
        std::string hdr = h;
        for (auto& c : hdr) if (c == '\\') c = '/';
        if (normalized.size() >= hdr.size() &&
            normalized.compare(normalized.size() - hdr.size(), hdr.size(), hdr) == 0)
            return true;
    }
    return false;
}

/* ── Visit cursor tree ────────────────────────────────────── */
static CXChildVisitResult class_visitor(CXCursor cursor, CXCursor parent, CXClientData data) {
    (void)parent; (void)data;
    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_ClassDecl || kind == CXCursor_StructDecl) {
        std::string name = cxstr(clang_getCursorSpelling(cursor));
        if (!name.empty() && is_from_input_header(cursor)) {
            wrap_class(cursor, name);
        }
    }

    if (kind == CXCursor_ClassTemplate) {
        std::string name = cxstr(clang_getCursorSpelling(cursor));
        if (!name.empty() && is_from_input_header(cursor)) {
            /* For templates, only wrap if explicit instantiation requested */
            if (g_verbose)
                std::cerr << "[cppwrap] skipping template class: " << name
                          << " (use explicit instantiation)\n";
        }
        return CXChildVisit_Continue; /* Don't recurse into template */
    }

    /* Skip function templates */
    if (kind == CXCursor_FunctionTemplate) {
        return CXChildVisit_Continue;
    }

    /* Skip namespaces but recurse into them */
    if (kind == CXCursor_Namespace) {
        return CXChildVisit_Recurse;
    }

    return CXChildVisit_Recurse;
}

/* ════════════════════════════════════════════════════════════
   Main
   ════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: aurora-cppwrap header.h [options]\n";
        std::cerr << "Options:\n";
        std::cerr << "  -o dir          Output directory (default: .)\n";
        std::cerr << "  -l lib          Library name for Aurora extern bindings (default: cppwrap)\n";
        std::cerr << "  -m module       Module name for file naming (default: cpplib)\n";
        std::cerr << "  --no-cpp        Skip C wrapper generation\n";
        std::cerr << "  --no-au         Skip Aurora binding generation\n";
        std::cerr << "  --with-exceptions Wrap thunks in try/catch for exception safety\n";
        std::cerr << "  -I path         Include path\n";
        std::cerr << "  -D def          Macro definition\n";
        std::cerr << "  -std standard   C++ standard (c++11, c++14, c++17, c++20)\n";
        std::cerr << "  --verbose       Verbose output\n";
        return 1;
    }

    /* Parse args */
    g_clang_args_storage.reserve(argc);  /* prevent reallocation, keeping c_str() pointers valid */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            g_output_dir = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            g_lib_name = argv[++i];
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            g_module_name = argv[++i];
        } else if (strcmp(argv[i], "--no-cpp") == 0) {
            g_no_cpp = true;
        } else if (strcmp(argv[i], "--no-au") == 0) {
            g_no_au = true;
        } else if (strcmp(argv[i], "--with-exceptions") == 0) {
            g_with_exceptions = true;
        } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            g_clang_args_storage.push_back("-I" + std::string(argv[++i]));
            g_clang_args.push_back(g_clang_args_storage.back().c_str());
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            g_clang_args_storage.push_back(argv[i]);
            g_clang_args.push_back(g_clang_args_storage.back().c_str());
        } else if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            g_clang_args_storage.push_back("-D" + std::string(argv[++i]));
            g_clang_args.push_back(g_clang_args_storage.back().c_str());
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            g_clang_args_storage.push_back(argv[i]);
            g_clang_args.push_back(g_clang_args_storage.back().c_str());
        } else if (strcmp(argv[i], "-std") == 0 && i + 1 < argc) {
            g_clang_args_storage.push_back("-std=" + std::string(argv[++i]));
            g_clang_args.push_back(g_clang_args_storage.back().c_str());
        } else if (strcmp(argv[i], "--verbose") == 0) {
            g_verbose = true;
        } else if (argv[i][0] != '-') {
            g_header_paths.push_back(argv[i]);
        }
    }

    if (g_header_paths.empty()) {
        std::cerr << "Error: no input header files\n";
        return 1;
    }

    /* Open output files */
    if (!g_no_cpp) {
        std::string hdr_path = g_output_dir + "/" + g_module_name + "_cppwrap.h";
        std::string src_path = g_output_dir + "/" + g_module_name + "_cppwrap.cpp";
        g_hdr_os.open(hdr_path);
        g_src_os.open(src_path);
        if (!g_hdr_os || !g_src_os) {
            std::cerr << "Error: could not open output files\n";
            return 1;
        }
        if (g_verbose) {
            std::cerr << "[cppwrap] header: " << hdr_path << "\n";
            std::cerr << "[cppwrap] source: " << src_path << "\n";
        }
    }

    if (!g_no_au) {
        std::string au_path = g_output_dir + "/" + g_module_name + "_cppwrap.auf";
        g_au_os.open(au_path);
        if (!g_au_os) {
            std::cerr << "Error: could not open .au output file\n";
            return 1;
        }
        if (g_verbose) {
            std::cerr << "[cppwrap] aurora bindings: " << au_path << "\n";
        }
    }

    /* Create libclang index */
    CXIndex index = clang_createIndex(1, 1);
    if (!index) {
        std::cerr << "Error: could not create libclang index\n";
        return 1;
    }

    /* Generate header guard */
    if (!g_no_cpp) gen_c_header_begin();
    if (!g_no_au) gen_au_begin();

    /* Source file: include the header and wrapper header */
    if (!g_no_cpp) {
        g_src_os << "/* ════════════════════════════════════════════════════════════\n";
        g_src_os << "   Auto-generated C++ extern \"C\" wrapper for: " << g_module_name << "\n";
        g_src_os << "   ════════════════════════════════════════════════════════════ */\n\n";
        g_src_os << "#include \"" << g_module_name << "_cppwrap.h\"\n";
        g_src_os << "#include <exception>\n";
        g_src_os << "#include <cstdio>\n";
        for (auto& h : g_header_paths) {
            /* Try to make include relative */
            std::string include_path = h;
            g_src_os << "#include \"" << include_path << "\"\n";
        }
        g_src_os << "\n";
        g_src_os << "/* ════════════════════════════════════════════════════════════\n";
        g_src_os << "   extern \"C\" thunks — one indirection per method call\n";
        g_src_os << "   ════════════════════════════════════════════════════════════ */\n\n";
    }

    /* Parse each header */
    for (auto& hdr : g_header_paths) {
        if (g_verbose) std::cerr << "[cppwrap] parsing: " << hdr << "\n";

        CXTranslationUnit tu = clang_parseTranslationUnit(
            index, hdr.c_str(),
            g_clang_args.data(), (int)g_clang_args.size(),
            nullptr, 0, 0);

        if (!tu) {
            std::cerr << "Error: could not parse '" << hdr << "'\n";
            continue;
        }

        CXCursor tu_cursor = clang_getTranslationUnitCursor(tu);
        clang_visitChildren(tu_cursor, class_visitor, nullptr);
        clang_disposeTranslationUnit(tu);
    }

    clang_disposeIndex(index);

    if (!g_no_cpp) gen_c_header_end();

    if (!g_no_au) {
        g_au_os << "/* end of " << g_module_name << " bindings */\n";
    }

    /* Summary */
    std::cout << "[cppwrap] wrapped " << g_wrapped_classes.size() << " classes from "
              << g_header_paths.size() << " header(s)\n";

    return 0;
}
