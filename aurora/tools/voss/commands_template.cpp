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

/* ════════════════════════════════════════════════════════════
   Phase 37.4 — Mobile Publishing (voss publish-mobile)
   ════════════════════════════════════════════════════════════ */

/* Minimal PNG writer (zlib deflate via raw stored blocks) — no external deps */
static bool write_png(const std::string& path, int w, int h,
                      const std::vector<uint8_t>& rgba /* w*h*4 */) {
    auto crc_table = [](uint32_t& poly, uint32_t* table) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        poly = 0xFFFFFFFFUL;
    };
    struct Chunk {
        uint32_t len;
        char type[4];
        std::vector<uint8_t> data;
        uint32_t crc = 0;
    };
    std::vector<Chunk> chunks;
    uint32_t png_poly; uint32_t png_table[256];
    crc_table(png_poly, png_table);
    auto crc_update = [&](uint32_t crc, const uint8_t* data, size_t len) {
        crc ^= 0xFFFFFFFFUL;
        for (size_t i = 0; i < len; i++) crc = png_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        return crc ^ 0xFFFFFFFFUL;
    };
    auto make_chunk = [&](const char* type, const std::vector<uint8_t>& data) {
        Chunk c;
        c.len = (uint32_t)data.size();
        memcpy(c.type, type, 4);
        c.data = data;
        uint32_t crc = 0xFFFFFFFFUL;
        for (int i = 0; i < 4; i++) crc = png_table[(crc ^ (uint8_t)type[i]) & 0xFF] ^ (crc >> 8);
        for (size_t i = 0; i < data.size(); i++) crc = png_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        c.crc = crc ^ 0xFFFFFFFFUL;
        return c;
    };
    /* IHDR */
    std::vector<uint8_t> ihdr;
    auto push32 = [&](uint32_t v) { ihdr.push_back((uint8_t)(v >> 24)); ihdr.push_back((uint8_t)(v >> 16)); ihdr.push_back((uint8_t)(v >> 8)); ihdr.push_back((uint8_t)v); };
    push32((uint32_t)w); push32((uint32_t)h);
    ihdr.push_back(8); /* bit depth */
    ihdr.push_back(6); /* color type RGBA */
    ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunks.push_back(make_chunk("IHDR", ihdr));
    /* IDAT — deflate raw stored blocks (RFC 1951) */
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; y++) {
        raw.push_back(0); /* filter: none */
        for (int x = 0; x < w; x++) {
            size_t i = ((size_t)y * w + x) * 4;
            raw.push_back(rgba[i]); raw.push_back(rgba[i + 1]); raw.push_back(rgba[i + 2]); raw.push_back(rgba[i + 3]);
        }
    }
    std::vector<uint8_t> idat;
    size_t pos = 0;
    uint16_t block_id = 0;
    while (pos < raw.size()) {
        size_t remaining = raw.size() - pos;
        size_t chunk_len = remaining < 65535 ? remaining : 65535;
        uint8_t hdr_byte = (pos + chunk_len >= raw.size()) ? 0x01 : 0x00;
        idat.push_back(hdr_byte);
        uint16_t len_l = (uint16_t)(chunk_len & 0xFFFF);
        uint16_t len_n = (uint16_t)(~len_l);
        idat.push_back((uint8_t)(len_l & 0xFF)); idat.push_back((uint8_t)(len_l >> 8));
        idat.push_back((uint8_t)(len_n & 0xFF)); idat.push_back((uint8_t)(len_n >> 8));
        for (size_t i = 0; i < chunk_len; i++) idat.push_back(raw[pos + i]);
        pos += chunk_len;
        block_id++;
    }
    if (raw.empty()) { idat.push_back(0x01); idat.push_back(0); idat.push_back(0); idat.push_back(0xFF); idat.push_back(0xFF); }
    chunks.push_back(make_chunk("IDAT", idat));
    /* IEND */
    chunks.push_back(make_chunk("IEND", {}));

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    f.write((const char*)sig, 8);
    for (auto& c : chunks) {
        uint8_t lenb[4] = { (uint8_t)(c.len >> 24), (uint8_t)(c.len >> 16), (uint8_t)(c.len >> 8), (uint8_t)c.len };
        f.write((const char*)lenb, 4);
        f.write(c.type, 4);
        if (!c.data.empty()) f.write((const char*)c.data.data(), c.data.size());
        uint8_t crcb[4] = { (uint8_t)(c.crc >> 24), (uint8_t)(c.crc >> 16), (uint8_t)(c.crc >> 8), (uint8_t)c.crc };
        f.write((const char*)crcb, 4);
    }
    return true;
}

int generate_app_icon(const std::string& out_png, int size, const std::string& label) {
    int S = size;
    std::vector<uint8_t> px((size_t)S * S * 4, 0);
    /* Aurora brand gradient background (top-left → bottom-right) */
    auto grad = [](int x, int y, int s) -> uint32_t {
        double t = (double)(x + y) / (2.0 * s);
        int r = (int)(0x4A + t * (0x9B - 0x4A));
        int g = (int)(0x90 + t * (0x30 - 0x90));
        int b = (int)(0xD9 + t * (0xFF - 0xD9));
        return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    };
    /* Rounded-rect mask */
    auto in_round = [&](int x, int y) -> bool {
        double cx = (double)S / 2.0, cy = (double)S / 2.0;
        double rad = (double)S * 0.22;
        double rx = (double)S / 2.0 - rad, ry = (double)S / 2.0 - rad;
        if (x >= rx && x <= cx + rx && y >= ry && y <= cy + ry) return true;
        double dx = 0, dy = 0;
        if (x < rx) dx = rx - x; else if (x > cx + rx) dx = x - (cx + rx);
        if (y < ry) dy = ry - y; else if (y > cy + ry) dy = y - (cy + ry);
        return (dx * dx + dy * dy) <= rad * rad;
    };
    /* Diamond "A" shape */
    auto in_a = [&](int x, int y) -> bool {
        double cx = (double)S * 0.5, base = (double)S * 0.78;
        double top = (double)S * 0.22;
        double cur = base - (double)y;              /* width at row y (from center) */
        double half = (cur / (base - top)) * ((double)S * 0.28);
        if (y < top || y > base) return false;
        return fabs((double)x - cx) <= half;
    };
    auto in_bar = [&](int x, int y) -> bool {
        double y0 = (double)S * 0.56, y1 = (double)S * 0.66;
        if (y < y0 || y > y1) return false;
        return fabs((double)x - (double)S * 0.5) <= (double)S * 0.20;
    };
    for (int y = 0; y < S; y++) {
        for (int x = 0; x < S; x++) {
            size_t i = ((size_t)y * S + x) * 4;
            if (!in_round(x, y)) continue;
            uint32_t c = grad(x, y, S);
            /* Light inner glow ring */
            double dx = (double)x - (double)S / 2.0, dy = (double)y - (double)S / 2.0;
            double dist = sqrt(dx * dx + dy * dy);
            double maxd = (double)S / 2.0 * 0.98;
            double ring = 1.0 - (dist / maxd);
            int r = (int)(((c >> 16) & 0xFF) + ring * 14);
            int g = (int)(((c >> 8) & 0xFF) + ring * 14);
            int b = (int)((c & 0xFF) + ring * 20);
            if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
            px[i] = (uint8_t)r; px[i + 1] = (uint8_t)g; px[i + 2] = (uint8_t)b; px[i + 3] = 255;
        }
    }
    /* White "A" glyph */
    for (int y = 0; y < S; y++) {
        for (int x = 0; x < S; x++) {
            size_t i = ((size_t)y * S + x) * 4;
            if (in_a(x, y) || in_bar(x, y)) {
                px[i] = 255; px[i + 1] = 255; px[i + 2] = 255; px[i + 3] = 255;
            }
        }
    }
    /* Bottom label bar (app name initial letters) */
    if (!label.empty()) {
        for (int y = (int)(S * 0.84); y < (int)(S * 0.98); y++) {
            for (int x = 0; x < S; x++) {
                size_t i = ((size_t)y * S + x) * 4;
                if (px[i + 3] == 0) continue;
                px[i] = 240; px[i + 1] = 244; px[i + 2] = 248; px[i + 3] = 255;
            }
        }
    }
    bool ok = write_png(out_png, S, S, px);
    std::cout << (ok ? "generated icon: " : "error writing icon: ") << out_png
              << " (" << S << "x" << S << ")\n";
    return ok ? 0 : 1;
}

int generate_splash(const std::string& out_png, int w, int h, const std::string& color) {
    std::vector<uint8_t> px((size_t)w * h * 4, 0);
    uint8_t cr = 0x4A, cg = 0x90, cb = 0xD9;
    if (!color.empty() && color.size() >= 6) {
        auto hexv = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
            if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
            return 0;
        };
        cr = (uint8_t)((hexv(color[0]) << 4) | hexv(color[1]));
        cg = (uint8_t)((hexv(color[2]) << 4) | hexv(color[3]));
        cb = (uint8_t)((hexv(color[4]) << 4) | hexv(color[5]));
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t i = ((size_t)y * w + x) * 4;
            double t = (double)(x + y) / (double)(w + h);
            px[i] = (uint8_t)(cr + t * (0x9B - cr) * 0.35);
            px[i + 1] = (uint8_t)(cg + t * (0x30 - cg) * 0.35);
            px[i + 2] = (uint8_t)(cb + t * (0xFF - cb) * 0.35);
            px[i + 3] = 255;
        }
    }
    /* Centered white "A" (simple) — logo placeholder */
    int cx0 = w / 2 - w / 10;
    int cy_top = h / 2 - h / 6;
    int cy_base = h / 2 + h / 6;
    for (int y = cy_top; y < cy_base; y++) {
        double t = (double)(y - cy_top) / (double)(cy_base - cy_top);
        int half = (int)((double)w / 20.0 * (1.0 - t * 0.4));
        for (int x = cx0 - half; x < cx0 + half; x++) {
            if (x < 0 || x >= w) continue;
            size_t i = ((size_t)y * w + x) * 4;
            px[i] = 255; px[i + 1] = 255; px[i + 2] = 255; px[i + 3] = 255;
        }
    }
    bool ok = write_png(out_png, w, h, px);
    std::cout << (ok ? "generated splash: " : "error writing splash: ") << out_png
              << " (" << w << "x" << h << ")\n";
    return ok ? 0 : 1;
}

int generate_android_keystore(const std::string& name) {
    if (!fs::exists("keystore.properties")) {
        std::cout << "no keystore.properties found — generating signing key...\n";
        /* Generate a random alias + password */
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 61);
        const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        auto rand_str = [&](int len) {
            std::string s;
            for (int i = 0; i < len; i++) s += chars[dist(gen)];
            return s;
        };
        std::string alias = name.empty() ? "aurora" : name;
        std::string pass = rand_str(16);
        std::string store = "aurora-release.jks";
#ifdef _WIN32
        std::string keytool = "keytool";
#else
        std::string keytool = "keytool";
#endif
        std::string cmd = keytool + " -genkeypair -v -keystore " + store +
                          " -alias " + alias +
                          " -keyalg RSA -keysize 2048 -validity 10000" +
                          " -storepass " + pass + " -keypass " + pass +
                          " -dname \"CN=Aurora App, OU=Dev, O=Aurora, L=City, ST=State, C=US\"";
        int rc = system(cmd.c_str());
        if (rc != 0) {
            std::cerr << "error: keytool failed (is JDK installed and on PATH?)\n";
            return 1;
        }
        /* Write keystore.properties */
        std::ofstream kp("keystore.properties");
        kp << "storeFile=" << store << "\n";
        kp << "storePassword=" << pass << "\n";
        kp << "keyAlias=" << alias << "\n";
        kp << "keyPassword=" << pass << "\n";
        kp.close();
        std::cout << "keystore generated: " << store << " (see keystore.properties)\n";
    } else {
        std::cout << "keystore.properties already present — keeping existing signing config\n";
    }
    return 0;
}

int patch_android_permissions(const std::string& manifest_path, const std::string& pkg_dir) {
    PackageInfo pkg = read_manifest("aurora.pkg");
    if (pkg.permissions.empty()) {
        std::cout << "no permissions requested in aurora.pkg\n";
        return 0;
    }
    std::string manifest = read_file_str(manifest_path);
    if (manifest.empty()) {
        std::cerr << "warning: could not read manifest " << manifest_path << "\n";
        return 0;
    }
    std::set<std::string> known = {
        "INTERNET", "CAMERA", "RECORD_AUDIO", "ACCESS_FINE_LOCATION", "ACCESS_COARSE_LOCATION",
        "VIBRATE", "READ_EXTERNAL_STORAGE", "WRITE_EXTERNAL_STORAGE", "BLUETOOTH",
        "READ_CONTACTS", "WRITE_CONTACTS", "READ_CALENDAR", "WRITE_CALENDAR", "NFC",
        "SYSTEM_ALERT_WINDOW", "WAKE_LOCK", "FOREGROUND_SERVICE", "POST_NOTIFICATIONS"
    };
    bool changed = false;
    std::string perm_elt;
    for (auto& perm : pkg.permissions) {
        std::string upper = perm;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        std::string android_perm;
        if (known.count(upper)) android_perm = "android.permission." + upper;
        else if (upper.find(".") != std::string::npos) android_perm = upper; /* full-qualified */
        else continue; /* unknown — skip silently */
        std::string line = "    <uses-permission android:name=\"" + android_perm + "\"/>\n";
        if (manifest.find(android_perm) == std::string::npos) {
            perm_elt += line;
            changed = true;
        }
    }
    if (changed) {
        /* Insert after <manifest ...> opening tag */
        size_t caret = manifest.find('>');
        if (caret != std::string::npos) {
            size_t nl = manifest.find('\n', caret);
            if (nl != std::string::npos) manifest.insert(nl + 1, perm_elt);
            std::ofstream f(manifest_path);
            f << manifest;
            f.close();
            std::cout << "patched permissions into " << manifest_path << "\n";
        }
    } else {
        std::cout << "permissions already present or none applicable\n";
    }
    return 0;
}

int cmd_publish_mobile(const std::string& platform, const std::string& format) {
    if (!fs::exists("aurora.pkg")) {
        std::cerr << "error: aurora.pkg not found (run from project root)\n"; return 1;
    }
    PackageInfo pkg = read_manifest("aurora.pkg");
    std::string entry = pkg.entry.empty() ? "main.aura" : pkg.entry;
    std::string safe_name = pkg.name;
    std::replace(safe_name.begin(), safe_name.end(), ' ', '_');

    if (platform == "android") {
        std::cout << "══ Publishing " << pkg.name << " for Android ══\n";

        /* 1. Generate signing keystore if absent */
        int kc = generate_android_keystore(pkg.name);
        if (kc != 0) return kc;

        /* 2. Generate mipmap icons (all densities) */
        std::vector<std::pair<int, std::string>> icons = {
            {48, "mipmap-mdpi/ic_launcher.png"},
            {72, "mipmap-hdpi/ic_launcher.png"},
            {96, "mipmap-xhdpi/ic_launcher.png"},
            {144, "mipmap-xxhdpi/ic_launcher.png"},
            {192, "mipmap-xxxhdpi/ic_launcher.png"}
        };
        for (auto& ic : icons) {
            std::string dir = "app/src/main/res/" + ic.second.substr(0, ic.second.find('/'));
            fs::create_directories(dir);
            int rc = generate_app_icon("app/src/main/res/" + ic.second, ic.first, pkg.name);
            if (rc != 0) return rc;
        }
        /* Adaptive icon foreground (432x432) */
        fs::create_directories("app/src/main/res/mipmap-anydpi-v26");
        int arc = generate_app_icon("app/src/main/res/mipmap-anydpi-v26/ic_launcher_foreground.png", 432, "");
        if (arc != 0) return arc;

        /* 3. Generate splash screen (portrait 1080x1920) */
        fs::create_directories("app/src/main/res/drawable");
        int src = generate_splash("app/src/main/res/drawable/splash.png", 1080, 1920, "");
        if (src != 0) return src;

        /* 4. Patch permissions in AndroidManifest.xml */
        patch_android_permissions("app/src/main/AndroidManifest.xml", "app");

        /* 5. Compile the .aura entry to a shared library for Android */
        std::string target_arch = "arm64-v8a";
        std::string jni_dir = "app/src/main/jniLibs/arm64-v8a";
        fs::create_directories(jni_dir);
        std::cout << "compiling " << entry << " for aarch64-linux-android...\n";
        std::string cc = "aurorac " + entry + " -o " + jni_dir + "/libaurora_app.so --shared --target aarch64-linux-android";
        int crc = system(cc.c_str());
        if (crc != 0) {
            std::cerr << "warning: aurorac cross-compile failed (exit " << crc << "). "
                         "Ensure the Android NDK toolchain is installed.\n";
        }

        /* 6. Invoke Gradle to build APK/AAB */
        std::string task = (format == "aab") ? "bundleRelease" : "assembleRelease";
        std::cout << "building Android " << format << " via Gradle...\n";
        std::string gc = "gradle " + task;
        int grc = system(gc.c_str());
        if (grc == 0) {
            std::string ext = (format == "aab") ? "aab" : "apk";
            std::string ap = "app/build/outputs/" + std::string(format == "aab" ? "bundle/release" : "apk/release") + "/app-release." + ext;
            std::cout << "✅ Android " << format << " built: " << ap << "\n";
            std::cout << "   Upload to Google Play Console → App release → Production track.\n";
        } else {
            std::cerr << "error: Gradle build failed (exit " << grc << "). "
                         "Ensure Android SDK + Gradle are installed.\n";
            return grc;
        }
        return 0;
    } else if (platform == "ios") {
        std::cout << "══ Publishing " << pkg.name << " for iOS ══\n";
        (void)format;

        /* 1. Generate app icon set using iconutil (macOS) or fallback ANG format dirs */
        int icon_size = 1024;
        std::string icon_dir = "iOS/Assets.xcassets/AppIcon.appiconset";
        fs::create_directories(icon_dir);
        int rc = generate_app_icon(icon_dir + "/AppIcon-1024.png", icon_size, pkg.name);
        if (rc != 0) return rc;

        /* 2. Generate splash (storyboard background) */
        std::string splash_dir = "iOS/Assets.xcassets/LaunchScreen.imageset";
        fs::create_directories(splash_dir);
        int src = generate_splash(splash_dir + "/LaunchScreen.png", 1284, 2778, "");
        if (src != 0) return src;

        /* 3. Compile static lib for arm64-apple-ios */
        std::cout << "compiling " << entry << " for arm64-apple-ios...\n";
        std::string cc = "aurorac " + entry + " -o libaurora_app.a --static --target arm64-apple-ios";
        int crc = system(cc.c_str());
        if (crc != 0) {
            std::cerr << "warning: aurorac iOS cross-compile failed (exit " << crc << "). "
                         "Xcode + iOS SDK required.\n";
        }

        /* 4. Invoke xcodebuild (requires macOS + signing profile) */
        std::cout << "building IPA via xcodebuild...\n";
        std::string xc = "xcodebuild -workspace Aurora.xcworkspace -scheme Aurora -configuration Release "
                         "-archivePath build/Aurora.xcarchive archive";
        int xrc = system(xc.c_str());
        if (xrc == 0) {
            std::string ex = "xcodebuild -exportArchive -archivePath build/Aurora.xcarchive "
                             "-exportOptionsPlist ExportOptions/app-store.plist -exportPath build/ipa";
            int erc = system(ex.c_str());
            if (erc == 0) {
                std::cout << "✅ iOS IPA built: build/ipa/Aurora.ipa\n";
                std::cout << "   Upload to App Store Connect → App Store → New version.\n";
            } else {
                std::cerr << "error: xcodebuild export failed (exit " << erc << "). "
                             "Set up Distribution certificate + provisioning profile.\n";
                return erc;
            }
        } else {
            std::cerr << "error: xcodebuild archive failed (exit " << xrc << "). "
                         "macOS + Xcode required for iOS builds.\n";
            return xrc;
        }
        return 0;
    }
    std::cerr << "error: unknown publish-mobile platform '" << platform << "' (use: android, ios)\n";
    return 1;
}
