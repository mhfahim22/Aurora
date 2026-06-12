/* ── Aurora Runtime — Event Bus (signal/emit system) ── */
#include "runtime/async.hpp"
#include <string>
#include <vector>
#include <unordered_map>

/* ── Global event bus ── */
static struct {
    std::unordered_map<std::string, std::vector<void(*)(void*)>> handlers;
    std::unordered_map<std::string, std::vector<void*>> user_data;
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
    g_ebus.handlers[name].push_back(handler);
    g_ebus.user_data[name].push_back(user_data);
}

void aurora_event_off(const char* event_name, void (*handler)(void*)) {
    if (!event_name || !handler) return;
    std::lock_guard<std::mutex> lock(g_ebus.mtx);
    std::string name(event_name);
    auto it = g_ebus.handlers.find(name);
    if (it == g_ebus.handlers.end()) return;
    auto& vec = it->second;
    auto& ud_vec = g_ebus.user_data[name];
    for (size_t i = 0; i < vec.size(); i++) {
        if (vec[i] == handler) {
            vec.erase(vec.begin() + i);
            if (i < ud_vec.size())
                ud_vec.erase(ud_vec.begin() + i);
            break;
        }
    }
}

void aurora_event_emit(const char* event_name, void* arg) {
    if (!event_name) return;
    std::string name(event_name);
    std::vector<void(*)(void*)> snapshot;
    std::vector<void*> ud_snapshot;
    {
        std::lock_guard<std::mutex> lock(g_ebus.mtx);
        auto it = g_ebus.handlers.find(name);
        if (it == g_ebus.handlers.end()) return;
        snapshot = it->second;
        auto ud_it = g_ebus.user_data.find(name);
        if (ud_it != g_ebus.user_data.end())
            ud_snapshot = ud_it->second;
    }
    for (size_t i = 0; i < snapshot.size(); i++) {
        if (snapshot[i])
            snapshot[i](ud_snapshot.size() > i ? ud_snapshot[i] : arg);
    }
}

void aurora_event_bus_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_ebus.mtx);
    g_ebus.handlers.clear();
    g_ebus.user_data.clear();
    g_ebus.inited = false;
}

}
