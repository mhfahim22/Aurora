#pragma once
#include <unordered_set>
#include <string>

/* ════════════════════════════════════════════════
   Aurora Language — Full Keyword Table
   ════════════════════════════════════════════════ */

inline const std::unordered_set<std::string>& aurora_keywords() {
    static const std::unordered_set<std::string> kw = {
        /* values */
        "true", "false", "null",

        /* control flow */
        "if", "elseif", "else",
        "while", "for", "loop", "repeat", "until",
        "break", "continue", "skip",
        "match", "case", "default", "switch",

        /* functions & classes */
        "function", "return", "lambda",
        "class", "new",

        /* logic */
        "and", "or", "not", "xor", "equals", "in",

        /* exception */
        "try", "catch", "finally", "throw", "ensure", "panic",

        /* async */
        "async", "await", "wait", "spawn", "parallel", "thread",
        "callback", "event", "signal", "emit",

        /* OOP */
        "private", "public", "protected",
        "static", "final", "abstract",
        "interface", "extends", "implements", "enum",

        /* memory */
        "delete", "copy", "move", "free",
        "drop", "borrow",                          /* Phase 2: RAII */
        "reference", "pointer", "shared", "weak",
        "constant", "mutable", "safe", "unsafe",

        /* modules */
        "import", "from", "alias", "global", "outer",
        "namespace", "module", "package", "extern",

        /* types */
        "type", "struct", "union",
        "list", "map", "set", "array", "tuple",
        "vector", "stack", "queue", "json",

        /* I/O */
        "output", "debug", "log",

        /* attributes */
        "performance", "inline", "noinline", "constexpr",

        /* misc */
        "typeof", "sizeof", "convert", "clone",
        "using", "yield", "pass", "end",

        /* UI/Frontend */
        "component", "state", "properties", "render",
        "style", "theme", "route", "page", "layout",
        "animate", "transition",

        /* backend */
        "server", "request", "response", "api",
        "middleware", "database", "query", "model",
        "cache", "session", "token", "auth",

        /* game */
        "scene", "entity", "object", "sprite", "camera",
        "physics", "collision", "audio", "animation",
        "input", "update", "tick",

        /* AI/ML */
        "ai", "train", "predict", "tensor", "neural",

        /* time/util */
        "sleep", "time", "random",
    };
    return kw;
}

inline bool is_aurora_keyword(const std::string& word) {
    return aurora_keywords().count(word) > 0;
}
