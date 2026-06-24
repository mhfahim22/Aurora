/* ════════════════════════════════════════════════════════════
   Documentation Generator for Aurora
   ════════════════════════════════════════════════════════════
   Parses `##` doc comments and generates HTML documentation.
   ════════════════════════════════════════════════════════════ */

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>

/* ── HTML escaping ── */
static std::string html_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

/* ── Parse doc comments from source ── */
struct DocEntry {
    std::string name;
    std::string kind;  /* function, struct, class, enum, variable, etc. */
    std::string signature;
    std::string doc_comment;
    int line;
};

static std::vector<DocEntry> parse_docs(const std::string& source) {
    std::vector<DocEntry> entries;
    std::istringstream stream(source);
    std::string line;
    std::string current_doc;
    int line_no = 0;

    while (std::getline(stream, line)) {
        line_no++;

        /* Collect doc comments (lines starting with ##) */
        size_t first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) {
            current_doc.clear();
            continue;
        }

        if (line.substr(first, 2) == "##") {
            if (!current_doc.empty()) current_doc += "\n";
            current_doc += line.substr(first + 2);
            /* strip leading space */
            if (!current_doc.empty() && current_doc[0] == ' ')
                current_doc = current_doc.substr(1);
            continue;
        }

        /* If we have a doc comment and this is a declaration, create entry */
        if (!current_doc.empty()) {
            /* Check for various declarations */
            std::string trimmed = line;
            trimmed.erase(0, trimmed.find_first_not_of(" \t\r"));
            trimmed.erase(trimmed.find_last_not_of(" \t\r") + 1);

            DocEntry entry;
            entry.line = line_no;
            entry.doc_comment = current_doc;

            if (trimmed.rfind("function ", 0) == 0) {
                entry.kind = "function";
                size_t start = 9;
                size_t end = trimmed.find("(");
                if (end != std::string::npos)
                    entry.name = trimmed.substr(start, end - start);
                else
                    entry.name = trimmed.substr(start);
                entry.name.erase(entry.name.find_last_not_of(" \t\r") + 1);
                entry.signature = html_escape(trimmed);
            } else if (trimmed.rfind("struct ", 0) == 0) {
                entry.kind = "struct";
                entry.name = trimmed.substr(7);
                entry.name.erase(entry.name.find_last_not_of(":\t\r ") + 1);
                if (!entry.name.empty() && entry.name.back() == ':')
                    entry.name.pop_back();
                entry.signature = html_escape(trimmed);
            } else if (trimmed.rfind("class ", 0) == 0) {
                entry.kind = "class";
                entry.name = trimmed.substr(6);
                entry.name.erase(entry.name.find_last_not_of(":\t\r ") + 1);
                if (!entry.name.empty() && entry.name.back() == ':')
                    entry.name.pop_back();
                entry.signature = html_escape(trimmed);
            } else if (trimmed.rfind("enum ", 0) == 0) {
                entry.kind = "enum";
                entry.name = trimmed.substr(5);
                entry.name.erase(entry.name.find_last_not_of(":\t\r ") + 1);
                if (!entry.name.empty() && entry.name.back() == ':')
                    entry.name.pop_back();
                entry.signature = html_escape(trimmed);
            } else if (trimmed.rfind("interface ", 0) == 0) {
                entry.kind = "interface";
                entry.name = trimmed.substr(10);
                entry.name.erase(entry.name.find_last_not_of(":\t\r ") + 1);
                if (!entry.name.empty() && entry.name.back() == ':')
                    entry.name.pop_back();
                entry.signature = html_escape(trimmed);
            } else if (trimmed.rfind("const ", 0) == 0 || trimmed.rfind("let ", 0) == 0 || trimmed.rfind("var ", 0) == 0) {
                entry.kind = "variable";
                entry.name = trimmed.substr(trimmed.find(' ') + 1);
                entry.name.erase(entry.name.find_last_not_of("=\t\r ") + 1);
                entry.signature = html_escape(trimmed);
            } else if (trimmed.rfind("extern ", 0) == 0) {
                entry.kind = "extern";
                entry.name = trimmed;
                /* Find function name */
                size_t fn_pos = trimmed.rfind("function ");
                if (fn_pos != std::string::npos) {
                    entry.name = trimmed.substr(fn_pos + 9);
                    size_t paren = entry.name.find("(");
                    if (paren != std::string::npos)
                        entry.name = entry.name.substr(0, paren);
                    entry.name.erase(entry.name.find_last_not_of(" \t\r") + 1);
                }
                entry.signature = html_escape(trimmed);
            }

            if (!entry.name.empty()) {
                entries.push_back(entry);
            }
            current_doc.clear();
        }
    }

    return entries;
}

/* ── Generate HTML documentation ── */
static std::string generate_html(const std::string& title, const std::vector<DocEntry>& entries) {
    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
    html << "<meta charset=\"UTF-8\">\n";
    html << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "<title>" << html_escape(title) << " - Aurora API Docs</title>\n";
    html << "<style>\n";
    html << "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;";
    html << "max-width:960px;margin:0 auto;padding:20px;background:#fafafa;color:#333;}\n";
    html << "h1{border-bottom:2px solid #6c5ce7;padding-bottom:10px;color:#2d3436;}\n";
    html << "h2{color:#6c5ce7;margin-top:30px;font-size:1.3em;}\n";
    html << ".entry{background:#fff;border:1px solid #e0e0e0;border-radius:8px;";
    html << "padding:16px;margin:12px 0;box-shadow:0 1px 3px rgba(0,0,0,0.08);}\n";
    html << ".entry.kind-function{border-left:4px solid #00b894;}\n";
    html << ".entry.kind-struct{border-left:4px solid #0984e3;}\n";
    html << ".entry.kind-class{border-left:4px solid #e17055;}\n";
    html << ".entry.kind-enum{border-left:4px solid #fdcb6e;}\n";
    html << ".entry.kind-interface{border-left:4px solid #a29bfe;}\n";
    html << ".entry.kind-extern{border-left:4px solid #636e72;}\n";
    html << ".entry.kind-variable{border-left:4px solid #55efc4;}\n";
    html << ".entry-name{font-size:1.1em;font-weight:600;color:#2d3436;}\n";
    html << ".entry-kind{display:inline-block;font-size:0.75em;padding:2px 8px;";
    html << "border-radius:4px;color:#fff;margin-left:8px;}\n";
    html << ".kind-function .entry-kind{background:#00b894;}\n";
    html << ".kind-struct .entry-kind{background:#0984e3;}\n";
    html << ".kind-class .entry-kind{background:#e17055;}\n";
    html << ".kind-enum .entry-kind{background:#fdcb6e;color:#333;}\n";
    html << ".kind-interface .entry-kind{background:#a29bfe;}\n";
    html << ".kind-extern .entry-kind{background:#636e72;}\n";
    html << ".kind-variable .entry-kind{background:#55efc4;color:#333;}\n";
    html << ".entry-signature{font-family:'JetBrains Mono','Fira Code',Consolas,monospace;";
    html << "font-size:0.9em;background:#f8f9fa;padding:8px 12px;border-radius:4px;";
    html << "margin:8px 0;overflow-x:auto;color:#636e72;}\n";
    html << ".entry-doc{color:#555;line-height:1.6;margin-top:8px;}\n";
    html << ".entry-line{font-size:0.8em;color:#b2bec3;}\n";
    html << ".nav{position:fixed;top:20px;right:20px;background:#fff;";
    html << "border:1px solid #e0e0e0;border-radius:8px;padding:12px;max-height:80vh;";
    html << "overflow-y:auto;width:200px;box-shadow:0 2px 8px rgba(0,0,0,0.1);}\n";
    html << ".nav a{display:block;color:#6c5ce7;text-decoration:none;";
    html << "padding:3px 0;font-size:0.9em;}\n";
    html << ".nav a:hover{color:#2d3436;}\n";
    html << ".nav-title{font-weight:600;margin-bottom:8px;color:#2d3436;}\n";
    html << "@media(max-width:768px){.nav{display:none;}}\n";
    html << "</style>\n</head>\n<body>\n";

    html << "<h1>" << html_escape(title) << " — Aurora API</h1>\n";

    /* Sidebar nav */
    html << "<div class=\"nav\"><div class=\"nav-title\">Contents</div>\n";
    for (const auto& entry : entries) {
        html << "<a href=\"#" << entry.name << "\">"
             << html_escape(entry.name) << "</a>\n";
    }
    html << "</div>\n";

    /* Count by kind */
    int fn_count = 0, struct_count = 0, class_count = 0, other_count = 0;
    for (const auto& e : entries) {
        if (e.kind == "function") fn_count++;
        else if (e.kind == "struct") struct_count++;
        else if (e.kind == "class") class_count++;
        else other_count++;
    }
    html << "<p>"
         << fn_count << " functions, "
         << struct_count << " structs, "
         << class_count << " classes"
         << (other_count ? ", " + std::to_string(other_count) + " other" : "")
         << "</p>\n";

    /* Entries grouped by kind */
    std::vector<std::string> kind_order = {"function", "struct", "class", "enum", "interface", "extern", "variable"};

    for (const auto& kind : kind_order) {
        bool has_kind = false;
        for (const auto& e : entries)
            if (e.kind == kind) { has_kind = true; break; }
        if (!has_kind) continue;

        std::string kind_title = kind;
        kind_title[0] = toupper(kind_title[0]);
        kind_title += "s";
        html << "<h2>" << kind_title << "</h2>\n";

        for (const auto& entry : entries) {
            if (entry.kind != kind) continue;
            html << "<div class=\"entry kind-" << entry.kind << "\" id=\"" << entry.name << "\">\n";
            html << "<div><span class=\"entry-name\">" << html_escape(entry.name) << "</span>";
            html << "<span class=\"entry-kind\">" << entry.kind << "</span></div>\n";
            html << "<div class=\"entry-signature\">" << entry.signature << "</div>\n";
            html << "<div class=\"entry-doc\">";
            /* Parse markdown-like formatting */
            std::string doc = html_escape(entry.doc_comment);
            /* Convert newlines to <br> */
            size_t pos = 0;
            while ((pos = doc.find('\n', pos)) != std::string::npos) {
                doc.replace(pos, 1, "<br>");
                pos += 4;
            }
            html << doc;
            html << "</div>\n";
            html << "<div class=\"entry-line\">line " << entry.line << "</div>\n";
            html << "</div>\n";
        }
    }

    html << "</body>\n</html>\n";
    return html.str();
}

/* ── Main entry: generate docs from .aura source ── */
int run_doc_generator(const std::string& source_path, const std::string& output_path) {
    std::ifstream f(source_path);
    if (!f.is_open()) {
        std::cerr << "doc: cannot open file: " << source_path << "\n";
        return 1;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string source = ss.str();

    auto entries = parse_docs(source);

    if (entries.empty()) {
        std::cout << "doc: no documentation comments found in " << source_path << "\n";
        return 0;
    }

    std::filesystem::path p(source_path);
    std::string title = p.stem().string();
    std::string html = generate_html(title, entries);

    std::string out = output_path.empty() ? (title + "_docs.html") : output_path;
    std::ofstream of(out);
    if (!of.is_open()) {
        std::cerr << "doc: cannot write output: " << out << "\n";
        return 1;
    }
    of << html;
    of.close();

    std::cout << "doc: generated " << entries.size() << " entries → " << out << "\n";
    return 0;
}
