#include "voss.h"

/* ── Template definitions ── */
struct TemplateSpec {
    std::string name;
    std::string description;
};

static const TemplateSpec TEMPLATES[] = {
    {"web-api",        "HTTP web API server with router, middleware, JSON responses"},
    {"library",        "Reusable library with exported functions ready for publishing"},
    {"desktop-app",    "Desktop GUI application (Win32 skeleton with event loop)"},
    {"mobile-app",     "Mobile app for Android and iOS with touch input and screen navigation"},
    {"cross-app",      "Cross-platform app targeting all 5 platforms (Win/Lin/Mac/Android/iOS)"},
    {"chat-app",       "Real-time chat application with WebSocket, rooms, user list"},
    {"social-app",     "Social media app with profiles, posts, feed, likes, comments"},
    {"ecommerce-app",  "Online store with products, cart, checkout, orders"},
    {"dashboard-app",  "Admin dashboard with charts, tables, metrics, sidebar navigation"},
    {"game-app",       "2D game with canvas rendering, sprites, input, game loop"},
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
        mf << "## " << name << " -- Desktop GUI Application\n\n"
           << "import \"app\"\n\n"
           << "function main()\n"
           << "    win = app_init(\"" << name << "\", 800, 600)\n"
           << "    lbl = app_label(win, \"Welcome to " << name << "\", 10, 10, 780, 30)\n"
           << "    btn = app_button(win, \"Click Me\", 10, 50, 120, 30)\n"
           << "    app_on_click(btn, lambda() output(\"Button clicked!\") end)\n"
           << "    txt = app_textbox(win, \"\", 10, 100, 300, 25)\n"
           << "    app_on_change(txt, lambda() output(\"Text: \" + app_get_text(txt)) end)\n"
           << "    app_run(win)\n"
           << "end\n\n"
           << "main()\n";
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n";
        /* Build script */
        std::ofstream bf(dir + "/build.sh");
        bf << "#!/bin/bash\n"
           << "# Build " << name << " for desktop\n"
           << "aurorac main.aura -o " << name << " --run\n";
    } else if (tpl_name == "mobile-app") {
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: Mobile application\n"
          << "entry: main.aura\n"
          << "dependencies:\npermissions:\n  - ui\n  - network\n";
        std::ofstream mf(dir + "/main.aura");
        mf << "## " << name << " -- Mobile Application\n\n"
           << "import \"app\"\n\n"
           << "function main()\n"
           << "    win = app_init(\"" << name << "\", 0, 0)\n"
           << "    lbl = app_label(win, \"Hello from " << name << "\", 10, 10, 300, 30)\n"
           << "    btn = app_button(win, \"Tap Me\", 10, 50, 200, 40)\n"
           << "    app_on_click(btn, lambda() app_set_text(lbl, \"Tapped!\") end)\n"
           << "    app_run(win)\n"
           << "end\n\n"
           << "main()\n";
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n*.apk\n*.ipa\n";
        /* Android build script */
        fs::create_directories(dir + "/android");
        std::ofstream af(dir + "/build_android.sh");
        af << "#!/bin/bash\n"
           << "# Build " << name << " for Android\n"
           << "aurorac main.aura -o libaurora_app.so --shared --target aarch64-linux-android\n"
           << "cd android && ./gradlew assembleRelease\n"
           << "echo \"APK: android/app/build/outputs/apk/release/\"\n";
        /* iOS build script */
        std::ofstream iof(dir + "/build_ios.sh");
        iof << "#!/bin/bash\n"
            << "# Build " << name << " for iOS\n"
            << "aurorac main.aura -o libaurora_app.a --static --target arm64-apple-ios\n"
            << "xcodebuild -project ios/AuroraApp.xcodeproj -scheme AuroraApp build\n"
            << "echo \"IPA: ios/build/\"\n";
        /* app.json */
        std::ofstream aj(dir + "/app.json");
        aj << "{\n"
           << "  \"name\": \"" << name << "\",\n"
           << "  \"version\": \"0.1.0\",\n"
           << "  \"icon\": \"icon.png\",\n"
           << "  \"orientation\": \"portrait\",\n"
           << "  \"splash_screen\": true\n"
           << "}\n";
    } else if (tpl_name == "cross-app") {
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: Cross-platform application (all 5 platforms)\n"
          << "entry: main.aura\n"
          << "dependencies:\npermissions:\n  - ui\n  - network\n  - storage\n";
        std::ofstream mf(dir + "/main.aura");
        mf << "## " << name << " -- Cross-Platform Application\n\n"
           << "import \"app\"\n\n"
           << "function main()\n"
           << "    win = app_init(\"" << name << "\", 400, 500)\n"
           << "    col = layout_column(win)\n"
           << "    lbl = app_label(col, \"Welcome to " << name << "\", 0, 0, 360, 40)\n"
           << "    app_set_font_size(lbl, 24)\n"
           << "    btn = app_button(col, \"Click Me\", 0, 0, 200, 44)\n"
           << "    counter = 0\n"
           << "    app_on_click(btn, lambda()\n"
           << "        counter = counter + 1\n"
           << "        app_set_text(lbl, \"Count: \" + counter)\n"
           << "    end)\n"
           << "    theme_set_light()\n"
           << "    app_run(win)\n"
           << "end\n\n"
           << "main()\n";
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n*.apk\n*.ipa\n*.dmg\n*.AppImage\n";
        /* Build scripts for all platforms */
        std::ofstream bf(dir + "/build_all.sh");
        bf << "#!/bin/bash\n"
           << "# Build " << name << " for all platforms\n"
           << "echo \"Building for Windows...\"\n"
           << "aurorac main.aura -o " << name << "_win.exe --target x86_64-pc-windows-msvc\n"
           << "echo \"Building for Linux...\"\n"
           << "aurorac main.aura -o " << name << "_linux --target x86_64-unknown-linux-gnu\n"
           << "echo \"Building for macOS...\"\n"
           << "aurorac main.aura -o " << name << "_mac --target aarch64-apple-darwin\n"
           << "echo \"Building for Android...\"\n"
           << "aurorac main.aura -o lib" << name << ".so --shared --target aarch64-linux-android\n"
           << "echo \"Building for iOS...\"\n"
           << "aurorac main.aura -o lib" << name << ".a --static --target arm64-apple-ios\n"
           << "echo \"All builds complete\"\n";
        /* app.json */
        std::ofstream aj(dir + "/app.json");
        aj << "{\n"
           << "  \"name\": \"" << name << "\",\n"
           << "  \"version\": \"0.1.0\",\n"
           << "  \"icon\": \"icon.png\",\n"
           << "  \"platforms\": [\"windows\", \"linux\", \"macos\", \"android\", \"ios\"],\n"
           << "  \"orientation\": \"auto\",\n"
           << "  \"splash_screen\": true\n"
           << "}\n";
    } else if (tpl_name == "chat-app") {
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: Real-time chat application\n"
          << "entry: main.aura\n"
          << "dependencies:\n  - websocket\npermissions:\n  - network\n";
        std::ofstream mf(dir + "/main.aura");
        mf << "## " << name << " -- Real-Time Chat Application\n\n"
           << "import \"app\"\nimport \"websocket\"\n\n"
           << "function main()\n"
           << "    win = app_init(\"Chat\", 400, 600)\n"
           << "    msg_list = app_listbox(win, 0, 0, 390, 450)\n"
           << "    input_box = app_textbox(win, \"\", 0, 460, 300, 35)\n"
           << "    send_btn = app_button(win, \"Send\", 310, 460, 80, 35)\n"
           << "    lbl = app_label(win, \"Welcome to " << name << "\", 0, 510, 390, 30)\n"
           << "    ws = ws_connect(\"wss://chat.example.com/ws\")\n"
           << "    app_on_click(send_btn, lambda()\n"
           << "        msg = app_get_text(input_box)\n"
           << "        ws_send(ws, msg)\n"
           << "        app_listbox_add(msg_list, \"You: \" + msg)\n"
           << "        app_set_text(input_box, \"\")\n"
           << "    end)\n"
           << "    app_run(win)\n"
           << "end\n\n"
           << "main()\n";
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n";
    } else if (tpl_name == "social-app") {
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: Social media application\n"
          << "entry: main.aura\n"
          << "dependencies:\n  - http\npermissions:\n  - network\n  - storage\n";
        std::ofstream mf(dir + "/main.aura");
        mf << "## " << name << " -- Social Media App\n\n"
           << "import \"app\"\nimport \"http\"\n\n"
           << "function main()\n"
           << "    win = app_init(\"" << name << "\", 400, 700)\n"
           << "    feed = app_listbox(win, 0, 40, 390, 500)\n"
           << "    post_input = app_textbox(win, \"What's on your mind?\", 0, 550, 300, 60)\n"
           << "    post_btn = app_button(win, \"Post\", 310, 550, 80, 30)\n"
           << "    app_on_click(post_btn, lambda()\n"
           << "        text = app_get_text(post_input)\n"
           << "        app_listbox_add(feed, \"You: \" + text)\n"
           << "        app_set_text(post_input, \"\")\n"
           << "    end)\n"
           << "    nav_bar = app_tabbar(win, 0, 660, 390, 40)\n"
           << "    app_run(win)\n"
           << "end\n\n"
           << "main()\n";
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n";
    } else if (tpl_name == "ecommerce-app") {
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: Online store application\n"
          << "entry: main.aura\n"
          << "dependencies:\n  - http\npermissions:\n  - network\n  - storage\n";
        std::ofstream mf(dir + "/main.aura");
        mf << "## " << name << " -- Online Store\n\n"
           << "import \"app\"\nimport \"http\"\n\n"
           << "function main()\n"
           << "    win = app_init(\"" << name << "\", 400, 700)\n"
           << "    products = app_listbox(win, 0, 40, 390, 400)\n"
           << "    cart = app_label(win, \"Cart: 0 items\", 0, 450, 390, 30)\n"
           << "    add_btn = app_button(win, \"Add to Cart\", 0, 490, 190, 40)\n"
           << "    checkout_btn = app_button(win, \"Checkout\", 200, 490, 190, 40)\n"
           << "    cart_count = 0\n"
           << "    app_on_click(add_btn, lambda()\n"
           << "        cart_count = cart_count + 1\n"
           << "        app_set_text(cart, \"Cart: \" + cart_count + \" items\")\n"
           << "    end)\n"
           << "    app_on_click(checkout_btn, lambda()\n"
           << "        app_label(win, \"Order placed!\", 0, 550, 390, 30)\n"
           << "    end)\n"
           << "    app_run(win)\n"
           << "end\n\n"
           << "main()\n";
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n";
    } else if (tpl_name == "dashboard-app") {
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: Admin dashboard application\n"
          << "entry: main.aura\n"
          << "dependencies:\n  - http\npermissions:\n  - network\n  - ui\n";
        std::ofstream mf(dir + "/main.aura");
        mf << "## " << name << " -- Admin Dashboard\n\n"
           << "import \"app\"\n\n"
           << "function main()\n"
           << "    win = app_init(\"" << name << "\", 900, 600)\n"
           << "    sidebar = app_listbox(win, 0, 0, 200, 600)\n"
           << "    app_listbox_add(sidebar, \"Dashboard\")\n"
           << "    app_listbox_add(sidebar, \"Analytics\")\n"
           << "    app_listbox_add(sidebar, \"Users\")\n"
           << "    app_listbox_add(sidebar, \"Settings\")\n"
           << "    title = app_label(win, \"Dashboard Overview\", 210, 10, 670, 30)\n"
           << "    app_set_font_size(title, 24)\n"
           << "    metric1 = app_label(win, \"Revenue: $12,430\", 210, 50, 200, 40)\n"
           << "    metric2 = app_label(win, \"Users: 1,245\", 430, 50, 200, 40)\n"
           << "    metric3 = app_label(win, \"Orders: 342\", 650, 50, 200, 40)\n"
           << "    chart_area = app_label(win, \"Chart area\", 210, 120, 670, 300)\n"
           << "    app_run(win)\n"
           << "end\n\n"
           << "main()\n";
        std::ofstream gf(dir + "/.gitignore");
        gf << ".voss/\nbuild/\n*.exe\n*.obj\n";
    } else if (tpl_name == "game-app") {
        std::ofstream f(dir + "/aurora.pkg");
        f << "name: " << name << "\n"
          << "version: 0.1.0\n"
          << "description: 2D game application\n"
          << "entry: main.aura\n"
          << "dependencies:\npermissions:\n  - ui\n";
        std::ofstream mf(dir + "/main.aura");
        mf << "## " << name << " -- 2D Game\n\n"
           << "import \"app\"\n\n"
           << "player_x = 200\n"
           << "player_y = 300\n"
           << "score = 0\n\n"
           << "function update()\n"
           << "    # Game logic here\n"
           << "end\n\n"
           << "function draw(canvas)\n"
           << "    canvas_clear(canvas, \"#000\")\n"
           << "    canvas_fill_rect(canvas, player_x, player_y, 32, 32, \"#0f0\")\n"
           << "    canvas_draw_text(canvas, \"Score: \" + score, 10, 10, 20, \"#fff\")\n"
           << "end\n\n"
           << "function main()\n"
           << "    win = app_init(\"" << name << "\", 480, 640)\n"
           << "    canvas = app_canvas(win, 0, 0, 480, 640)\n"
           << "    canvas_set_paint_fn(canvas, draw)\n"
           << "    app_set_interval(update, 16)\n"
           << "    app_run(win)\n"
           << "end\n\n"
           << "main()\n";
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

/* ── Packaging Command ── */
int cmd_package(const std::string& target, const std::string& format) {
    /* Read manifest */
    if (!fs::exists("aurora.pkg")) {
        std::cerr << "error: aurora.pkg not found (run from project root)\n"; return 1;
    }
    PackageInfo pkg = read_manifest("aurora.pkg");
    std::string entry = pkg.entry.empty() ? "main.aura" : pkg.entry;

    if (target == "windows") {
        if (format == "msi") {
            std::cout << "packaging " << pkg.name << " for Windows (MSI)...\n";
            std::string cmd = "aurorac " + entry + " -o " + pkg.name + ".exe && "
                              "echo \"Creating MSI installer for " + pkg.name + "\"";
            return system(cmd.c_str());
        } else if (format == "exe") {
            std::cout << "packaging " << pkg.name << " for Windows (EXE)...\n";
            std::string cmd = "aurorac " + entry + " -o " + pkg.name + ".exe && "
                              "echo \"Creating Inno Setup installer for " + pkg.name + "\"";
            return system(cmd.c_str());
        } else {
            std::cerr << "error: unknown format '" << format << "' for target 'windows' (use: msi, exe)\n";
            return 1;
        }
    } else if (target == "linux") {
        if (format == "appimage") {
            std::cout << "packaging " << pkg.name << " for Linux (AppImage)...\n";
            std::string cmd = "aurorac " + entry + " -o " + pkg.name + " && "
                              "echo \"Creating AppImage for " + pkg.name + "\"";
            return system(cmd.c_str());
        } else if (format == "deb") {
            std::cout << "packaging " << pkg.name << " for Linux (DEB)...\n";
            std::string cmd = "aurorac " + entry + " -o " + pkg.name + " && "
                              "echo \"Creating .deb package for " + pkg.name + "\"";
            return system(cmd.c_str());
        } else {
            std::cerr << "error: unknown format '" << format << "' for target 'linux' (use: appimage, deb)\n";
            return 1;
        }
    } else if (target == "macos") {
        if (format == "dmg") {
            std::cout << "packaging " << pkg.name << " for macOS (DMG)...\n";
            std::string cmd = "aurorac " + entry + " -o " + pkg.name + " && "
                              "echo \"Creating .dmg for " + pkg.name + "\"";
            return system(cmd.c_str());
        } else {
            std::cerr << "error: unknown format '" << format << "' for target 'macos' (use: dmg)\n";
            return 1;
        }
    } else if (target == "android") {
        if (format == "apk") {
            std::cout << "packaging " << pkg.name << " for Android (APK)...\n";
            std::string cmd = "aurorac " + entry + " -o libaurora_app.so --shared --target aarch64-linux-android && "
                              "echo \"Building APK via Gradle...\"";
            return system(cmd.c_str());
        } else if (format == "aab") {
            std::cout << "packaging " << pkg.name << " for Android (AAB)...\n";
            std::string cmd = "aurorac " + entry + " -o libaurora_app.so --shared --target aarch64-linux-android && "
                              "echo \"Building AAB via Gradle...\"";
            return system(cmd.c_str());
        } else {
            std::cerr << "error: unknown format '" << format << "' for target 'android' (use: apk, aab)\n";
            return 1;
        }
    } else if (target == "ios") {
        if (format == "ipa") {
            std::cout << "packaging " << pkg.name << " for iOS (IPA)...\n";
            std::string cmd = "aurorac " + entry + " -o libaurora_app.a --static --target arm64-apple-ios && "
                              "echo \"Building IPA via xcodebuild...\"";
            return system(cmd.c_str());
        } else {
            std::cerr << "error: unknown format '" << format << "' for target 'ios' (use: ipa)\n";
            return 1;
        }
    } else if (target == "all") {
        std::cout << "packaging " << pkg.name << " for all platforms...\n";
        cmd_package("windows", "exe");
        cmd_package("linux", "appimage");
        cmd_package("macos", "dmg");
        cmd_package("android", "apk");
        cmd_package("ios", "ipa");
        return 0;
    } else {
        std::cerr << "error: unknown target '" << target << "' (use: windows, linux, macos, android, ios, all)\n";
        return 1;
    }
    return 0;
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
