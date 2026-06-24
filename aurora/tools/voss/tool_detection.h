#pragma once
#include <string>

struct ToolInfo {
    std::string name;
    std::string path;
    bool found{false};
    std::string version;
    bool is_portable{false};
};

/* ── Tool detection ── */
ToolInfo detect_c_compiler();
ToolInfo detect_python();
ToolInfo detect_node();
ToolInfo detect_cargo();

/* ── Ensure tools (detect + auto-download portable if missing) ── */
ToolInfo ensure_c_compiler();
ToolInfo ensure_python();
ToolInfo ensure_node();

/* ── Utilities ── */
std::string tools_cache_dir();
bool download_and_extract(const std::string& url, const std::string& dest_dir,
                          const std::string& expected_exe);
