#include <clang-c/Index.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <sstream>
#include <fstream>

/* ════════════════════════════════════════════════════════════
   aurora-bindgen — C header → Aurora binding generator
   ════════════════════════════════════════════════════════════
   Uses libclang to parse C headers and emit Aurora .au
   extern declaration files for FFI.

   Usage: aurora-bindgen header.h [-o output.au] [-l library]
          aurora-bindgen -- header1.h header2.h ... [-o output.au] [-l library]
   ════════════════════════════════════════════════════════════ */

/* ── Configuration ────────────────────────────────────────── */
static std::string g_lib_name = "c";   /* default library name */
static std::string g_call_conv = "";   /* calling convention override */
static std::string g_package = "";     /* optional package/namespace wrapper */
static bool        g_no_macros = false;
static bool        g_no_functions = false;
static bool        g_no_structs = false;
static bool        g_no_unions = false;
static bool        g_no_typedefs = false;
static bool        g_verbose = false;
static std::ostream*        g_out      = &std::cout;
static std::ofstream        g_ofs;
static std::set<std::string> g_emitted_types;     /* avoid duplicate struct defs */
static std::vector<std::string> g_header_paths;
static std::vector<const char*> g_clang_args;     /* extra clang arguments (from -I, -D, etc.) */

/* ── Helpers ──────────────────────────────────────────────── */
static std::string cxstr(CXString s) {
    std::string r = clang_getCString(s);
    clang_disposeString(s);
    return r;
}

static std::string sanitize(const std::string& name) {
    /* Replace special characters that aren't valid in Aurora identifiers */
    std::string r = name;
    for (auto& c : r) {
        if (!isalnum((unsigned char)c) && c != '_')
            c = '_';
    }
    return r;
}

static std::string indent(int depth) {
    return std::string(depth * 4, ' ');
}

/* ── Type mapper: C type → Aurora type string ──────────── */
static std::string c_type_to_aurora(CXType ct) {
    ct = clang_getUnqualifiedType(ct);
    CXTypeKind kind = ct.kind;
    switch (kind) {
        case CXType_Void:      return "void";
        case CXType_Bool:      return "bool";
        case CXType_Char_S:
        case CXType_SChar:     return "i8";
        case CXType_Char_U:
        case CXType_UChar:     return "u8";
        case CXType_Short:     return "i16";
        case CXType_UShort:    return "u16";
        case CXType_Int:       return "i32";
        case CXType_UInt:      return "u32";
        case CXType_Long:
        case CXType_LongLong:  return "i64";
        case CXType_ULong:
        case CXType_ULongLong: return "u64";
        case CXType_Float:     return "f32";
        case CXType_Double:    return "double";
        case CXType_WChar:
        case CXType_Char16:
        case CXType_Char32:    return "i32";
        case CXType_ConstantArray:
        case CXType_IncompleteArray:
        case CXType_DependentSizedArray:
            return "pointer";
        case CXType_Pointer: {
            CXType pointee = clang_getPointeeType(ct);
            CXTypeKind pk = pointee.kind;
            if (pk == CXType_Char_S || pk == CXType_SChar) {
                return "cstring";   /* char* → cstring */
            }
            if (pk == CXType_Void) {
                return "pointer";   /* void* → pointer (NOT cstring) */
            }
            if (pk == CXType_FunctionProto || pk == CXType_FunctionNoProto) {
                /* Function pointer → callback type */
                return "callback";  /* will be expanded later */
            }
            /* Otherwise treat as opaque pointer */
            return "pointer";
        }
        case CXType_Record: {
            std::string name = cxstr(clang_getTypeSpelling(ct));
            /* Strip "struct ", "union ", "enum " prefix if present */
            if (name.compare(0, 7, "struct ") == 0)
                name = name.substr(7);
            else if (name.compare(0, 6, "union ") == 0)
                name = name.substr(6);
            else if (name.compare(0, 5, "enum ") == 0)
                name = name.substr(5);
            return sanitize(name);
        }
        case CXType_Enum: {
            std::string name = cxstr(clang_getTypeSpelling(ct));
            if (name.compare(0, 5, "enum ") == 0)
                name = name.substr(5);
            return sanitize(name);
        }
        case CXType_Typedef: {
            std::string name = cxstr(clang_getTypeSpelling(ct));
            /* Map known standard typedefs */
            if (name == "size_t") return "u64";
            if (name == "uint8_t")  return "u8";
            if (name == "uint16_t") return "u16";
            if (name == "uint32_t") return "u32";
            if (name == "uint64_t") return "u64";
            if (name == "int8_t")   return "i8";
            if (name == "int16_t")  return "i16";
            if (name == "int32_t")  return "i32";
            if (name == "int64_t")  return "i64";
            if (name == "uintptr_t") return "u64";
            if (name == "intptr_t")  return "i64";
            if (name == "ssize_t")   return "i64";
            if (name == "wchar_t")   return "i32";
            if (name == "char16_t")  return "u16";
            if (name == "char32_t")  return "u32";
            /* Resolve the underlying type */
            CXType underlying = clang_getTypedefDeclUnderlyingType(
                clang_getTypeDeclaration(ct));
            /* For function pointer typedefs, keep as-is and handle later */
            CXTypeKind uk = underlying.kind;
            if (uk == CXType_Pointer) {
                CXType pointee = clang_getPointeeType(underlying);
                if (pointee.kind == CXType_FunctionProto || pointee.kind == CXType_FunctionNoProto) {
                    return sanitize(name);
                }
            }
            return c_type_to_aurora(underlying);
        }
        default: {
            std::string name = cxstr(clang_getTypeSpelling(ct));
            return sanitize(name);
        }
    }
}

/* ── Write Aurora comments ─────────────────────────────── */
static void write_comment(const std::string& text, int depth = 0) {
    *g_out << indent(depth) << "/* " << text << " */\n";
}

/* ── Generate a callback type string from a function pointer type ── */
static std::string gen_callback_type(CXType fn_ptr_type, int depth) {
    CXType fn_type = clang_getPointeeType(fn_ptr_type);
    int num_args = clang_getNumArgTypes(fn_type);
    std::string result = "callback(";
    for (int i = 0; i < num_args; i++) {
        if (i > 0) result += ", ";
        CXType arg_type = clang_getArgType(fn_type, i);
        result += c_type_to_aurora(arg_type);
    }
    result += ")";
    CXType ret_type = clang_getResultType(fn_type);
    if (ret_type.kind != CXType_Void) {
        result += " -> " + c_type_to_aurora(ret_type);
    }
    return result;
}

/* ── Generate a function pointer typedef as a `type` alias ── */
static bool emit_typedef_fn_ptr(CXCursor cursor, const std::string& name, int depth) {
    CXType underlying = clang_getTypedefDeclUnderlyingType(cursor);
    if (underlying.kind != CXType_Pointer) return false;
    CXType pointee = clang_getPointeeType(underlying);
    if (pointee.kind != CXType_FunctionProto && pointee.kind != CXType_FunctionNoProto)
        return false;

    /* Generate: type Name = callback(params...) -> ret */
    std::string cb = gen_callback_type(underlying, depth);
    *g_out << indent(depth) << "type " << sanitize(name) << " = " << cb << "\n";
    g_emitted_types.insert(name);
    return true;
}

/* ── Forward declarations ── */
static void emit_struct(CXCursor cursor, int depth);
static void emit_union(CXCursor cursor, int depth);
static void emit_enum(CXCursor cursor, int depth);
static void emit_function(CXCursor cursor, int depth);
static void emit_typedef(CXCursor cursor, int depth);
static void emit_macro(CXCursor cursor, int depth);

/* ── Emit "const Name = value" for #define numeric constants ── */
/* (emit_macro is placed after other emitters so it can reference them) */

/* ── Cursor visitor ──────────────────────────────────────── */
static CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData data) {
    (void)parent; (void)data;
    CXCursorKind kind = clang_getCursorKind(cursor);
    int depth = *((int*)data);

    switch (kind) {
        case CXCursor_FunctionDecl:
            if (!g_no_functions) emit_function(cursor, depth);
            break;
        case CXCursor_StructDecl:
            if (!g_no_structs) emit_struct(cursor, depth);
            break;
        case CXCursor_UnionDecl:
            if (!g_no_unions) emit_union(cursor, depth);
            break;
        case CXCursor_EnumDecl:
            emit_enum(cursor, depth);
            break;
        case CXCursor_TypedefDecl:
            if (!g_no_typedefs) emit_typedef(cursor, depth);
            break;
        case CXCursor_MacroDefinition:
            if (!g_no_macros) emit_macro(cursor, depth);
            break;
        default:
            break;
    }
    return CXChildVisit_Recurse;
}

/* ── Emit struct: extern struct Name { fields } ── */
static void emit_struct(CXCursor cursor, int depth) {
    std::string name = cxstr(clang_getCursorSpelling(cursor));
    if (name.empty()) return;
    if (g_emitted_types.count(name)) return;
    g_emitted_types.insert(name);

    /* Check if this is a forward declaration (no fields) → opaque */
    int num_children = 0;
    clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) {
        if (clang_getCursorKind(c) == CXCursor_FieldDecl)
            (*((int*)d))++;
        return CXChildVisit_Continue;
    }, &num_children);

    if (num_children == 0) {
        *g_out << indent(depth) << "extern struct " << sanitize(name) << "\n";
        if (g_verbose) std::cerr << "[bindgen]   opaque struct " << name << "\n";
    } else {
        *g_out << indent(depth) << "extern struct " << sanitize(name) << " {\n";
        clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) {
            if (clang_getCursorKind(c) != CXCursor_FieldDecl)
                return CXChildVisit_Continue;
            int depth2 = *((int*)d) + 1;
            std::string fname = cxstr(clang_getCursorSpelling(c));
            CXType ftype = clang_getCursorType(c);
            std::string atype;
            if (ftype.kind == CXType_Pointer) {
                CXType pointee = clang_getPointeeType(ftype);
                if (pointee.kind == CXType_FunctionProto || pointee.kind == CXType_FunctionNoProto) {
                    atype = gen_callback_type(ftype, depth2);
                } else {
                    atype = c_type_to_aurora(ftype);
                }
            } else {
                atype = c_type_to_aurora(ftype);
            }
            *g_out << indent(depth2) << sanitize(fname) << ": " << atype << "\n";
            return CXChildVisit_Continue;
        }, &depth);
        *g_out << indent(depth) << "}\n";
    }
}

/* ── Emit union: extern union Name { fields } ── */
static void emit_union(CXCursor cursor, int depth) {
    std::string name = cxstr(clang_getCursorSpelling(cursor));
    if (name.empty()) return;
    if (g_emitted_types.count(name)) return;
    g_emitted_types.insert(name);

    int num_children = 0;
    clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) {
        if (clang_getCursorKind(c) == CXCursor_FieldDecl)
            (*((int*)d))++;
        return CXChildVisit_Continue;
    }, &num_children);

    if (num_children == 0) {
        *g_out << indent(depth) << "extern union " << sanitize(name) << "\n";
    } else {
        *g_out << indent(depth) << "extern union " << sanitize(name) << " {\n";
        clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) {
            if (clang_getCursorKind(c) != CXCursor_FieldDecl)
                return CXChildVisit_Continue;
            int depth2 = *((int*)d) + 1;
            std::string fname = cxstr(clang_getCursorSpelling(c));
            CXType ftype = clang_getCursorType(c);
            std::string atype;
            if (ftype.kind == CXType_Pointer) {
                CXType pointee = clang_getPointeeType(ftype);
                if (pointee.kind == CXType_FunctionProto || pointee.kind == CXType_FunctionNoProto) {
                    atype = gen_callback_type(ftype, depth2);
                } else {
                    atype = c_type_to_aurora(ftype);
                }
            } else {
                atype = c_type_to_aurora(ftype);
            }
            *g_out << indent(depth2) << sanitize(fname) << ": " << atype << "\n";
            return CXChildVisit_Continue;
        }, &depth);
        *g_out << indent(depth) << "}\n";
    }
}

/* ── Emit enum: extern enum Name { Variant1, Variant2, ... } ── */
static void emit_enum(CXCursor cursor, int depth) {
    std::string name = cxstr(clang_getCursorSpelling(cursor));
    if (name.empty()) return;
    if (g_emitted_types.count(name)) return;
    g_emitted_types.insert(name);

    *g_out << indent(depth) << "extern enum " << sanitize(name);
    bool first = true;
    clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) {
        if (clang_getCursorKind(c) != CXCursor_EnumConstantDecl)
            return CXChildVisit_Continue;
        bool* fp = (bool*)d;
        std::string vname = cxstr(clang_getCursorSpelling(c));
        if (*fp) { *g_out << ", "; }
        *g_out << vname;
        *fp = false;
        return CXChildVisit_Continue;
    }, &first);
    *g_out << "\n";
}

/* ── Detect calling convention from cursor attributes ── */
/* Uses libclang's `clang_getCursorCallingConv` if available,
   plus manual attribute inspection as fallback. */
static std::string detect_calling_conv(CXCursor cursor) {
    /* Try libclang's built-in calling convention detection */
    /* This is available in libclang >= 3.9 */
    CXType fn_type = clang_getCursorType(cursor);

    /* Check type spelling for calling convention keywords embedded by the frontend */
    /* On MSVC, __stdcall appears as part of the canonical type */
    std::string full_spelling = cxstr(clang_getTypeSpelling(fn_type));

    /* MSVC prepends calling convention to function pointer types.
       For regular functions, check the cursor's underlying declaration type. */
    if (full_spelling.find("__stdcall") != std::string::npos ||
        full_spelling.find("_stdcall")  != std::string::npos)
        return "stdcall";
    if (full_spelling.find("__fastcall") != std::string::npos ||
        full_spelling.find("_fastcall")  != std::string::npos)
        return "fastcall";
    if (full_spelling.find("__thiscall") != std::string::npos ||
        full_spelling.find("_thiscall")  != std::string::npos)
        return "thiscall";
    if (full_spelling.find("__vectorcall") != std::string::npos ||
        full_spelling.find("_vectorcall")  != std::string::npos)
        return "vectorcall";

    /* Also check the cursor's result type for calling convention annotations */
    CXType result_type = clang_getResultType(fn_type);
    if (result_type.kind != CXType_Invalid) {
        std::string ret_spelling = cxstr(clang_getTypeSpelling(result_type));
        if (ret_spelling.find("__stdcall") != std::string::npos ||
            ret_spelling.find("_stdcall")  != std::string::npos)
            return "stdcall";
    }

    /* Check the cursor kind — for CXCursor_UnexposedAttr we can inspect the token */
    /* (libclang may not expose all attributes via typed cursor kinds) */

    return "";
}

/* ── Detect varargs ── */
static bool is_vararg_fn(CXCursor cursor) {
    CXType fn_type = clang_getCursorType(cursor);
    return clang_isFunctionTypeVariadic(fn_type);
}

/* ── Emit function: extern ["cc"] "lib" function name(params) -> ret ── */
static void emit_function(CXCursor cursor, int depth) {
    std::string name = cxstr(clang_getCursorSpelling(cursor));
    if (name.empty()) return;

    CXType fn_type = clang_getCursorType(cursor);
    int num_args = clang_getNumArgTypes(fn_type);
    CXType ret_type = clang_getResultType(fn_type);
    bool vararg = is_vararg_fn(cursor);

    /* Determine calling convention: CLI override > detected from cursor > "c" */
    std::string cc = g_call_conv;
    if (cc.empty()) {
        cc = detect_calling_conv(cursor);
    }

    *g_out << indent(depth);
    *g_out << "extern";
    if (!cc.empty())
        *g_out << " \"" << cc << "\"";
    if (!g_lib_name.empty())
        *g_out << " \"" << g_lib_name << "\"";
    *g_out << " function " << sanitize(name) << "(";

    for (int i = 0; i < num_args; i++) {
        if (i > 0) *g_out << ", ";
        CXType arg_type = clang_getArgType(fn_type, i);
        std::string atype = c_type_to_aurora(arg_type);
        /* Generate param name (use "p0", "p1", etc.) */
        std::string pname = "p" + std::to_string(i);

        /* Handle array-type params — arrays decay to pointer in C */
        if (arg_type.kind == CXType_ConstantArray ||
            arg_type.kind == CXType_IncompleteArray ||
            arg_type.kind == CXType_DependentSizedArray) {
            *g_out << pname << ": pointer";
            continue;
        }

        /* Handle function pointer params — generate callback type inline */
        if (arg_type.kind == CXType_Pointer) {
            CXType pointee = clang_getPointeeType(arg_type);
            if (pointee.kind == CXType_FunctionProto || pointee.kind == CXType_FunctionNoProto) {
                *g_out << pname << ": " << gen_callback_type(arg_type, 0);
                continue;
            }
        }

        *g_out << pname << ": " << atype;
    }

    /* Varargs: emit "..." as last parameter (Aurora syntax) */
    if (vararg) {
        if (num_args > 0) *g_out << ", ";
        *g_out << "...";
    }

    *g_out << ")";

    if (ret_type.kind == CXType_Pointer) {
        CXType pointee = clang_getPointeeType(ret_type);
        if (pointee.kind == CXType_Char_S || pointee.kind == CXType_SChar) {
            *g_out << " -> cstring";  /* char* return → cstring */
        } else {
            *g_out << " -> " << c_type_to_aurora(ret_type);
        }
    } else if (ret_type.kind != CXType_Void)
        *g_out << " -> " << c_type_to_aurora(ret_type);
    *g_out << "\n";
}

/* ── Emit typedef: type Name = BaseType ── */
static void emit_typedef(CXCursor cursor, int depth) {
    std::string name = cxstr(clang_getCursorSpelling(cursor));
    if (name.empty()) return;
    if (g_emitted_types.count(name)) return;

    /* Check if it's a function pointer typedef */
    if (emit_typedef_fn_ptr(cursor, name, depth))
        return;

    CXType underlying = clang_getTypedefDeclUnderlyingType(cursor);
    std::string atype = c_type_to_aurora(underlying);

    /* Skip trivial typedefs that map to the same base type */
    if (atype == sanitize(name)) return;

    *g_out << indent(depth) << "type " << sanitize(name) << " = " << atype << "\n";
    g_emitted_types.insert(name);
}

/* ── Emit simple #define constants ──
   Emits numeric macro constants from user headers.
   Skips: function-like macros, system macros (starting with _), string macros. */
static void emit_macro(CXCursor cursor, int depth) {
    (void)depth;
    std::string name = cxstr(clang_getCursorSpelling(cursor));
    if (name.empty()) return;
    /* Skip all system/compiler internal macros (start with underscore) */
    if (name[0] == '_') return;
    if (name == "TRUE" || name == "FALSE" || name == "true" || name == "false" ||
        name == "NULL" || name == "nullptr" || name == "stdin" ||
        name == "stdout" || name == "stderr") return;
    /* Try to get macro value via libclang tokenization */
    CXToken* tokens = nullptr;
    unsigned num_tokens = 0;
    CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
    clang_tokenize(tu, clang_getCursorExtent(cursor), &tokens, &num_tokens);

    if (num_tokens >= 2) {
        /* tokens[0] is "identifier" (the macro name), check tokens[1] */
        CXTokenKind tk_kind = clang_getTokenKind(tokens[1]);
        if (tk_kind == CXToken_Literal) {
            std::string raw = cxstr(clang_getTokenSpelling(tu, tokens[1]));
            /* Check if it's a numeric literal */
            if (!raw.empty() && (isdigit((unsigned char)raw[0]) || raw[0] == '-' || raw[0] == '+')) {
                *g_out << indent(depth) << sanitize(name) << " = " << raw << "\n";
            }
        }
    }

    if (tokens) clang_disposeTokens(tu, tokens, num_tokens);
}

/* ════════════════════════════════════════════════════════════
   Main
   ════════════════════════════════════════════════════════════ */
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: aurora-bindgen header.h [-o output.au] [-l library] [--cc stdcall] [-I path] [-D def]\n";
        std::cerr << "       aurora-bindgen -- header1.h header2.h ... [-o output.au] [-l library] [--cc stdcall]\n";
        std::cerr << "\n";
        std::cerr << "Options:\n";
        std::cerr << "  -o file        Output file (default: stdout)\n";
        std::cerr << "  -l lib         Library name for extern bindings (default: 'c')\n";
        std::cerr << "  -p pkg         Package name (wraps in namespace pkg { ... })\n";
        std::cerr << "  --cc conv      Calling convention: stdcall, fastcall, thiscall, vectorcall\n";
        std::cerr << "  -I path        Add include path for header parsing\n";
        std::cerr << "  -D def         Add macro definition for header parsing\n";
        std::cerr << "  -std standard  C standard (c99, c11, c17, c23)\n";
        std::cerr << "  --include hdr  Force-include a header before parsing\n";
        std::cerr << "  --no-macros    Skip #define constant emission\n";
        std::cerr << "  --no-functions Skip function declarations\n";
        std::cerr << "  --no-structs   Skip struct declarations\n";
        std::cerr << "  --no-unions    Skip union declarations\n";
        std::cerr << "  --no-typedefs  Skip typedef declarations\n";
        std::cerr << "  --verbose / -v Verbose output\n";
        return 1;
    }

    /* Parse arguments */
    std::string output_path;
    int arg_idx = 1;

    /* Check for -- separator */
    if (argc >= 2 && strcmp(argv[1], "--") == 0) {
        arg_idx = 2;
        while (arg_idx < argc && argv[arg_idx][0] != '-')
            g_header_paths.push_back(argv[arg_idx++]);
    }

    for (int i = arg_idx; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            g_lib_name = argv[++i];
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--package") == 0) && i + 1 < argc) {
            g_package = argv[++i];
        } else if (strcmp(argv[i], "--cc") == 0 && i + 1 < argc) {
            g_call_conv = argv[++i];
        } else if (strcmp(argv[i], "-I") == 0 && i + 1 < argc) {
            std::string inc = "-I";
            inc += argv[++i];
            g_clang_args.push_back(strdup(inc.c_str()));
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            g_clang_args.push_back(strdup(argv[i]));
        } else if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            std::string def = "-D";
            def += argv[++i];
            g_clang_args.push_back(strdup(def.c_str()));
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            g_clang_args.push_back(strdup(argv[i]));
        } else if (strcmp(argv[i], "-std") == 0 && i + 1 < argc) {
            std::string st = "-std=";
            st += argv[++i];
            g_clang_args.push_back(strdup(st.c_str()));
        } else if (strcmp(argv[i], "--include") == 0 && i + 1 < argc) {
            std::string inc = "-include";
            g_clang_args.push_back(strdup(inc.c_str()));
            g_clang_args.push_back(strdup(argv[++i]));
        } else if (strcmp(argv[i], "--no-macros") == 0) {
            g_no_macros = true;
        } else if (strcmp(argv[i], "--no-functions") == 0) {
            g_no_functions = true;
        } else if (strcmp(argv[i], "--no-structs") == 0) {
            g_no_structs = true;
        } else if (strcmp(argv[i], "--no-unions") == 0) {
            g_no_unions = true;
        } else if (strcmp(argv[i], "--no-typedefs") == 0) {
            g_no_typedefs = true;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            g_verbose = true;
        } else if (argv[i][0] != '-') {
            g_header_paths.push_back(argv[i]);
        }
    }

    if (g_header_paths.empty()) {
        std::cerr << "Error: no input header files specified\n";
        return 1;
    }

    /* Open output file if requested */
    if (!output_path.empty()) {
        g_ofs.open(output_path);
        if (!g_ofs) {
            std::cerr << "Error: could not open output file '" << output_path << "'\n";
            return 1;
        }
        g_out = &g_ofs;
    }

    /* Create libclang index */
    CXIndex index = clang_createIndex(0, 0);
    if (!index) {
        std::cerr << "Error: could not create libclang index\n";
        return 1;
    }

    /* Generate header comment */
    *g_out << "/* ════════════════════════════════════════════════════════════\n";
    *g_out << "   Auto-generated Aurora FFI bindings\n";
    *g_out << "   Library: " << g_lib_name << "\n";
    *g_out << "   Headers:\n";
    for (auto& h : g_header_paths)
        *g_out << "     " << h << "\n";
    *g_out << "   Generated by aurora-bindgen\n";
    *g_out << "   ════════════════════════════════════════════════════════════ */\n\n";

    /* Optional package wrapping */
    bool package_open = false;
    if (!g_package.empty()) {
        *g_out << "package " << g_package << "\n\n";
        *g_out << "namespace " << g_package << "\n";
        package_open = true;
    }

    /* Parse each header */
    for (auto& hdr : g_header_paths) {
        if (g_verbose) std::cerr << "[bindgen] parsing " << hdr << "\n";
        write_comment("Bindings from " + hdr);
        *g_out << "\n";

        CXTranslationUnit tu = clang_parseTranslationUnit(index, hdr.c_str(),
            g_clang_args.data(), (int)g_clang_args.size(),
            nullptr, 0, CXTranslationUnit_DetailedPreprocessingRecord);

        if (!tu) {
            std::cerr << "Error: could not parse '" << hdr << "'\n";
            continue;
        }

        CXCursor cursor = clang_getTranslationUnitCursor(tu);
        int depth = 0;
        clang_visitChildren(cursor, visitor, &depth);

        clang_disposeTranslationUnit(tu);
        *g_out << "\n";
    }

    clang_disposeIndex(index);

    /* Close package wrapping if opened */
    if (package_open) {
        *g_out << "\n";
        *g_out << "/* end of package " << g_package << " */\n";
    }

    if (g_ofs.is_open())
        g_ofs.close();

    return 0;
}
