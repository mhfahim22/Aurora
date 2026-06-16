#include "voss.h"

/* ── Template definitions ── */
struct TemplateSpec {
    std::string name;
    std::string description;
};

static const TemplateSpec TEMPLATES[] = {
    {"web-api",      "HTTP web API server with router, middleware, JSON responses"},
    {"library",      "Reusable library with exported functions ready for publishing"},
    {"desktop-app",  "Desktop GUI application (Win32 skeleton with event loop)"},
};

static void copy_template_file(const std::string& src, const std::string& dst) {
    fs::create_directories(fs::path(dst).parent_path());
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary);
    out << in.rdbuf();
}

static int cmd_new_template(const std::string& name, const std::string& tpl_name) {
    std::string dir = name;
    if (fs::exists(dir + "/aurora.pkg")) {
        std::cerr << "error: " << dir << "/aurora.pkg already exists\n"; return 1;
    }
    fs::create_directories(dir);

    std::string tpl_dir = (fs::path("templates") / tpl_name).string();
    if (!fs::exists(tpl_dir)) {
        /* Fallback: builtin templates are in the voss source directory */
        tpl_dir = (fs::path("aurora/tools/voss/templates") / tpl_name).string();
    }

    if (tpl_name == "web-api") {
        /* aurora.pkg */
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: Web API server\n"
          << "entry: main.aura\n"
          << "dependencies:\n  - http\npermissions:\n  - network\n";
        /* main.aura */
        std::ofstream mf(dir + "/main.aura");
        mf << "import \"http\"\n\n"
           << "server myapp on port 8080:\n"
           << "    route \"/\", method:\"GET\":\n"
           << "        return status 200\n"
           << "        return json {\"message\": \"Hello from " << name << "\"}\n"
           << "    end\n"
           << "    route \"/api/health\", method:\"GET\":\n"
           << "        return status 200\n"
           << "        return json {\"status\": \"ok\", \"service\": \"" << name << "\"}\n"
           << "    end\n"
           << "    route \"/api/echo\", method:\"POST\":\n"
           << "        # body contains request payload\n"
           << "        return status 200\n"
           << "        return json body\n"
           << "    end\n"
           << "end\n";
        /* .gitignore */
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n";
        /* CI */
        fs::create_directories(dir + "/.github/workflows");
        std::ofstream cf(dir + "/.github/workflows/build.yml");
        cf << "name: build\non: [push, pull_request]\njobs:\n"
           << "  build:\n    runs-on: ubuntu-latest\n    steps:\n"
           << "      - uses: actions/checkout@v4\n"
           << "      - uses: aurora-lang/setup-aurora@v1\n"
           << "      - run: voss build\n"
           << "      - run: voss test\n";
    } else if (tpl_name == "library") {
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: " << name << " library\n"
          << "entry: main.aura\n"
          << "dependencies:\npermissions:\n";
        std::ofstream mf(dir + "/main.aura");
        mf << "/* " << name << " — public API */\n\n"
           << "function add(a, b):\n    return a + b\nend\n\n"
           << "function greet(name):\n    return \"Hello, \" + name + \"!\"\nend\n\n"
           << "function fib(n):\n    if n <= 1:\n        return n\n    end\n"
           << "    return fib(n - 1) + fib(n - 2)\nend\n";
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n";
        /* Test file */
        fs::create_directories(dir + "/tests");
        std::ofstream tf(dir + "/tests/test_main.aura");
        tf << "import \"test\"\n\n"
           << "import \"" << name << "\"\n\n"
           << "test_begin(\"add should sum two numbers\")\n"
           << "assert_equal(add(2, 3), 5)\n"
           << "test_pass()\n\n"
           << "test_begin(\"greet should return greeting string\")\n"
           << "assert_equal(greet(\"World\"), \"Hello, World!\")\n"
           << "test_pass()\n\n"
           << "test_begin(\"fib should compute fibonacci\")\n"
           << "assert_equal(fib(0), 0)\n"
           << "assert_equal(fib(1), 1)\n"
           << "assert_equal(fib(10), 55)\n"
           << "test_pass()\n\n"
           << "test_report()\n";
    } else if (tpl_name == "desktop-app") {
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: Desktop GUI application\n"
          << "entry: main.aura\n"
          << "dependencies:\npermissions:\n  - ui\n";
        std::ofstream mf(dir + "/main.aura");
        mf << "## " << name << " — Desktop GUI Application\n\n"
           << "window w with title \"" << name << "\", width: 800, height: 600:\n"
           << "    label \"Welcome to " << name << "\" at 10, 10, width: 780, height: 30\n\n"
           << "    button \"Click Me\" at 10, 50, width: 120, height: 30:\n"
           << "        output(\"Button clicked!\")\n"
           << "    end\n\n"
           << "    textbox \"\" at 10, 100, width: 300, height: 25:\n"
           << "        output(\"Text changed: \" + value)\n"
           << "    end\n"
           << "end\n";
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n";
    } else {
        std::cerr << "error: unknown template '" << tpl_name << "'\n";
        return 1;
    }

    std::cout << "created new " << tpl_name << " project '" << name << "'\n";
    return 0;
}

int cmd_new(int argc, char** argv) {
    std::string tpl = "web-api";
    std::string name;

    /* Parse: voss new <name> or voss new <name> --template <type> */
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--template" || a == "-t") {
            if (i + 1 < argc) tpl = argv[++i];
        } else {
            name = a;
        }
    }

    if (name.empty()) {
        std::cerr << "usage: voss new <name> [--template <type>]\n"
                  << "templates:\n";
        for (auto& t : TEMPLATES)
            std::cerr << "  " << t.name << ": " << t.description << "\n";
        return 1;
    }

    return cmd_new_template(name, tpl);
}

/* ── 6.2: Doc Generation ── */
static std::string extract_doc_comment(const std::string& line) {
    /* Strip leading whitespace, ##, and any leading whitespace after ## */
    std::string s = trim(line);
    if (s.find("##") == 0) {
        s = s.substr(2);
        if (!s.empty() && s[0] == ' ')
            s = s.substr(1);
        return s;
    }
    return "";
}

static std::string extract_func_sig(const std::string& line) {
    std::string s = trim(line);
    /* Match lines starting with 'function' or 'extern function' */
    size_t pos = s.find("function");
    if (pos == std::string::npos) return "";
    /* Grab from 'function' to end or ':' */
    return s.substr(pos);
}

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

static void gen_doc_page(const std::string& src_file, std::ostream& html, const std::string& rel_path) {
    std::ifstream f(src_file);
    if (!f) return;

    std::string line;
    std::string doc_comment;
    bool in_doc_block = false;

    html << "<div class=\"file-section\">\n"
         << "<h2 id=\"" << html_escape(rel_path) << "\">" << html_escape(rel_path) << "</h2>\n";

    while (std::getline(f, line)) {
        std::string comment = extract_doc_comment(line);
        if (!comment.empty()) {
            /* Doc comment line */
            if (!in_doc_block) {
                html << "<div class=\"doc-block\">\n";
                in_doc_block = true;
            }
            html << "<p>" << html_escape(comment) << "</p>\n";
            continue;
        }

        if (in_doc_block) {
            /* Check if next line is a function */
            std::string sig = extract_func_sig(line);
            if (!sig.empty()) {
                html << "</div>\n";
                html << "<div class=\"function\">\n";
                html << "<code>" << html_escape(sig) << "</code>\n";
                html << "</div>\n";
                in_doc_block = false;
                doc_comment.clear();
                continue;
            }
            /* Non-comment, non-function — close block */
            html << "</div>\n";
            in_doc_block = false;
        }
    }
    if (in_doc_block) html << "</div>\n";
    html << "</div>\n";
}

int cmd_doc(const std::string& output_dir, bool serve) {
    std::vector<std::string> src_files;
    std::string search_dir = ".";

    /* Find all .aura files */
    for (auto& entry : fs::recursive_directory_iterator(search_dir)) {
        if (entry.path().extension() == ".aura") {
            src_files.push_back(entry.path().string());
        }
    }
    /* Also scan libc/ and packages/ */
    for (auto& extra : {"libc", "packages"}) {
        if (fs::exists(extra)) {
            for (auto& entry : fs::recursive_directory_iterator(extra)) {
                if (entry.path().extension() == ".aura") {
                    src_files.push_back(entry.path().string());
                }
            }
        }
    }

    if (src_files.empty()) {
        std::cerr << "warning: no .aura files found\n";
    }

    std::string out = output_dir.empty() ? ".voss/docs" : output_dir;
    fs::create_directories(out);

    /* Generate index.html */
    std::ofstream idx(out + "/index.html");
    idx << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
        << "<meta charset=\"UTF-8\">\n"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        << "<title>Aurora Documentation</title>\n"
        << "<style>\n"
        << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
        << "max-width: 960px; margin: 0 auto; padding: 2em; background: #fafafa; color: #333; }\n"
        << "h1 { border-bottom: 2px solid #4A90D9; padding-bottom: 0.3em; }\n"
        << "h2 { color: #4A90D9; margin-top: 2em; }\n"
        << ".file-section { margin: 1em 0; padding: 1em; background: #fff; border-radius: 6px; "
        << "box-shadow: 0 1px 3px rgba(0,0,0,0.1); }\n"
        << ".doc-block { border-left: 3px solid #4A90D9; padding-left: 1em; margin: 0.5em 0; color: #555; }\n"
        << ".doc-block p { margin: 0.2em 0; }\n"
        << ".function { background: #f0f4f8; padding: 0.5em 1em; border-radius: 4px; "
        << "font-family: 'JetBrains Mono', 'Fira Code', monospace; font-size: 0.9em; overflow-x: auto; }\n"
        << "nav { position: fixed; top: 0; left: 0; bottom: 0; width: 220px; overflow-y: auto; "
        << "background: #fff; border-right: 1px solid #ddd; padding: 1em; }\n"
        << "nav a { display: block; padding: 0.3em 0; color: #4A90D9; text-decoration: none; }\n"
        << "nav a:hover { text-decoration: underline; }\n"
        << "main { margin-left: 240px; }\n"
        << "</style>\n</head>\n<body>\n";
    idx << "<nav><h3>Files</h3>\n";
    for (auto& sf : src_files) {
        std::string rel = fs::relative(sf, ".").string();
        idx << "<a href=\"#" << html_escape(rel) << "\">" << html_escape(rel) << "</a>\n";
    }
    idx << "</nav>\n<main>\n";
    idx << "<h1>Aurora Documentation</h1>\n"
        << "<p>Generated from " << src_files.size() << " source files.</p>\n";

    for (auto& sf : src_files) {
        std::string rel = fs::relative(sf, ".").string();
        gen_doc_page(sf, idx, rel);
    }

    idx << "</main>\n</body>\n</html>\n";
    idx.close();

    std::cout << "generated docs: " << out << "/index.html (" << src_files.size() << " files)\n";

    if (serve) {
        /* Simple HTTP server using aurora HTTP server or a basic C++ socket server */
        std::cout << "serving docs at http://localhost:8080\n";
        std::cout << "(press Ctrl+C to stop)\n";

#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        int sock = (int)socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { std::cerr << "error: socket\n"; return 1; }
        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8080);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "error: bind\n"; return 1;
        }
        listen(sock, 5);

        while (true) {
            struct sockaddr_in cli;
#ifdef _WIN32
            int cli_len = sizeof(cli);
#else
            socklen_t cli_len = sizeof(cli);
#endif
            int cs = (int)accept(sock, (struct sockaddr*)&cli, &cli_len);
            if (cs < 0) break;

            char buf[4096];
            int n = (int)recv(cs, buf, sizeof(buf) - 1, 0);
            if (n > 0) {
                buf[n] = 0;
                /* Parse GET path */
                std::string req(buf);
                size_t gpos = req.find("GET ");
                size_t hpos = req.find(" HTTP/");
                std::string path = "/";
                if (gpos != std::string::npos && hpos != std::string::npos) {
                    path = req.substr(gpos + 4, hpos - gpos - 4);
                }
                if (path == "/") path = "/index.html";

                /* Serve file from output directory */
                std::string filepath = out + path;
                std::string content;
                std::string mime = "text/plain";
                if (path.find(".html") != std::string::npos) mime = "text/html";
                else if (path.find(".css") != std::string::npos) mime = "text/css";
                else if (path.find(".js") != std::string::npos) mime = "application/javascript";

                std::ifstream rf(filepath, std::ios::binary);
                if (rf) {
                    std::stringstream ss;
                    ss << rf.rdbuf();
                    content = ss.str();
                } else {
                    content = "<h1>404 Not Found</h1><p>" + html_escape(filepath) + "</p>";
                }

                std::stringstream res;
                res << "HTTP/1.1 " << (rf ? "200 OK" : "404 Not Found") << "\r\n"
                    << "Content-Type: " << mime << "\r\n"
                    << "Content-Length: " << content.size() << "\r\n"
                    << "Connection: close\r\n\r\n"
                    << content;
                std::string rs = res.str();
                send(cs, rs.c_str(), (int)rs.size(), 0);
            }
#ifdef _WIN32
            closesocket(cs);
#else
            close(cs);
#endif
        }
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
    }

    return 0;
}
