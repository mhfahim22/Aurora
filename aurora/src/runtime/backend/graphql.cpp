#include "runtime/graphql.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

/* ── Internal helpers ── */

static char* strdup_c(const char* s) {
    if (!s) return nullptr;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── JSON builder (simplified) ── */

struct JsonBuilder {
    std::string buf;
    void append(const char* s) { buf += s; }
    void append(char c) { buf += c; }
    void append(const std::string& s) { buf += s; }
    static std::string escape(const std::string& s) {
        std::string r;
        r.reserve(s.size() + 2);
        for (char c : s) {
            switch (c) {
                case '"': r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default: r += c;
            }
        }
        return r;
    }
    void key(const char* k) { append('"'); append(escape(k)); append("\":"); }
};

/* ── Type system ── */

enum class GqlTypeKind { Object, Scalar, Enum, InputObject, List, NonNull };

struct GqlFieldDef {
    std::string name;
    std::string type_name;
    std::string description;
    AuroraGQLResolver resolver;
    void* ctx;
    bool is_list = false;
    bool is_non_null = false;
};

struct GqlTypeDef {
    std::string name;
    std::string description;
    GqlTypeKind kind;
    std::vector<GqlFieldDef> fields;
    std::vector<std::string> enum_values;
};

struct AuroraGQLSchema {
    std::unordered_map<std::string, GqlTypeDef> types;
    std::string query_type_name;
    std::string mutation_type_name;
};

struct AuroraGQLResult {
    std::string json;
    std::string errors;
    bool has_err;
};

/* ── Schema operations ── */

AuroraGQLSchema* aurora_gql_schema_new(void) {
    AuroraGQLSchema* s = new AuroraGQLSchema();
    if (!s) return nullptr;
    /* Add built-in scalar types */
    GqlTypeDef str_t;
    str_t.name = "String"; str_t.kind = GqlTypeKind::Scalar;
    s->types["String"] = str_t;
    GqlTypeDef int_t;
    int_t.name = "Int"; int_t.kind = GqlTypeKind::Scalar;
    s->types["Int"] = int_t;
    GqlTypeDef float_t;
    float_t.name = "Float"; float_t.kind = GqlTypeKind::Scalar;
    s->types["Float"] = float_t;
    GqlTypeDef bool_t;
    bool_t.name = "Boolean"; bool_t.kind = GqlTypeKind::Scalar;
    s->types["Boolean"] = bool_t;
    GqlTypeDef id_t;
    id_t.name = "ID"; id_t.kind = GqlTypeKind::Scalar;
    s->types["ID"] = id_t;
    /* Default Query type */
    s->query_type_name = "Query";
    GqlTypeDef query_t;
    query_t.name = "Query"; query_t.kind = GqlTypeKind::Object;
    query_t.description = "Root query type";
    s->types["Query"] = query_t;
    return s;
}

void aurora_gql_schema_free(AuroraGQLSchema* schema) {
    delete schema;
}

static GqlTypeDef* find_or_create_type(AuroraGQLSchema* schema, const char* name) {
    auto it = schema->types.find(name);
    if (it != schema->types.end()) return &it->second;
    return nullptr;
}

int aurora_gql_type_add_object(AuroraGQLSchema* schema, const char* name, const char* description) {
    if (!schema || !name) return 0;
    auto it = schema->types.find(name);
    if (it != schema->types.end()) return 0;
    GqlTypeDef t;
    t.name = name;
    t.description = description ? description : "";
    t.kind = GqlTypeKind::Object;
    schema->types[name] = t;
    return 1;
}

int aurora_gql_type_add_enum(AuroraGQLSchema* schema, const char* name, const char* values_csv) {
    if (!schema || !name) return 0;
    auto it = schema->types.find(name);
    if (it != schema->types.end()) return 0;
    GqlTypeDef t;
    t.name = name;
    t.kind = GqlTypeKind::Enum;
    schema->types[name] = t;
    if (values_csv) {
        std::string v = values_csv;
        size_t pos = 0;
        while ((pos = v.find(',')) != std::string::npos) {
            t.enum_values.push_back(v.substr(0, pos));
            v.erase(0, pos + 1);
        }
        if (!v.empty()) t.enum_values.push_back(v);
    }
    return 1;
}

int aurora_gql_type_add_scalar(AuroraGQLSchema* schema, const char* name) {
    if (!schema || !name) return 0;
    auto it = schema->types.find(name);
    if (it != schema->types.end()) return 0;
    GqlTypeDef t;
    t.name = name; t.kind = GqlTypeKind::Scalar;
    schema->types[name] = t;
    return 1;
}

int aurora_gql_type_add_input(AuroraGQLSchema* schema, const char* name) {
    if (!schema || !name) return 0;
    auto it = schema->types.find(name);
    if (it != schema->types.end()) return 0;
    GqlTypeDef t;
    t.name = name; t.kind = GqlTypeKind::InputObject;
    schema->types[name] = t;
    return 1;
}

int aurora_gql_field_add(AuroraGQLSchema* schema, const char* type_name,
                         const char* field_name, const char* field_type,
                         const char* description, AuroraGQLResolver resolver, void* ctx) {
    if (!schema || !type_name || !field_name || !field_type) return 0;
    GqlTypeDef* t = find_or_create_type(schema, type_name);
    if (!t) return 0;
    GqlFieldDef f;
    f.name = field_name;
    f.type_name = field_type;
    f.description = description ? description : "";
    f.resolver = resolver;
    f.ctx = ctx;
    t->fields.push_back(f);
    return 1;
}

int aurora_gql_query_add(AuroraGQLSchema* schema, const char* field_name,
                         const char* field_type, const char* description,
                         AuroraGQLResolver resolver, void* ctx) {
    return aurora_gql_field_add(schema, schema->query_type_name.c_str(),
                                field_name, field_type, description, resolver, ctx);
}

int aurora_gql_mutation_add(AuroraGQLSchema* schema, const char* field_name,
                            const char* field_type, const char* description,
                            AuroraGQLResolver resolver, void* ctx) {
    if (schema->mutation_type_name.empty()) {
        schema->mutation_type_name = "Mutation";
        GqlTypeDef m;
        m.name = "Mutation"; m.kind = GqlTypeKind::Object;
        schema->types["Mutation"] = m;
    }
    return aurora_gql_field_add(schema, schema->mutation_type_name.c_str(),
                                field_name, field_type, description, resolver, ctx);
}

/* ── Simple query lexer/tokenizer ── */

enum class TokenKind { Name, BraceL, BraceR, ParenL, ParenR, Colon, At, Dollar, Bang, Equal,
                       Comma, Dot, IntVal, StringVal, Eof, Unknown };

struct Token {
    TokenKind kind;
    std::string value;
};

struct Lexer {
    const char* p;
    const char* end;
    Token cur;

    Lexer(const char* input) : p(input), end(input ? input + strlen(input) : nullptr) {}

    static bool is_name_start(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    static bool is_name_cont(char c) { return is_name_start(c) || (c >= '0' && c <= '9'); }

    void skip_ws() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',')) ++p;
        /* skip # comments */
        if (p < end && *p == '#') {
            while (p < end && *p != '\n') ++p;
            skip_ws();
        }
    }

    Token next() {
        skip_ws();
        if (p >= end) return { TokenKind::Eof, "" };
        char c = *p;
        if (c == '{') { ++p; return { TokenKind::BraceL, "{" }; }
        if (c == '}') { ++p; return { TokenKind::BraceR, "}" }; }
        if (c == '(') { ++p; return { TokenKind::ParenL, "(" }; }
        if (c == ')') { ++p; return { TokenKind::ParenR, ")" }; }
        if (c == ':') { ++p; return { TokenKind::Colon, ":" }; }
        if (c == '@') { ++p; return { TokenKind::At, "@" }; }
        if (c == '$') { ++p; return { TokenKind::Dollar, "$" }; }
        if (c == '!') { ++p; return { TokenKind::Bang, "!" }; }
        if (c == '=') { ++p; return { TokenKind::Equal, "=" }; }
        if (c == '.') {
            if (p + 2 < end && p[0] == '.' && p[1] == '.' && p[2] == '.') { p += 3; return { TokenKind::Dot, "..." }; }
            ++p; return { TokenKind::Unknown, "." };
        }
        if (c == '"') {
            ++p;
            std::string val;
            while (p < end && *p != '"') {
                if (*p == '\\' && p + 1 < end) { ++p; val += *p++; }
                else val += *p++;
            }
            if (p < end) ++p;
            return { TokenKind::StringVal, val };
        }
        if (c >= '0' && c <= '9') {
            std::string val;
            while (p < end && (*p >= '0' && *p <= '9')) val += *p++;
            return { TokenKind::IntVal, val };
        }
        if (is_name_start(c)) {
            std::string val;
            while (p < end && is_name_cont(*p)) val += *p++;
            return { TokenKind::Name, val };
        }
        ++p;
        return { TokenKind::Unknown, std::string(1, c) };
    }

    Token peek() {
        if (cur.kind == TokenKind::Unknown) cur = next();
        return cur;
    }

    Token consume() {
        if (cur.kind == TokenKind::Unknown) cur = next();
        Token t = cur;
        cur = Token{TokenKind::Unknown, ""};
        return t;
    }
};

/* ── Query AST ── */

struct GqlArg {
    std::string name;
    std::string value;
};

struct GqlField {
    std::string name;
    std::string alias;
    std::vector<GqlArg> args;
    std::vector<GqlField> children;
};

struct GqlOp {
    std::string type; /* query | mutation */
    std::string name;
    std::vector<GqlField> fields;
};

/* ── Query parser ── */

static GqlField parse_field(Lexer& lex);

static std::vector<GqlField> parse_selection_set(Lexer& lex) {
    std::vector<GqlField> fields;
    Token t = lex.peek();
    if (t.kind != TokenKind::BraceL) return fields;
    lex.consume();
    while (true) {
        t = lex.peek();
        if (t.kind == TokenKind::BraceR) { lex.consume(); break; }
        if (t.kind == TokenKind::Eof) break;
        fields.push_back(parse_field(lex));
    }
    return fields;
}

static GqlField parse_field(Lexer& lex) {
    GqlField f;
    Token t = lex.consume();
    if (t.kind != TokenKind::Name) return f;
    f.name = t.value;
    /* Check for alias */
    Token t2 = lex.peek();
    if (t2.kind == TokenKind::Colon) {
        lex.consume();
        f.alias = f.name;
        t = lex.consume();
        if (t.kind == TokenKind::Name) f.name = t.value;
    }
    /* Arguments */
    if (lex.peek().kind == TokenKind::ParenL) {
        lex.consume();
        while (true) {
            Token an = lex.peek();
            if (an.kind == TokenKind::ParenR) { lex.consume(); break; }
            if (an.kind == TokenKind::Name) {
                lex.consume();
                GqlArg arg;
                arg.name = an.value;
                if (lex.peek().kind == TokenKind::Colon) {
                    lex.consume();
                    Token av = lex.consume();
                    arg.value = av.value;
                }
                f.args.push_back(arg);
            } else break;
        }
    }
    /* Children */
    if (lex.peek().kind == TokenKind::BraceL) {
        f.children = parse_selection_set(lex);
    }
    return f;
}

static GqlOp parse_operation(Lexer& lex) {
    GqlOp op;
    op.type = "query";
    Token t = lex.peek();
    if (t.kind == TokenKind::Name && (t.value == "query" || t.value == "mutation")) {
        lex.consume();
        op.type = t.value;
        Token t2 = lex.peek();
        if (t2.kind == TokenKind::Name) {
            lex.consume();
            op.name = t2.value;
        }
    }
    Token t3 = lex.peek();
    if (t3.kind == TokenKind::ParenL) {
        lex.consume();
        while (lex.peek().kind != TokenKind::ParenR && lex.peek().kind != TokenKind::Eof) lex.consume();
        if (lex.peek().kind == TokenKind::ParenR) lex.consume();
    }
    if (lex.peek().kind == TokenKind::BraceL) {
        op.fields = parse_selection_set(lex);
    }
    return op;
}

/* ── Resolve field type name (strip ! and []) ── */
static std::string base_type_name(const std::string& t) {
    std::string r;
    for (char c : t) {
        if (c != '!' && c != '[' && c != ']') r += c;
    }
    return r;
}

/* ── Execute a field against the schema ── */
static void execute_field(const GqlTypeDef* parent_type, const GqlField& qfield,
                          AuroraGQLSchema* schema, const char* parent_json,
                          JsonBuilder& out, std::string& errors) {
    /* Find field definition */
    const GqlFieldDef* field_def = nullptr;
    for (const auto& fd : parent_type->fields) {
        if (fd.name == qfield.name) { field_def = &fd; break; }
    }
    if (!field_def) {
        if (!errors.empty()) errors += ",";
        errors += "{\"message\":\"Cannot query field '" + qfield.name + "' on type '" + parent_type->name + "'\"}";
        out.key(qfield.name.c_str()); out.append("null");
        return;
    }

    /* Call resolver */
    std::string args_json = "{";
    for (size_t i = 0; i < qfield.args.size(); i++) {
        if (i > 0) args_json += ",";
        args_json += "\"" + JsonBuilder::escape(qfield.args[i].name) + "\":\"" + JsonBuilder::escape(qfield.args[i].value) + "\"";
    }
    args_json += "}";

    char* result = nullptr;
    if (field_def->resolver) {
        result = field_def->resolver(parent_json ? parent_json : "null", args_json.c_str(), field_def->ctx);
    }
    std::string result_str = result ? result : "null";
    if (result) free(result);

    /* If field has children and parent type is object, recurse */
    if (!qfield.children.empty()) {
        /* Find return type def */
        std::string bt = base_type_name(field_def->type_name);
        GqlTypeDef* ret_type = find_or_create_type(schema, bt.c_str());
        if (ret_type && ret_type->kind == GqlTypeKind::Object) {
            out.key(qfield.name.c_str());
            out.append("{");
            for (size_t i = 0; i < qfield.children.size(); i++) {
                if (i > 0) out.append(",");
                execute_field(ret_type, qfield.children[i], schema, result_str.c_str(), out, errors);
            }
            out.append("}");
            return;
        }
    }

    /* Emit resolver result directly */
    out.key(qfield.name.c_str());
    out.append(result_str);
}

/* ── Execute query ── */

AuroraGQLResult* aurora_gql_execute(AuroraGQLSchema* schema, const char* query,
                                    const char* variables_json) {
    AuroraGQLResult* res = new AuroraGQLResult();
    if (!res) return nullptr;
    res->has_err = false;

    if (!schema || !query) {
        res->errors = "[{\"message\":\"No schema or query provided\"}]";
        res->has_err = true;
        return res;
    }

    Lexer lex(query);
    GqlOp op = parse_operation(lex);

    /* Find root type */
    const char* root_name = (op.type == "mutation" && !schema->mutation_type_name.empty())
        ? schema->mutation_type_name.c_str() : schema->query_type_name.c_str();
    GqlTypeDef* root_type = find_or_create_type(schema, root_name);
    if (!root_type) {
        res->errors = "[{\"message\":\"Root type '" + std::string(root_name) + "' not defined\"}]";
        res->has_err = true;
        return res;
    }

    /* Build JSON result */
    JsonBuilder jb;
    jb.append("{\"data\":{");
    for (size_t i = 0; i < op.fields.size(); i++) {
        if (i > 0) jb.append(",");
        execute_field(root_type, op.fields[i], schema, nullptr, jb, res->errors);
    }
    jb.append("}}");

    if (!res->errors.empty()) {
        res->has_err = true;
        res->errors = "[" + res->errors + "]";
    }

    res->json = jb.buf;
    return res;
}

const char* aurora_gql_result_json(AuroraGQLResult* result) {
    return result ? result->json.c_str() : nullptr;
}

const char* aurora_gql_result_errors(AuroraGQLResult* result) {
    return result ? result->errors.c_str() : nullptr;
}

int aurora_gql_result_has_errors(AuroraGQLResult* result) {
    return result ? (result->has_err ? 1 : 0) : 0;
}

void aurora_gql_result_free(AuroraGQLResult* result) {
    delete result;
}

/* ── SDL parser (minimal) ── */

int aurora_gql_parse_sdl(AuroraGQLSchema* schema, const char* sdl) {
    if (!schema || !sdl) return 0;
    Lexer lex(sdl);
    int count = 0;
    while (true) {
        Token t = lex.peek();
        if (t.kind == TokenKind::Eof) break;
        if (t.kind != TokenKind::Name) { lex.consume(); continue; }
        std::string kw = t.value;
        lex.consume();
        if (kw == "type" || kw == "extend") {
            Token name = lex.consume();
            if (name.kind != TokenKind::Name) continue;
            aurora_gql_type_add_object(schema, name.value.c_str(), nullptr);
            if (kw == "type" && name.value == "Query") schema->query_type_name = "Query";
            if (kw == "type" && name.value == "Mutation") schema->mutation_type_name = "Mutation";
            /* Skip implements */
            if (lex.peek().kind == TokenKind::Name && lex.peek().value == "implements") {
                while (lex.peek().kind == TokenKind::Name || lex.peek().kind == TokenKind::Comma) lex.consume();
            }
            if (lex.peek().kind == TokenKind::BraceL) {
                lex.consume();
                while (true) {
                    Token ft = lex.peek();
                    if (ft.kind == TokenKind::BraceR) { lex.consume(); break; }
                    if (ft.kind == TokenKind::Eof) break;
                    if (ft.kind == TokenKind::Name) {
                        std::string fname = ft.value;
                        lex.consume();
                        if (lex.peek().kind == TokenKind::ParenL) {
                            int depth = 0;
                            while (true) {
                                Token at = lex.consume();
                                if (at.kind == TokenKind::ParenL) depth++;
                                if (at.kind == TokenKind::ParenR) { if (--depth <= 0) break; }
                                if (at.kind == TokenKind::Eof) break;
                            }
                        }
                        if (lex.peek().kind == TokenKind::Colon) {
                            lex.consume();
                            std::string ftype;
                            Token ftok = lex.consume();
                            ftype += ftok.value;
                            while (lex.peek().kind == TokenKind::Bang || lex.peek().kind == TokenKind::BraceR ||
                                   lex.peek().kind == TokenKind::BraceL) {
                                if (lex.peek().kind == TokenKind::Bang) { ftype += "!"; lex.consume(); break; }
                                break;
                            }
                            /* Handle [Type] syntax */
                            if (ftok.value == "[") {
                                Token inner = lex.consume();
                                ftype += inner.value;
                                if (lex.peek().kind == TokenKind::Bang) { ftype += "!"; lex.consume(); }
                                if (lex.peek().kind == TokenKind::BraceR) { ftype += "]"; lex.consume(); }
                                if (lex.peek().kind == TokenKind::Bang) { ftype += "!"; lex.consume(); }
                            }
                            aurora_gql_field_add(schema, name.value.c_str(), fname.c_str(),
                                                 ftype.c_str(), nullptr, nullptr, nullptr);
                        }
                    } else {
                        lex.consume();
                    }
                }
            }
            count++;
        } else if (kw == "enum") {
            Token name = lex.consume();
            if (name.kind != TokenKind::Name) continue;
            std::string values;
            if (lex.peek().kind == TokenKind::BraceL) {
                lex.consume();
                while (true) {
                    Token vt = lex.peek();
                    if (vt.kind == TokenKind::BraceR) { lex.consume(); break; }
                    if (vt.kind == TokenKind::Eof) break;
                    if (vt.kind == TokenKind::Name) {
                        if (!values.empty()) values += ",";
                        values += vt.value;
                        lex.consume();
                    } else {
                        lex.consume();
                    }
                }
            }
            aurora_gql_type_add_enum(schema, name.value.c_str(), values.empty() ? nullptr : values.c_str());
            count++;
        } else if (kw == "scalar") {
            Token name = lex.consume();
            if (name.kind == TokenKind::Name) {
                aurora_gql_type_add_scalar(schema, name.value.c_str());
                count++;
            }
        } else if (kw == "input") {
            Token name = lex.consume();
            if (name.kind == TokenKind::Name) {
                aurora_gql_type_add_input(schema, name.value.c_str());
                count++;
            }
        } else if (kw == "schema") {
            if (lex.peek().kind == TokenKind::BraceL) {
                lex.consume();
                while (true) {
                    Token d = lex.peek();
                    if (d.kind == TokenKind::BraceR) { lex.consume(); break; }
                    if (d.kind == TokenKind::Eof) break;
                    if (d.kind == TokenKind::Name) {
                        std::string dir = d.value;
                        lex.consume();
                        if (lex.peek().kind == TokenKind::Colon) {
                            lex.consume();
                            Token val = lex.consume();
                            if (dir == "query") schema->query_type_name = val.value;
                            if (dir == "mutation") schema->mutation_type_name = val.value;
                        }
                    } else lex.consume();
                }
            }
        } else kw = ""; /* skip */
    }
    return count > 0 ? 1 : 0;
}

/* ── Introspection ── */

char* aurora_gql_introspect(AuroraGQLSchema* schema) {
    if (!schema) return nullptr;
    JsonBuilder jb;
    jb.append("{\"__schema\":{\"queryType\":{\"name\":\"");
    jb.append(schema->query_type_name);
    jb.append("\"},\"mutationType\":");
    if (schema->mutation_type_name.empty()) {
        jb.append("null");
    } else {
        jb.append("{\"name\":\"");
        jb.append(schema->mutation_type_name);
        jb.append("\"}");
    }
    jb.append(",\"types\":[");
    bool first = true;
    for (auto& pair : schema->types) {
        auto& t = pair.second;
        if (!first) jb.append(",");
        first = false;
        jb.append("{\"kind\":\"");
        switch (t.kind) {
            case GqlTypeKind::Object: jb.append("OBJECT"); break;
            case GqlTypeKind::Scalar: jb.append("SCALAR"); break;
            case GqlTypeKind::Enum:   jb.append("ENUM"); break;
            case GqlTypeKind::InputObject: jb.append("INPUT_OBJECT"); break;
            default: jb.append("SCALAR"); break;
        }
        jb.append("\",\"name\":\"");
        jb.append(t.name);
        jb.append("\",\"description\":");
        jb.append(t.description.empty() ? "null" : ("\"" + JsonBuilder::escape(t.description) + "\""));
        jb.append(",\"fields\":");
        if (t.fields.empty()) {
            jb.append("null");
        } else {
            jb.append("[");
            for (size_t i = 0; i < t.fields.size(); i++) {
                if (i > 0) jb.append(",");
                jb.append("{\"name\":\"");
                jb.append(t.fields[i].name);
                jb.append("\",\"description\":");
                jb.append(t.fields[i].description.empty() ? "null" : ("\"" + JsonBuilder::escape(t.fields[i].description) + "\""));
                jb.append(",\"type\":{\"name\":\"");
                jb.append(t.fields[i].type_name);
                jb.append("\"},\"args\":[]}");
            }
            jb.append("]");
        }
        jb.append(",\"enumValues\":");
        if (t.enum_values.empty()) {
            jb.append("null");
        } else {
            jb.append("[");
            for (size_t i = 0; i < t.enum_values.size(); i++) {
                if (i > 0) jb.append(",");
                jb.append("{\"name\":\"");
                jb.append(t.enum_values[i]);
                jb.append("\"}");
            }
            jb.append("]");
        }
        jb.append("}");
    }
    jb.append("]}}");
    return strdup_c(jb.buf.c_str());
}

/* ── GraphQL endpoint helper ── */

void aurora_gql_handle_request(AuroraGQLSchema* schema, const char* body_json,
                               char* out_buffer, int out_size) {
    if (!out_buffer || out_size <= 0) return;
    out_buffer[0] = '\0';

    /* Parse body JSON to extract "query" field (simple search) */
    const char* q = body_json ? strstr(body_json, "\"query\"") : nullptr;
    if (!q) {
        snprintf(out_buffer, (size_t)out_size, "{\"errors\":[{\"message\":\"No query in request\"}]}");
        return;
    }
    q = strchr(q, ':');
    if (!q) {
        snprintf(out_buffer, (size_t)out_size, "{\"errors\":[{\"message\":\"Malformed request\"}]}");
        return;
    }
    q++;
    while (*q && (*q == ' ' || *q == '\t' || *q == '"')) q++;
    const char* qend = q;
    while (*qend && *qend != '"') {
        if (*qend == '\\' && qend[1]) qend++;
        qend++;
    }
    std::string query_str(q, (size_t)(qend - q));

    /* Extract variables (simplified) */
    const char* v = strstr(body_json, "\"variables\"");
    std::string vars = "{}";
    if (v) {
        v = strchr(v, ':');
        if (v) {
            v++;
            while (*v && (*v == ' ' || *v == '\t')) v++;
            const char* vend = v;
            int depth = 0;
            while (*vend) {
                if (*vend == '{') depth++;
                if (*vend == '}') { depth--; if (depth < 0) break; }
                vend++;
            }
            vars = std::string(v, (size_t)(vend - v + 1));
        }
    }

    AuroraGQLResult* res = aurora_gql_execute(schema, query_str.c_str(), vars.c_str());
    if (res) {
        const char* j = aurora_gql_result_json(res);
        if (j) {
            size_t len = strlen(j);
            if ((int)len < out_size - 1) {
                memcpy(out_buffer, j, len);
                out_buffer[len] = '\0';
            }
        }
        aurora_gql_result_free(res);
    }
}
