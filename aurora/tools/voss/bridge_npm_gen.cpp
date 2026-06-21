#include "bridge_npm_impl.hpp"
#include "bridge_shared.h"
#include <string>
#include <fstream>
#include <cstdlib>
void gen_npm_au_binding(const std::string& pkg, const JsonValue& json,
                         const std::string& ver, std::ostream& os)
{
    if (pkg.empty()) return;
    std::string safe = pkg;
    for (auto& c : safe) if (c == '-' || c == '.' || c == '/' || c == '@') c = '_';
    std::string desc = json.type != JsonValue::Null ? json.get_string("description") : "";

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

        /* ── Forward declarations ── */
        cw << "static void flush_deferred_free(void);\n\n";
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
        cw << "    /* Schedule Node.js object cleanup (JSON already captured) */\n";
        cw << "    if (new_obj_id > 1) {\n";
        cw << "        if (g_deferred_free_count >= MAX_DEFERRED_FREE) flush_deferred_free();\n";
        cw << "        g_deferred_free[g_deferred_free_count++] = new_obj_id;\n";
        cw << "    }\n";
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
        cw << "    /* Schedule Node.js object cleanup */\n";
        cw << "    if (new_obj_id > 1) {\n";
        cw << "        if (g_deferred_free_count >= MAX_DEFERRED_FREE) flush_deferred_free();\n";
        cw << "        g_deferred_free[g_deferred_free_count++] = new_obj_id;\n";
        cw << "    }\n";
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

