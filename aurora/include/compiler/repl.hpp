#pragma once
#include <string>
#include <vector>

/* ════════════════════════════════════════════════════════════
   Aurora REPL — interactive shell (main.cpp --repl)
   Phase 42.4: history, line editing, tab completion,
   :type / :doc / :load / :paste / :history commands.
   ════════════════════════════════════════════════════════════ */

struct ReplOptions {
    std::string source_path;
    bool strict_indent    { false };
    bool enable_debug     { false };
    bool enable_dap       { false };
    bool enable_coverage  { false };
};

/* Run the interactive REPL. Returns process exit code. */
int run_repl(const ReplOptions& opts);
