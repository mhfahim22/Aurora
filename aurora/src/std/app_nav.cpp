#include "std/app.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>

/* ════════════════════════════════════════════════════════════
   Screen-based navigation / routing
   ════════════════════════════════════════════════════════════ */

struct AppNav {
    std::vector<std::string> stack;
    std::map<std::string, void*> screens;
    void (*on_change)(const char* from, const char* to);
};

extern "C" {

void* aurora_app_nav_init(void) {
    auto* nav = new AppNav();
    nav->on_change = nullptr;
    return nav;
}

void aurora_app_nav_destroy(void* nav) {
    delete (AppNav*)nav;
}

void aurora_app_nav_register(void* nav, const char* name, void* screen) {
    if (nav && name) ((AppNav*)nav)->screens[name] = screen;
}

int aurora_app_nav_push(void* nav, const char* name) {
    auto* n = (AppNav*)nav;
    if (!n || !name) return -1;
    if (n->screens.find(name) == n->screens.end()) return -1;
    std::string prev = n->stack.empty() ? "" : n->stack.back();
    n->stack.push_back(name);
    if (n->on_change) n->on_change(prev.c_str(), name);
    return 0;
}

int aurora_app_nav_pop(void* nav) {
    auto* n = (AppNav*)nav;
    if (!n || n->stack.empty()) return -1;
    std::string prev = n->stack.back();
    n->stack.pop_back();
    std::string curr = n->stack.empty() ? "" : n->stack.back();
    if (n->on_change) n->on_change(prev.c_str(), curr.c_str());
    return 0;
}

int aurora_app_nav_replace(void* nav, const char* name) {
    auto* n = (AppNav*)nav;
    if (!n || !name) return -1;
    if (n->screens.find(name) == n->screens.end()) return -1;
    std::string prev = n->stack.empty() ? "" : n->stack.back();
    if (!n->stack.empty()) n->stack.pop_back();
    n->stack.push_back(name);
    if (n->on_change) n->on_change(prev.c_str(), name);
    return 0;
}

const char* aurora_app_nav_current(void* nav) {
    auto* n = (AppNav*)nav;
    if (!n || n->stack.empty()) return "";
    return n->stack.back().c_str();
}

int aurora_app_nav_depth(void* nav) {
    auto* n = (AppNav*)nav;
    return n ? (int)n->stack.size() : 0;
}

void aurora_app_nav_set_on_change(void* nav, void (*cb)(const char*, const char*)) {
    if (nav) ((AppNav*)nav)->on_change = cb;
}

} // extern "C"
