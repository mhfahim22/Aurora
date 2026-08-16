/* ═══════════════════════════════════════════════════════════════
   auroradoc — Aurora package documentation generator (Phase 41.4)
   ═══════════════════════════════════════════════════════════════
   Parses .auf/.aura source files, extracts `##` doc-comment blocks
   and `extern function` / `function` / `struct` / `enum` signatures,
   and generates per-package HTML documentation with a searchable
   index — similar to rustdoc.

   Usage:
     auroradoc [--pkg <name>@<version>] [--desc <text>] [--out <dir>] [<src files or dirs>...]
   ───────────────────────────────────────────────────────────── */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static std::string g_pkg_name = "aurora";
static std::string g_pkg_version = "0.0.0";
static std::string g_pkg_desc = "";
static std::string g_out_dir = "docs";

/* ── Escape HTML special characters ── */
static std::string html_escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break;
            default: r += c;
        }
    }
    return r;
}

/* ── Extract a doc-comment (## ...) line ── */
static std::string extract_doc_comment(const std::string& line) {
    std::string s = line;
    /* trim leading whitespace */
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    s = s.substr(start);
    if (s.rfind("##", 0) != 0) return "";
    s = s.substr(2);
    if (!s.empty() && s[0] == ' ') s = s.substr(1);
    return s;
}

/* ── Signature line candidates ── */
static bool is_signature_line(const std::string& line) {
    std::string s = line;
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return false;
    s = s.substr(start);
    return s.rfind("function", 0) == 0 || s.rfind("extern function", 0) == 0 ||
           s.rfind("struct ", 0) == 0 || s.rfind("enum ", 0) == 0 ||
           s.rfind("type ", 0) == 0 || s.rfind("class ", 0) == 0 ||
           s.rfind("interface ", 0) == 0;
}

/* ── One documented item ── */
struct DocItem {
    std::string signature;
    std::string doc;
    std::string file;
    bool is_extern = false;
};

static std::vector<DocItem> g_items;

/* ── Parse a source file ── */
static void parse_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;
    std::string rel = fs::relative(path, fs::current_path()).string();
    if (rel.empty()) rel = path;

    std::string line;
    std::string pending_doc;
    bool in_doc_block = false;
    int line_no = 0;
    while (std::getline(f, line)) {
        line_no++;
        std::string comment = extract_doc_comment(line);
        if (!comment.empty()) {
            if (!in_doc_block) { pending_doc = ""; in_doc_block = true; }
            if (!pending_doc.empty()) pending_doc += " ";
            pending_doc += comment;
            continue;
        }
        if (is_signature_line(line)) {
            std::string sig = line;
            size_t s0 = sig.find_first_not_of(" \t");
            if (s0 != std::string::npos) sig = sig.substr(s0);
            DocItem it;
            it.signature = sig;
            it.doc = pending_doc;
            it.file = rel;
            it.is_extern = sig.rfind("extern", 0) == 0;
            g_items.push_back(it);
            pending_doc = "";
            in_doc_block = false;
            continue;
        }
        if (in_doc_block) {
            /* Doc block ended without hitting a signature — drop it */
            pending_doc = "";
            in_doc_block = false;
        }
    }
}

/* ── Group items by file ── */
static std::map<std::string, std::vector<DocItem*>> group_by_file() {
    std::map<std::string, std::vector<DocItem*>> m;
    for (auto& it : g_items) m[it.file].push_back(&it);
    return m;
}

static const char* PAGE_CSS =
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
    "max-width:1000px;margin:0 auto;padding:2em;background:#fafafa;color:#333;line-height:1.5}"
    "h1{border-bottom:2px solid #4A90D9;padding-bottom:.3em}"
    "h2{color:#4A90D9;margin-top:2em}"
    ".item{background:#fff;border:1px solid #e0e0e0;border-radius:6px;padding:.8em 1em;margin:.6em 0}"
    ".sig{font-family:'JetBrains Mono','Fira Code',monospace;font-size:.92em;color:#1a1a2e;"
    "background:#f0f4f8;padding:.3em .6em;border-radius:4px;display:inline-block}"
    ".extern{background:#fff3cd;border-color:#f0e0a0}"
    ".doc{margin-top:.5em;color:#555}"
    ".doc p{margin:.2em 0}"
    ".file{margin-top:.4em;font-size:.8em;color:#999;font-family:monospace}"
    "nav{position:fixed;top:0;left:0;bottom:0;width:230px;overflow-y:auto;background:#fff;"
    "border-right:1px solid #ddd;padding:1em}"
    "nav a{display:block;padding:.25em 0;color:#4A90D9;text-decoration:none}"
    "nav a:hover{text-decoration:underline}"
    "main{margin-left:250px}"
    "#search{width:100%;padding:.5em;margin-bottom:1em;border:1px solid #ccc;border-radius:4px}"
    ".badge{font-size:.75em;background:#4A90D9;color:#fff;border-radius:10px;padding:.1em .5em;margin-left:.5em}";

int main(int argc, char* argv[]) {
    std::vector<std::string> inputs;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pkg") == 0 && i + 1 < argc) { g_pkg_name = argv[++i]; }
        else if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) { g_pkg_version = argv[++i]; }
        else if (strcmp(argv[i], "--desc") == 0 && i + 1 < argc) { g_pkg_desc = argv[++i]; }
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) { g_out_dir = argv[++i]; }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            std::cout << "auroradoc — Aurora package documentation generator\n\n"
                      << "Usage: auroradoc [options] <file|dir> [<file|dir> ...]\n\n"
                      << "  --pkg <name>        package name (default: aurora)\n"
                      << "  --version <ver>     package version (default: 0.0.0)\n"
                      << "  --desc <text>       package description\n"
                      << "  --out <dir>         output directory (default: docs)\n";
            return 0;
        } else {
            inputs.push_back(argv[i]);
        }
    }
    if (inputs.empty()) inputs.push_back(".");

    /* Collect source files */
    std::vector<std::string> files;
    for (auto& in : inputs) {
        if (fs::is_directory(in)) {
            for (auto& e : fs::recursive_directory_iterator(in)) {
                if (e.is_regular_file()) {
                    std::string ext = e.path().extension().string();
                    if (ext == ".auf" || ext == ".aura") files.push_back(e.path().string());
                }
            }
        } else if (fs::exists(in)) {
            files.push_back(in);
        }
    }
    std::sort(files.begin(), files.end());

    for (auto& f : files) parse_file(f);

    fs::create_directories(g_out_dir);

    /* ── index.html ── */
    std::ofstream idx(g_out_dir + "/index.html");
    idx << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"UTF-8\">\n"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        << "<title>" << html_escape(g_pkg_name) << " " << html_escape(g_pkg_version) << " — API Reference</title>\n"
        << "<style>" << PAGE_CSS << "</style>\n</head>\n<body>\n";

    idx << "<nav><h3>API Reference</h3>\n";
    idx << "<input id=\"search\" placeholder=\"Search functions...\" oninput=\"filterItems(this.value)\">\n";
    idx << "<h4>Files</h4>\n";
    auto groups = group_by_file();
    for (auto& [file, _items] : groups) {
        idx << "<a href=\"#" << html_escape(file) << "\">" << html_escape(file) << "</a>\n";
    }
    idx << "</nav>\n<main>\n";

    idx << "<h1>" << html_escape(g_pkg_name) << " <span class=\"badge\">" << html_escape(g_pkg_version) << "</span></h1>\n";
    if (!g_pkg_desc.empty()) idx << "<p>" << html_escape(g_pkg_desc) << "</p>\n";
    idx << "<p>Generated from " << files.size() << " source file(s), "
        << g_items.size() << " documented item(s).</p>\n";

    size_t global_idx = 0;
    for (auto& [file, items] : groups) {
        idx << "<h2 id=\"" << html_escape(file) << "\">" << html_escape(file) << "</h2>\n";
        for (auto* it : items) {
            idx << "<div class=\"item" << (it->is_extern ? " extern" : "") << "\" id=\"item-" << global_idx++ << "\">\n";
            idx << "<span class=\"sig\">" << html_escape(it->signature) << "</span>";
            if (it->is_extern) idx << " <span class=\"badge\" style=\"background:#B8860B\">extern</span>";
            idx << "\n";
            if (!it->doc.empty()) {
                idx << "<div class=\"doc\">";
                std::stringstream ss(it->doc);
                std::string seg;
                while (std::getline(ss, seg, '#')) {
                    std::string t = seg;
                    size_t b = t.find_first_not_of(" ");
                    if (b != std::string::npos) t = t.substr(b);
                    if (!t.empty()) idx << "<p>" << html_escape(t) << "</p>";
                }
                idx << "</div>\n";
            }
            idx << "<div class=\"file\">" << html_escape(file) << "</div>\n";
            idx << "</div>\n";
        }
    }

    idx << "<script>\n"
        << "function filterItems(q){q=q.toLowerCase();var items=document.querySelectorAll('.item');"
        << "var sigs=document.querySelectorAll('.sig');for(var i=0;i<items.length;i++){"
        << "var s=sigs[i]?sigs[i].textContent.toLowerCase():'';items[i].style.display=s.indexOf(q)>=0?'':'none';}}\n"
        << "</script>\n";
    idx << "</main>\n</body>\n</html>\n";
    idx.close();

    std::cout << "auroradoc: generated " << g_items.size() << " doc items for "
              << g_pkg_name << "@" << g_pkg_version << " → " << g_out_dir << "/index.html\n";
    return 0;
}