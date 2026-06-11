#pragma once
/* ════════════════════════════════════════════════════════════
   Aurora Built-in Functions — Declarations
   ════════════════════════════════════════════════════════════
   Central registry for all built-in functions.
   Each built-in has:
     - Registration in typechecker (return type, param types)
     - Codegen declaration (LLVM function declaration)
     - Optional: runtime implementation in aurora_runtime

   To add a new built-in:
     1. Add declaration here
     2. Register in typechecker.cpp
     3. Declare in codegen_core.cpp
     4. Implement in runtime or codegen
   ════════════════════════════════════════════════════════════ */

#include <string>
#include <vector>

namespace aurora {
namespace builtins {

/* ── Built-in function metadata ── */
struct BuiltinInfo {
    const char* name;
    const char* description;
    int         param_count;  /* -1 = variadic */
};

/* ── List of all built-in functions ── */
inline const BuiltinInfo all_builtins[] = {
    /* I/O */
    {"output",    "Print a value to stdout",            1},
    {"outputln",  "Print a value without newline",      1},
    {"input",     "Read a line from stdin",             0},
    {"ask",       "Ask user for confirmation",          1},

    /* Array */
    {"len",       "Return length of array or string",   1},
    {"sum",       "Return sum of array elements",       1},
    {"min",       "Return minimum of array elements",   -1},
    {"max",       "Return maximum of array elements",   -1},

    /* Range */
    {"range",     "Create array [0..n) or [start..end)", -1},

    /* Type checks */
    {"type",      "Return type name of value",           1},
    {"sizeof",    "Return size in bytes",               1},

    /* String */
    {"upper",     "Convert string to uppercase",        1},
    {"lower",     "Convert string to lowercase",        1},
    {"trim",      "Trim whitespace from string",        1},
    {"replace",   "Replace substring in string",        3},
    {"split",     "Split string by delimiter",          2},
    {"join",      "Join array with separator",          2},
    {"has",       "Check if string/array contains",     2},
    {"starts",    "Check if string starts with prefix", 2},
    {"ends",      "Check if string ends with suffix",   2},
    {"reverse",   "Reverse string or array",            1},

    /* Math */
    {"abs",       "Absolute value",                    1},
    {"sqrt",      "Square root",                        1},
    {"floor",     "Floor value",                        1},
    {"ceil",      "Ceiling value",                      1},
    {"round",     "Round value",                        1},
    {"pow",       "Power (x^y)",                        2},
    {"clamp",     "Clamp value between bounds",         3},
    {"rand",      "Random integer",                     0},

    /* File */
    {"read",      "Read file content",                  1},
    {"write",     "Write to file",                      2},
    {"append",    "Append to file",                     2},
    {"exists",    "Check if file exists",               1},
    {"delete",    "Delete file",                        1},
    {"copy",      "Copy file",                          2},
    {"move",      "Move/rename file",                   2},

    /* Path */
    {"cwd",       "Get current directory",              0},
    {"cd",        "Change directory",                   1},
    {"path",      "Get directory path",                 1},
    {"name",      "Get file name",                      1},
    {"ext",       "Get file extension",                 1},

    /* Time */
    {"now",       "Get current time (epoch)",           0},
    {"stamp",     "Get unix timestamp",                 0},
    {"sleep",     "Sleep for milliseconds",             1},

    /* JSON */
    {"encode",    "Encode value to JSON",               1},
    {"decode",    "Decode JSON string",                 1},

    /* HTTP */
    {"get",       "HTTP GET request",                   1},
    {"post",      "HTTP POST request",                  2},

    /* OS */
    {"os",        "Get OS name",                        0},
    {"cpu",       "Get CPU core count",                 0},
    {"mem",       "Get memory usage",                   0},
    {"env",       "Get environment variable",           1},
    {"run",       "Run shell command",                  1},
    {"exit",      "Exit program",                       1},

    /* Collection */
    {"push",      "Push to array",                      2},
    {"pop",       "Pop from array",                     1},
    {"insert",    "Insert into array",                  3},
    {"remove",    "Remove from array",                  2},
    {"clear",     "Clear array",                        1},
    {"sort",      "Sort array",                         1},
    {"unique",    "Unique values from array",           1},

    /* Error */
    {"error",     "Create error value",                 1},
    {"panic",     "Halt with error message",            1},
    {"debug",     "Print debug message",                1},

    /* Type conversion */
    {"char",      "Get first character of string",      1},

    /* Path aliases */
    {"dir",       "Get directory path of file",         1},

    /* Async */
    {"spawn",     "Spawn async task",                   1},
    {"await",     "Wait for task completion",            1},
    {"chan",      "Create a channel",                    1},
    {"send",      "Send value to channel",               2},
    {"recv",      "Receive value from channel",          1},

    /* Performance */
    {"measure",   "Measure execution time of block",    1},
    {"bench",     "Benchmark function",                  2},
    {"profile",   "Profile execution",                   1},
    {"trace",     "Trace execution",                     1},

    /* Reflection */
    {"fields",    "Get object field names",              1},
    {"methods",   "Get object method names",             1},

    /* Package */
    {"install",   "Install package",                     1},
    {"update",    "Update package",                      1},
    {"search",    "Search packages",                     1},

    /* AI */
    {"ai",        "AI entry point",                      1},
    {"chat",      "AI chat",                             1},
    {"embed",     "Create embeddings",                   1},
    {"classify",  "Classify input",                      2},
    {"translate", "Translate text",                      2},
    {"summarize", "Summarize text",                      1},
    {"code",      "Generate code",                       1},

    /* AI aliases */
    {"gen",       "Generate code (alias for code)",      1},
    {"tl",        "Translate (alias for translate)",     1},

    /* ════════════════════════════════════════
       Backend / Server builtins
       ════════════════════════════════════════ */

    /* Route/Middleware */
    {"route_group",    "Define a route group",                          1},
    {"middleware",     "Register middleware function",                  1},
    {"next",          "Call next middleware in chain",                  0},
    {"rate_limit",    "Rate limit requests (max, window_ms)",          2},
    {"cors",          "Enable CORS headers",                            0},
    {"csrf",          "Enable CSRF protection",                         0},

    /* Session */
    {"session",       "Create/get session context",                     0},
    {"session_get",   "Get session value by key",                       1},
    {"session_set",   "Set session value",                              2},
    {"session_delete","Delete session value",                           1},

    /* Cookie */
    {"cookie_get",    "Get cookie by name",                             1},
    {"cookie_set",    "Set cookie value",                               2},
    {"cookie_delete", "Delete cookie",                                  1},

    /* Proxy/Streaming */
    {"proxy",         "Proxy request to URL",                           1},
    {"reverse_proxy", "Reverse proxy to upstream",                      1},
    {"stream",        "Stream data to client",                          1},
    {"stream_file",   "Stream file to client",                          1},
    {"sse",           "Server-Sent Events endpoint",                    1},
    {"webhook",       "Register webhook endpoint",                      1},

    /* Health/Metrics */
    {"health",        "Health check endpoint",                          0},
    {"metrics",       "Metrics endpoint",                               0},
    {"trace_id",      "Get current trace ID",                           0},
    {"request_id",    "Get current request ID",                         0},
    {"audit",         "Log audit event",                                1},

    /* Lock/Sync */
    {"lock",          "Acquire a named lock",                           1},
    {"unlock",        "Release a named lock",                           1},
    {"atomic",        "Execute function atomically",                    1},
    {"retry",         "Retry function on failure",                      1},
    {"timeout",       "Execute with timeout (fn, ms)",                  2},
    {"circuit_breaker","Wrap function with circuit breaker",            1},

    /* Pool */
    {"pool",          "Create a connection pool",                       1},
    {"worker_pool",   "Create a worker pool",                           1},
    {"batch",         "Batch process list items",                       2},
    {"paginate",      "Paginate data",                                  1},

    /* DB/ORM */
    {"index",         "Create database index",                          2},
    {"migrate",       "Run database migrations",                        0},
    {"seed",          "Seed database",                                  0},
    {"model",         "Define a data model",                            1},
    {"schema",        "Define a schema",                                1},
    {"validate",      "Validate data against schema",                   2},
    {"sanitize",      "Sanitize input data",                            1},

    /* Throttle/Debounce */
    {"throttle",      "Throttle requests per second",                   1},
    {"debounce",      "Debounce function calls (ms)",                   1},

    /* Crypto */
    {"sign",          "Sign data",                                      1},
    {"verify",        "Verify signature",                               2},
    {"secret",        "Get secret by name",                             1},
    {"vault",         "Get vault value by name",                        1},

    /* Compress/Serialize */
    {"compress",      "Compress data",                                  1},
    {"decompress",    "Decompress data",                                1},
    {"serialize",     "Serialize object to string",                     1},
    {"deserialize",   "Deserialize string to object",                   1},

    /* Event/PubSub */
    {"event",         "Define an event",                                1},
    {"emit",          "Emit an event",                                  2},
    {"listen",        "Listen for an event",                            2},
    {"publish",       "Publish to a topic",                             2},
    {"subscribe",     "Subscribe to a topic",                           2},

    /* RPC/Cluster */
    {"rpc",           "Define an RPC service",                          1},
    {"discover",      "Discover service",                               1},
    {"cluster",       "Enable clustering",                              0},
    {"node_id",       "Get current node ID",                            0},
    {"leader",        "Check if current node is leader",                0},
    {"shard",         "Shard data by key",                              1},
    {"replica",       "Get replica count / info",                       0},

    /* Backup */
    {"backup",        "Backup database",                                1},
    {"restore",       "Restore database",                               1},

    /* Monitor/Profile */
    {"monitor",       "Start monitoring",                               0},
    {"profile_request","Profile current request",                       0},
    {"memory_snapshot","Take memory snapshot",                          0},
    {"gc_collect",    "Force garbage collection",                       0},
    {"hot_reload",    "Hot reload configuration",                       0},

    /* Plugin/FeatureFlag */
    {"plugin",        "Load a plugin",                                  1},
    {"feature_flag",  "Check or set a feature flag",                    1},

    /* Tenant */
    {"tenant",        "Set tenant context",                             1},
    {"tenant_context","Get current tenant context",                     0},

    /* Geo/Captcha */
    {"geoip",         "Geo-locate an IP address",                       1},
    {"captcha_verify","Verify a captcha token",                         1},

    /* Payment/Analytics */
    {"payment",       "Initialize payment provider",                    1},
    {"invoice",       "Create/process an invoice",                      1},
    {"analytics",     "Track analytics event",                          1},

    /* Search/AI extended */
    {"search_engine", "Initialize search engine",                       0},
    {"vector_search", "Perform vector search",                          1},
    {"semantic_search","Perform semantic search",                       1},
    {"embed_store",   "Store embedding vector",                         1},
    {"embed_query",   "Query embeddings",                               1},
    {"ai_agent",      "Create an AI agent",                             1},
    {"tool",          "Define a tool for AI agent",                     1},
    {"workflow",      "Define a workflow",                              1},
    {"pipeline",      "Define a pipeline",                              1},
    {"step",          "Define a pipeline step",                         1},

    /* Terminate list */
    {nullptr, nullptr, 0}
};

/* ── Check if a function name is a built-in ── */
inline bool is_builtin(const std::string& name) {
    for (int i = 0; all_builtins[i].name != nullptr; i++) {
        if (name == all_builtins[i].name) return true;
    }
    return false;
}

/* ── Get built-in info (returns nullptr if not found) ── */
inline const BuiltinInfo* get_builtin(const std::string& name) {
    for (int i = 0; all_builtins[i].name != nullptr; i++) {
        if (name == all_builtins[i].name) return &all_builtins[i];
    }
    return nullptr;
}

} /* namespace builtins */
} /* namespace aurora */
