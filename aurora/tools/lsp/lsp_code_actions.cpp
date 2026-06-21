#include "../../include/tools/lsp.hpp"
#include <sstream>
#include <algorithm>

/* ════════════════════════════════════════════════════════════
   Code actions / quick fixes
   ════════════════════════════════════════════════════════════ */

std::vector<LspCodeAction> LspServer::get_code_actions(DocumentState& doc, const std::string& diagMsg) {
    std::vector<LspCodeAction> actions;

    if (diagMsg.empty()) {
        /* Quick fixes available even without diagnostics */
        LspCodeAction sortImport;
        sortImport.title = "Organize imports";
        sortImport.kind = "source.organizeImports";
        actions.push_back(sortImport);
        return actions;
    }

    /* ── Quick fix: missing 'end function' → add it ── */
    if (diagMsg.find("Missing 'end function'") != std::string::npos ||
        diagMsg.find("Missing end") != std::string::npos) {
        LspCodeAction action;
        action.title = "Add 'end function'";
        action.kind = "quickfix";
        action.diagnosticMessage = diagMsg;

        /* Find last non-empty line and detect indentation */
        std::istringstream stream(doc.text);
        std::string line;
        int last_line = 0;
        int total_lines = 0;
        std::string file_indent = "    ";
        while (std::getline(stream, line)) {
            if (!line.empty() && line.find_first_not_of(" \t\r") != std::string::npos) {
                last_line = total_lines;
                size_t indent_end = line.find_first_not_of(" \t");
                if (indent_end > 0) file_indent = line.substr(0, indent_end);
            }
            total_lines++;
        }

        LspTextEdit edit;
        edit.range.start = {(int)doc.lines.size(), 0};
        edit.range.end = {(int)doc.lines.size(), 0};
        edit.newText = "\n" + file_indent + "end function";
        action.edits.push_back(edit);
        actions.push_back(action);
    }

    /* ── Quick fix: unused variable → remove declaration ── */
    if (diagMsg.find("Unused variable") != std::string::npos) {
        LspCodeAction action;
        action.title = "Remove unused variable declaration";
        action.kind = "quickfix";
        action.diagnosticMessage = diagMsg;

        /* Parse variable name from message: "Unused variable 'foo'" */
        std::string vname;
        size_t q1 = diagMsg.find('\'');
        size_t q2 = (q1 != std::string::npos) ? diagMsg.find('\'', q1 + 1) : std::string::npos;
        if (q1 != std::string::npos && q2 != std::string::npos)
            vname = diagMsg.substr(q1 + 1, q2 - q1 - 1);

        /* Search AST for the variable declaration to get its line */
        if (!vname.empty() && doc.ast) {
            int decl_line = -1;
            walk_ast(doc.ast.get(), [&](ASTNode* n) {
                if (n->type == NodeType::Var && n->value == vname && decl_line < 0) {
                    decl_line = n->src_line - 1;
                }
            });
            if (decl_line >= 0) {
                LspTextEdit edit;
                edit.range.start = {decl_line, 0};
                edit.range.end = {decl_line + 1, 0};
                edit.newText = "";
                action.edits.push_back(edit);
            }
        }
        actions.push_back(action);
    }

    /* ── Generic quick fix: add comment (only when diagnostic present) ── */
    if (!diagMsg.empty()) {
        LspCodeAction suppress;
        suppress.title = "Suppress warning with comment";
        suppress.kind = "quickfix";
        suppress.diagnosticMessage = diagMsg;
        actions.push_back(suppress);
    }

    return actions;
}

/* ════════════════════════════════════════════════════════════
   Folding ranges from AST
   ════════════════════════════════════════════════════════════ */

std::vector<LspFoldingRange> LspServer::get_folding_ranges(DocumentState& doc) {
    std::vector<LspFoldingRange> ranges;
    const auto& lines = doc.lines;

    /* Use indent-based folding */
    std::vector<int> indent_stack;
    int fold_start = -1;
    int fold_indent = -1;

    for (int i = 0; i < (int)lines.size(); i++) {
        int indent = lines[i].indent;
        const auto& toks = lines[i].tokens;

        if (toks.empty() && indent == 0) continue;

        if (fold_start >= 0 && indent <= fold_indent) {
            if (i - 1 > fold_start) {
                LspFoldingRange r;
                r.startLine = fold_start;
                r.endLine = i - 1;
                r.kind = "region";
                ranges.push_back(r);
            }
            fold_start = -1;
        }

        /* Start folding on function/class/if/for/while/try keywords */
        if (!toks.empty()) {
            std::string first = toks[0].value;
            if (first == "function" || first == "class" || first == "struct" ||
                first == "enum" || first == "interface" || first == "if" ||
                first == "for" || first == "while" || first == "try" ||
                first == "match" || first == "namespace" || first == "module" ||
                first == "scene" || first == "component" || first == "server") {
                fold_start = i;
                fold_indent = indent;
            }
        }
    }

    /* Close last fold */
    if (fold_start >= 0 && (int)lines.size() - 1 > fold_start) {
        LspFoldingRange r;
        r.startLine = fold_start;
        r.endLine = (int)lines.size() - 1;
        r.kind = "region";
        ranges.push_back(r);
    }

    /* Add comment-based folding regions */
    bool in_comment_block = false;
    int comment_start = -1;
    for (int i = 0; i < (int)lines.size(); i++) {
        const auto& toks = lines[i].tokens;
        for (auto& t : toks) {
            if (t.value == "/*" && !t.is_string()) {
                in_comment_block = true;
                comment_start = i;
            }
            if (t.value == "*/" && in_comment_block) {
                if (i > comment_start) {
                    LspFoldingRange r;
                    r.startLine = comment_start;
                    r.endLine = i;
                    r.kind = "comment";
                    ranges.push_back(r);
                }
                in_comment_block = false;
                comment_start = -1;
            }
        }
    }

    return ranges;
}
