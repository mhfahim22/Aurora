#include "../../include/tools/lsp.hpp"
#include <sstream>
#include <set>

/* ════════════════════════════════════════════════════════════
   Document formatting — uses lexer indent info + AST structure
   ════════════════════════════════════════════════════════════ */

std::vector<LspTextEdit> LspServer::get_formatting_edits(DocumentState& doc) {
    std::vector<LspTextEdit> edits;
    const auto& lines = doc.lines;

    /* Use doc.lines directly for indentation analysis */
    int indent_level = 0;

    /* Track keywords that increase indent */
    std::set<std::string> indent_keywords = {
        "function", "class", "struct", "enum", "interface",
        "if", "elseif", "else", "while", "for", "loop", "repeat",
        "try", "catch", "finally", "match", "switch",
        "namespace", "module", "async", "spawn", "parallel",
        "scene", "entity", "component", "server", "api",
        "ai", "train", "predict", "tensor", "neural",
        "unsafe", "safe",
    };

    std::set<std::string> dedent_keywords = {
        "elseif", "else", "catch", "finally", "case", "default",
    };

    for (int line_num = 0; line_num < (int)lines.size(); line_num++) {
        const auto& ll = lines[line_num];
        const auto& toks = ll.tokens;
        int raw_indent = ll.indent;

        if (toks.empty()) continue;

        /* Get first token value */
        std::string first_word = toks[0].value;

        /* Process dedent BEFORE indent (handle else/elseif/catch/finally correctly) */
        if (dedent_keywords.count(first_word) && indent_level > 0) {
            indent_level--;
        }

        int actual_indent = raw_indent;
        int target_indent = indent_level * 4;

        if (actual_indent != target_indent) {
            LspTextEdit edit;
            edit.range.start = {line_num, 0};
            edit.range.end = {line_num, actual_indent};
            edit.newText = std::string(target_indent, ' ');
            edits.push_back(edit);
        }

        /* Increase indent if line starts with an indent keyword */
        if (indent_keywords.count(first_word)) {
            indent_level++;
        }

        /* Check for end keyword */
        if (first_word == "end") {
            if (indent_level > 0) indent_level--;
        }
    }

    return edits;
}
