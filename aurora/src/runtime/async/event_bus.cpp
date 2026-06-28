/* ── Aurora Runtime — Event Bus (signal/emit system) ── */
#include "runtime/async.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <utility>

/* ── Global event bus ── */
static struct {
    /* Single vector of handler+userdata pairs — never desynchronises */
    std::unordered_map<std::string, std::vector<std::pair<void(*)(void*), void*>>> handlers;
    std::mutex mtx;
    bool inited;
} g_ebus;

extern "C" {

void aurora_event_bus_init(void) {
    std::lock_guard<std::mutex> lock(g_ebus.mtx);
    if (g_ebus.inited) return;
    g_ebus.inited = true;
}

void aurora_event_on(const char* event_name, void (*handler)(void*), void* user_data) {
    if (!event_name || !handler) return;
    std::lock_guard<std::mutex> lock(g_ebus.mtx);
    if (!g_ebus.inited) {
        g_ebus.inited = true;
    }
    std::string name(event_name);
    g_ebus.handlers[name].emplace_back(handler, user_data);
}

void aurora_event_off(const char* event_name, void (*handler)(void*)) {
    if (!event_name || !handler) return;
    std::lock_guard<std::mutex> lock(g_ebus.mtx);
    std::string name(event_name);
    auto it = g_ebus.handlers.find(name);
    if (it == g_ebus.handlers.end()) return;
    auto& vec = it->second;
    for (size_t i = 0; i < vec.size(); i++) {
        if (vec[i].first == handler) {
            vec.erase(vec.begin() + i);
            break;
        }
    }
}

void aurora_event_emit(const char* event_name, void* arg) {
    if (!event_name) return;
    std::string name(event_name);
    std::vector<std::pair<void(*)(void*), void*>> snapshot;
    {
        std::lock_guard<std::mutex> lock(g_ebus.mtx);
        auto it = g_ebus.handlers.find(name);
        if (it == g_ebus.handlers.end()) return;
        snapshot = it->second;
    }
    for (size_t i = 0; i < snapshot.size(); i++) {
        if (snapshot[i].first)
            snapshot[i].first(snapshot[i].second ? snapshot[i].second : arg);
    }
}

void aurora_event_bus_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_ebus.mtx);
    g_ebus.handlers.clear();
    g_ebus.inited = false;
}

}
