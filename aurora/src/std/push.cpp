#include "std/push.hpp"
#include "std/json.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <ctime>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

static struct {
    int initialized;
    int registered;
    char token[512];
    void* callback;
    std::string device_id;
} g_push = {};

static std::string gen_device_id() {
    char buf[64];
#ifdef _WIN32
    DWORD serial = 0;
    if (GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0)) {
        snprintf(buf, sizeof(buf), "WIN-%08X", serial);
        return buf;
    }
    return "WIN-DEFAULT";
#else
    FILE* f = fopen("/etc/machine-id", "r");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);
            size_t len = strlen(buf);
            if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
            return "LNX-" + std::string(buf);
        }
        fclose(f);
    }
    return "LNX-DEFAULT";
#endif
}

int aurora_push_init(void) {
    memset(&g_push, 0, sizeof(g_push));
    g_push.initialized = 1;
    g_push.device_id = gen_device_id();
    g_push.callback = nullptr;
    return 1;
}

int aurora_push_register(void) {
    if (!g_push.initialized) return 0;
    /* Simulate registration with push notification service */
    snprintf(g_push.token, sizeof(g_push.token), "%s-PUSH-TOKEN-%ld",
             g_push.device_id.c_str(), (long)time(nullptr));
    g_push.registered = 1;
    return 1;
}

int aurora_push_unregister(void) {
    if (!g_push.initialized) return 0;
    g_push.registered = 0;
    g_push.token[0] = '\0';
    return 1;
}

int aurora_push_is_registered(void) {
    if (!g_push.initialized) return 0;
    return g_push.registered;
}

int aurora_push_set_callback(void* callback) {
    if (!g_push.initialized) return 0;
    g_push.callback = callback;
    return 1;
}

const char* aurora_push_get_token(void) {
    if (!g_push.initialized || !g_push.registered) return nullptr;
    return g_push.token;
}

int aurora_push_send_local(const char* title, const char* body, int delay_ms) {
    if (!g_push.initialized || !title || !body) return 0;
#ifdef _WIN32
    /* Use Windows notification toast */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "powershell -Command \"& { "
             "[Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType=WindowsRuntime] > $null; "
             "$template = [Windows.UI.Notifications.ToastNotificationManager]::GetTemplateContent([Windows.UI.Notifications.ToastTemplateType]::ToastText02); "
             "$textNodes = $template.GetElementsByTagName('text'); "
             "$textNodes.Item(0).AppendChild($template.CreateTextNode('%s')) > $null; "
             "$textNodes.Item(1).AppendChild($template.CreateTextNode('%s')) > $null; "
             "$toast = [Windows.UI.Notifications.ToastNotification]::new($template); "
             "[Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier().Show($toast) }\"",
             title, body);
    system(cmd);
#else
    /* Use notify-send on Linux */
    std::string cmd = "notify-send \"" + std::string(title) + "\" \"" + std::string(body) + "\"";
    system(cmd.c_str());
#endif
    (void)delay_ms;
    return 1;
}

int aurora_push_cancel_local(int notification_id) {
    if (!g_push.initialized) return 0;
    (void)notification_id;
    return 1;
}

void aurora_push_shutdown(void) {
    if (g_push.registered) aurora_push_unregister();
    memset(&g_push, 0, sizeof(g_push));
}
