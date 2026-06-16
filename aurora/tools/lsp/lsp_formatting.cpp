#include "../../include/tools/lsp.hpp"
#include <sstream>
#include <set>

/* ════════════════════════════════════════════════════════════
   Document formatting — uses lexer indent info + AST structure
   ════════════════════════════════════════════════════════════ */

std::vector<LspTextEdit> LspServer::get_formatting_edits(DocumentState& doc) {
    std::vector<LspTextEdit> edits;
    const auto& lines = doc.lines;

    /* Check each line's indentation */
    std::istringstream stream(doc.text);
    std::string raw_line;
    int line_num = 0;
    int expected_indent = 0;
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

    while (std::getline(stream, raw_line)) {
        /* Skip CR */
        if (!raw_line.empty() && raw_line.back() == '\r') raw_line.pop_back();

        std::string trimmed = raw_line;
        size_t first_non_space = trimmed.find_first_not_of(" \t");
        if (first_non_space == std::string::npos) {
            /* Empty line or whitespace-only — keep as is */
            line_num++;
            continue;
        }

        /* Check if line starts with a dedent keyword */
        std::string first_word;
        {
            std::istringstream ws(trimmed.substr(first_non_space));
            ws >> first_word;
        }
        if (dedent_keywords.count(first_word) && indent_level > 0) {
            indent_level--;
        }

        int actual_indent = (int)first_non_space;
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

        line_num++;
    }

    return edits;
}
