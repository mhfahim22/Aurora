#include "bridge_shared.h"

/* ── Detect if an npm package has native addon (N-API / node-gyp) ── */
bool npm_has_native_addon(const JsonValue& json) {
    /* Check top-level native addon indicators */
    if (json.get("gypfile")) {
        auto* gyp = json.get("gypfile");
        if (gyp && gyp->type == JsonValue::Bool && gyp->bool_val) return true;
    }
    if (json.get("binary")) {
        /* Has prebuild binary section = native addon */
        return true;
    }
    if (json.get("napi")) {
        /* Explicitly declares N-API support */
        return true;
    }
    /* Check dependencies for native addon tooling */
    auto* deps = json.get("dependencies");
    if (deps && deps->type == JsonValue::Object) {
        static const char* native_dep_markers[] = {
            "node-addon-api", "node-gyp", "node-gyp-build",
            "node-pre-gyp", "@mapbox/node-pre-gyp",
            "prebuild", "prebuildify", "prebuild-install",
            "nan", "bindings", "node-gyp", "napi-build-utils",
            nullptr
        };
        for (auto& [dname, dver] : deps->obj) {
            for (int i = 0; native_dep_markers[i]; i++) {
                if (dname.find(native_dep_markers[i]) != std::string::npos)
                    return true;
            }
        }
    }
    return false;
}

static const char* npm_cjs_fallback(const std::string& pkg) {
    static const struct { const char* name; const char* cjs_ver; } esm_only[] = {
        {"chalk", "4"},
        {"nanocolors", "0"},
        {"got", "11"},
        {"ky", "0"},
        {"execa", "5"},
        {"globby", "11"},
        {"p-map", "4"},
        {"p-limit", "3"},
        {NULL, NULL}
    };
    for (int i = 0; esm_only[i].name; i++) {
        if (pkg == esm_only[i].name) return esm_only[i].cjs_ver;
    }
    return NULL;
}

void gen_npm_au_binding(const std::string& pkg, const JsonValue& json,
                         const std::string& ver, std::ostream& os)
{
    std::string safe = pkg;
    for (auto& c : safe) if (c == '-') c = '_';
    std::string desc = json.get_string("description");

    os << "/* ════════════════════════════════════════════════════════════\n";
    os << "   npm Bridge — Auto-generated Aurora FFI Bindings\n";
    os << "   Package: " << pkg << "@" << ver << "\n";
    os << "   ════════════════════════════════════════════════════════════ */\n\n";
    os << "/* Module init */\n";
    os << "@cost(zero)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_require() -> pointer\n\n";
    os << "/* FFI entry points */\n";
    os << "@cost(zero)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_call(mod: pointer, fn: cstring, args: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_call1(mod: pointer, fn: cstring, arg: pointer) -> pointer\n";
    os << "@cost(zero)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_free(handle: pointer)\n";
    os << "@cost(zero)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_free_cstr(handle: pointer)\n\n";
    os << "/* Value conversion helpers */\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_str(s: cstring) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_int(v: i64) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_float(v: f64) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_call_kw(mod: pointer, fn: cstring, args: pointer, kwargs: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_getattr(obj: pointer, name: cstring) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_call2(mod: pointer, fn: cstring, a: pointer, b: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_call3(mod: pointer, fn: cstring, a: pointer, b: pointer, c: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_call4(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_call5(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer, a5: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_call6(mod: pointer, fn: cstring, a1: pointer, a2: pointer, a3: pointer, a4: pointer, a5: pointer, a6: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_tuple2(a: pointer, b: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_tuple3(a: pointer, b: pointer, c: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_tuple4(a: pointer, b: pointer, c: pointer, d: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_list2(a: pointer, b: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_list3(a: pointer, b: pointer, c: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_list4(a: pointer, b: pointer, c: pointer, d: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_to_cstr(obj: pointer) -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_dict() -> pointer\n";
    os << "@cost(alloc)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_dict_set(d: pointer, key: cstring, val: pointer) -> i32\n";
    os << "/* Diagnostics */\n";
    os << "@cost(zero)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_get_perf_stats() -> cstring\n";
    os << "@cost(zero)\n";
    os << "extern \"npm_" << safe << "\" function " << safe << "_get_last_error() -> cstring\n\n";
    os << "/* Usage:\n";
    os << "     import \"" << pkg << "\"\n";
    os << "     let mod = " << safe << "_require()\n";
    os << "     let name = " << safe << "_str(\"world\")\n";
    os << "     let result = " << safe << "_call1(mod, \"fn\", name)\n";
    os << "     printf(\"%s\n\", " << safe << "_to_cstr(result))\n";
    os << "     " << safe << "_free(result)\n";
    os << "*/\n";
}

void gen_npm_cpp_wrapper(const std::string& pkg, const std::string& dir,
                         const std::string& node_path)
{
    /* ── Generate bridge_helper.js (embedded in bridge directory) ── */
    {
        std::ostringstream js;
        js << "// Persistent Node.js bridge for " << pkg << "\n";
        js << "// Communicates via JSON-RPC over stdin/stdout\n";
        js << "const pkg = require(process.argv[2]);\n";
        js << "let nextId = 2;\n";
        js << "const objects = {1: pkg};\n";
        js << "let buf = '';\n";
        js << "process.stdin.on('data', data => {\n";
        js << "    buf += data.toString();\n";
        js << "    const lines = buf.split('\n');\n";
        js << "    buf = lines.pop();\n";
        js << "    for (const line of lines) {\n";
        js << "        if (!line.trim()) continue;\n";
        js << "        try {\n";
        js << "            const req = JSON.parse(line);\n";
        js << "            handle(req);\n";
        js << "        } catch(e) {\n";
        js << "            write({id:-1, ok:false, error:e.message});\n";
        js << "        }\n";
        js << "    }\n";
        js << "});\n";
        js << "function handleSingle(req) {\n";
        js << "    if (req.type === 'call') {\n";
        js << "        const obj = objects[req.obj_id];\n";
        js << "        if (!obj) return {id:req.id, ok:false, error:'no such obj'};\n";
        js << "        const args = req.args || [];\n";
        js << "        const result = obj[req.method].apply(obj, args);\n";
        js << "        const newId = nextId++;\n";
        js << "        objects[newId] = result;\n";
        js << "        const json = result !== null && result !== undefined ? JSON.stringify(result) : 'null';\n";
        js << "        return {id:req.id, ok:true, obj_id:newId, json:json};\n";
        js << "    } else if (req.type === 'get') {\n";
        js << "        const obj = objects[req.obj_id];\n";
        js << "        if (!obj) return {id:req.id, ok:false, error:'no such obj'};\n";
        js << "        const val = obj[req.key];\n";
        js << "        const newId = nextId++;\n";
        js << "        objects[newId] = val;\n";
        js << "        const json = val !== null && val !== undefined ? JSON.stringify(val) : 'null';\n";
        js << "        return {id:req.id, ok:true, obj_id:newId, json:json};\n";
        js << "    } else if (req.type === 'free') {\n";
        js << "        delete objects[req.obj_id];\n";
        js << "        return null;\n";  // fire-and-forget: no response needed
        js << "    } else if (req.type === 'exit') {\n";
        js << "        process.exit(0);\n";
        js << "    } else {\n";
        js << "        return {id:req.id, ok:false, error:'unknown type'};\n";
        js << "    }\n";
        js << "}\n";
        js << "function handle(req) {\n";
        js << "    if (Array.isArray(req)) {\n";
        js << "        const results = [];\n";
        js << "        for (const r of req) {\n";
        js << "            try {\n";
        js << "                const res = handleSingle(r);\n";
        js << "                if (res) results.push(res);\n";
        js << "            } catch(e) {\n";
        js << "                results.push({id:r.id, ok:false, error:e.message});\n";
        js << "            }\n";
        js << "        }\n";
        js << "        if (results.length) write(results);\n";
        js << "    } else {\n";
        js << "        try {\n";
        js << "            const res = handleSingle(req);\n";
        js << "            if (res) write(res);\n";
        js << "        } catch(e) {\n";
        js << "            write({id:req.id, ok:false, error:e.message});\n";
        js << "        }\n";
        js << "    }\n";
        js << "}\n";
        js << "function write(msg) {\n";
        js << "    process.stdout.write(JSON.stringify(msg) + '\n');\n";
        js << "}\n";
        js << "process.stdin.on('end', () => process.exit(0));\n";
        if (write_file(dir + "/_bridge.js", js.str()))
            std::cout << "[bridge]   " << dir << "/_bridge.js\n";
    }

    /* ── Generate the C bridge DLL ── */
    {
        std::ostringstream cw;

        /* ── Includes and defines ── */
        cw << "/* Auto-generated npm bridge DLL for " << pkg << " */\n";
        cw << "/* Persistent Node.js child process with JSON-RPC */\n";
#ifdef _WIN32
        cw << "#define _CRT_SECURE_NO_WARNINGS\n";
        cw << "#include <windows.h>\n";
        cw << "#include <stdio.h>\n";
        cw << "#include <string.h>\n";
        cw << "#include <stdlib.h>\n";
        cw << "#include <stdint.h>\n\n";
#else
        cw << "#include <stdio.h>\n";
        cw << "#include <string.h>\n";
        cw << "#include <stdlib.h>\n";
        cw << "#include <stdint.h>\n";
        cw << "#include <unistd.h>\n";
        cw << "#include <sys/wait.h>\n";
        cw << "#include <poll.h>\n\n";
#endif

        /* ── Constants ── */
        cw << "static const char* BRIDGE_DIR = \"" << dir << "\";\n";
        cw << "static const char* BRIDGE_PKG = \"" << pkg << "\";\n";
        /* Embed node binary path — if empty string, rely on PATH lookup */
        cw << "static const char* NODE_PATH = \""
           << (node_path.empty() ? "node" : node_path) << "\";\n";
        cw << "#define RPC_TIMEOUT_MS 30000\n\n";

        /* ── JSON-RPC bridge process globals ── */
#ifdef _WIN32
        cw << "static HANDLE g_hChildStdoutRd = NULL;\n";
        cw << "static HANDLE g_hChildStdinWr = NULL;\n";
        cw << "static HANDLE g_hProcess = NULL;\n";
        cw << "static CRITICAL_SECTION g_rpc_lock;\n";
        cw << "static int g_rpc_ok = 0;\n";
        cw << "static int g_rpc_next_id = 1;\n";
        cw << "static volatile LONG g_restarting = 0;\n";
        cw << "/* Read buffer cache — reduces pipe ReadFile syscalls */\n";
        cw << "static char g_read_buf[262144];\n";
        cw << "static DWORD g_read_buf_start = 0;\n";
        cw << "static DWORD g_read_buf_end = 0;\n";
        cw << "/* Deferred free queue — batches free RPCs */\n";
        cw << "#define MAX_DEFERRED_FREE 128\n";
        cw << "static int64_t g_deferred_free[MAX_DEFERRED_FREE];\n";
        cw << "static int g_deferred_free_count = 0;\n";
        cw << "/* getattr cache — avoids RPC for repeated property lookups */\n";
        cw << "#define ATTR_CACHE_SIZE 64\n";
        cw << "static struct { int64_t obj_id; char key[48]; int result_id; } g_attr_cache[ATTR_CACHE_SIZE];\n";
        cw << "static int g_attr_cache_next = 0;\n";
        cw << "static char g_last_error[4096]=\"\";\n";
        cw << "/* Performance counters */\n";
        cw << "static struct{unsigned rpc_calls;unsigned rpc_time_ms;unsigned batch_items;\n";
        cw << "  unsigned batch_calls;unsigned restarts;unsigned attr_hits;unsigned attr_misses;}g_perf={0};\n\n";
#else
        cw << "static int g_child_stdin = -1;\n";
        cw << "static int g_child_stdout = -1;\n";
        cw << "static pid_t g_child_pid = 0;\n";
        cw << "static pthread_mutex_t g_rpc_lock = PTHREAD_MUTEX_INITIALIZER;\n";
        cw << "static int g_rpc_ok = 0;\n";
        cw << "static int g_rpc_next_id = 1;\n";
        cw << "static volatile int g_restarting = 0;\n";
        cw << "/* Read buffer cache */\n";
        cw << "static char g_read_buf[262144];\n";
        cw << "static size_t g_read_buf_start = 0;\n";
        cw << "static size_t g_read_buf_end = 0;\n";
        cw << "/* Deferred free queue */\n";
        cw << "#define MAX_DEFERRED_FREE 128\n";
        cw << "static int64_t g_deferred_free[MAX_DEFERRED_FREE];\n";
        cw << "static int g_deferred_free_count = 0;\n";
        cw << "/* getattr cache */\n";
        cw << "#define ATTR_CACHE_SIZE 64\n";
        cw << "static struct { int64_t obj_id; char key[48]; int result_id; } g_attr_cache[ATTR_CACHE_SIZE];\n";
        cw << "static int g_attr_cache_next = 0;\n";
        cw << "static char g_last_error[4096]=\"\";\n";
        cw << "/* Performance counters */\n";
        cw << "static struct{unsigned rpc_calls;unsigned rpc_time_ms;unsigned batch_items;\n";
        cw << "  unsigned batch_calls;unsigned restarts;unsigned attr_hits;unsigned attr_misses;}g_perf={0};\n\n";
#endif

        /* ── JSON escape helper ── */
        cw << "static void json_escape(const char* in, char* out, size_t outsz) {\n";
        cw << "    size_t j = 0;\n";
        cw << "    for (size_t i = 0; in[i] && j+6 < outsz; i++) {\n";
        cw << "        unsigned char c = (unsigned char)in[i];\n";
        cw << "        if (c == '\\\\' || c == '\"') { out[j++] = '\\\\'; out[j++] = c; }\n";
        cw << "        else if (c == '\n') { out[j++] = '\\\\'; out[j++] = 'n'; }\n";
        cw << "        else if (c == '\\r') { out[j++] = '\\\\'; out[j++] = 'r'; }\n";
        cw << "        else if (c == '\\t') { out[j++] = '\\\\'; out[j++] = 't'; }\n";
        cw << "        else out[j++] = c;\n";
        cw << "    }\n";
        cw << "    out[j] = '\\0';\n";
        cw << "}\n\n";
        cw << "static void set_last_error(const char* m){strncpy(g_last_error,m,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}\n\n";

        /* ── Local JSON handle store (for simple values, not Node objects) ── */
        cw << "static void* store_json(const char* json) {\n";
        cw << "    if (!json) return NULL;\n";
        cw << "    char** pp = (char**)malloc(sizeof(char*));\n";
        cw << "    if (!pp) return NULL;\n";
        cw << "    *pp = (char*)malloc(strlen(json) + 1);\n";
        cw << "    if (!*pp) { free(pp); return NULL; }\n";
        cw << "    strcpy(*pp, json);\n";
        cw << "    return (void*)pp;\n";
        cw << "}\n\n";
        cw << "static const char* get_json(void* handle) {\n";
        cw << "    if (!handle) return \"null\";\n";
        cw << "    return *((const char**)handle);\n";
        cw << "}\n\n";

        /* ── Spawn the persistent Node.js process ── */
        cw << "static int spawn_bridge(void) {\n";
        cw << "    g_perf.restarts++;\n";
#ifdef _WIN32
        cw << "    char old_cwd[4096];\n";
        cw << "    GetCurrentDirectoryA(sizeof(old_cwd), old_cwd);\n";
        cw << "    SetCurrentDirectoryA(BRIDGE_DIR);\n\n";
        cw << "    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};\n";
        cw << "    HANDLE hStdoutRd, hStdoutWr, hStdinRd, hStdinWr;\n";
        cw << "    CreatePipe(&hStdoutRd, &hStdoutWr, &sa, 0);\n";
        cw << "    CreatePipe(&hStdinRd, &hStdinWr, &sa, 0);\n";
        cw << "    SetHandleInformation(hStdoutRd, HANDLE_FLAG_INHERIT, 0);\n";
        cw << "    SetHandleInformation(hStdinWr, HANDLE_FLAG_INHERIT, 0);\n\n";
        cw << "    char cmd[4096];\n";
        cw << "    snprintf(cmd, sizeof(cmd), \"\\\"%s\\\" _bridge.js %s\", NODE_PATH, BRIDGE_PKG);\n\n";
        cw << "    STARTUPINFOA si = {sizeof(si)};\n";
        cw << "    si.hStdOutput = hStdoutWr;\n";
        cw << "    si.hStdInput = hStdinRd;\n";
        cw << "    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);\n";
        cw << "    si.dwFlags |= STARTF_USESTDHANDLES;\n";
        cw << "    PROCESS_INFORMATION pi = {};\n";
        cw << "    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, TRUE,\n";
        cw << "                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);\n";
        cw << "    CloseHandle(hStdoutWr);\n";
        cw << "    CloseHandle(hStdinRd);\n";
        cw << "    SetCurrentDirectoryA(old_cwd);\n";
        cw << "    if (!ok) { set_last_error(\"spawn: CreateProcess failed\"); CloseHandle(hStdoutRd); CloseHandle(hStdinWr); return 0; }\n";
        cw << "    g_hChildStdoutRd = hStdoutRd;\n";
        cw << "    g_hChildStdinWr = hStdinWr;\n";
        cw << "    g_hProcess = pi.hProcess;\n";
        cw << "    CloseHandle(pi.hThread);\n";
        cw << "    InitializeCriticalSection(&g_rpc_lock);\n";
        cw << "    g_rpc_ok = 1;\n";
        cw << "    return 1;\n";
#else
        cw << "    char old_cwd[4096];\n";
        cw << "    getcwd(old_cwd, sizeof(old_cwd));\n";
        cw << "    chdir(BRIDGE_DIR);\n\n";
        cw << "    int stdin_pipe[2], stdout_pipe[2];\n";
        cw << "    pipe(stdin_pipe); pipe(stdout_pipe);\n";
        cw << "    pid_t pid = fork();\n";
        cw << "    if (pid == 0) {\n";
        cw << "        dup2(stdin_pipe[0], STDIN_FILENO); close(stdin_pipe[0]);\n";
        cw << "        dup2(stdout_pipe[1], STDOUT_FILENO); close(stdout_pipe[1]);\n";
        cw << "        close(stdin_pipe[1]); close(stdout_pipe[0]);\n";
        cw << "        execlp(NODE_PATH, NODE_PATH, \"_bridge.js\", BRIDGE_PKG, NULL);\n";
        cw << "        _exit(1);\n";
        cw << "    }\n";
        cw << "    close(stdin_pipe[0]); close(stdout_pipe[1]);\n";
        cw << "    g_child_stdin = stdin_pipe[1];\n";
        cw << "    g_child_stdout = stdout_pipe[0];\n";
        cw << "    g_child_pid = pid;\n";
        cw << "    chdir(old_cwd);\n";
        cw << "    if (pid < 0) { set_last_error(\"spawn: fork failed\"); close(stdin_pipe[1]); close(stdout_pipe[0]); chdir(old_cwd); return 0; }\n";
        cw << "    g_rpc_ok = 1;\n";
        cw << "  return 1;\n";
#endif
        cw << "}\n\n";
#ifdef _WIN32
        cw << "static int try_restart_bridge(void) {\n";
        cw << "    if (g_rpc_ok) return 1;\n";
        cw << "    if (InterlockedExchange(&g_restarting, 1)) return 0;\n";
        cw << "    if (g_hProcess) { WaitForSingleObject(g_hProcess, 0); CloseHandle(g_hProcess); g_hProcess = NULL; }\n";
        cw << "    if (g_hChildStdoutRd) { CloseHandle(g_hChildStdoutRd); g_hChildStdoutRd = NULL; }\n";
        cw << "    if (g_hChildStdinWr) { CloseHandle(g_hChildStdinWr); g_hChildStdinWr = NULL; }\n";
        cw << "    g_rpc_ok = 0; g_rpc_next_id = 1;\n";
        cw << "    g_read_buf_start = g_read_buf_end = 0;\n";
        cw << "    g_attr_cache_next = 0;\n";
        cw << "    g_deferred_free_count = 0;\n";
        cw << "    int ok = spawn_bridge();\n";
        cw << "    g_restarting = 0;\n";
        cw << "    return ok;\n";
        cw << "}\n\n";
#else
        cw << "static int try_restart_bridge(void) {\n";
        cw << "    if (g_rpc_ok) return 1;\n";
        cw << "    if (__sync_lock_test_and_set(&g_restarting, 1)) return 0;\n";
        cw << "    if (g_child_stdin >= 0) { close(g_child_stdin); g_child_stdin = -1; }\n";
        cw << "    if (g_child_stdout >= 0) { close(g_child_stdout); g_child_stdout = -1; }\n";
        cw << "    g_child_pid = 0;\n";
        cw << "    g_rpc_ok = 0; g_rpc_next_id = 1;\n";
        cw << "    g_read_buf_start = g_read_buf_end = 0;\n";
        cw << "    g_attr_cache_next = 0;\n";
        cw << "    g_deferred_free_count = 0;\n";
        cw << "    int ok = spawn_bridge();\n";
        cw << "    __sync_lock_release(&g_restarting);\n";
        cw << "    return ok;\n";
        cw << "}\n\n";
#endif

        /* ── Minimal but correct JSON field extractor ── */
        /* Extracts a named field from a flat JSON object. Handles string, number, true, false, null.
           Correctly processes escaped quotes (\\\") and backslashes (\\\\) in string values.
           Returns 1 if found, 0 otherwise. */
        cw << "static int extract_json_field(const char*json,const char*key,char*out,size_t out_sz){\n";
        cw << "  char pat[256];int pl=snprintf(pat,sizeof(pat),\"\\\"%s\\\":\",key);\n";
        cw << "  char*p=strstr(json,pat);if(!p)return 0;p+=pl;\n";
        cw << "  while(*p==' ')p++;\n";
        cw << "  if(*p=='\"'){\n";
        cw << "    p++;size_t w=0;\n";
        cw << "    while(*p&&w<out_sz-1){\n";
        cw << "      if(*p=='\\\\'&&*(p+1)=='\"'){out[w++]='\"';p+=2;}\n";
        cw << "      else if(*p=='\\\\'&&*(p+1)=='\\\\'){out[w++]='\\\\';p+=2;}\n";
        cw << "      else if(*p=='\\\\'&&*(p+1)=='n'){out[w++]='\n';p+=2;}\n";
        cw << "      else if(*p=='\\\\'&&*(p+1)=='t'){out[w++]='\\t';p+=2;}\n";
        cw << "      else if(*p=='\\\\'&&*(p+1)=='/'){out[w++]='/';p+=2;}\n";
        cw << "      else if(*p=='\"')break;\n";
        cw << "      else out[w++]=*p++;\n";
        cw << "    }\n";
        cw << "    out[w]=0;return 1;\n";
        cw << "  }else if(*p=='t'&&strncmp(p,\"true\",4)==0){strcpy(out,\"true\");return 1;}\n";
        cw << "  else if(*p=='f'&&strncmp(p,\"false\",5)==0){strcpy(out,\"false\");return 1;}\n";
        cw << "  else if(*p=='n'&&strncmp(p,\"null\",4)==0){strcpy(out,\"null\");return 1;}\n";
        cw << "  else if((*p>='0'&&*p<='9')||*p=='-'){\n";
        cw << "    size_t w=0;\n";
        cw << "    while((*p>='0'&&*p<='9')||*p=='-'||*p=='.'||*p=='e'||*p=='E'||*p=='+'){\n";
        cw << "      if(w<out_sz-1)out[w++]=*p;p++;\n";
        cw << "    }\n";
        cw << "    out[w]=0;return 1;\n";
        cw << "  }\n";
        cw << "  return 0;}\n\n";

        /* ── Send RPC and receive response (with read buffering) ── */
        /* Returns pointer to static response buffer. Thread-safe. */
#ifdef _WIN32
        cw << "static int send_rpc(const char* req_json, int* out_obj_id, char* out_json, size_t out_json_sz) {\n";
        cw << "    if (!g_rpc_ok) return 0;\n";
        cw << "    EnterCriticalSection(&g_rpc_lock);\n";
        cw << "    g_perf.rpc_calls++;\n";
        cw << "    LARGE_INTEGER t0,tf,freq;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&t0);\n";
        cw << "    DWORD written;\n";
        cw << "    char full_req[65536];\n";
        cw << "    snprintf(full_req, sizeof(full_req), \"%s\n\", req_json);\n";
        cw << "    WriteFile(g_hChildStdinWr, full_req, (DWORD)strlen(full_req), &written, NULL);\n";
        cw << "    /* Read from buffered pipe: refill buffer if needed */\n";
        cw << "    char* line_start = NULL;\n";
        cw << "    while (1) {\n";
        cw << "        /* Scan buffer for newline */\n";
        cw << "        for (DWORD i = g_read_buf_start; i < g_read_buf_end; i++) {\n";
        cw << "            if (g_read_buf[i] == '\n') {\n";
        cw << "                line_start = g_read_buf + g_read_buf_start;\n";
        cw << "                g_read_buf[i] = '\\0';\n";
        cw << "                g_read_buf_start = i + 1;\n";
        cw << "                break;\n";
        cw << "            }\n";
        cw << "        }\n";
        cw << "        if (line_start) break;\n";
        cw << "        /* Refill buffer */\n";
        cw << "        if (g_read_buf_start > 0 && g_read_buf_start < g_read_buf_end) {\n";
        cw << "            memmove(g_read_buf, g_read_buf + g_read_buf_start, g_read_buf_end - g_read_buf_start);\n";
        cw << "            g_read_buf_end -= g_read_buf_start;\n";
        cw << "            g_read_buf_start = 0;\n";
        cw << "        } else if (g_read_buf_start >= g_read_buf_end) {\n";
        cw << "            g_read_buf_start = g_read_buf_end = 0;\n";
        cw << "        }\n";
        cw << "        DWORD chunk = 0;\n";
        cw << "        if (WaitForSingleObject(g_hChildStdoutRd, RPC_TIMEOUT_MS) != WAIT_OBJECT_0) {\n";
        cw << "            g_rpc_ok = 0;\n";
        cw << "            LeaveCriticalSection(&g_rpc_lock);\n";
        cw << "            return 0;\n";
        cw << "        }\n";
        cw << "        if (!ReadFile(g_hChildStdoutRd, g_read_buf + g_read_buf_end,\n";
        cw << "                      (DWORD)(sizeof(g_read_buf) - g_read_buf_end - 1), &chunk, NULL)\n";
        cw << "            || chunk == 0) {\n";
        cw << "            g_rpc_ok = 0;\n";
        cw << "            LeaveCriticalSection(&g_rpc_lock);\n";
        cw << "            return 0;\n";
        cw << "        }\n";
        cw << "        g_read_buf_end += chunk;\n";
        cw << "        g_read_buf[g_read_buf_end] = '\\0';\n";
        cw << "    }\n";
        cw << "    QueryPerformanceCounter(&tf);\n";
        cw << "    g_perf.rpc_time_ms+=(unsigned)(((tf.QuadPart-t0.QuadPart)*1000)/freq.QuadPart);\n";
        cw << "    LeaveCriticalSection(&g_rpc_lock);\n";
        cw << "    char* buf = line_start;\n";
#else
        cw << "static int send_rpc(const char* req_json, int* out_obj_id, char* out_json, size_t out_json_sz) {\n";
        cw << "    if (!g_rpc_ok) return 0;\n";
        cw << "    pthread_mutex_lock(&g_rpc_lock);\n";
        cw << "    g_perf.rpc_calls++;\n";
        cw << "    struct timespec pt0,ptf;clock_gettime(CLOCK_MONOTONIC,&pt0);\n";
        cw << "    dprintf(g_child_stdin, \"%s\n\", req_json);\n";
        cw << "    /* Read from buffered pipe */\n";
        cw << "    char* line_start = NULL;\n";
        cw << "    while (1) {\n";
        cw << "        for (size_t i = g_read_buf_start; i < g_read_buf_end; i++) {\n";
        cw << "            if (g_read_buf[i] == '\n') {\n";
        cw << "                line_start = g_read_buf + g_read_buf_start;\n";
        cw << "                g_read_buf[i] = '\\0';\n";
        cw << "                g_read_buf_start = i + 1;\n";
        cw << "                break;\n";
        cw << "            }\n";
        cw << "        }\n";
        cw << "        if (line_start) break;\n";
        cw << "        if (g_read_buf_start > 0 && g_read_buf_start < g_read_buf_end) {\n";
        cw << "            memmove(g_read_buf, g_read_buf + g_read_buf_start, g_read_buf_end - g_read_buf_start);\n";
        cw << "            g_read_buf_end -= g_read_buf_start; g_read_buf_start = 0;\n";
        cw << "        } else if (g_read_buf_start >= g_read_buf_end) {\n";
        cw << "            g_read_buf_start = g_read_buf_end = 0;\n";
        cw << "        }\n";
        cw << "        struct pollfd pfd = { .fd = g_child_stdout, .events = POLLIN };\n";
        cw << "        if (poll(&pfd, 1, RPC_TIMEOUT_MS) <= 0) { g_rpc_ok = 0; pthread_mutex_unlock(&g_rpc_lock); return 0; }\n";
        cw << "        ssize_t n = read(g_child_stdout, g_read_buf + g_read_buf_end, sizeof(g_read_buf) - g_read_buf_end - 1);\n";
        cw << "        if (n <= 0) { g_rpc_ok = 0; pthread_mutex_unlock(&g_rpc_lock); return 0; }\n";
        cw << "        g_read_buf_end += n;\n";
        cw << "        g_read_buf[g_read_buf_end] = '\\0';\n";
        cw << "    }\n";
        cw << "    clock_gettime(CLOCK_MONOTONIC,&ptf);\n";
        cw << "    g_perf.rpc_time_ms+=(unsigned)(((ptf.tv_sec-pt0.tv_sec)*1000+(ptf.tv_nsec-pt0.tv_nsec)/1000000));\n";
        cw << "    pthread_mutex_unlock(&g_rpc_lock);\n";
        cw << "    char* buf = line_start;\n";
#endif
        cw << "    /* Parse response JSON via extract_json_field */\n";
        cw << "    char ok_buf[16];\n";
        cw << "    if(!extract_json_field(buf,\"ok\",ok_buf,sizeof(ok_buf))||strcmp(ok_buf,\"true\")!=0)return 0;\n";
        cw << "    if(out_obj_id){\n";
        cw << "      char oid_buf[32];\n";
        cw << "      if(extract_json_field(buf,\"obj_id\",oid_buf,sizeof(oid_buf)))*out_obj_id=atoi(oid_buf);\n";
        cw << "      else *out_obj_id=0;\n";
        cw << "    }\n";
        cw << "    if(out_json&&out_json_sz>0){\n";
        cw << "      if(!extract_json_field(buf,\"json\",out_json,out_json_sz))out_json[0]='\\0';\n";
        cw << "    }\n";
        cw << "    return 1;\n";
        cw << "}\n\n";

        /* ── Cleanup bridge process ── */
        cw << "static void cleanup_bridge(void) {\n";
        cw << "    if (!g_rpc_ok) return;\n";
        cw << "    flush_deferred_free();\n";
        cw << "    send_rpc(\"{\\\"type\\\":\\\"exit\\\",\\\"id\\\":0}\", NULL, NULL, 0);\n";
#ifdef _WIN32
        cw << "    if (g_hProcess) {\n";
        cw << "        WaitForSingleObject(g_hProcess, 3000);\n";
        cw << "        CloseHandle(g_hProcess); g_hProcess = NULL;\n";
        cw << "    }\n";
        cw << "    if (g_hChildStdoutRd) { CloseHandle(g_hChildStdoutRd); g_hChildStdoutRd = NULL; }\n";
        cw << "    if (g_hChildStdinWr) { CloseHandle(g_hChildStdinWr); g_hChildStdinWr = NULL; }\n";
        cw << "    DeleteCriticalSection(&g_rpc_lock);\n";
#else
        cw << "    if (g_child_pid > 0) {\n";
        cw << "        int wstatus;\n";
        cw << "        for (int wait_i = 0; wait_i < 300; wait_i++) {\n";
        cw << "            pid_t wret = waitpid(g_child_pid, &wstatus, WNOHANG);\n";
        cw << "            if (wret == g_child_pid || wret == -1) break;\n";
        cw << "            usleep(10000);\n";
        cw << "        }\n";
        cw << "        g_child_pid = 0;\n";
        cw << "    }\n";
        cw << "    if (g_child_stdin >= 0) { close(g_child_stdin); g_child_stdin = -1; }\n";
        cw << "    if (g_child_stdout >= 0) { close(g_child_stdout); g_child_stdout = -1; }\n";
#endif
        cw << "    g_rpc_ok = 0;\n";
        cw << "}\n\n";

        /* ── _require ── */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_require(void) {\n";
#else
        cw << "void* " << pkg << "_require(void) {\n";
#endif
        cw << "    if (!g_rpc_ok && !try_restart_bridge()) return NULL;\n";
        cw << "    return (void*)1;  /* module is always obj_id 1 */\n";
        cw << "}\n\n";

        /* ── Flush deferred free queue ── */
        cw << "static void flush_deferred_free(void) {\n";
        cw << "    if (g_deferred_free_count == 0) return;\n";
        cw << "    g_perf.batch_calls++;\n";
        cw << "    g_perf.batch_items += (unsigned)g_deferred_free_count;\n";
        cw << "    /* Build a JSON array batch of all pending free requests */\n";
        cw << "    char batch[4096];\n";
        cw << "    int pos = snprintf(batch, sizeof(batch), \"[\");\n";
        cw << "    for (int i = 0; i < g_deferred_free_count; i++) {\n";
        cw << "        if (i > 0) pos += snprintf(batch + pos, sizeof(batch) - pos, \",\");\n";
        cw << "        pos += snprintf(batch + pos, sizeof(batch) - pos,\n";
        cw << "            \"{\\\"type\\\":\\\"free\\\",\\\"obj_id\\\":%lld,\\\"id\\\":%d}\",\n";
        cw << "            (long long)g_deferred_free[i], g_rpc_next_id++);\n";
        cw << "    }\n";
        cw << "    pos += snprintf(batch + pos, sizeof(batch) - pos, \"]\\n\");\n";
#ifdef _WIN32
        cw << "    DWORD written;\n";
        cw << "    WriteFile(g_hChildStdinWr, batch, (DWORD)pos, &written, NULL);\n";
#else
        cw << "    dprintf(g_child_stdin, \"%s\", batch);\n";
#endif
        cw << "    g_deferred_free_count = 0;\n";
        cw << "}\n\n";

        /* ── _free (uses deferred queue, flushes at threshold) ── */
#ifdef _WIN32
        cw << "__declspec(dllexport) void " << pkg << "_free(void* handle) {\n";
#else
        cw << "void " << pkg << "_free(void* handle) {\n";
#endif
        cw << "    if (!handle || handle == (void*)1) return;\n";
        cw << "    char** pp = (char**)handle;\n";
        cw << "    if (*pp) free(*pp);\n";
        cw << "    free(pp);\n";
        cw << "    if (g_deferred_free_count >= MAX_DEFERRED_FREE) flush_deferred_free();\n";
        cw << "    g_deferred_free[g_deferred_free_count++] = (int64_t)(intptr_t)handle;\n";
        cw << "}\n\n";

        /* ── _free_cstr ── */
#ifdef _WIN32
        cw << "__declspec(dllexport) void " << pkg << "_free_cstr(void* s) {\n";
#else
        cw << "void " << pkg << "_free_cstr(void* s) {\n";
#endif
        cw << "    if (s) free(s);\n";
        cw << "}\n";
#ifdef _WIN32
        cw << "__declspec(dllexport) char* " << pkg << "_get_last_error(void) { return g_last_error; }\n";
#else
        cw << "char* " << pkg << "_get_last_error(void) { return g_last_error; }\n";
#endif
        cw << "\n";
#ifdef _WIN32
        cw << "__declspec(dllexport) const char* " << pkg << "_get_perf_stats(void) {\n";
#else
        cw << "const char* " << pkg << "_get_perf_stats(void) {\n";
#endif
        cw << "  static char buf[1024];\n";
        cw << "  snprintf(buf,sizeof(buf),\"{\\\"rpc_calls\\\":%u,\\\"rpc_time_ms\\\":%u,"
               "\\\"batch_items\\\":%u,\\\"batch_calls\\\":%u,"
               "\\\"restarts\\\":%u,\\\"attr_hits\\\":%u,\\\"attr_misses\\\":%u}\",\n";
        cw << "    g_perf.rpc_calls,g_perf.rpc_time_ms,g_perf.batch_items,\n";
        cw << "    g_perf.batch_calls,g_perf.restarts,g_perf.attr_hits,g_perf.attr_misses);\n";
        cw << "  return buf;\n";
        cw << "}\n\n";

        /* ── Local JSON helpers (no RPC needed) ── */

        /* _str */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_str(const char* s) {\n";
#else
        cw << "void* " << pkg << "_str(const char* s) {\n";
#endif
        cw << "    if (!s) return store_json(\"null\");\n";
        cw << "    char escaped[8192];\n";
        cw << "    json_escape(s, escaped, sizeof(escaped));\n";
        cw << "    char buf[8194];\n";
        cw << "    snprintf(buf, sizeof(buf), \"\\\"%s\\\"\", escaped);\n";
        cw << "    return store_json(buf);\n";
        cw << "}\n\n";

        /* _int */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_int(long long v) {\n";
#else
        cw << "void* " << pkg << "_int(long long v) {\n";
#endif
        cw << "    char buf[64];\n";
        cw << "    snprintf(buf, sizeof(buf), \"%lld\", v);\n";
        cw << "    return store_json(buf);\n";
        cw << "}\n\n";

        /* _float */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_float(double v) {\n";
#else
        cw << "void* " << pkg << "_float(double v) {\n";
#endif
        cw << "    char buf[64];\n";
        cw << "    snprintf(buf, sizeof(buf), \"%g\", v);\n";
        cw << "    return store_json(buf);\n";
        cw << "}\n\n";

        /* _to_cstr */
#ifdef _WIN32
        cw << "__declspec(dllexport) char* " << pkg << "_to_cstr(void* obj) {\n";
#else
        cw << "char* " << pkg << "_to_cstr(void* obj) {\n";
#endif
        cw << "    if (!obj) return NULL;\n";
        cw << "    const char* json = get_json(obj);\n";
        cw << "    char* out = (char*)malloc(strlen(json) + 1);\n";
        cw << "    strcpy(out, json);\n";
        cw << "    return out;\n";
        cw << "}\n\n";

        /* _tuple2 */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_tuple2(void* a, void* b) {\n";
#else
        cw << "void* " << pkg << "_tuple2(void* a, void* b) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s]\", get_json(a), get_json(b));\n";
        cw << "    return store_json(buf);\n";
        cw << "}\n\n";

        /* _tuple3 */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_tuple3(void* a, void* b, void* c) {\n";
#else
        cw << "void* " << pkg << "_tuple3(void* a, void* b, void* c) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s,%s]\", get_json(a), get_json(b), get_json(c));\n";
        cw << "    return store_json(buf);\n";
        cw << "}\n\n";

        /* _tuple4 */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_tuple4(void* a, void* b, void* c, void* d) {\n";
#else
        cw << "void* " << pkg << "_tuple4(void* a, void* b, void* c, void* d) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s,%s,%s]\", get_json(a), get_json(b), get_json(c), get_json(d));\n";
        cw << "    return store_json(buf);\n";
        cw << "}\n\n";

        /* _list2-4 (same as tuple for JSON) */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_list2(void* a, void* b) {\n";
#else
        cw << "void* " << pkg << "_list2(void* a, void* b) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s]\", get_json(a), get_json(b));\n";
        cw << "    return store_json(buf);\n";
        cw << "}\n\n";

#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_list3(void* a, void* b, void* c) {\n";
#else
        cw << "void* " << pkg << "_list3(void* a, void* b, void* c) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s,%s]\", get_json(a), get_json(b), get_json(c));\n";
        cw << "    return store_json(buf);\n";
        cw << "}\n\n";

#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_list4(void* a, void* b, void* c, void* d) {\n";
#else
        cw << "void* " << pkg << "_list4(void* a, void* b, void* c, void* d) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s,%s,%s]\", get_json(a), get_json(b), get_json(c), get_json(d));\n";
        cw << "    return store_json(buf);\n";
        cw << "}\n\n";

        /* _tuple (variadic via pointer array) */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_tuple(void** items, int count) {\n";
#else
        cw << "void* " << pkg << "_tuple(void** items, int count) {\n";
#endif
        cw << "    if (count <= 0) return store_json(\"[]\");\n";
        cw << "    char* buf = (char*)malloc(16384);\n";
        cw << "    if (!buf) return NULL;\n";
        cw << "    int pos = 0;\n";
        cw << "    pos += snprintf(buf + pos, 16384 - pos, \"[\");\n";
        cw << "    for (int i = 0; i < count; i++) {\n";
        cw << "        if (i > 0) pos += snprintf(buf + pos, 16384 - pos, \",\");\n";
        cw << "        pos += snprintf(buf + pos, 16384 - pos, \"%s\", get_json(items[i]));\n";
        cw << "    }\n";
        cw << "    pos += snprintf(buf + pos, 16384 - pos, \"]\");\n";
        cw << "    void* h = store_json(buf);\n";
        cw << "    free(buf);\n";
        cw << "    return h;\n";
        cw << "}\n\n";

        /* _list (same as _tuple for JSON) */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_list(void** items, int count) {\n";
#else
        cw << "void* " << pkg << "_list(void** items, int count) {\n";
#endif
        cw << "    if (count <= 0) return store_json(\"[]\");\n";
        cw << "    char* buf = (char*)malloc(16384);\n";
        cw << "    if (!buf) return NULL;\n";
        cw << "    int pos = 0;\n";
        cw << "    pos += snprintf(buf + pos, 16384 - pos, \"[\");\n";
        cw << "    for (int i = 0; i < count; i++) {\n";
        cw << "        if (i > 0) pos += snprintf(buf + pos, 16384 - pos, \",\");\n";
        cw << "        pos += snprintf(buf + pos, 16384 - pos, \"%s\", get_json(items[i]));\n";
        cw << "    }\n";
        cw << "    pos += snprintf(buf + pos, 16384 - pos, \"]\");\n";
        cw << "    void* h = store_json(buf);\n";
        cw << "    free(buf);\n";
        cw << "    return h;\n";
        cw << "}\n\n";

        /* _dict */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_dict(void) {\n";
#else
        cw << "void* " << pkg << "_dict(void) {\n";
#endif
        cw << "    return store_json(\"{}\");\n";
        cw << "}\n\n";

        /* _dict_set */
#ifdef _WIN32
        cw << "__declspec(dllexport) int " << pkg << "_dict_set(void* d, const char* key, void* val) {\n";
#else
        cw << "int " << pkg << "_dict_set(void* d, const char* key, void* val) {\n";
#endif
        cw << "    if (!d || !key) return -1;\n";
        cw << "    char** pp = (char**)d;\n";
        cw << "    const char* val_json = get_json(val);\n";
        cw << "    char escaped_key[512];\n";
        cw << "    json_escape(key, escaped_key, sizeof(escaped_key));\n";
        cw << "    char entry[2048];\n";
        cw << "    int elen = snprintf(entry, sizeof(entry), \"\\\"%s\\\":%s\", escaped_key, val_json);\n";
        cw << "    if (elen < 0) return -1;\n";
        cw << "    size_t old_len = strlen(*pp);\n";
        cw << "    if (old_len <= 2) {\n";
        cw << "        char* new_s = (char*)malloc(elen + 3);\n";
        cw << "        snprintf(new_s, elen + 3, \"{%s}\", entry);\n";
        cw << "        free(*pp); *pp = new_s;\n";
        cw << "    } else {\n";
        cw << "        char* new_s = (char*)malloc(old_len + elen + 2);\n";
        cw << "        snprintf(new_s, old_len + elen + 2, \"%.*s,%s}\", (int)(old_len - 1), *pp, entry);\n";
        cw << "        free(*pp); *pp = new_s;\n";
        cw << "    }\n";
        cw << "    return 0;\n";
        cw << "}\n\n";

        /* ── RPC-based exports ── */

        /* _call (via persistent process) */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_call(void* mod, const char* fn, void* args) {\n";
#else
        cw << "void* " << pkg << "_call(void* mod, const char* fn, void* args) {\n";
#endif
        cw << "    if (!mod || !fn) return NULL;\n";
        cw << "    if (!g_rpc_ok && !try_restart_bridge()) return NULL;\n";
        cw << "    char req[131072];\n";
        cw << "    int id = g_rpc_next_id++;\n";
        cw << "    long long obj_id = (mod == (void*)1) ? 1 : (long long)mod;\n";
        cw << "    const char* args_json = args ? get_json(args) : \"[]\";\n";
        cw << "    snprintf(req, sizeof(req), \"{\\\"type\\\":\\\"call\\\",\\\"obj_id\\\":%lld,\\\"method\\\":\\\"%s\\\",\\\"args\\\":%s,\\\"id\\\":%d}\",\n";
        cw << "             obj_id, fn, args_json, id);\n";
        cw << "    int new_obj_id = 0;\n";
        cw << "    char result_json[65536];\n";
        cw << "    if (!send_rpc(req, &new_obj_id, result_json, sizeof(result_json)))\n";
        cw << "        return NULL;\n";
        cw << "    void* h = store_json(result_json);\n";
        cw << "    return h;\n";
        cw << "}\n\n";

#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_call1(void* mod, const char* fn, void* a) {\n";
#else
        cw << "void* " << pkg << "_call1(void* mod, const char* fn, void* a) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s]\", a ? get_json(a) : \"null\");\n";
        cw << "    void* _" << pkg << "_arg = buf; return " << pkg << "_call(mod, fn, &_" << pkg << "_arg);\n";
        cw << "}\n\n";

#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_call2(void* mod, const char* fn, void* a, void* b) {\n";
#else
        cw << "void* " << pkg << "_call2(void* mod, const char* fn, void* a, void* b) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s]\", get_json(a), get_json(b));\n";
        cw << "    void* _" << pkg << "_arg = buf; return " << pkg << "_call(mod, fn, &_" << pkg << "_arg);\n";
        cw << "}\n\n";

#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_call3(void* mod, const char* fn, void* a, void* b, void* c) {\n";
#else
        cw << "void* " << pkg << "_call3(void* mod, const char* fn, void* a, void* b, void* c) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s,%s]\", get_json(a), get_json(b), get_json(c));\n";
        cw << "    void* _" << pkg << "_arg = buf; return " << pkg << "_call(mod, fn, &_" << pkg << "_arg);\n";
        cw << "}\n\n";

#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_call4(void* mod, const char* fn, void* a, void* b, void* c, void* d) {\n";
#else
        cw << "void* " << pkg << "_call4(void* mod, const char* fn, void* a, void* b, void* c, void* d) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s,%s,%s]\", get_json(a), get_json(b), get_json(c), get_json(d));\n";
        cw << "    void* _" << pkg << "_arg = buf; return " << pkg << "_call(mod, fn, &_" << pkg << "_arg);\n";
        cw << "}\n\n";

#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_call5(void* mod, const char* fn, void* a, void* b, void* c, void* d, void* e) {\n";
#else
        cw << "void* " << pkg << "_call5(void* mod, const char* fn, void* a, void* b, void* c, void* d, void* e) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s,%s,%s,%s]\", get_json(a), get_json(b), get_json(c), get_json(d), get_json(e));\n";
        cw << "    void* _" << pkg << "_arg = buf; return " << pkg << "_call(mod, fn, &_" << pkg << "_arg);\n";
        cw << "}\n\n";

#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_call6(void* mod, const char* fn, void* a, void* b, void* c, void* d, void* e, void* f) {\n";
#else
        cw << "void* " << pkg << "_call6(void* mod, const char* fn, void* a, void* b, void* c, void* d, void* e, void* f) {\n";
#endif
        cw << "    char buf[16384];\n";
        cw << "    snprintf(buf, sizeof(buf), \"[%s,%s,%s,%s,%s,%s]\", get_json(a), get_json(b), get_json(c), get_json(d), get_json(e), get_json(f));\n";
        cw << "    void* _" << pkg << "_arg = buf; return " << pkg << "_call(mod, fn, &_" << pkg << "_arg);\n";
        cw << "}\n\n";

        /* _call_kw */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_call_kw(void* mod, const char* fn, void* args, void* kwargs) {\n";
#else
        cw << "void* " << pkg << "_call_kw(void* mod, const char* fn, void* args, void* kwargs) {\n";
#endif
        cw << "    const char* args_str = get_json(args);\n";
        cw << "    const char* kwargs_str = get_json(kwargs);\n";
        cw << "    size_t alen = strlen(args_str);\n";
        cw << "    char buf[65536];\n";
        cw << "    if (alen >= 2 && alen <= 32768) {\n";
        cw << "        snprintf(buf, sizeof(buf), \"%.*s,%s]\", (int)(alen - 1), args_str, kwargs_str);\n";
        cw << "    } else {\n";
        cw << "        snprintf(buf, sizeof(buf), \"[%s]\", kwargs_str);\n";
        cw << "    }\n";
        cw << "    void* _" << pkg << "_arg = buf; return " << pkg << "_call(mod, fn, &_" << pkg << "_arg);\n";
        cw << "}\n\n";

        /* _getattr (with LRU cache for repeated lookups) */
#ifdef _WIN32
        cw << "__declspec(dllexport) void* " << pkg << "_getattr(void* obj, const char* name) {\n";
#else
        cw << "void* " << pkg << "_getattr(void* obj, const char* name) {\n";
#endif
        cw << "    if (!name) return NULL;\n";
        cw << "    if (!g_rpc_ok && !try_restart_bridge()) return NULL;\n";
        cw << "    long long obj_id = (obj == (void*)1) ? 1 : (long long)obj;\n";
        cw << "    /* Check cache */\n";
        cw << "    for (int i = 0; i < ATTR_CACHE_SIZE; i++) {\n";
        cw << "        if (g_attr_cache[i].obj_id == obj_id && strcmp(g_attr_cache[i].key, name) == 0) {\n";
        cw << "            char buf[64];\n";
        cw << "            snprintf(buf, sizeof(buf), \"%d\", g_attr_cache[i].result_id);\n";
        cw << "            g_perf.attr_hits++;\n";
        cw << "            return store_json(buf);\n";
        cw << "        }\n";
        cw << "    }\n";
        cw << "    g_perf.attr_misses++;\n";
        cw << "    /* Miss — do RPC */\n";
        cw << "    char req[131072];\n";
        cw << "    int id = g_rpc_next_id++;\n";
        cw << "    snprintf(req, sizeof(req), \"{\\\"type\\\":\\\"get\\\",\\\"obj_id\\\":%lld,\\\"key\\\":\\\"%s\\\",\\\"id\\\":%d}\",\n";
        cw << "             obj_id, name, id);\n";
        cw << "    int new_obj_id = 0;\n";
        cw << "    char result_json[65536];\n";
        cw << "    if (!send_rpc(req, &new_obj_id, result_json, sizeof(result_json)))\n";
        cw << "        return NULL;\n";
        cw << "    /* Store in cache */\n";
        cw << "    int slot = g_attr_cache_next++;\n";
        cw << "    if (slot >= ATTR_CACHE_SIZE) slot = 0;\n";
        cw << "    g_attr_cache[slot].obj_id = obj_id;\n";
        cw << "    strncpy(g_attr_cache[slot].key, name, sizeof(g_attr_cache[slot].key) - 1);\n";
        cw << "    g_attr_cache[slot].key[sizeof(g_attr_cache[slot].key) - 1] = '\\0';\n";
        cw << "    g_attr_cache[slot].result_id = new_obj_id;\n";
        cw << "    void* h = store_json(result_json);\n";
        cw << "    return h;\n";
        cw << "}\n\n";

        /* ── DllMain (Windows) / destructor (POSIX) for cleanup ── */
#ifdef _WIN32
        cw << "BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {\n";
        cw << "    if (fdwReason == DLL_PROCESS_DETACH) cleanup_bridge();\n";
        cw << "    return TRUE;\n";
        cw << "}\n";
#else
        cw << "__attribute__((destructor)) static void dll_cleanup(void) { cleanup_bridge(); }\n";
#endif

        /* Sanitize pkg name for C identifiers (hyphens → underscores) */
        {
            std::string c_code = cw.str();
            std::string pkg_safe = pkg;
            for (auto& c : pkg_safe) if (c == '-') c = '_';
            if (pkg_safe != pkg) {
                std::string old_pat = pkg + "_";
                std::string new_pat = pkg_safe + "_";
                size_t pos = 0;
                while ((pos = c_code.find(old_pat, pos)) != std::string::npos) {
                    c_code.replace(pos, old_pat.length(), new_pat);
                    pos += new_pat.length();
                }
            }
            std::string wrapper_path = dir + "/" + pkg + "_npm.c";
            if (write_file(wrapper_path, c_code))
                std::cout << "[bridge]   " << wrapper_path << "\n";
        }
    }
}


/* ── Sanitize package name for use as C identifier ── */
static std::string sanitize_ident(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (isalnum((unsigned char)c) || c == '_') out += c;
        else if (c == '-' || c == '.' || c == '@' || c == '/') out += '_';
    }
    if (out.empty() || !isalpha((unsigned char)out[0])) out = "pkg_" + out;
    return out;
}

/* ── Optional esbuild ESM→CJS transpilation at bridge generation time ── */
/* Runs npx esbuild to convert .mjs to .cjs for better CJS output.
   The generated load_module() will prefer the .cjs at runtime.
   Silently skips if node/esbuild is not available. */
static void run_esbuild_transpile(const std::string& entry_path) {
    if (entry_path.size() < 4 ||
        entry_path.compare(entry_path.size() - 4, 4, ".mjs") != 0)
        return;
    std::string outfile = entry_path.substr(0, entry_path.size() - 4) + ".cjs";
#ifdef _WIN32
    std::string cmd = "npx --yes esbuild \"" + entry_path
        + "\" --platform=node --format=cjs --target=es2020 --outfile=\""
        + outfile + "\" 2>nul";
#else
    std::string cmd = "npx --yes esbuild \"" + entry_path
        + "\" --platform=node --format=cjs --target=es2020 --outfile=\""
        + outfile + "\" 2>/dev/null";
#endif
    int ret = std::system(cmd.c_str());
    if (ret == 0)
        std::cout << "[bridge]   esbuild: " << entry_path << " -> " << outfile << "\n";
    /* If esbuild fails (node not found, esbuild not cached, etc.),
       silently fall back to C transpiler at runtime */
}

void gen_quickjs_npm_wrapper(const std::string& pkg, const std::string& dir)
{
    /* ── Pre-transpile .mjs files via esbuild (if available) ── */
    {
        std::vector<std::string> search_dirs;
        search_dirs.push_back(dir + "/node_modules/" + pkg);
        search_dirs.push_back("node_modules/" + pkg);
        if (fs::is_directory(dir + "/" + pkg))
            search_dirs.push_back(dir + "/" + pkg);
        for (const auto& sd : search_dirs) {
            if (!fs::is_directory(sd)) continue;
            std::error_code ec;
            for (auto& de : fs::recursive_directory_iterator(
                     sd, fs::directory_options::skip_permission_denied, ec)) {
                if (de.is_regular_file()) {
                    const std::string path = de.path().string();
                    if (path.size() >= 4 &&
                        path.compare(path.size() - 4, 4, ".mjs") == 0) {
                        run_esbuild_transpile(path);
                    }
                }
            }
        }
    }

    /* Generate the C bridge DLL using embedded QuickJS engine */
    {
        std::ostringstream cw;
        std::string pid = sanitize_ident(pkg);  /* safe C identifier */
        std::string pkg_upper = pid;
        for (auto& c : pkg_upper) c = (char)toupper((unsigned char)c);

        cw << "/* Auto-generated QuickJS npm bridge DLL for " << pkg << " */\n";
        cw << "#define _GNU_SOURCE\n";
        cw << "#include \"quickjs_config.h\"\n";
        cw << "#include \"quickjs.h\"\n";
        cw << "#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n#include <stdint.h>\n";
#ifdef _WIN32
        cw << "#define WIN32_LEAN_AND_MEAN\n#include <windows.h>\n#include <direct.h>\n#include <sys/stat.h>\n#include <io.h>\n#define EXPORT __declspec(dllexport)\n";
#else
        cw << "#include <unistd.h>\n#include <sys/stat.h>\n#include <dirent.h>\n#include <dlfcn.h>\n#define EXPORT __attribute__((visibility(\"default\")))\n";
#endif
        cw << "\nstatic JSRuntime *g_rt = NULL;\nstatic JSContext *g_ctx = NULL;\nstatic int g_inited = 0;\nstatic int g_loaded = 0;\n";
        cw << "#define MAX_OBJS 65536\nstatic JSValue g_objs[MAX_OBJS];\nstatic int g_next_id = 2;\n";
        cw << "static char g_last_error[4096]=\"\";\n";
        cw << "static void set_last_error(const char* m){strncpy(g_last_error,m,sizeof(g_last_error)-1);g_last_error[sizeof(g_last_error)-1]=0;}\n";
        cw << "/* Performance counters */\n";
        cw << "static struct{unsigned require_calls;unsigned bc_hits;unsigned bc_misses;\n";
        cw << "  unsigned exports_lookups;unsigned eval_time_ms;unsigned resolve_time_ms;}g_perf={0};\n\n";

        /* ── Embedded Node.js builtin polyfills ── */
        {
            std::string bjs;
            std::string bjs_path = quickjs_dir() + "/node_builtins.js";
            std::ifstream bjs_f(bjs_path);
            if (bjs_f.is_open()) {
                bjs.assign((std::istreambuf_iterator<char>(bjs_f)),
                           std::istreambuf_iterator<char>());
            }
            cw << "static const char node_builtins_js[] = \"";
            for (char c : bjs) {
                switch (c) {
                    case '\\': cw << "\\\\"; break;
                    case '"':  cw << "\\\""; break;
                    case '\n': cw << "\\n";  break;
                    case '\r': cw << "\\r";  break;
                    case '\t': cw << "\\t";  break;
                    default:   cw << c;      break;
                }
            }
            cw << "\";\n\n";
        }

        /* JSON helpers */
        cw << "static void json_escape(const char *in, char *out, size_t outsz) {\n";
        cw << "  size_t j=0; for(size_t i=0;in[i]&&j+6<outsz;i++){\n";
        cw << "    unsigned char c=(unsigned char)in[i];\n";
        cw << "    if(c=='\\\\'||c=='\"'){out[j++]='\\\\';out[j++]=c;}\n";
        cw << "    else if(c=='\\n'){out[j++]='\\\\';out[j++]='n';}\n";
        cw << "    else if(c=='\\r'){out[j++]='\\\\';out[j++]='r';}\n";
        cw << "    else if(c=='\\t'){out[j++]='\\\\';out[j++]='t';}\n";
        cw << "    else if(c<32){j+=snprintf(out+j,outsz-j,\"\\\\u%04x\",c);}\n";
        cw << "    else out[j++]=c;}\n";
        cw << "  out[j]='\\0';}\n\n";

        cw << "static void* store_json(const char* json){\n";
        cw << "  if(!json)return NULL;char**pp=(char**)malloc(sizeof(char*));if(!pp)return NULL;\n";
        cw << "  *pp=(char*)malloc(strlen(json)+1);if(!*pp){free(pp);return NULL;}strcpy(*pp,json);\n";
        cw << "  return (void*)((intptr_t)pp | 1);}\n";
        cw << "#define UNTAG(h) ((void*)((intptr_t)(h) & ~1))\n";
        cw << "static const char* get_json(void* handle){\n";
        cw << "  if(!handle)return\"null\";return*((const char**)UNTAG(handle));}\n\n";

        cw << "static char* jsval_to_json(JSContext*ctx,JSValue val){\n";
        cw << "  JSValue j=JS_JSONStringify(ctx,val,JS_NULL,JS_NULL);\n";
        cw << "  if(JS_IsException(j))return strdup(\"null\");\n";
        cw << "  const char*s=JS_ToCString(ctx,j);char*r=s?strdup(s):strdup(\"null\");\n";
        cw << "  if(s)JS_FreeCString(ctx,s);JS_FreeValue(ctx,j);return r;}\n\n";

        cw << "static void strip_dots(char*out,const char*md,const char*path){\n";
        cw << "  if(path[0]=='.'&&path[1]=='/')snprintf(out,4096,\"%s/%s\",md,path+2);\n";
        cw << "  else snprintf(out,4096,\"%s/%s\",md,path);}\n\n";

        cw << "/* Resolve exports key against requested subpath, handling wildcard patterns */\n";
        cw << "static void resolve_exports_key(const char*key,const char*subpath,JSValue val,\n";
        cw << "  const char*md,int*match,char*matched_path,int path_sz,JSContext*ctx){\n";
        cw << "  *match=0;matched_path[0]=0;\n";
        cw << "  /* Check if key matches subpath */\n";
        cw << "  int key_match=0;char wildcard[4096]=\"\";wildcard[0]=0;\n";
        cw << "  if(strcmp(key,subpath)==0){key_match=1;}\n";
        cw << "  else{\n";
        cw << "    /* Wildcard pattern: key contains '*' */\n";
        cw << "    const char*star=strchr(key,'*');\n";
        cw << "    if(star){\n";
        cw << "      size_t prefix_len=(size_t)(star-key);\n";
        cw << "      const char*suffix=star+1;\n";
        cw << "      size_t suffix_len=strlen(suffix);\n";
        cw << "      size_t sub_len=strlen(subpath);\n";
        cw << "      if(sub_len>=prefix_len+suffix_len&&\n";
        cw << "         strncmp(key,subpath,prefix_len)==0&&\n";
        cw << "         strncmp(suffix,subpath+sub_len-suffix_len,suffix_len)==0){\n";
        cw << "        key_match=1;\n";
        cw << "        size_t wl=sub_len-prefix_len-suffix_len;\n";
        cw << "        if(wl<sizeof(wildcard)-1){memcpy(wildcard,subpath+prefix_len,wl);wildcard[wl]=0;}\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  if(!key_match)return;\n";
        cw << "  /* Resolve value: could be string, condition object, or nested pattern */\n";
        cw << "  if(JS_IsString(val)){\n";
        cw << "    const char*vs=JS_ToCString(ctx,val);\n";
        cw << "    if(vs){char buf[4096];snprintf(buf,sizeof(buf),\"%s/%s\",md,vs[0]=='.'&&vs[1]=='/'?vs+2:vs);\n";
        cw << "      /* Substitute wildcard */\n";
        cw << "      if(wildcard[0]){char*wp=strstr(buf,\"*\");if(wp){*wp=0;snprintf(matched_path,path_sz,\"%s%s%s\",buf,wildcard,wp+1);}else snprintf(matched_path,path_sz,\"%s\",buf);}\n";
        cw << "      else snprintf(matched_path,path_sz,\"%s\",buf);\n";
        cw << "      *match=1;JS_FreeCString(ctx,vs);}\n";
        cw << "  }else if(JS_IsObject(val)){\n";
        cw << "    /* Try conditions: require > node > default > import */\n";
        cw << "    const char*conds[]={\"require\",\"node\",\"default\",\"import\",NULL};\n";
        cw << "    for(int ci=0;conds[ci]&&!*match;ci++){\n";
        cw << "      JSValue cv=JS_GetPropertyStr(ctx,val,conds[ci]);\n";
        cw << "      if(!JS_IsUndefined(cv)){\n";
        cw << "        /* Nested condition object */\n";
        cw << "        if(JS_IsObject(cv)&&!JS_IsString(cv)){\n";
        cw << "          resolve_exports_key(key,subpath,cv,md,match,matched_path,path_sz,ctx);\n";
        cw << "        }else if(JS_IsString(cv)){\n";
        cw << "          const char*cs=JS_ToCString(ctx,cv);\n";
        cw << "          if(cs){strip_dots(matched_path,md,cs);*match=1;JS_FreeCString(ctx,cs);}\n";
        cw << "        }\n";
        cw << "      }\n";
        cw << "      JS_FreeValue(ctx,cv);\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "}\n\n";
        cw << "static JSValue js_rust_load(JSContext*ctx,const char*name);\n";
        cw << "static JSValue bridge_npm_install(JSContext*ctx,const char*name);\n";
        cw << "static JSValue js_bridge_npm_install(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_console_log(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_pstdout(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_readFile(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_writeFile(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_exists(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_mkdir(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_readdir(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_stat(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_fs_unlink(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_exec(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_http_get(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "/* Current working directory for module resolution */\n";
        cw << "static char g_current_dir[4096]=\".\";\n\n";

        /* ── ESM→CJS transpiler ── */
        cw << "static char* bridge_esm_to_cjs(const char*s,size_t sl,size_t*ol){\n";
        cw << "  char*d=(char*)malloc(sl*3+8192);if(!d)return NULL;size_t di=0,i=0;\n";
        cw << "  int q1=0,q2=0,mlc=0;char pending[8192];size_t pi=0;\n";
        cw << "  #define P(s) do{const char*_p_=(s);size_t _pl_=strlen(_p_);memcpy(d+di,_p_,_pl_);di+=_pl_;}while(0)\n";
        cw << "  #define PC(s) do{P(s);pi=0;}while(0)\n";
        cw << "  #define FL(s) do{if(pi+(int)strlen(s)+1<(int)sizeof(pending)){memcpy(pending+pi,s,strlen(s));pi+=strlen(s);}}while(0)\n";
        cw << "  pending[0]=0;\n";
        cw << "  while(i<sl){\n";
        cw << "    if(mlc){d[di++]=s[i];if(s[i]=='*'&&i+1<sl&&s[i+1]=='/'){d[di++]=s[++i];mlc=0;}i++;continue;}\n";
        cw << "    if(q1){d[di++]=s[i];if(s[i]=='\\\\'&&i+1<sl)d[di++]=s[++i];else if(s[i]=='\\'')q1=0;i++;continue;}\n";
        cw << "    if(q2){d[di++]=s[i];if(s[i]=='\\\\'&&i+1<sl)d[di++]=s[++i];else if(s[i]=='\"')q2=0;i++;continue;}\n";
        cw << "    if(s[i]=='/'&&i+1<sl){if(s[i+1]=='/'){d[di++]=s[i++];d[di++]=s[i++];\n";
        cw << "      while(i<sl&&s[i]!='\\n')d[di++]=s[i++];continue;}\n";
        cw << "      if(s[i+1]=='*'){d[di++]=s[i++];d[di++]=s[i++];mlc=1;continue;}}\n";
        cw << "    if(s[i]=='\\''){q1=1;d[di++]=s[i++];continue;}\n";
        cw << "    if(s[i]=='\"'){q2=1;d[di++]=s[i++];continue;}\n";
        cw << "    if(s[i]=='`'){d[di++]=s[i++];while(i<sl&&s[i]!='`'){d[di++]=s[i++];}if(i<sl)d[di++]=s[i++];continue;}\n";
        cw << "    /* Detect 'import ' at line start (after whitespace) */\n";
        cw << "    {int ws=i;while(ws<(int)sl&&(s[ws]==' '||s[ws]=='\\t'))ws++;\n";
        cw << "    if(ws+6<(int)sl&&strncmp(s+ws,\"import\",6)==0&&s[ws+6]<=' '){\n";
        cw << "      int si=ws+7;while(si<(int)sl&&s[si]<=' ')si++;\n";
        cw << "      if(si<(int)sl&&s[si]=='\"'){/* import \"x\" */\n";
        cw << "        int ei=si+1;while(ei<(int)sl&&s[ei]!='\"')ei++;\n";
        cw << "        P(\"require(\");for(int j=si;j<=ei;j++)d[di++]=s[j];P(\");\");\n";
        cw << "        while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';')i++;\n";
        cw << "        continue;\n";
        cw << "      }\n";
        cw << "      /* import X from 'y' or import {X} from 'y' or import * as X from 'y' */\n";
        cw << "      int br=0;char bind[4096];int bi=0;\n";
        cw << "      while(si<(int)sl&&s[si]!=';'&&s[si]!='\\n'&&s[si]!='\\r'){\n";
        cw << "        if(s[si]=='{'||s[si]=='*'){br=1;}\n";
        cw << "        if(bi<(int)sizeof(bind)-1)bind[bi++]=s[si];si++;\n";
        cw << "      }\n";
        cw << "      bind[bi]=0;\n";
        cw << "      /* Find 'from' keyword */\n";
        cw << "      char*fr=strstr(bind,\"from\");if(fr){*fr=0;fr+=4;}\n";
        cw << "      /* Trim bind and from */\n";
        cw << "      int bj=(int)strlen(bind);while(bj>0&&bind[bj-1]<=' ')bind[--bj]=0;\n";
        cw << "      const char*mod=fr?fr:bind;\n";
        cw << "      while(*mod<=' ')mod++;\n";
        cw << "      /* Strip quotes from module */\n";
        cw << "      char modq[4096];int mqi=0;\n";
        cw << "      if(*mod=='\"'||*mod=='\\''){mod++;while(mod[mqi]&&mod[mqi]!='\"'&&mod[mqi]!='\\''){modq[mqi]=mod[mqi];mqi++;}modq[mqi]=0;}else snprintf(modq,sizeof(modq),\"%s\",mod);\n";
        cw << "      /* Check if it's a namespace import: * as X */\n";
        cw << "      char*star=strstr(bind,\"*\");\n";
        cw << "      if(star){char*as=strstr(star+1,\"as\");if(as){as+=2;while(*as<=' ')as++;\n";
        cw << "        P(\"const \");while(*as>' ')d[di++]=*as++;P(\"=require(\\\"\");P(modq);P(\"\\\")\");}\n";
        cw << "      }else if(!br){/* default import: X from 'y' */\n";
        cw << "        P(\"const \");P(bind);P(\"=require(\\\"\");P(modq);P(\"\\\").default||require(\\\"\");P(modq);P(\"\\\")\");\n";
        cw << "      }else{/* named import: {X,Y} from 'y' */\n";
        cw << "        P(\"const \");P(bind);P(\"=require(\\\"\");P(modq);P(\"\\\")\");}\n";
        cw << "      while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n'&&s[i]!='\\r')i++;if(s[i]==';')i++;\n";
        cw << "      continue;\n";
        cw << "    }}\n";
        cw << "    /* Detect 'export ' at line start (after whitespace) */\n";
        cw << "    {int ws=i;while(ws<(int)sl&&(s[ws]==' '||s[ws]=='\\t'))ws++;\n";
        cw << "    if(ws+7<(int)sl&&strncmp(s+ws,\"export \",7)==0){\n";
        cw << "      int si=ws+7;while(si<(int)sl&&s[si]<=' ')si++;\n";
        cw << "      if(strncmp(s+si,\"default\",7)==0){/* export default */\n";
        cw << "        si+=7;while(si<(int)sl&&s[si]<=' ')si++;\n";
        cw << "        int ei=si,dp=1;while(ei<(int)sl){if(s[ei]=='{')dp++;if(s[ei]=='}')dp--;if(s[ei]==';'&&dp==1)break;ei++;}\n";
        cw << "        P(\"module.exports.default=(\");for(int j=si;j<ei;j++)d[di++]=s[j];P(\")\");if(s[ei]==';'){d[di++]=s[ei];}else P(\";\");\n";
        cw << "        while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';')i++;continue;\n";
        cw << "      }\n";
        cw << "      if(s[si]=='{'){/* export {X,Y} */\n";
        cw << "        int ei=si+1;while(ei<(int)sl&&s[ei]!='}')ei++;\n";
        cw << "        char exps[4096];int eii=0;for(int j=si+1;j<ei;j++)if(s[j]>' ')exps[eii++]=s[j];exps[eii]=0;\n";
        cw << "        char*tk=strtok(exps,\",\");while(tk){\n";
        cw << "          P(\"module.exports.\");P(tk);P(\"=\");P(tk);P(\";\");tk=strtok(NULL,\",\");}\n";
        cw << "        /* Check for 'from' */\n";
        cw << "        char*ff=strstr(s+ei+1,\"from\");\n";
        cw << "        if(ff){while(*ff<=' ')ff++;ff+=4;while(*ff<=' ')ff++;\n";
        cw << "          char modq[256];int mi=0;\n";
        cw << "          if(*ff=='\"'||*ff=='\\''){ff++;while(*ff&&*ff!='\"'&&*ff!='\\'')modq[mi++]=*ff++;}modq[mi]=0;\n";
        cw << "          P(\"Object.assign(module.exports,require(\\\"\");P(modq);P(\"\\\"));\");}\n";
        cw << "        while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';')i++;continue;\n";
        cw << "      }\n";
        cw << "      if(s[si]=='*'){/* export * from 'y' */\n";
        cw << "        char*ff=strstr(s+si,\"from\");\n";
        cw << "        if(ff){ff+=4;while(*ff<=' ')ff++;\n";
        cw << "          char modq[256];int mi=0;\n";
        cw << "          if(*ff=='\"'||*ff=='\\''){ff++;while(*ff&&*ff!='\"'&&*ff!='\\'')modq[mi++]=*ff++;}modq[mi]=0;\n";
        cw << "          P(\"Object.assign(module.exports,require(\\\"\");P(modq);P(\"\\\"));\");}\n";
        cw << "        while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';')i++;continue;\n";
        cw << "      }\n";
        cw << "      /* export function/const/class/let/var Name ... */\n";
        cw << "      int sni=si;char name[256];int ni=0;\n";
        cw << "      while(sni<(int)sl&&s[sni]!=' '&&s[sni]!='\\t'&&s[sni]!='(')sni++;\n";
        cw << "      while(sni<(int)sl&&(s[sni]==' '||s[sni]=='\\t'))sni++;\n";
        cw << "      while(sni<(int)sl&&(s[sni]>' '&&s[sni]!='('&&s[sni]!='='&&s[sni]!='{')){if(ni<255)name[ni++]=s[sni];sni++;}name[ni]=0;\n";
        cw << "      P(\"module.exports.\");P(name);P(\"=(\");\n";
        cw << "      for(int j=si;j<(int)sl&&s[j]!=';'&&s[j]!='\\n';j++)d[di++]=s[j];\n";
        cw << "      P(\");\");while(i<(int)sl&&s[i]!=';'&&s[i]!='\\n')i++;if(s[i]==';'){d[di++]=s[i];i++;}continue;\n";
        cw << "    }}\n";
        cw << "    d[di++]=s[i++];\n";
        cw << "  }\n";
        cw << "  d[di]=0;*ol=di;return d;}\n\n";

        /* Module loader */
        cw << "static JSValue load_module(JSContext*ctx,const char*path,const char*name){\n";
        cw << "  FILE*f=fopen(path,\"rb\");if(!f){set_last_error(\"load_module: fopen failed\");return JS_ThrowReferenceError(ctx,\"mod %s\",name);}\n";
        cw << "  fseek(f,0,SEEK_END);long len=ftell(f);fseek(f,0,SEEK_SET);\n";
        cw << "  char*code=(char*)malloc((size_t)len+1);\n";
        cw << "  if(!code){fclose(f);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "  fread(code,1,len,f);code[len]=0;fclose(f);\n";
        cw << "  JSValue ex=JS_NewObject(ctx),mo=JS_NewObject(ctx);\n";
        cw << "  JS_SetPropertyStr(ctx,mo,\"exports\",JS_DupValue(ctx,ex));\n";
        cw << "  char dir[4096];strncpy(dir,path,sizeof(dir)-1);dir[sizeof(dir)-1]=0;\n";
        cw << "  char*p=strrchr(dir,'/');\n";
#ifdef _WIN32
        cw << "  char*p2=strrchr(dir,'\\\\');if(p2>p)p=p2;\n";
#endif
        cw << "  if(p)*p=0;\n";
        cw << "  /* Prefer pre-transpiled .cjs file (generated by esbuild at bridge-generation time) */\n";
        cw << "  int need_transpile=0;\n";
        cw << "  if(strlen(path)>4&&strcmp(path+strlen(path)-4,\".mjs\")==0){\n";
        cw << "    need_transpile=1;\n";
        cw << "    char cjs_path[4096];\n";
        cw << "    strncpy(cjs_path,path,sizeof(cjs_path)-1);cjs_path[sizeof(cjs_path)-1]=0;\n";
        cw << "    strcpy(cjs_path+strlen(cjs_path)-4,\".cjs\");\n";
        cw << "    FILE*cjs_f=fopen(cjs_path,\"rb\");\n";
        cw << "    if(cjs_f){\n";
        cw << "      fclose(cjs_f);free(code);\n";
        cw << "      cjs_f=fopen(cjs_path,\"rb\");if(!cjs_f){set_last_error(\"load_module: .cjs fopen failed\");JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_ThrowReferenceError(ctx,\"mod %s\",name);}\n";
        cw << "      fseek(cjs_f,0,SEEK_END);len=ftell(cjs_f);fseek(cjs_f,0,SEEK_SET);\n";
        cw << "      code=(char*)malloc((size_t)len+1);\n";
        cw << "      if(!code){fclose(cjs_f);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "      fread(code,1,len,cjs_f);code[len]=0;fclose(cjs_f);need_transpile=0;\n";
        cw << "    }\n";
        cw << "  }else if(strlen(path)>3&&strcmp(path+strlen(path)-3,\".js\")==0){\n";
        cw << "    /* Check nearest package.json for \"type\":\"module\" */\n";
        cw << "    char pj_check[4096];strncpy(pj_check,path,sizeof(pj_check)-1);\n";
        cw << "    while(1){\n";
        cw << "      char*ls=strrchr(pj_check,'/');\n";
#ifdef _WIN32
        cw << "      char*bs=strrchr(pj_check,'\\\\');if(bs>ls)ls=bs;\n";
#endif
        cw << "      if(!ls)break;*ls=0;\n";
        cw << "      char pjp[4096];snprintf(pjp,sizeof(pjp),\"%s/package.json\",pj_check);\n";
        cw << "      FILE*pjf=fopen(pjp,\"rb\");\n";
        cw << "      if(pjf){\n";
        cw << "        fseek(pjf,0,SEEK_END);long pjl=ftell(pjf);fseek(pjf,0,SEEK_SET);\n";
        cw << "        char*pj=(char*)malloc((size_t)pjl+1);\n";
        cw << "        if(pj){fread(pj,1,pjl,pjf);pj[pjl]=0;\n";
        cw << "          char*tp=strstr(pj,\"\\\"type\\\"\");\n";
        cw << "          if(tp){tp+=6;while(*tp&&*tp<=' ')tp++;if(*tp==':'){tp++;while(*tp&&*tp<=' ')tp++;\n";
        cw << "            if(*tp=='\"'){tp++;if(strncmp(tp,\"module\",6)==0)need_transpile=1;}}\n";
        cw << "          }\n";
        cw << "          free(pj);\n";
        cw << "        }\n";
        cw << "        fclose(pjf);break;\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  if(need_transpile){\n";
        cw << "    size_t slen=(size_t)len;\n";
        cw << "    char*tjs=bridge_esm_to_cjs(code,slen,&slen);\n";
        cw << "    if(tjs){free(code);code=tjs;len=(long)slen;}else{free(code);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "  }\n";
        cw << "  /* Set current dir for module resolution */\n";
        cw << "  char prev_dir[4096];strncpy(prev_dir,g_current_dir,sizeof(prev_dir)-1);\n";
        cw << "  strncpy(g_current_dir,dir,sizeof(g_current_dir)-1);\n";
        cw << "  JSValue g=JS_GetGlobalObject(ctx);\n";
        cw << "  JS_SetPropertyStr(ctx,g,\"exports\",JS_DupValue(ctx,ex));\n";
        cw << "  JS_SetPropertyStr(ctx,g,\"module\",JS_DupValue(ctx,mo));\n";
        cw << "  JS_SetPropertyStr(ctx,g,\"__filename\",JS_NewString(ctx,path));\n";
        cw << "  JS_SetPropertyStr(ctx,g,\"__dirname\",JS_NewString(ctx,dir));\n";
        cw << "  JS_FreeValue(ctx,g);\n";
        cw << "  /* Bytecode cache: check for .qbc file */\n";
        cw << "  char qbc_path[4096];snprintf(qbc_path,sizeof(qbc_path),\"%s.qbc\",path);\n";
        cw << "  FILE*qbc_f=fopen(qbc_path,\"rb\");JSValue r;\n";
        cw << "  if(qbc_f){\n";
        cw << "    fseek(qbc_f,0,SEEK_END);long qbc_len=ftell(qbc_f);fseek(qbc_f,0,SEEK_SET);\n";
        cw << "    uint8_t*qbc_buf=(uint8_t*)malloc((size_t)qbc_len);\n";
        cw << "    if(qbc_buf){fread(qbc_buf,1,qbc_len,qbc_f);r=JS_ReadObject(ctx,qbc_buf,qbc_len,JS_READ_OBJ_BYTECODE);free(qbc_buf);}\n";
        cw << "    else{r=JS_ThrowOutOfMemory(ctx);}\n";
        cw << "    fclose(qbc_f);free(code);g_perf.bc_hits++;\n";
        cw << "  }else{\n";
        cw << "    g_perf.bc_misses++;\n";
        cw << "#ifdef _WIN32\n";
        cw << "    LARGE_INTEGER e0,ef,efreq;QueryPerformanceFrequency(&efreq);QueryPerformanceCounter(&e0);\n";
        cw << "#else\n";
        cw << "    struct timespec e0,ef;clock_gettime(CLOCK_MONOTONIC,&e0);\n";
        cw << "#endif\n";
        cw << "    r=JS_Eval(ctx,code,(size_t)len,path,JS_EVAL_TYPE_GLOBAL);free(code);\n";
        cw << "#ifdef _WIN32\n";
        cw << "    QueryPerformanceCounter(&ef);g_perf.eval_time_ms+=(unsigned)(((ef.QuadPart-e0.QuadPart)*1000)/efreq.QuadPart);\n";
        cw << "#else\n";
        cw << "    clock_gettime(CLOCK_MONOTONIC,&ef);g_perf.eval_time_ms+=(unsigned)(((ef.tv_sec-e0.tv_sec)*1000+(ef.tv_nsec-e0.tv_nsec)/1000000));\n";
        cw << "#endif\n";
        cw << "    if(!JS_IsException(r)){\n";
        cw << "      size_t out_len;\n";
        cw << "      uint8_t*out_buf=JS_WriteObject(ctx,&out_len,r,JS_WRITE_OBJ_BYTECODE);\n";
        cw << "      if(out_buf){\n";
        cw << "        FILE*qbc_w=fopen(qbc_path,\"wb\");\n";
        cw << "        if(qbc_w){fwrite(out_buf,1,out_len,qbc_w);fclose(qbc_w);}\n";
        cw << "        js_free(ctx,out_buf);\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  /* Restore previous dir */\n";
        cw << "  strncpy(g_current_dir,prev_dir,sizeof(g_current_dir)-1);\n";
        cw << "  if(JS_IsException(r)){JS_FreeValue(ctx,r);JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return JS_EXCEPTION;}\n";
        cw << "  JS_FreeValue(ctx,r);\n";
        cw << "  JSValue fe=JS_GetPropertyStr(ctx,mo,\"exports\");\n";
        cw << "  JS_FreeValue(ctx,mo);JS_FreeValue(ctx,ex);return fe;}\n\n";

        /* Node module resolution */
        cw << "static char* resolve_mod(const char*name,const char*from){\n";
        cw << "  g_perf.exports_lookups++;\n";
        cw << "  static char buf[4096];char dir[4096];\n";
        cw << "  strncpy(dir,from?from:\".\",sizeof(dir)-1);dir[sizeof(dir)-1]=0;\n";
        cw << "  /* Relative path: try direct .js/.cjs/.mjs, /index.*, and /package.json */\n";
        cw << "  if(name[0]=='.'){\n";
        cw << "    snprintf(buf,sizeof(buf),\"%s/%s\",dir,name);\n";
        cw << "    /* Try exact match first */\n";
        cw << "    FILE*f=fopen(buf,\"rb\");if(f){fclose(f);return buf;}\n";
        cw << "    /* Try .js, .cjs, .mjs */\n";
        cw << "    char t[4096];\n";
        cw << "    const char*exts[]={\".js\",\".cjs\",\".mjs\",NULL};\n";
        cw << "    for(int ei=0;exts[ei];ei++){snprintf(t,sizeof(t),\"%s%s\",buf,exts[ei]);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}\n";
        cw << "    /* Try index.js, index.mjs, index.cjs */\n";
        cw << "    const char*idx[]={\"index.js\",\"index.mjs\",\"index.cjs\",NULL};\n";
        cw << "    for(int ii=0;idx[ii];ii++){snprintf(t,sizeof(t),\"%s/%s\",buf,idx[ii]);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}\n";
        cw << "    /* Try package.json */\n";
        cw << "    snprintf(t,sizeof(t),\"%s/package.json\",buf);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}\n";
        cw << "    return NULL;\n";
        cw << "  }\n";
        cw << "  /* Two-phase resolution for nested paths (a/b/c) */\n";
        cw << "  const char*slash_in_name=strchr(name,'/');\n";
        cw << "  if(slash_in_name){\n";
        cw << "    /* Phase 1: extract package name */\n";
        cw << "    size_t pkg_len=(size_t)(slash_in_name-name);\n";
        cw << "    char pkg_name[256];if(pkg_len<sizeof(pkg_name)){\n";
        cw << "      memcpy(pkg_name,name,pkg_len);pkg_name[pkg_len]=0;\n";
        cw << "      const char*subpath=slash_in_name+1;\n";
        cw << "      char pkg_root[4096];char tmp[4096];strncpy(tmp,dir,sizeof(tmp)-1);\n";
        cw << "      while(tmp[0]){\n";
        cw << "        snprintf(pkg_root,sizeof(pkg_root),\"%s/node_modules/%s/package.json\",tmp,pkg_name);\n";
        cw << "        FILE*f=fopen(pkg_root,\"rb\");\n";
        cw << "        if(f){fclose(f);\n";
        cw << "          snprintf(pkg_root,sizeof(pkg_root),\"%s/node_modules/%s\",tmp,pkg_name);\n";
        cw << "          /* Phase 2: resolve subpath relative to package root */\n";
        cw << "          snprintf(buf,sizeof(buf),\"%s/%s\",pkg_root,subpath);\n";
        cw << "          FILE*tf=fopen(buf,\"rb\");if(tf){fclose(tf);return buf;}\n";
        cw << "          /* Try with .js, .cjs, .mjs */\n";
        cw << "          const char*exts2[]={\".js\",\".cjs\",\".mjs\",NULL};\n";
        cw << "          for(int ei=0;exts2[ei];ei++){snprintf(tmp,sizeof(tmp),\"%s/%s%s\",pkg_root,subpath,exts2[ei]);tf=fopen(tmp,\"rb\");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}}\n";
        cw << "          /* Try /index.js, /index.mjs, /index.cjs */\n";
        cw << "          snprintf(tmp,sizeof(tmp),\"%s/%s/index.js\",pkg_root,subpath);tf=fopen(tmp,\"rb\");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}\n";
        cw << "          snprintf(tmp,sizeof(tmp),\"%s/%s/index.mjs\",pkg_root,subpath);tf=fopen(tmp,\"rb\");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}\n";
        cw << "          snprintf(tmp,sizeof(tmp),\"%s/%s/index.cjs\",pkg_root,subpath);tf=fopen(tmp,\"rb\");if(tf){fclose(tf);strncpy(buf,tmp,sizeof(buf)-1);return buf;}\n";
        cw << "          /* Not found via two-phase — fall through to flat search */\n";
        cw << "          break;\n";
        cw << "        }\n";
        cw << "        char*p=strrchr(tmp,'/');\n";
#ifdef _WIN32
        cw << "        char*p2=strrchr(tmp,'\\\\');if(p2>p)p=p2;\n";
#endif
        cw << "        if(!p||p==tmp)break;*p=0;\n";
        cw << "      }\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  /* Flat search (original algorithm) */\n";
        cw << "  while(1){\n";
        cw << "    snprintf(buf,sizeof(buf),\"%s/node_modules/%s/package.json\",dir,name);\n";
        cw << "    FILE*f=fopen(buf,\"rb\");if(f){fclose(f);snprintf(buf,sizeof(buf),\"%s/node_modules/%s\",dir,name);return buf;}\n";
        cw << "    const char*exts[]={\".js\",\".cjs\",\".mjs\",NULL};\n";
        cw << "    char t[4096];\n";
        cw << "    for(int ei=0;exts[ei];ei++){snprintf(t,sizeof(t),\"%s/node_modules/%s%s\",dir,name,exts[ei]);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}\n";
        cw << "    const char*idx2[]={\"index.js\",\"index.mjs\",\"index.cjs\",NULL};\n";
        cw << "    for(int ii=0;idx2[ii];ii++){snprintf(t,sizeof(t),\"%s/node_modules/%s/%s\",dir,name,idx2[ii]);f=fopen(t,\"rb\");if(f){fclose(f);strncpy(buf,t,sizeof(buf)-1);return buf;}}\n";
        cw << "    char*p=strrchr(dir,'/');\n";
#ifdef _WIN32
        cw << "    char*p2=strrchr(dir,'\\\\');if(p2>p)p=p2;\n";
#endif
        cw << "    if(!p||p==dir)break;*p=0;}\n";
        cw << "  return NULL;}\n\n";

        /* js_require implementation */
        /* require.cache in C (simple array of {name, exports}) */
        cw << "#define MAX_CACHED 256\n";
        cw << "static const char* g_cache_names[MAX_CACHED];\n";
        cw << "static JSValue g_cache_exports[MAX_CACHED];\n";
        cw << "static int g_cache_count=0;\n\n";

        cw << "static JSValue js_require(JSContext*ctx,JSValueConst t,int argc,JSValueConst*argv){\n";
        cw << "  g_perf.require_calls++;\n";
        cw << "  if(argc<1)return JS_ThrowTypeError(ctx,\"require:missing arg\");\n";
        cw << "  const char*name=JS_ToCString(ctx,argv[0]);if(!name)return JS_ThrowTypeError(ctx,\"bad arg\");\n";
        cw << "  /* Check require.cache first */\n";
        cw << "  for(int i=0;i<g_cache_count;i++){\n";
        cw << "    if(strcmp(g_cache_names[i],name)==0){\n";
        cw << "      JS_FreeCString(ctx,name);return JS_DupValue(ctx,g_cache_exports[i]);\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  /* Check Node.js builtins first */\n";
        cw << "  JSValue glob=JS_GetGlobalObject(ctx);\n";
        cw << "  JSValue bm=JS_GetPropertyStr(ctx,glob,\"__node_builtins\");\n";
        cw << "  if(!JS_IsUndefined(bm)){\n";
        cw << "    JSValue bmod=JS_GetPropertyStr(ctx,bm,name);\n";
        cw << "    if(!JS_IsUndefined(bmod)){\n";
        cw << "      JS_FreeCString(ctx,name);JS_FreeValue(ctx,bm);JS_FreeValue(ctx,glob);\n";
        cw << "      return bmod;\n";
        cw << "    }\n";
        cw << "    JS_FreeValue(ctx,bmod);\n";
        cw << "  }\n";
        cw << "  JS_FreeValue(ctx,bm);JS_FreeValue(ctx,glob);\n";
        cw << "  char*md=resolve_mod(name,g_current_dir);\n";
        cw << "  /* If not found on disk, auto-install npm or load rust crate */\n";
        cw << "  if(!md){\n";
        cw << "    if(strncmp(name,\"rust:\",5)==0){\n";
        cw << "      JSValue rmod=js_rust_load(ctx,name+5);\n";
        cw << "      JS_FreeCString(ctx,name);\n";
        cw << "      if(!JS_IsException(rmod)&&g_cache_count<MAX_CACHED){\n";
        cw << "        char* dn=strdup(name);if(dn){g_cache_names[g_cache_count]=dn;g_cache_exports[g_cache_count]=JS_DupValue(ctx,rmod);g_cache_count++;}\n";
        cw << "      }\n";
        cw << "      return rmod;\n";
        cw << "    }\n";
        cw << "    /* Auto-install missing npm package */\n";
        cw << "    JSValue bni=bridge_npm_install(ctx,name);\n";
        cw << "    if(JS_IsException(bni)||(JS_IsBool(bni)&&!JS_VALUE_GET_BOOL(bni))){\n";
        cw << "      JS_FreeValue(ctx,bni);\n";
        cw << "      JS_FreeCString(ctx,name);\n";
        cw << "      return JS_ThrowReferenceError(ctx,\"module %s not found and auto-install failed\",name);\n";
        cw << "    }\n";
        cw << "    md=resolve_mod(name,g_current_dir);\n";
        cw << "    if(!md){JS_FreeCString(ctx,name);return JS_ThrowReferenceError(ctx,\"module %s installed but not resolved\",name);}\n";
        cw << "  }\n";
        cw << "  char entry[4096],pj[4096];snprintf(pj,sizeof(pj),\"%s/package.json\",md);\n";
        cw << "  FILE*f=fopen(pj,\"rb\");\n";
        cw << "  if(f){\n";
        cw << "    fseek(f,0,SEEK_END);long jl=ftell(f);fseek(f,0,SEEK_SET);\n";
        cw << "    char*js=(char*)malloc((size_t)jl+1);if(!js){fclose(f);JS_FreeCString(ctx,name);return JS_ThrowReferenceError(ctx,\"OOM reading package.json\");}fread(js,1,jl,f);js[jl]=0;fclose(f);\n";
        cw << "    JSValue po=JS_ParseJSON(ctx,js,(size_t)jl,\"<pkg>\");free(js);\n";
        cw << "    if(!JS_IsException(po)){\n";
        cw << "      int resolved=0;\n";
        cw << "      /* Try 'exports' map before 'main' */\n";
        cw << "      JSValue ev=JS_GetPropertyStr(ctx,po,\"exports\");\n";
        cw << "      if(JS_IsString(ev)){\n";
        cw << "        const char*es=JS_ToCString(ctx,ev);\n";
        cw << "        if(es){strip_dots(entry,md,es);resolved=1;JS_FreeCString(ctx,es);}\n";
        cw << "        JS_FreeValue(ctx,ev);\n";
        cw << "      }else if(JS_IsObject(ev)){\n";
        cw << "        /* Determine the subpath requested */\n";
        cw << "        const char*subpath=\".\";\n";
        cw << "        char subbuf[4096];\n";
        cw << "        const char*slash=strchr(name,'/');\n";
        cw << "        if(slash&&slash>name){snprintf(subbuf,sizeof(subbuf),\"./%s\",slash+1);subpath=subbuf;}\n";
        cw << "        char best_match[4096];best_match[0]=0;\n";
        cw << "        /* Iterate exports keys using JS_GetOwnPropertyNames */\n";
        cw << "        {\n";
        cw << "          JSPropertyEnum*ptab=NULL;uint32_t plen=0;\n";
        cw << "          if(JS_GetOwnPropertyNames(ctx,&ptab,&plen,ev,JS_GPN_STRING_MASK)==0){\n";
        cw << "            for(uint32_t ki=0;ki<plen;ki++){\n";
        cw << "              const char*key=JS_AtomToCString(ctx,ptab[ki].atom);\n";
        cw << "              if(key){\n";
        cw << "                JSValue val=JS_GetPropertyStr(ctx,ev,key);\n";
        cw << "                int match=0;char matched_path[4096]=\"\";\n";
        cw << "                resolve_exports_key(key,subpath,val,md,&match,matched_path,sizeof(matched_path),ctx);\n";
        cw << "                if(match&&(!best_match[0]||strlen(key)>strlen(best_match))){\n";
        cw << "                  strncpy(best_match,key,sizeof(best_match)-1);\n";
        cw << "                  strncpy(entry,matched_path,sizeof(entry)-1);resolved=1;\n";
        cw << "                }\n";
        cw << "                JS_FreeValue(ctx,val);JS_FreeCString(ctx,key);\n";
        cw << "              }\n";
        cw << "            }\n";
        cw << "            JS_FreePropertyEnum(ctx,ptab,plen);\n";
        cw << "          }\n";
        cw << "        }\n";
        cw << "        JS_FreeValue(ctx,ev);\n";
        cw << "      }else JS_FreeValue(ctx,ev);\n";
        cw << "      if(!resolved){\n";
        cw << "        JSValue mv=JS_GetPropertyStr(ctx,po,\"main\");\n";
        cw << "        if(!JS_IsUndefined(mv)){\n";
        cw << "          const char*ms=JS_ToCString(ctx,mv);\n";
        cw << "          if(ms){strip_dots(entry,md,ms);JS_FreeCString(ctx,ms);}\n";
        cw << "          else snprintf(entry,sizeof(entry),\"%s/index.js\",md);\n";
        cw << "          JS_FreeValue(ctx,mv);\n";
        cw << "        }else snprintf(entry,sizeof(entry),\"%s/index.js\",md);\n";
        cw << "      }\n";
        cw << "      JS_FreeValue(ctx,po);\n";
        cw << "    }else snprintf(entry,sizeof(entry),\"%s/index.js\",md);\n";
        cw << "  }else{\n";
        cw << "    if(strstr(md,\".js\"))snprintf(entry,sizeof(entry),\"%s\",md);\n";
        cw << "    else snprintf(entry,sizeof(entry),\"%s/index.js\",md);\n";
        cw << "  }\n";
        cw << "  /* Pre-register in cache BEFORE eval to support circular dependencies */\n";
        cw << "  int cache_slot=-1;\n";
        cw << "  for(int i=0;i<g_cache_count;i++){if(strcmp(g_cache_names[i],name)==0){cache_slot=i;break;}}\n";
        cw << "  if(cache_slot<0&&g_cache_count<MAX_CACHED){\n";
        cw << "    char* dn=strdup(name);if(!dn){JS_FreeCString(ctx,name);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "    JSValue ce=JS_NewObject(ctx);if(JS_IsException(ce)){free(dn);JS_FreeCString(ctx,name);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "    cache_slot=g_cache_count++;\n";
        cw << "    g_cache_names[cache_slot]=dn;\n";
        cw << "    g_cache_exports[cache_slot]=ce;\n";
        cw << "  }\n";
        cw << "  JSValue ret=load_module(ctx,entry,name);\n";
        cw << "  if(!JS_IsException(ret)&&cache_slot>=0){\n";
        cw << "    JS_FreeValue(ctx,g_cache_exports[cache_slot]);\n";
        cw << "    g_cache_exports[cache_slot]=JS_DupValue(ctx,ret);\n";
        cw << "  }\n";
        cw << "  JS_FreeCString(ctx,name);return ret;}\n\n";

        /* ── Forward declarations used by init_qjs ── */
        cw << "static JSValue js_process_cwd(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_process_exit(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_process_nextTick(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue js_process_env(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_crypto_hash(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_crypto_random(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_net_connect(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_net_write(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_net_read(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_net_close(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_tls_connect(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_tls_write(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_tls_read(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_tls_close(JSContext*,JSValueConst,int,JSValueConst*);\n";
        cw << "static JSValue bridge_dns_lookup(JSContext*,JSValueConst,int,JSValueConst*);\n";

        /* QuickJS init */
        cw << "static int init_qjs(const char*dir){\n";
        cw << "  if(g_inited)return 1;g_rt=JS_NewRuntime();if(!g_rt){set_last_error(\"init: JS_NewRuntime failed\");return 0;}\n";
        cw << "  g_ctx=JS_NewContext(g_rt);if(!g_ctx){JS_FreeRuntime(g_rt);g_rt=NULL;return 0;}\n";
        cw << "  if(dir){\n";
#ifdef _WIN32
        cw << "    char dll_name[256];snprintf(dll_name,sizeof(dll_name),\"%s.dll\",dir);\n";
        cw << "    HMODULE hm=GetModuleHandleA(dll_name);\n";
        cw << "    if(hm){char dll_path[1024];GetModuleFileNameA(hm,dll_path,sizeof(dll_path));\n";
        cw << "      char*last=strrchr(dll_path,'\\\\');if(last)*last='\\0';\n";
        cw << "      SetCurrentDirectoryA(dll_path);}\n";
#else
        cw << "    /* POSIX: resolve dir relative to /proc/self/exe or dladdr */\n";
        cw << "    chdir(dir);\n";
#endif
        cw << "  }\n";
        cw << "  JSValue g=JS_GetGlobalObject(g_ctx);\n";
        cw << "  JSValue c=JS_NewObject(g_ctx);\n";
        cw << "  JS_SetPropertyStr(g_ctx,c,\"log\",JS_NewCFunction(g_ctx,js_console_log,\"log\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,c,\"warn\",JS_NewCFunction(g_ctx,js_console_log,\"warn\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,c,\"error\",JS_NewCFunction(g_ctx,js_console_log,\"error\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"console\",c);\n";
        cw << "  JSValue p=JS_NewObject(g_ctx);JSValue so=JS_NewObject(g_ctx);\n";
        cw << "  JS_SetPropertyStr(g_ctx,so,\"write\",JS_NewCFunction(g_ctx,js_pstdout,\"write\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"stdout\",so);\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"cwd\",JS_NewCFunction(g_ctx,js_process_cwd,\"cwd\",0));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"exit\",JS_NewCFunction(g_ctx,js_process_exit,\"exit\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"nextTick\",JS_NewCFunction(g_ctx,js_process_nextTick,\"nextTick\",1));\n";
        cw << "  {JSValue ev=js_process_env(g_ctx,JS_UNDEFINED,0,NULL);JS_SetPropertyStr(g_ctx,p,\"env\",ev);}\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"platform\",JS_NewString(g_ctx,\"win32\"));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"arch\",JS_NewString(g_ctx,\"x64\"));\n";
        cw << "  JS_SetPropertyStr(g_ctx,p,\"version\",JS_NewString(g_ctx,\"v18.17.0\"));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"process\",p);\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"require\",JS_NewCFunction(g_ctx,js_require,\"require\",1));\n";
        cw << "  /* Register bridge I/O helpers */\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_readFile\",JS_NewCFunction(g_ctx,bridge_fs_readFile,\"__bridge_fs_readFile\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_writeFile\",JS_NewCFunction(g_ctx,bridge_fs_writeFile,\"__bridge_fs_writeFile\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_exists\",JS_NewCFunction(g_ctx,bridge_fs_exists,\"__bridge_fs_exists\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_mkdir\",JS_NewCFunction(g_ctx,bridge_fs_mkdir,\"__bridge_fs_mkdir\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_readdir\",JS_NewCFunction(g_ctx,bridge_fs_readdir,\"__bridge_fs_readdir\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_stat\",JS_NewCFunction(g_ctx,bridge_fs_stat,\"__bridge_fs_stat\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_fs_unlink\",JS_NewCFunction(g_ctx,bridge_fs_unlink,\"__bridge_fs_unlink\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_exec\",JS_NewCFunction(g_ctx,bridge_exec,\"__bridge_exec\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_http_get\",JS_NewCFunction(g_ctx,bridge_http_get,\"__bridge_http_get\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_npm_install\",JS_NewCFunction(g_ctx,js_bridge_npm_install,\"__bridge_npm_install\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_crypto_hash\",JS_NewCFunction(g_ctx,bridge_crypto_hash,\"__bridge_crypto_hash\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_crypto_random\",JS_NewCFunction(g_ctx,bridge_crypto_random,\"__bridge_crypto_random\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_net_connect\",JS_NewCFunction(g_ctx,bridge_net_connect,\"__bridge_net_connect\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_net_write\",JS_NewCFunction(g_ctx,bridge_net_write,\"__bridge_net_write\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_net_read\",JS_NewCFunction(g_ctx,bridge_net_read,\"__bridge_net_read\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_net_close\",JS_NewCFunction(g_ctx,bridge_net_close,\"__bridge_net_close\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_tls_connect\",JS_NewCFunction(g_ctx,bridge_tls_connect,\"__bridge_tls_connect\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_tls_write\",JS_NewCFunction(g_ctx,bridge_tls_write,\"__bridge_tls_write\",2));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_tls_read\",JS_NewCFunction(g_ctx,bridge_tls_read,\"__bridge_tls_read\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_tls_close\",JS_NewCFunction(g_ctx,bridge_tls_close,\"__bridge_tls_close\",1));\n";
        cw << "  JS_SetPropertyStr(g_ctx,g,\"__bridge_dns_lookup\",JS_NewCFunction(g_ctx,bridge_dns_lookup,\"__bridge_dns_lookup\",1));\n";
        cw << "  JS_FreeValue(g_ctx,g);\n";
        cw << "  /* Evaluate Node.js builtin polyfills */\n";
        cw << "  {JSValue _ev=JS_Eval(g_ctx,node_builtins_js,strlen(node_builtins_js),\"<builtins>\",JS_EVAL_TYPE_GLOBAL);JS_FreeValue(g_ctx,_ev);}\n";
        cw << "  g_inited=1;return 1;}\n\n";

        /* console.log & process.stdout.write */
        cw << "static JSValue js_console_log(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  for(int i=0;i<ac;i++){if(i)fputc(' ',stderr);const char*s=JS_ToCString(ctx,av[i]);if(s){fputs(s,stderr);JS_FreeCString(ctx,s);}}\n";
        cw << "  fputc('\\n',stderr);return JS_UNDEFINED;}\n";
        cw << "static JSValue js_pstdout(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac>0){const char*s=JS_ToCString(ctx,av[0]);if(s){printf(\"%s\",s);fflush(stdout);JS_FreeCString(ctx,s);}}\n";
        cw << "  return JS_UNDEFINED;}\n\n";

        /* ── process polyfills ── */
        cw << "static JSValue js_process_cwd(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
#ifdef _WIN32
        cw << "  char buf[4096];GetCurrentDirectoryA(sizeof(buf),buf);return JS_NewString(ctx,buf);}\n";
#else
        cw << "  char buf[4096];return getcwd(buf,sizeof(buf))?JS_NewString(ctx,buf):JS_NULL;}\n";
#endif
        cw << "static JSValue js_process_exit(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  int code=0;if(ac>0)JS_ToInt32(ctx,&code,av[0]);exit(code);return JS_UNDEFINED;}\n";
        cw << "static JSValue js_process_nextTick(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1||!JS_IsFunction(ctx,av[0]))return JS_ThrowTypeError(ctx,\"nextTick:need fn\");\n";
        cw << "  JSValue r=JS_Call(ctx,av[0],JS_UNDEFINED,ac-1,ac>1?av+1:NULL);\n";
        cw << "  return JS_IsException(r)?r:JS_UNDEFINED;}\n";
        cw << "static JSValue js_process_env(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  JSValue o=JS_NewObject(ctx);if(JS_IsException(o))return o;\n";
#ifdef _WIN32
        cw << "  wchar_t*ew=GetEnvironmentStringsW();if(ew){wchar_t*ep=ew;\n";
        cw << "    while(*ep){size_t el=wcslen(ep);char*mb=(char*)malloc(el*4+1);if(mb){WideCharToMultiByte(CP_UTF8,0,ep,-1,mb,(int)(el*4),NULL,NULL);\n";
        cw << "      char*eq=strchr(mb,'=');if(eq){*eq=0;JS_SetPropertyStr(ctx,o,mb,JS_NewString(ctx,eq+1));}free(mb);}ep+=el+1;}\n";
        cw << "    FreeEnvironmentStringsW(ew);}\n";
#else
        cw << "  extern char**environ;if(environ){for(char**e=environ;*e;e++){char*cp=strchr(*e,'=');\n";
        cw << "    if(cp){*cp=0;JS_SetPropertyStr(ctx,o,*e,JS_NewString(ctx,cp+1));*cp='=';}}}\n";
#endif
        cw << "  return o;}\n\n";

        /* ── Bridge I/O helpers (used by Node.js polyfills) ── */
        /* bridge_fs_readFile */
        cw << "static JSValue bridge_fs_readFile(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"readFile:missing path\");\n";
        cw << "  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_ThrowTypeError(ctx,\"bad path\");\n";
        cw << "  FILE*f=fopen(path,\"rb\");if(!f){set_last_error(\"readFile: fopen failed\");JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);\n";
        cw << "  char*buf=(char*)malloc((size_t)sz+1);if(!buf){fclose(f);JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  fread(buf,1,sz,f);buf[sz]=0;fclose(f);\n";
        cw << "  JSValue r=JS_NewStringLen(ctx,buf,(size_t)sz);free(buf);JS_FreeCString(ctx,path);return r;}\n\n";

        /* bridge_fs_writeFile */
        cw << "static JSValue bridge_fs_writeFile(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"writeFile:missing args\");\n";
        cw << "  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_ThrowTypeError(ctx,\"bad path\");\n";
        cw << "  const char*data=JS_ToCString(ctx,av[1]);if(!data){JS_FreeCString(ctx,path);return JS_ThrowTypeError(ctx,\"bad data\");}\n";
        cw << "  FILE*f=fopen(path,\"wb\");if(!f){set_last_error(\"writeFile: fopen failed\");JS_FreeCString(ctx,data);JS_FreeCString(ctx,path);return JS_FALSE;}\n";
        cw << "  fwrite(data,1,strlen(data),f);fclose(f);\n";
        cw << "  JS_FreeCString(ctx,data);JS_FreeCString(ctx,path);return JS_TRUE;}\n\n";

        /* bridge_fs_exists */
        cw << "static JSValue bridge_fs_exists(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;\n";
        cw << "  const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;\n";
        cw << "  FILE*f=fopen(path,\"rb\");int ok=(f!=NULL);if(f)fclose(f);\n";
        cw << "  JS_FreeCString(ctx,path);return ok?JS_TRUE:JS_FALSE;}\n\n";

        /* bridge_fs_mkdir */
        cw << "static JSValue bridge_fs_mkdir(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
#ifdef _WIN32
        cw << "  if(ac<1)return JS_FALSE;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;\n";
        cw << "  int r=_mkdir(path);JS_FreeCString(ctx,path);return r==0?JS_TRUE:JS_FALSE;}\n\n";
#else
        cw << "  if(ac<1)return JS_FALSE;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;\n";
        cw << "  int r=mkdir(path,0755);JS_FreeCString(ctx,path);return r==0?JS_TRUE:JS_FALSE;}\n\n";
#endif

        /* bridge_fs_readdir */
        cw << "static JSValue bridge_fs_readdir(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
#ifdef _WIN32
        cw << "  if(ac<1)return JS_NULL;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_NULL;\n";
        cw << "  char pat[4096];snprintf(pat,sizeof(pat),\"%s/*\",path);WIN32_FIND_DATAA fd;\n";
        cw << "  HANDLE h=FindFirstFileA(pat,&fd);if(h==INVALID_HANDLE_VALUE){JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  JSValue arr=JS_NewArray(ctx);if(JS_IsException(arr)){FindClose(h);JS_FreeCString(ctx,path);return JS_EXCEPTION;}int idx=0;\n";
        cw << "  do{JSValue nm=JS_NewString(ctx,fd.cFileName);JS_SetPropertyUint32(ctx,arr,idx++,nm);}while(FindNextFileA(h,&fd));\n";
        cw << "  FindClose(h);JS_FreeCString(ctx,path);return arr;}\n\n";
#else
        cw << "  if(ac<1)return JS_NULL;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_NULL;\n";
        cw << "  DIR*d=opendir(path);if(!d){JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  JSValue arr=JS_NewArray(ctx);if(JS_IsException(arr)){closedir(d);JS_FreeCString(ctx,path);return JS_EXCEPTION;}int idx=0;struct dirent*e;\n";
        cw << "  while((e=readdir(d))!=NULL){JSValue nm=JS_NewString(ctx,e->d_name);JS_SetPropertyUint32(ctx,arr,idx++,nm);}\n";
        cw << "  closedir(d);JS_FreeCString(ctx,path);return arr;}\n\n";
#endif

        /* bridge_fs_stat */
        cw << "static JSValue bridge_fs_stat(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_NULL;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_NULL;\n";
        cw << "  struct stat st;if(stat(path,&st)!=0){JS_FreeCString(ctx,path);return JS_NULL;}\n";
        cw << "  JSValue o=JS_NewObject(ctx);if(JS_IsException(o)){JS_FreeCString(ctx,path);return JS_EXCEPTION;}\n";
        cw << "  JS_SetPropertyStr(ctx,o,\"size\",JS_NewInt64(ctx,st.st_size));\n";
        cw << "  JS_SetPropertyStr(ctx,o,\"mode\",JS_NewInt64(ctx,st.st_mode));\n";
        cw << "  JS_SetPropertyStr(ctx,o,\"isFile\",JS_NewBool(ctx,S_ISREG(st.st_mode)));\n";
        cw << "  JS_SetPropertyStr(ctx,o,\"isDirectory\",JS_NewBool(ctx,S_ISDIR(st.st_mode)));\n";
        cw << "  JS_FreeCString(ctx,path);return o;}\n\n";

        /* bridge_fs_unlink */
        cw << "static JSValue bridge_fs_unlink(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;const char*path=JS_ToCString(ctx,av[0]);if(!path)return JS_FALSE;\n";
        cw << "  int r=remove(path);JS_FreeCString(ctx,path);return r==0?JS_TRUE:JS_FALSE;}\n\n";

#ifdef _WIN32
        /* bridge_crypto_hash — Windows CNG real SHA256/SHA1/MD5 + CryptGenRandom */
        cw << "#include <bcrypt.h>\n";
        cw << "#pragma comment(lib,\"bcrypt.lib\")\n";
        cw << "#pragma comment(lib,\"crypt32.lib\")\n";
        cw << "static JSValue bridge_crypto_hash(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"crypto_hash:need algo+data\");\n";
        cw << "  const char*algo=JS_ToCString(ctx,av[0]);if(!algo)return JS_ThrowTypeError(ctx,\"bad algo\");\n";
        cw << "  const char*data=JS_ToCString(ctx,av[1]);if(!data){JS_FreeCString(ctx,algo);return JS_ThrowTypeError(ctx,\"bad data\");}\n";
        cw << "  LPCWSTR walgo=NULL;ULONG hash_len=0;\n";
        cw << "  if(strcmp(algo,\"sha256\")==0){walgo=BCRYPT_SHA256_ALGORITHM;hash_len=32;}\n";
        cw << "  else if(strcmp(algo,\"sha1\")==0){walgo=BCRYPT_SHA1_ALGORITHM;hash_len=20;}\n";
        cw << "  else if(strcmp(algo,\"md5\")==0){walgo=BCRYPT_MD5_ALGORITHM;hash_len=16;}\n";
        cw << "  else{JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"unsupported algo\");}\n";
        cw << "  BCRYPT_ALG_HANDLE hAlg=NULL;\n";
        cw << "  if(BCryptOpenAlgorithmProvider(&hAlg,walgo,NULL,0)!=0||!hAlg){\n";
        cw << "    JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"CNG init failed\");}\n";
        cw << "  BCRYPT_HASH_HANDLE hHash=NULL;\n";
        cw << "  if(BCryptCreateHash(hAlg,&hHash,NULL,0,NULL,0,0)!=0||!hHash){\n";
        cw << "    BCryptCloseAlgorithmProvider(hAlg,0);JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"CNG hash failed\");}\n";
        cw << "  BCryptHashData(hHash,(PUCHAR)data,(ULONG)strlen(data),0);\n";
        cw << "  UCHAR hash[64];\n";
        cw << "  BCryptFinishHash(hHash,hash,hash_len,0);\n";
        cw << "  BCryptDestroyHash(hHash);BCryptCloseAlgorithmProvider(hAlg,0);\n";
        cw << "  char hex[129];for(ULONG i=0;i<hash_len;i++)snprintf(hex+i*2,3,\"%02x\",hash[i]);\n";
        cw << "  JS_FreeCString(ctx,algo);JS_FreeCString(ctx,data);\n";
        cw << "  return JS_NewString(ctx,hex);}\n\n";

        /* bridge_crypto_random — CryptGenRandom */
        cw << "#include <wincrypt.h>\n";
        cw << "static JSValue bridge_crypto_random(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"random:need size\");\n";
        cw << "  int32_t sz=0;JS_ToInt32(ctx,&sz,av[0]);if(sz<1||sz>65536)sz=32;\n";
        cw << "  HCRYPTPROV prov=0;\n";
        cw << "  if(!CryptAcquireContextA(&prov,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)){\n";
        cw << "    return JS_ThrowTypeError(ctx,\"CSP failed\");}\n";
        cw << "  BYTE*buf=(BYTE*)malloc((size_t)sz);if(!buf){CryptReleaseContext(prov,0);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "  CryptGenRandom(prov,sz,buf);CryptReleaseContext(prov,0);\n";
        cw << "  /* Return as hex string */\n";
        cw << "  char*hex=(char*)malloc((size_t)sz*2+1);if(!hex){free(buf);return JS_ThrowOutOfMemory(ctx);}\n";
        cw << "  for(int32_t i=0;i<sz;i++)snprintf(hex+i*2,3,\"%02x\",buf[i]);\n";
        cw << "  free(buf);JSValue r=JS_NewString(ctx,hex);free(hex);return r;}\n\n";
#else
        cw << "/* No crypto on non-Windows yet */\n";
        cw << "static JSValue bridge_crypto_hash(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return JS_ThrowTypeError(ctx,\"crypto not available on this platform\");}\n";
        cw << "static JSValue bridge_crypto_random(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return JS_ThrowTypeError(ctx,\"crypto not available on this platform\");}\n\n";
#endif

        /* bridge_exec (child_process.execSync) */
        cw << "static JSValue bridge_exec(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_NULL;const char*cmd=JS_ToCString(ctx,av[0]);if(!cmd)return JS_NULL;\n";
#ifdef _WIN32
        cw << "  FILE*pipe=_popen(cmd,\"r\");\n";
#else
        cw << "  FILE*pipe=popen(cmd,\"r\");\n";
#endif
        cw << "  if(!pipe){set_last_error(\"exec: popen failed\");JS_FreeCString(ctx,cmd);return JS_NULL;}\n";
        cw << "  char buf[4096];size_t tot=0;char*out=(char*)malloc(1);if(!out){_pclose(pipe);JS_FreeCString(ctx,cmd);return JS_NULL;}out[0]=0;\n";
        cw << "  while(fgets(buf,sizeof(buf),pipe)){size_t l=strlen(buf);char*np=(char*)realloc(out,tot+l+1);if(!np){free(out);_pclose(pipe);JS_FreeCString(ctx,cmd);return JS_NULL;}out=np;memcpy(out+tot,buf,l);tot+=l;out[tot]=0;}\n";
#ifdef _WIN32
        cw << "  _pclose(pipe);\n";
#else
        cw << "  pclose(pipe);\n";
#endif
        cw << "  JSValue r=JS_NewStringLen(ctx,out,tot);free(out);JS_FreeCString(ctx,cmd);return r;}\n\n";

        /* bridge_http_get (basic HTTP GET via system shell) */
        cw << "static JSValue bridge_http_get(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_NULL;const char*url=JS_ToCString(ctx,av[0]);if(!url)return JS_NULL;\n";
        cw << "  char tmpfile[256]=\"\";const char* tmpdir=getenv(\"TMPDIR\");if(!tmpdir)tmpdir=\"/tmp\";\n";
        cw << "  snprintf(tmpfile,sizeof(tmpfile),\"%s/qjs_http_XXXXXX\",tmpdir);\n";
#ifdef _WIN32
        cw << "  char cmd[65536];snprintf(cmd,sizeof(cmd),\"powershell -Command \\\"try{$r=Invoke-WebRequest -Uri '%s' -UseBasicParsing;if($r.Content){$r.Content}else{'{}'}}catch{'{}'}\\\" 2>nul\",url);\n";
        cw << "  FILE*pipe=_popen(cmd,\"r\");\n";
#else
        cw << "  char cmd[65536];snprintf(cmd,sizeof(cmd),\"curl -sL '%s' 2>/dev/null\",url);\n";
        cw << "  FILE*pipe=popen(cmd,\"r\");\n";
#endif
        cw << "  if(!pipe){set_last_error(\"http_get: popen failed\");JS_FreeCString(ctx,url);return JS_NULL;}\n";
        cw << "  char buf[4096];size_t tot=0;char*out=(char*)malloc(1);if(!out){_pclose(pipe);JS_FreeCString(ctx,url);return JS_NULL;}out[0]=0;\n";
        cw << "  while(fgets(buf,sizeof(buf),pipe)){size_t l=strlen(buf);char*np=(char*)realloc(out,tot+l+1);if(!np){free(out);_pclose(pipe);JS_FreeCString(ctx,url);return JS_NULL;}out=np;memcpy(out+tot,buf,l);tot+=l;out[tot]=0;}\n";
#ifdef _WIN32
        cw << "  _pclose(pipe);\n";
#else
        cw << "  pclose(pipe);\n";
#endif
        cw << "  JSValue r=JS_NewStringLen(ctx,out,tot);free(out);JS_FreeCString(ctx,url);return r;}\n\n";

        cw << "/* ── net helpers (WinSock2 via dynamic loading of ws2_32.dll) ── */\n";
        cw << "#include <winsock2.h>\n#include <ws2tcpip.h>\n";
        cw << "static int (__stdcall *pWSAStartup)(WORD,LPWSADATA);\n";
        cw << "static int (__stdcall *psocket)(int,int,int);\n";
        cw << "static int (__stdcall *pconnect)(int,const struct sockaddr*,int);\n";
        cw << "static int (__stdcall *psend)(int,const char*,int,int);\n";
        cw << "static int (__stdcall *precv)(int,char*,int,int);\n";
        cw << "static int (__stdcall *pclosesocket)(int);\n";
        cw << "static int (__stdcall *pgetaddrinfo)(const char*,const char*,const struct addrinfo*,struct addrinfo**);\n";
        cw << "static void(__stdcall *pfreeaddrinfo)(struct addrinfo*);\n";
        cw << "static const char*(__stdcall *pinet_ntop)(int,const void*,char*,socklen_t);\n";
        cw << "static int net_init(void){\n";
        cw << "  static int n=0;if(n)return n;\n";
        cw << "  HMODULE h=LoadLibraryA(\"ws2_32.dll\");if(!h){n=-1;return n;}\n";
        cw << "  pWSAStartup=(void*)GetProcAddress(h,\"WSAStartup\");\n";
        cw << "  psocket=(void*)GetProcAddress(h,\"socket\");\n";
        cw << "  pconnect=(void*)GetProcAddress(h,\"connect\");\n";
        cw << "  psend=(void*)GetProcAddress(h,\"send\");\n";
        cw << "  precv=(void*)GetProcAddress(h,\"recv\");\n";
        cw << "  pclosesocket=(void*)GetProcAddress(h,\"closesocket\");\n";
        cw << "  pgetaddrinfo=(void*)GetProcAddress(h,\"getaddrinfo\");\n";
        cw << "  pfreeaddrinfo=(void*)GetProcAddress(h,\"freeaddrinfo\");\n";
        cw << "  pinet_ntop=(void*)GetProcAddress(h,\"inet_ntop\");\n";
        cw << "  if(!pWSAStartup||!psocket||!pconnect||!psend||!precv||!pclosesocket||!pgetaddrinfo||!pfreeaddrinfo||!pinet_ntop){n=-1;return n;}\n";
        cw << "  WSADATA wd;n=(pWSAStartup(MAKEWORD(2,2),&wd)==0)?1:-1;return n;}\n";
        cw << "static JSValue bridge_net_connect(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"net_connect:need host port\");\n";
        cw << "  if(net_init()<0)return JS_ThrowTypeError(ctx,\"WSAStartup failed\");\n";
        cw << "  const char*host=JS_ToCString(ctx,av[0]);if(!host)return JS_ThrowTypeError(ctx,\"bad host\");\n";
        cw << "  int32_t port=0;JS_ToInt32(ctx,&port,av[1]);if(port<1||port>65535){JS_FreeCString(ctx,host);return JS_ThrowRangeError(ctx,\"bad port\");}\n";
        cw << "  struct addrinfo hints,*res;memset(&hints,0,sizeof(hints));hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;\n";
        cw << "  char port_str[16];snprintf(port_str,sizeof(port_str),\"%d\",(int)port);\n";
        cw << "  int gai=pgetaddrinfo(host,port_str,&hints,&res);JS_FreeCString(ctx,host);\n";
        cw << "  if(gai!=0||!res){if(res)pfreeaddrinfo(res);return JS_ThrowTypeError(ctx,\"getaddrinfo failed\");}\n";
        cw << "  int fd=(int)psocket(res->ai_family,res->ai_socktype,res->ai_protocol);\n";
        cw << "  if(fd<0){pfreeaddrinfo(res);return JS_ThrowTypeError(ctx,\"socket failed\");}\n";
        cw << "  if(pconnect(fd,res->ai_addr,(int)res->ai_addrlen)<0){\n";
        cw << "    pclosesocket(fd);pfreeaddrinfo(res);return JS_ThrowTypeError(ctx,\"connect failed\");}\n";
        cw << "  pfreeaddrinfo(res);return JS_NewInt32(ctx,fd);}\n\n";
        cw << "static JSValue bridge_net_write(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"net_write:need fd data\");\n";
        cw << "  int32_t fd=0;JS_ToInt32(ctx,&fd,av[0]);\n";
        cw << "  const char*data=JS_ToCString(ctx,av[1]);if(!data)return JS_ThrowTypeError(ctx,\"bad data\");\n";
        cw << "  size_t len=strlen(data);\n";
        cw << "  int n=(int)psend(fd,data,(int)len,0);JS_FreeCString(ctx,data);\n";
        cw << "  if(n<0)return JS_ThrowTypeError(ctx,\"send failed\");\n";
        cw << "  return JS_NewInt32(ctx,n);}\n\n";
        cw << "static JSValue bridge_net_read(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"net_read:need fd\");\n";
        cw << "  int32_t fd=0;JS_ToInt32(ctx,&fd,av[0]);\n";
        cw << "  char buf[4096];\n";
        cw << "  int n=(int)precv(fd,buf,(int)sizeof(buf)-1,0);\n";
        cw << "  if(n<=0){if(n==0)return JS_NULL;return JS_ThrowTypeError(ctx,\"recv failed\");}\n";
        cw << "  buf[n]=0;return JS_NewString(ctx,buf);}\n\n";
        cw << "static JSValue bridge_net_close(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;int32_t fd=0;JS_ToInt32(ctx,&fd,av[0]);\n";
        cw << "  pclosesocket(fd);\n";
        cw << "  return JS_TRUE;}\n\n";

        /* ── TLS helper (SChannel on Win32, stub on POSIX) ── */
#ifdef _WIN32
        cw << "#define MAX_TLS_SESS 32\n";
        cw << "/* SSPI function typedefs */\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pAcquireCredentialsHandleA_t)(SEC_CHAR*,SEC_CHAR*,ULONG,void*,void*,void*,void*,CredHandle*,TimeStamp*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pInitializeSecurityContextA_t)(CredHandle*,CtxtHandle*,SEC_CHAR*,ULONG,ULONG,ULONG,SecBufferDesc*,ULONG,CtxtHandle*,SecBufferDesc*,ULONG*,TimeStamp*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pDeleteSecurityContext_t)(CtxtHandle*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pFreeCredentialsHandle_t)(CredHandle*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pEncryptMessage_t)(CtxtHandle*,ULONG,SecBufferDesc*,ULONG);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pDecryptMessage_t)(CtxtHandle*,SecBufferDesc*,ULONG,void*);\n";
        cw << "typedef SECURITY_STATUS(WINAPI *pQueryContextAttributesA_t)(CtxtHandle*,ULONG,void*);\n";
        cw << "static pAcquireCredentialsHandleA_t pAcquireCredentialsHandleA=NULL;\n";
        cw << "static pInitializeSecurityContextA_t pInitializeSecurityContextA=NULL;\n";
        cw << "static pDeleteSecurityContext_t pDeleteSecurityContext=NULL;\n";
        cw << "static pFreeCredentialsHandle_t pFreeCredentialsHandle=NULL;\n";
        cw << "static pEncryptMessage_t pEncryptMessage=NULL;\n";
        cw << "static pDecryptMessage_t pDecryptMessage=NULL;\n";
        cw << "static pQueryContextAttributesA_t pQueryContextAttributesA=NULL;\n";
        cw << "static int tls_init(void){\n";
        cw << "  static int n=0;if(n)return n;\n";
        cw << "  HMODULE h=LoadLibraryA(\"secur32.dll\");if(!h){n=-1;return n;}\n";
        cw << "  pAcquireCredentialsHandleA=(void*)GetProcAddress(h,\"AcquireCredentialsHandleA\");\n";
        cw << "  pInitializeSecurityContextA=(void*)GetProcAddress(h,\"InitializeSecurityContextA\");\n";
        cw << "  pDeleteSecurityContext=(void*)GetProcAddress(h,\"DeleteSecurityContext\");\n";
        cw << "  pFreeCredentialsHandle=(void*)GetProcAddress(h,\"FreeCredentialsHandle\");\n";
        cw << "  pEncryptMessage=(void*)GetProcAddress(h,\"EncryptMessage\");\n";
        cw << "  pDecryptMessage=(void*)GetProcAddress(h,\"DecryptMessage\");\n";
        cw << "  pQueryContextAttributesA=(void*)GetProcAddress(h,\"QueryContextAttributesA\");\n";
        cw << "  if(!pAcquireCredentialsHandleA||!pInitializeSecurityContextA||!pDeleteSecurityContext||!pFreeCredentialsHandle||!pEncryptMessage||!pDecryptMessage||!pQueryContextAttributesA){n=-1;return n;}\n";
        cw << "  n=1;return 1;}\n";
        cw << "  static struct tls_ses{int fd;CredHandle cred;CtxtHandle ctx;BOOL ok;\n";
        cw << "  SecPkgContext_StreamSizes sz;}g_tls_ses[MAX_TLS_SESS];\n";
        cw << "static int tls_handshake(int fd,CredHandle*cred,CtxtHandle*ctx,SecPkgContext_StreamSizes*sz){\n";
        cw << "  if(tls_init()<0)return-10;\n";
        cw << "  SCHANNEL_CRED sc={sizeof(sc),SCH_CRED_NO_DEFAULT_CREDS};\n";
        cw << "  sc.grbitEnabledProtocols=SP_PROT_TLS1_2_CLIENT|SP_PROT_TLS1_3_CLIENT;\n";
        cw << "  TimeStamp ts;SECURITY_STATUS ss;\n";
        cw << "  ss=pAcquireCredentialsHandleA(NULL,UNISP_NAME,SECPKG_CRED_OUTBOUND,NULL,&sc,NULL,NULL,cred,&ts);\n";
        cw << "  if(ss<0)return-1;\n";
        cw << "  SecBuffer outb[1];SecBufferDesc outd;outd.ulVersion=SECBUFFER_VERSION;outd.cBuffers=1;outd.pBuffers=outb;\n";
        cw << "  SecBuffer inb[2];SecBufferDesc ind;ind.ulVersion=SECBUFFER_VERSION;ind.cBuffers=2;ind.pBuffers=inb;\n";
        cw << "  DWORD ctxFlags=ISC_REQ_STREAM|ISC_REQ_CONFIDENTIALITY;\n";
        cw << "  char obuf[16384];outb[0].cbBuffer=sizeof(obuf);outb[0].BufferType=SECBUFFER_TOKEN;outb[0].pvBuffer=obuf;\n";
        cw << "  inb[0].cbBuffer=0;inb[0].BufferType=SECBUFFER_TOKEN;inb[0].pvBuffer=NULL;\n";
        cw << "  inb[1].cbBuffer=0;inb[1].BufferType=SECBUFFER_EMPTY;inb[1].pvBuffer=NULL;\n";
        cw << "  ULONG attr;ss=pInitializeSecurityContextA(cred,NULL,NULL,ctxFlags,0,0,NULL,0,ctx,&outd,&attr,&ts);\n";
        cw << "  if(ss!=SEC_I_CONTINUE_NEEDED&&ss<0){pFreeCredentialsHandle(cred);return-2;}\n";
        cw << "  if(outb[0].cbBuffer>0&&psend(fd,(char*)outb[0].pvBuffer,outb[0].cbBuffer,0)<0){pFreeCredentialsHandle(cred);return-3;}\n";
        cw << "  char ibuf[16384];int n=(int)precv(fd,ibuf,sizeof(ibuf)-1,0);\n";
        cw << "  if(n<=0){pFreeCredentialsHandle(cred);return-4;}\n";
        cw << "  inb[0].cbBuffer=n;inb[0].pvBuffer=ibuf;inb[0].BufferType=SECBUFFER_TOKEN;\n";
        cw << "  inb[1].cbBuffer=0;inb[1].BufferType=SECBUFFER_EMPTY;\n";
        cw << "  int max_loop=50;while(--max_loop){\n";
        cw << "    ss=pInitializeSecurityContextA(cred,ctx,NULL,ctxFlags,0,0,&ind,0,NULL,&outd,&attr,&ts);\n";
        cw << "    if(ss<0&&ss!=SEC_I_CONTINUE_NEEDED&&ss!=SEC_I_INCOMPLETE_CREDENTIALS){pDeleteSecurityContext(ctx);pFreeCredentialsHandle(cred);return-5;}\n";
        cw << "    if(outb[0].cbBuffer>0&&psend(fd,(char*)outb[0].pvBuffer,outb[0].cbBuffer,0)<0){pDeleteSecurityContext(ctx);pFreeCredentialsHandle(cred);return-6;}\n";
        cw << "    if(ss==SEC_E_OK){ss=pQueryContextAttributesA(ctx,SECPKG_ATTR_STREAM_SIZES,sz);if(ss<0){pDeleteSecurityContext(ctx);pFreeCredentialsHandle(cred);return-7;}return 0;}\n";
        cw << "    n=(int)precv(fd,ibuf,sizeof(ibuf)-1,0);if(n<=0){pDeleteSecurityContext(ctx);pFreeCredentialsHandle(cred);return-8;}\n";
        cw << "    inb[0].cbBuffer=n;inb[0].pvBuffer=ibuf;inb[0].BufferType=SECBUFFER_TOKEN;\n";
        cw << "  }return-9;}\n";
        cw << "static JSValue bridge_tls_connect(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"tls_connect:need host port\");\n";
        cw << "  int32_t port;JS_ToInt32(ctx,&port,av[1]);if(port<1)return JS_ThrowTypeError(ctx,\"bad port\");\n";
        cw << "  JSValue fdv=bridge_net_connect(ctx,t,ac,av);\n";
        cw << "  if(JS_IsException(fdv))return JS_EXCEPTION;\n";
        cw << "  int32_t fd;JS_ToInt32(ctx,&fd,fdv);JS_FreeValue(ctx,fdv);\n";
        cw << "  int slot=-1;for(int i=0;i<MAX_TLS_SESS;i++){if(!g_tls_ses[i].ok){slot=i;break;}}\n";
        cw << "  if(slot<0){pclosesocket(fd);return JS_ThrowTypeError(ctx,\"tls:no sessions\");}\n";
        cw << "  memset(&g_tls_ses[slot],0,sizeof(g_tls_ses[slot]));\n";
        cw << "  g_tls_ses[slot].fd=fd;\n";
        cw << "  int r=tls_handshake(fd,&g_tls_ses[slot].cred,&g_tls_ses[slot].ctx,&g_tls_ses[slot].sz);\n";
        cw << "  if(r<0){memset(&g_tls_ses[slot],0,sizeof(g_tls_ses[slot]));pclosesocket(fd);return JS_ThrowTypeError(ctx,\"tls:handshake failed\");}\n";
        cw << "  g_tls_ses[slot].ok=1;return JS_NewInt32(ctx,slot+1);}\n\n";
        cw << "static JSValue bridge_tls_write(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<2)return JS_ThrowTypeError(ctx,\"tls_write:need handle data\");\n";
        cw << "  int32_t h;JS_ToInt32(ctx,&h,av[0]);if(h<1||h>MAX_TLS_SESS||!g_tls_ses[h-1].ok)return JS_ThrowTypeError(ctx,\"bad handle\");\n";
        cw << "  struct tls_ses*ss=&g_tls_ses[h-1];\n";
        cw << "  const char*data=JS_ToCString(ctx,av[1]);if(!data)return JS_ThrowTypeError(ctx,\"bad data\");\n";
        cw << "  size_t dlen=strlen(data);\n";
        cw << "  if(dlen>ss->sz.cbMaximumMessage){JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"data too large\");}\n";
        cw << "  size_t bufsz=ss->sz.cbHeader+ss->sz.cbMaximumMessage+ss->sz.cbTrailer;\n";
        cw << "  char*buf=(char*)malloc(bufsz);if(!buf){JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"OOM\");}\n";
        cw << "  memcpy(buf+ss->sz.cbHeader,data,dlen);\n";
        cw << "  SecBuffer eb[4];SecBufferDesc ed;ed.ulVersion=SECBUFFER_VERSION;ed.cBuffers=4;ed.pBuffers=eb;\n";
        cw << "  eb[0].cbBuffer=bufsz;eb[0].BufferType=SECBUFFER_STREAM;eb[0].pvBuffer=buf;\n";
        cw << "  eb[1].cbBuffer=0;eb[1].BufferType=SECBUFFER_DATA;eb[1].pvBuffer=NULL;\n";
        cw << "  eb[2].cbBuffer=0;eb[2].BufferType=SECBUFFER_DATA;eb[2].pvBuffer=NULL;\n";
        cw << "  eb[3].cbBuffer=0;eb[3].BufferType=SECBUFFER_DATA;eb[3].pvBuffer=NULL;\n";
        cw << "  SECURITY_STATUS ss=pEncryptMessage(&ss->ctx,0,&ed,0);\n";
        cw << "  if(ss<0){free(buf);JS_FreeCString(ctx,data);return JS_ThrowTypeError(ctx,\"encrypt failed\");}\n";
        cw << "  int n=(int)psend(ss->fd,buf+ss->sz.cbHeader,eb[0].cbBuffer,0);\n";
        cw << "  free(buf);JS_FreeCString(ctx,data);\n";
        cw << "  if(n<0)return JS_ThrowTypeError(ctx,\"send failed\");return JS_NewInt32(ctx,n);}\n\n";
        cw << "static JSValue bridge_tls_read(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"tls_read:need handle\");\n";
        cw << "  int32_t h;JS_ToInt32(ctx,&h,av[0]);if(h<1||h>MAX_TLS_SESS||!g_tls_ses[h-1].ok)return JS_ThrowTypeError(ctx,\"bad handle\");\n";
        cw << "  struct tls_ses*ss=&g_tls_ses[h-1];\n";
        cw << "  char ibuf[16384];int n=(int)precv(ss->fd,ibuf,sizeof(ibuf)-1,0);\n";
        cw << "  if(n<=0){if(n==0)return JS_NULL;return JS_ThrowTypeError(ctx,\"recv failed\");}\n";
        cw << "  SecBuffer rb[4];SecBufferDesc rd;rd.ulVersion=SECBUFFER_VERSION;rd.cBuffers=4;rd.pBuffers=rb;\n";
        cw << "  rb[0].cbBuffer=n;rb[0].BufferType=SECBUFFER_DATA;rb[0].pvBuffer=ibuf;\n";
        cw << "  rb[1].cbBuffer=0;rb[1].BufferType=SECBUFFER_EMPTY;\n";
        cw << "  rb[2].cbBuffer=0;rb[2].BufferType=SECBUFFER_EMPTY;\n";
        cw << "  rb[3].cbBuffer=0;rb[3].BufferType=SECBUFFER_EMPTY;\n";
        cw << "  SECURITY_STATUS ss=pDecryptMessage(&ss->ctx,&rd,0,NULL);\n";
        cw << "  if(ss<0)return JS_ThrowTypeError(ctx,\"decrypt failed\");\n";
        cw << "  for(int i=0;i<4;i++){if(rb[i].BufferType==SECBUFFER_DATA&&rb[i].cbBuffer>0){return JS_NewStringLen(ctx,(const char*)rb[i].pvBuffer,rb[i].cbBuffer);}}\n";
        cw << "  return JS_NULL;}\n\n";
        cw << "static JSValue bridge_tls_close(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;int32_t h;JS_ToInt32(ctx,&h,av[0]);\n";
        cw << "  if(h<1||h>MAX_TLS_SESS||!g_tls_ses[h-1].ok)return JS_FALSE;\n";
        cw << "  struct tls_ses*ss=&g_tls_ses[h-1];\n";
        cw << "  pDeleteSecurityContext(&ss->ctx);pFreeCredentialsHandle(&ss->cred);\n";
        cw << "  pclosesocket(ss->fd);memset(ss,0,sizeof(*ss));return JS_TRUE;}\n\n";
#else
        cw << "/* TLS stub for POSIX — falls back to raw TCP */\n";
        cw << "static JSValue bridge_tls_connect(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return bridge_net_connect(ctx,t,ac,av);}\n";
        cw << "static JSValue bridge_tls_write(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return bridge_net_write(ctx,t,ac,av);}\n";
        cw << "static JSValue bridge_tls_read(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return bridge_net_read(ctx,t,ac,av);}\n";
        cw << "static JSValue bridge_tls_close(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  return bridge_net_close(ctx,t,ac,av);}\n\n";
#endif

        /* ── dns helper (getaddrinfo via dynamic loading) ── */
        cw << "static JSValue bridge_dns_lookup(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_ThrowTypeError(ctx,\"dns_lookup:need hostname\");\n";
        cw << "  if(net_init()<0)return JS_ThrowTypeError(ctx,\"WSAStartup failed\");\n";
        cw << "  const char*host=JS_ToCString(ctx,av[0]);if(!host)return JS_ThrowTypeError(ctx,\"bad host\");\n";
        cw << "  struct addrinfo hints,*res;memset(&hints,0,sizeof(hints));hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;\n";
        cw << "  int gai=pgetaddrinfo(host,\"80\",&hints,&res);JS_FreeCString(ctx,host);\n";
        cw << "  if(gai!=0||!res){if(res)pfreeaddrinfo(res);return JS_NULL;}\n";
        cw << "  char addr[64]=\"\";\n";
        cw << "  struct sockaddr_in*sin=(struct sockaddr_in*)res->ai_addr;\n";
        cw << "  pinet_ntop(AF_INET,&sin->sin_addr,addr,sizeof(addr));\n";
        cw << "  pfreeaddrinfo(res);return addr[0]?JS_NewString(ctx,addr):JS_NULL;}\n\n";

        /* JS-callable wrapper for bridge_npm_install */
        cw << "static JSValue js_bridge_npm_install(JSContext*ctx,JSValueConst t,int ac,JSValueConst*av){\n";
        cw << "  if(ac<1)return JS_FALSE;const char*n=JS_ToCString(ctx,av[0]);if(!n)return JS_FALSE;\n";
        cw << "  JSValue r=bridge_npm_install(ctx,n);JS_FreeCString(ctx,n);return r;}\n\n";

        /* ── CJS version fallback for ESM-only packages ── */
        cw << "static const char* cjs_version(const char*name){\n";
        cw << "  static const struct{const char*n;const char*v;}esm[]={\n";
        cw << "    {\"chalk\",\"4\"},{\"nanocolors\",\"0\"},{\"got\",\"11\"},\n";
        cw << "    {\"ky\",\"0\"},{\"execa\",\"5\"},{\"globby\",\"11\"},\n";
        cw << "    {\"p-map\",\"4\"},{\"p-limit\",\"3\"},{NULL,NULL}\n";
        cw << "  };\n";
        cw << "  for(int i=0;esm[i].n;i++)if(strcmp(name,esm[i].n)==0)return esm[i].v;\n";
        cw << "  return NULL;}\n\n";
        cw << "/* ── bridge_npm_install — auto-download & extract npm package ── */\n";
        cw << "static JSValue bridge_npm_install(JSContext*ctx,const char*name){\n";
        cw << "  /* Support scoped packages: @scope/pkg → URL-encode as @scope%2Fpkg */\n";
        cw << "  char scope_encoded[4096];\n";
        cw << "  const char*reg_name=name;\n";
        cw << "  const char*slash_pos=strchr(name,'/');\n";
        cw << "  if(slash_pos&&name[0]=='@'){\n";
        cw << "    size_t slen=(size_t)(slash_pos-name);\n";
        cw << "    if(slen+8<sizeof(scope_encoded)){\n";
        cw << "      memcpy(scope_encoded,name,slen);\n";
        cw << "      memcpy(scope_encoded+slen,\"%2F\",3);\n";
        cw << "      strcpy(scope_encoded+slen+3,slash_pos+1);\n";
        cw << "      reg_name=scope_encoded;\n";
        cw << "    }\n";
        cw << "  }\n";
        cw << "  /* Check CJS fallback for ESM-only packages */\n";
        cw << "  const char*pin=cjs_version(name);\n";
        cw << "  /* Fetch package metadata from npm registry */\n";
        cw << "  char url[4096];\n";
        cw << "  if(pin)snprintf(url,sizeof(url),\"https://registry.npmjs.org/%s/%s\",reg_name,pin);\n";
        cw << "  else snprintf(url,sizeof(url),\"https://registry.npmjs.org/%s/latest\",reg_name);\n";
        cw << "  /* Retry loop for HTTP fetch (up to 3 attempts with backoff) */\n";
        cw << "  JSValue meta=JS_NULL;\n";
        cw << "  for(int attempt=0;attempt<3;attempt++){\n";
        cw << "    if(attempt>0){int ms=500<<(attempt-1);\n";
#ifdef _WIN32
        cw << "      Sleep(ms);\n";
#else
        cw << "      usleep(ms*1000);\n";
#endif
        cw << "    }\n";
        cw << "    JSValueConst meta_args[1];meta_args[0]=JS_NewString(ctx,url);\n";
        cw << "    meta=bridge_http_get(ctx,JS_NULL,1,meta_args);\n";
        cw << "    JS_FreeValue(ctx,meta_args[0]);\n";
        cw << "    if(!JS_IsException(meta)&&!JS_IsNull(meta))break;\n";
        cw << "    JS_FreeValue(ctx,meta);meta=JS_NULL;\n";
        cw << "  }\n";
        cw << "  if(JS_IsNull(meta)||JS_IsException(meta))return JS_FALSE;\n";
        cw << "  const char*ms=JS_ToCString(ctx,meta);if(!ms){JS_FreeValue(ctx,meta);return JS_FALSE;}\n";
        cw << "  JSValue po=JS_ParseJSON(ctx,ms,strlen(ms),\"<meta>\");JS_FreeCString(ctx,ms);JS_FreeValue(ctx,meta);\n";
        cw << "  if(JS_IsException(po))return JS_FALSE;\n";
        cw << "  JSValue tv=JS_GetPropertyStr(ctx,po,\"tarball\");\n";
        cw << "  if(JS_IsUndefined(tv)){JS_FreeValue(ctx,tv);JS_FreeValue(ctx,po);return JS_FALSE;}\n";
        cw << "  const char*tarball=JS_ToCString(ctx,tv);\n";
        cw << "  if(!tarball){JS_FreeValue(ctx,tv);JS_FreeValue(ctx,po);return JS_FALSE;}\n";
        cw << "  /* Download tarball (with retry) */\n";
        cw << "  JSValue dl=JS_NULL;\n";
        cw << "  for(int attempt=0;attempt<3;attempt++){\n";
        cw << "    if(attempt>0){int ms=500<<(attempt-1);\n";
#ifdef _WIN32
        cw << "      Sleep(ms);\n";
#else
        cw << "      usleep(ms*1000);\n";
#endif
        cw << "    }\n";
        cw << "    JSValueConst dl_args[1];dl_args[0]=JS_NewString(ctx,tarball);\n";
        cw << "    dl=bridge_http_get(ctx,JS_NULL,1,dl_args);\n";
        cw << "    JS_FreeValue(ctx,dl_args[0]);\n";
        cw << "    if(!JS_IsException(dl)&&!JS_IsNull(dl))break;\n";
        cw << "    JS_FreeValue(ctx,dl);dl=JS_NULL;\n";
        cw << "  }\n";
        cw << "  JS_FreeCString(ctx,tarball);JS_FreeValue(ctx,tv);JS_FreeValue(ctx,po);\n";
        cw << "  if(JS_IsNull(dl)||JS_IsException(dl))return JS_FALSE;\n";
        cw << "  const char*tgz=JS_ToCString(ctx,dl);if(!tgz){JS_FreeValue(ctx,dl);return JS_FALSE;}\n";
        cw << "  /* Write tarball to temp file */\n";
        cw << "  char tmpfile[1024]=\"\";const char*td=getenv(\"TMPDIR\");if(!td)td=\"/tmp\";\n";
#ifdef _WIN32
        cw << "  snprintf(tmpfile,sizeof(tmpfile),\"%s/qjs_npm_XXXXXX\",td);_mktemp_s(tmpfile,sizeof(tmpfile));\n";
#else
        cw << "  snprintf(tmpfile,sizeof(tmpfile),\"%s/qjs_npm_XXXXXX\",td);close(mkstemp(tmpfile));\n";
#endif
        cw << "  FILE*fw=fopen(tmpfile,\"wb\");if(fw){fwrite(tgz,1,strlen(tgz),fw);fclose(fw);}\n";
        cw << "  JS_FreeCString(ctx,tgz);JS_FreeValue(ctx,dl);\n";
        cw << "  /* Ensure node_modules and extract */\n";
#ifdef _WIN32
        cw << "  _mkdir(\"node_modules\");\n";
        cw << "  /* Create scope subdirectory for scoped packages */\n";
        cw << "  if(slash_pos&&name[0]=='@'){\n";
        cw << "    char scopedir[4096];snprintf(scopedir,sizeof(scopedir),\"node_modules/%.*s\",(int)(slash_pos-name),name);\n";
        cw << "    _mkdir(scopedir);\n";
        cw << "  }\n";
        cw << "  char cmd[65536];snprintf(cmd,sizeof(cmd),\"tar xzf \\\"%s\\\" -C node_modules 2>nul\",tmpfile);\n";
#else
        cw << "  mkdir(\"node_modules\",0755);\n";
        cw << "  /* Create scope subdirectory for scoped packages */\n";
        cw << "  if(slash_pos&&name[0]=='@'){\n";
        cw << "    char scopedir[4096];snprintf(scopedir,sizeof(scopedir),\"node_modules/%.*s\",(int)(slash_pos-name),name);\n";
        cw << "    mkdir(scopedir,0755);\n";
        cw << "  }\n";
        cw << "  char cmd[65536];snprintf(cmd,sizeof(cmd),\"tar xzf '%s' -C node_modules 2>/dev/null\",tmpfile);\n";
#endif
        cw << "  int rc=system(cmd);remove(tmpfile);\n";
        cw << "  if(rc!=0)return JS_FALSE;\n";
        cw << "  /* Rename node_modules/package -> node_modules/<name> */\n";
        cw << "  char oldp[4096],newp[4096];\n";
        cw << "  snprintf(oldp,sizeof(oldp),\"node_modules/package\");\n";
        cw << "  snprintf(newp,sizeof(newp),\"node_modules/%s\",name);\n";
#ifdef _WIN32
        cw << "  _unlink(newp);_rmdir(newp);\n";
#else
        cw << "  unlink(newp);rmdir(newp);\n";
#endif
        cw << "  rename(oldp,newp);\n";
        cw << "  return JS_TRUE;}\n\n";

        /* ── js_rust_load — compile and load a Rust crate as JS object ── */
        cw << "static JSValue js_rust_load(JSContext*ctx,const char*name){\n";
        cw << "  char tmpdir[1024]=\"\";const char*td=getenv(\"TMPDIR\");if(!td)td=\"/tmp\";\n";
        cw << "  snprintf(tmpdir,sizeof(tmpdir),\"%s/qjs_rust_XXXXXX\",td);\n";
#ifdef _WIN32
        cw << "  _mktemp_s(tmpdir,sizeof(tmpdir));_mkdir(tmpdir);\n";
#else
        cw << "  close(mkstemp(tmpdir));unlink(tmpdir);mkdir(tmpdir,0755);\n";
#endif
        cw << "  char dir[4096];snprintf(dir,sizeof(dir),\"%s\",tmpdir);\n";
        cw << "  /* Write Cargo.toml */\n";
        cw << "  char tp[4096];snprintf(tp,sizeof(tp),\"%s/Cargo.toml\",dir);\n";
        cw << "  FILE*f=fopen(tp,\"w\");if(!f){rmdir(dir);return JS_NULL;}\n";
        cw << "  fprintf(f,\"[package]\\nname = \\\"%s_bridge\\\"\\nversion = \\\"0.1.0\\\"\\nedition = \\\"2021\\\"\\n\\n\",name);\n";
        cw << "  fprintf(f,\"[lib]\\ncrate-type = [\\\"cdylib\\\"]\\n\\n\");\n";
        cw << "  fprintf(f,\"[dependencies]\\nserde_json = \\\"1\\\"\\n%s = \\\"*\\\"\\n\",name);\n";
        cw << "  fprintf(f,\"futures = \\\"0.3\\\"\\n\");fclose(f);\n";
        cw << "  /* Write src/lib.rs */\n";
        cw << "  snprintf(tp,sizeof(tp),\"%s/src\",dir);\n";
#ifdef _WIN32
        cw << "  _mkdir(tp);\n";
#else
        cw << "  mkdir(tp,0755);\n";
#endif
        cw << "  snprintf(tp,sizeof(tp),\"%s/src/lib.rs\",dir);\n";
        cw << "  f=fopen(tp,\"w\");if(!f){rmdir(dir);return JS_NULL;}\n";
        cw << "  fprintf(f,\"use std::collections::HashMap;\\n\");\n";

        cw << "  fprintf(f,\"type RustFn = fn(Vec<serde_json::Value>)->Result<serde_json::Value,String>;\\n\");\n";

        cw << "  fprintf(f,\"#[no_mangle]\\npub extern \\\"C\\\" fn rust_bridge_get_fns()->*mut HashMap<String,RustFn>{\\n\");\n";

        cw << "  fprintf(f,\"  let mut m:HashMap<String,RustFn> = HashMap::new();\\n\");\n";

        cw << "  fprintf(f,\"  Box::into_raw(Box::new(m))\\n}\");\n";
        cw << "  fclose(f);\n";
        cw << "  /* Cargo build */\n";
        cw << "  snprintf(tp,sizeof(tp),\"cd %s && cargo build --release 2>&1\",dir);\n";
        cw << "  if(system(tp)!=0){rmdir(dir);return JS_NULL;}\n";
        cw << "  /* Load the .dll/.so */\n";
        cw << "  char libpath[4096];\n";
#ifdef _WIN32
        cw << "  snprintf(libpath,sizeof(libpath),\"%s/target/release/%s_bridge.dll\",dir,name);\n";
        cw << "  HMODULE lib=LoadLibraryA(libpath);\n";
        cw << "  if(!lib){rmdir(dir);return JS_NULL;}\n";
        cw << "  typedef void*(*GetFns)();GetFns gf=(GetFns)GetProcAddress(lib,\"rust_bridge_get_fns\");\n";
        cw << "  if(!gf){FreeLibrary(lib);rmdir(dir);return JS_NULL;}\n";
#else
        cw << "  snprintf(libpath,sizeof(libpath),\"%s/target/release/lib%s_bridge.so\",dir,name);\n";
        cw << "  void*lib=dlopen(libpath,RTLD_NOW|RTLD_LOCAL);\n";
        cw << "  if(!lib){rmdir(dir);return JS_NULL;}\n";
        cw << "  typedef void*(*GetFns)();GetFns gf=(GetFns)dlsym(lib,\"rust_bridge_get_fns\");\n";
        cw << "  if(!gf){dlclose(lib);rmdir(dir);return JS_NULL;}\n";
#endif
        cw << "  JSValue obj=JS_NewObject(ctx);if(JS_IsException(obj)){";
#ifdef _WIN32
        cw << "FreeLibrary(lib);";
#else
        cw << "dlclose(lib);";
#endif
        cw << "rmdir(dir);return JS_EXCEPTION;}\n";
        cw << "  JS_SetPropertyStr(ctx,obj,\"__rust_crate\",JS_NewString(ctx,libpath));\n";
        cw << "  return obj;}\n\n";

        /* Cleanup */
        cw << "static void cleanup_qjs(void){\n";
        cw << "  /* Free cached module names and exports (JS refs) before destroying context */\n";
        cw << "  for(int i=0;i<g_cache_count;i++){free((void*)g_cache_names[i]);JS_FreeValue(g_ctx,g_cache_exports[i]);}\n";
        cw << "  g_cache_count=0;\n";
        cw << "  if(g_ctx){for(int i=0;i<MAX_OBJS;i++){if(!JS_IsNull(g_objs[i])&&!JS_IsUndefined(g_objs[i]))JS_FreeValue(g_ctx,g_objs[i]);g_objs[i]=JS_NULL;}\n";
        cw << "    JS_FreeContext(g_ctx);g_ctx=NULL;}\n";
        cw << "  if(g_rt){JS_FreeRuntime(g_rt);g_rt=NULL;}\n";
        cw << "  g_inited=0;g_next_id=2;}\n\n";

        /* qjs_call_impl — core call dispatcher */
        cw << "static void* qjs_call(void*mod,const char*fn,const char*ajs){\n";
        cw << "  if(!mod||!g_ctx||!g_inited)return NULL;\n";
        cw << "  int oid=(int)(intptr_t)mod;\n";
        cw << "  if(oid<0||oid>=MAX_OBJS||JS_IsNull(g_objs[oid])||JS_IsUndefined(g_objs[oid]))return NULL;\n";
        cw << "  JSValue obj=g_objs[oid];\n";
        cw << "  JSValue func=fn&&fn[0]?JS_GetPropertyStr(g_ctx,obj,fn):JS_DupValue(g_ctx,obj);\n";
        cw << "  if(JS_IsException(func))return NULL;\n";
        cw << "  if(!JS_IsFunction(g_ctx,func)){\n";
        cw << "    if(JS_IsObject(func)){int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,func);return NULL;}g_objs[nid]=func;return(void*)(intptr_t)nid;}\n";
        cw << "    char*j=jsval_to_json(g_ctx,func);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,func);return h;}\n";
        cw << "  JSValue av=JS_NULL;JSValue*argv=NULL;int argc=0;\n";
        cw << "  if(ajs&&ajs[0]&&strcmp(ajs,\"null\")!=0){\n";
        cw << "    av=JS_ParseJSON(g_ctx,ajs,strlen(ajs),\"<a>\");\n";
        cw << "    if(!JS_IsException(av)&&JS_IsArray(g_ctx,av)){\n";
        cw << "      JSValue lv=JS_GetPropertyStr(g_ctx,av,\"length\");JS_ToInt32(g_ctx,&argc,lv);JS_FreeValue(g_ctx,lv);\n";
        cw << "      if(argc>0){argv=(JSValue*)malloc((size_t)argc*sizeof(JSValue));if(!argv){argc=0;}else{for(int i=0;i<argc;i++)argv[i]=JS_GetPropertyUint32(g_ctx,av,i);}}\n";
        cw << "      JS_FreeValue(g_ctx,av);\n";
        cw << "    }else if(!JS_IsException(av)){argc=1;argv=(JSValue*)malloc(sizeof(JSValue));if(argv)argv[0]=av;else{argc=0;JS_FreeValue(g_ctx,av);}}\n";
        cw << "  }\n";
        cw << "  JSValue r=JS_Call(g_ctx,func,obj,argc,argv);JS_FreeValue(g_ctx,func);\n";
        cw << "  if(argv){for(int i=0;i<argc;i++)JS_FreeValue(g_ctx,argv[i]);free(argv);}\n";
        cw << "  if(JS_IsException(r))return NULL;\n";
        cw << "  if(JS_IsObject(r)||JS_IsFunction(g_ctx,r)){\n";
        cw << "    int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,r);return NULL;}\n";
        cw << "    g_objs[nid]=JS_DupValue(g_ctx,r);JS_FreeValue(g_ctx,r);return(void*)(intptr_t)nid;}\n";
        cw << "  char*j=jsval_to_json(g_ctx,r);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,r);return h;}\n\n";

        /* ── Per-package exposed API ── */

        /* _require */
        cw << "EXPORT void* " << pid << "_require(void){\n";
        cw << "  if(!g_inited&&!init_qjs(\"" << dir << "\"))return NULL;\n";
        cw << "  if(g_ctx&&!g_loaded){\n";
        cw << "    for(int i=0;i<MAX_OBJS;i++)g_objs[i]=JS_NULL;\n";
        cw << "    JSValue g=JS_GetGlobalObject(g_ctx);\n";
        cw << "    JSValue rv=JS_GetPropertyStr(g_ctx,g,\"require\");\n";
        cw << "    if(JS_IsFunction(g_ctx,rv)){\n";
        cw << "      JSValue nm=JS_NewString(g_ctx,\"" << pkg << "\");\n";
        cw << "      JSValue md=JS_Call(g_ctx,rv,JS_UNDEFINED,1,&nm);JS_FreeValue(g_ctx,nm);\n";
        cw << "      if(!JS_IsException(md)){g_objs[1]=JS_DupValue(g_ctx,md);JS_FreeValue(g_ctx,md);g_loaded=1;}\n";
        cw << "    }\n";
        cw << "    JS_FreeValue(g_ctx,rv);JS_FreeValue(g_ctx,g);\n";
        cw << "  }\n";
        cw << "  return g_inited&&g_loaded?(void*)(intptr_t)1:NULL;}\n\n";

        /* _free */
cw << "EXPORT void " << pid << "_free(void*h){\n";
        cw << "  if(!h||(intptr_t)h<2)return;\n";
        cw << "  if(((intptr_t)h & 1) != 0){\n";
        cw << "    char**pp=(char**)UNTAG(h);free(*pp);free(pp);return;}\n";
        cw << "  int id=(int)(intptr_t)h;\n";
        cw << "  if(id<MAX_OBJS){if(!JS_IsNull(g_objs[id])&&!JS_IsUndefined(g_objs[id]))JS_FreeValue(g_ctx,g_objs[id]);g_objs[id]=JS_NULL;}}\n\n";

        /* _free_cstr */
        cw << "EXPORT void " << pid << "_free_cstr(void*s){if(s)free(s);}\n";
        cw << "EXPORT char* " << pid << "_get_last_error(void){return g_last_error;}\n";
        cw << "EXPORT const char* " << pid << "_get_perf_stats(void){\n";
        cw << "  static char buf[1024];\n";
        cw << "  snprintf(buf,sizeof(buf),\"{\\\"require_calls\\\":%u,\\\"bc_hits\\\":%u,\\\"bc_misses\\\":%u,\"\n";
        cw << "    \"\\\"exports_lookups\\\":%u,\\\"eval_time_ms\\\":%u,\\\"resolve_time_ms\\\":%u}\",\n";
        cw << "    g_perf.require_calls,g_perf.bc_hits,g_perf.bc_misses,\n";
        cw << "    g_perf.exports_lookups,g_perf.eval_time_ms,g_perf.resolve_time_ms);\n";
        cw << "  return buf;\n";
        cw << "}\n\n";

        /* _str */
        cw << "EXPORT void* " << pid << "_str(const char*s){\n";
        cw << "  if(!s)return store_json(\"null\");char esc[8192];json_escape(s,esc,sizeof(esc));\n";
        cw << "  char buf[8194];snprintf(buf,sizeof(buf),\"\\\"%s\\\"\",esc);return store_json(buf);}\n\n";

        /* _int */
        cw << "EXPORT void* " << pid << "_int(long long v){\n";
        cw << "  char buf[64];snprintf(buf,sizeof(buf),\"%lld\",v);return store_json(buf);}\n\n";

        /* _float */
        cw << "EXPORT void* " << pid << "_float(double v){\n";
        cw << "  char buf[64];snprintf(buf,sizeof(buf),\"%g\",v);return store_json(buf);}\n\n";

        /* _to_cstr */
        cw << "EXPORT char* " << pid << "_to_cstr(void*obj){\n";
        cw << "  if(!obj)return NULL;const char*j=get_json(obj);\n";
        cw << "  char*out=(char*)malloc(strlen(j)+1);if(!out)return NULL;strcpy(out,j);return out;}\n\n";

        /* _tuple2-4, _tuple, _list2-4, _list */
        cw << "EXPORT void* " << pid << "_tuple2(void*a,void*b){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s]\",get_json(a),get_json(b));return store_json(buf);}\n";
        cw << "EXPORT void* " << pid << "_tuple3(void*a,void*b,void*c){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s]\",get_json(a),get_json(b),get_json(c));return store_json(buf);}\n";
        cw << "EXPORT void* " << pid << "_tuple4(void*a,void*b,void*c,void*d){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s,%s]\",get_json(a),get_json(b),get_json(c),get_json(d));return store_json(buf);}\n";
        cw << "EXPORT void* " << pid << "_tuple(void**items,int count){\n";
        cw << "  if(count<=0)return store_json(\"[]\");char*b=(char*)malloc(16384);if(!b)return NULL;\n";
        cw << "  int p=0;p+=snprintf(b+p,16384-p,\"[\");for(int i=0;i<count;i++){if(i>0)p+=snprintf(b+p,16384-p,\",\");p+=snprintf(b+p,16384-p,\"%s\",get_json(items[i]));}\n";
        cw << "  p+=snprintf(b+p,16384-p,\"]\");void*h=store_json(b);free(b);return h;}\n";
        cw << "EXPORT void* " << pid << "_list2(void*a,void*b){return " << pid << "_tuple2(a,b);}\n";
        cw << "EXPORT void* " << pid << "_list3(void*a,void*b,void*c){return " << pid << "_tuple3(a,b,c);}\n";
        cw << "EXPORT void* " << pid << "_list4(void*a,void*b,void*c,void*d){return " << pid << "_tuple4(a,b,c,d);}\n";
        cw << "EXPORT void* " << pid << "_list(void**items,int count){return " << pid << "_tuple(items,count);}\n";

        /* _dict, _dict_set */
        cw << "EXPORT void* " << pid << "_dict(void){return store_json(\"{}\");}\n";
cw << "EXPORT int " << pid << "_dict_set(void*d,const char*key,void*val){\n";
        cw << "  if(!d||!key)return -1;char**pp=(char**)UNTAG(d);const char*vj=get_json(val);\n";
        cw << "  char ek[512];json_escape(key,ek,sizeof(ek));char entry[2048];\n";
        cw << "  int el=(int)snprintf(entry,sizeof(entry),\"\\\"%s\\\":%s\",ek,vj);if(el<0)return -1;\n";
        cw << "  size_t ol=strlen(*pp);\n";
        cw << "  if(ol<=2){char*ns=(char*)malloc((size_t)el+3);if(!ns)return -1;snprintf(ns,(size_t)el+3,\"{%s}\",entry);free(*pp);*pp=ns;}\n";
        cw << "  else{size_t nl=ol+(size_t)el+2;char*ns=(char*)malloc(nl);if(!ns)return -1;snprintf(ns,nl,\"%.*s,%s}\",(int)(ol-1),*pp,entry);free(*pp);*pp=ns;}\n";
        cw << "  return 0;}\n\n";

        /* _call */
        cw << "EXPORT void* " << pid << "_call(void*mod,const char*fn,void*args){\n";
        cw << "  if(!args)return qjs_call(mod,fn,\"[]\");return qjs_call(mod,fn,get_json(args));}\n\n";

        /* _call1..6, _call_kw */
        cw << "EXPORT void* " << pid << "_call1(void*mod,const char*fn,void*a){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s]\",get_json(a));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call2(void*mod,const char*fn,void*a,void*b){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s]\",get_json(a),get_json(b));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call3(void*mod,const char*fn,void*a,void*b,void*c){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s]\",get_json(a),get_json(b),get_json(c));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call4(void*mod,const char*fn,void*a,void*b,void*c,void*d){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s,%s]\",get_json(a),get_json(b),get_json(c),get_json(d));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call5(void*mod,const char*fn,void*a,void*b,void*c,void*d,void*e){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s,%s,%s]\",get_json(a),get_json(b),get_json(c),get_json(d),get_json(e));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call6(void*mod,const char*fn,void*a,void*b,void*c,void*d,void*e,void*f){\n";
        cw << "  char buf[16384];snprintf(buf,sizeof(buf),\"[%s,%s,%s,%s,%s,%s]\",get_json(a),get_json(b),get_json(c),get_json(d),get_json(e),get_json(f));return qjs_call(mod,fn,buf);}\n";
        cw << "EXPORT void* " << pid << "_call_kw(void*mod,const char*fn,void*args,void*kwargs){\n";
        cw << "  const char*as=get_json(args);const char*ks=get_json(kwargs);size_t al=strlen(as);char buf[65536];\n";
        cw << "  if(al>=2&&al<=32768)snprintf(buf,sizeof(buf),\"%.*s,%s]\",(int)(al-1),as,ks);\n";
        cw << "  else snprintf(buf,sizeof(buf),\"[%s]\",ks);return qjs_call(mod,fn,buf);}\n\n";

        /* _getattr */
        cw << "EXPORT void* " << pid << "_getattr(void*obj,const char*name){\n";
        cw << "  if(!obj||!name||!g_ctx||!g_inited)return NULL;\n";
        cw << "  int oid=(int)(intptr_t)obj;\n";
        cw << "  if(oid<0||oid>=MAX_OBJS||JS_IsNull(g_objs[oid])||JS_IsUndefined(g_objs[oid]))return NULL;\n";
        cw << "  JSValue v=JS_GetPropertyStr(g_ctx,g_objs[oid],name);\n";
        cw << "  if(JS_IsException(v))return NULL;\n";
        cw << "  if(JS_IsObject(v)||JS_IsFunction(g_ctx,v)){\n";
        cw << "    int nid=g_next_id++;if(nid>=MAX_OBJS){JS_FreeValue(g_ctx,v);return NULL;}\n";
        cw << "    g_objs[nid]=JS_DupValue(g_ctx,v);JS_FreeValue(g_ctx,v);return(void*)(intptr_t)nid;}\n";
        cw << "  char*j=jsval_to_json(g_ctx,v);void*h=store_json(j);free(j);JS_FreeValue(g_ctx,v);return h;}\n\n";

        /* DllMain */
#ifdef _WIN32
        cw << "BOOL WINAPI DllMain(HINSTANCE h,DWORD r,LPVOID l){if(r==DLL_PROCESS_DETACH)cleanup_qjs();return TRUE;}\n";
#else
        cw << "__attribute__((destructor)) static void dll_cleanup(void){cleanup_qjs();}\n";
#endif

        std::string wrapper_path = dir + "/" + pkg + "_npm.c";
        if (write_file(wrapper_path, cw.str()))
            std::cout << "[bridge]   " << wrapper_path << " (QuickJS)\n";
    }
}