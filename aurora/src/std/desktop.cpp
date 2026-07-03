#include "std/desktop.hpp"
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#ifndef DWMSBT_MAINWINDOW
#define DWMSBT_MAINWINDOW 3
#endif
#ifndef DWMSBT_ACRYLIC
#define DWMSBT_ACRYLIC 4
#endif
#ifndef DWMSBT_NONE
#define DWMSBT_NONE 1
#endif
#ifndef DWM_SYSTEMBACKDROP_TYPE
#define DWM_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_DEFAULT
#define DWMWCP_DEFAULT 0
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#endif

static char* dup_str(const char* s) {
    if (!s) return nullptr;
    size_t n = strlen(s) + 1;
    char* d = (char*)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

#ifdef _WIN32

static wchar_t* to_wide_alloc(const char* s) {
    if (!s || !*s) { wchar_t* z = (wchar_t*)malloc(2); if (z) z[0] = 0; return z; }
    int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    wchar_t* ws = (wchar_t*)malloc(len * sizeof(wchar_t));
    if (ws) MultiByteToWideChar(CP_UTF8, 0, s, -1, ws, len);
    return ws;
}

static const wchar_t* tray_class = L"AuroraDesktopWindow";

struct TrayData {
    NOTIFYICONDATAW nid;
    HMENU menu;
    void* callback;
    int   visible;
};

struct HotkeyEntry {
    int  id;
    void* callback;
};

static HWND          g_tray_hwnd     = nullptr;
static HotkeyEntry   g_hotkeys[64];
static int           g_hotkey_count  = 0;

static LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_COMMAND && HIWORD(wp) == 0) {
        int id = (int)LOWORD(wp);
        TrayData* td = (TrayData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (td && td->callback) ((void(*)(int))td->callback)(id);
        return 0;
    }
    if (msg == WM_HOTKEY) {
        int id = (int)wp;
        for (int i = 0; i < g_hotkey_count; i++) {
            if (g_hotkeys[i].id == id && g_hotkeys[i].callback) {
                ((void(*)())g_hotkeys[i].callback)();
                break;
            }
        }
        return 0;
    }
    if (msg >= WM_APP && msg <= WM_APP + 1) {
        TrayData* td = (TrayData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (td && td->callback && LOWORD(lp) == NIN_BALLOONUSERCLICK)
            ((void(*)(int))td->callback)(-1);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND ensure_tray_hwnd(void) {
    if (g_tray_hwnd) return g_tray_hwnd;
    WNDCLASSW wc = {};
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance   = GetModuleHandleW(nullptr);
    wc.lpszClassName = tray_class;
    RegisterClassW(&wc);
    g_tray_hwnd = CreateWindowExW(0, tray_class, L"AuroraDesktop", WS_OVERLAPPED,
        0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);
    return g_tray_hwnd;
}

#endif /* _WIN32 */

void aurora_desktop_init(void) {
#ifdef _WIN32
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    ensure_tray_hwnd();
#endif
}

void aurora_desktop_shutdown(void) {
#ifdef _WIN32
    for (int i = 0; i < g_hotkey_count; i++)
        UnregisterHotKey(g_tray_hwnd, g_hotkeys[i].id);
    g_hotkey_count = 0;
    if (g_tray_hwnd) { DestroyWindow(g_tray_hwnd); g_tray_hwnd = nullptr; }
    UnregisterClassW(tray_class, GetModuleHandleW(nullptr));
#endif
}

void* aurora_desktop_tray_create(const char* tooltip) {
#ifndef _WIN32
    (void)tooltip; return nullptr;
#else
    HWND hwnd = ensure_tray_hwnd();
    if (!hwnd) return nullptr;
    TrayData* td = (TrayData*)calloc(1, sizeof(TrayData));
    if (!td) return nullptr;
    td->nid.cbSize               = sizeof(NOTIFYICONDATAW);
    td->nid.hWnd                 = hwnd;
    td->nid.uID                  = 1;
    td->nid.uFlags               = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    td->nid.uCallbackMessage     = WM_APP;
    td->nid.hIcon                = LoadIconW(nullptr, IDI_APPLICATION);
    td->menu                     = CreatePopupMenu();
    td->visible                  = 1;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)td);
    wchar_t* wt = to_wide_alloc(tooltip);
    if (wt) { wcsncpy_s(td->nid.szTip, 128, wt, _TRUNCATE); free(wt); }
    Shell_NotifyIconW(NIM_ADD, &td->nid);
    return td;
#endif
}

void aurora_desktop_tray_destroy(void* tray) {
#ifndef _WIN32
    (void)tray;
#else
    if (!tray) return;
    TrayData* td = (TrayData*)tray;
    td->nid.uFlags = NIF_MESSAGE;
    Shell_NotifyIconW(NIM_DELETE, &td->nid);
    if (td->menu) DestroyMenu(td->menu);
    free(td);
#endif
}

void aurora_desktop_tray_set_tooltip(void* tray, const char* tip) {
#ifndef _WIN32
    (void)tray; (void)tip;
#else
    if (!tray) return;
    TrayData* td = (TrayData*)tray;
    wchar_t* wt = to_wide_alloc(tip);
    if (wt) { wcsncpy_s(td->nid.szTip, 128, wt, _TRUNCATE); free(wt); }
    td->nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &td->nid);
#endif
}

void aurora_desktop_tray_set_icon(void* tray, const char* path) {
#ifndef _WIN32
    (void)tray; (void)path;
#else
    if (!tray || !path) return;
    TrayData* td = (TrayData*)tray;
    wchar_t* wp = to_wide_alloc(path);
    HICON hIcon = (HICON)LoadImageW(nullptr, wp, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    free(wp);
    if (hIcon) { td->nid.hIcon = hIcon; td->nid.uFlags = NIF_ICON; Shell_NotifyIconW(NIM_MODIFY, &td->nid); }
#endif
}

void aurora_desktop_tray_add_menu_item(void* tray, int id, const char* text) {
#ifndef _WIN32
    (void)tray; (void)id; (void)text;
#else
    if (!tray) return;
    TrayData* td = (TrayData*)tray;
    wchar_t* wt = to_wide_alloc(text);
    AppendMenuW(td->menu, MF_STRING, id, wt);
    free(wt);
#endif
}

void aurora_desktop_tray_add_menu_separator(void* tray) {
#ifndef _WIN32
    (void)tray;
#else
    if (!tray) return;
    AppendMenuW(((TrayData*)tray)->menu, MF_SEPARATOR, 0, nullptr);
#endif
}

void aurora_desktop_tray_show_balloon(void* tray, const char* title, const char* text, int icon_type) {
#ifndef _WIN32
    (void)tray; (void)title; (void)text; (void)icon_type;
#else
    if (!tray) return;
    TrayData* td = (TrayData*)tray;
    td->nid.uFlags = NIF_INFO;
    wchar_t* wt = to_wide_alloc(title);
    wchar_t* wm = to_wide_alloc(text);
    if (wt) { wcsncpy_s(td->nid.szInfoTitle, 64, wt, _TRUNCATE); free(wt); }
    if (wm) { wcsncpy_s(td->nid.szInfo, 256, wm, _TRUNCATE); free(wm); }
    if      (icon_type == 1) td->nid.dwInfoFlags = NIIF_WARNING;
    else if (icon_type == 2) td->nid.dwInfoFlags = NIIF_ERROR;
    else                     td->nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &td->nid);
#endif
}

void aurora_desktop_tray_set_callback(void* tray, void* callback) {
#ifndef _WIN32
    (void)tray; (void)callback;
#else
    if (tray) ((TrayData*)tray)->callback = callback;
#endif
}

void aurora_desktop_tray_set_visible(void* tray, int visible) {
#ifndef _WIN32
    (void)tray; (void)visible;
#else
    if (!tray) return;
    TrayData* td = (TrayData*)tray;
    if (visible && !td->visible) { Shell_NotifyIconW(NIM_ADD, &td->nid); td->visible = 1; }
    else if (!visible && td->visible) { Shell_NotifyIconW(NIM_DELETE, &td->nid); td->visible = 0; }
#endif
}

int aurora_desktop_notification_show(const char* title, const char* message) {
#ifndef _WIN32
    (void)title; (void)message; return -1;
#else
    HWND hwnd = ensure_tray_hwnd();
    if (!hwnd) return -1;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID  = 2;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wchar_t* wt = to_wide_alloc(title);
    wchar_t* wm = to_wide_alloc(message);
    if (wt) { wcsncpy_s(nid.szInfoTitle, 64, wt, _TRUNCATE); free(wt); }
    if (wm) { wcsncpy_s(nid.szInfo, 256, wm, _TRUNCATE); free(wm); }
    return Shell_NotifyIconW(NIM_ADD, &nid) ? 0 : -1;
#endif
}

void aurora_desktop_notification_hide(void) {
#ifndef _WIN32
#else
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_tray_hwnd;
    nid.uID  = 2;
    Shell_NotifyIconW(NIM_DELETE, &nid);
#endif
}

int aurora_desktop_clipboard_set_text(const char* text) {
    if (!text) return -1;
#ifndef _WIN32
    (void)text; return -1;
#else
    if (!OpenClipboard(nullptr)) return -1;
    EmptyClipboard();
    size_t len = strlen(text) + 1;
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!h) { CloseClipboard(); return -1; }
    memcpy(GlobalLock(h), text, len);
    GlobalUnlock(h);
    SetClipboardData(CF_TEXT, h);
    CloseClipboard();
    return 0;
#endif
}

char* aurora_desktop_clipboard_get_text(void) {
#ifndef _WIN32
    return dup_str("");
#else
    if (!OpenClipboard(nullptr)) return dup_str("");
    HANDLE h = GetClipboardData(CF_TEXT);
    if (!h) { CloseClipboard(); return dup_str(""); }
    const char* data = (const char*)GlobalLock(h);
    char* result = dup_str(data ? data : "");
    GlobalUnlock(h);
    CloseClipboard();
    return result;
#endif
}

struct DropTargetData {
    HWND    hwnd;
    WNDPROC orig_proc;
    void*   callback;
};

static LRESULT CALLBACK DropWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DropTargetData* dd = (DropTargetData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_DROPFILES && dd && dd->callback) {
        HDROP hDrop = (HDROP)wp;
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; i++) {
            wchar_t path[MAX_PATH];
            DragQueryFileW(hDrop, i, path, MAX_PATH);
            int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
            char* f = (char*)malloc(len);
            if (f) { WideCharToMultiByte(CP_UTF8, 0, path, -1, f, len, nullptr, nullptr);
                ((void(*)(const char*, float, float))dd->callback)(f, 0, 0); free(f); }
        }
        DragFinish(hDrop);
        return 0;
    }
    if (dd) return CallWindowProcW(dd->orig_proc, hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void* aurora_desktop_drop_target_create(void* hwnd, void* callback) {
#ifndef _WIN32
    (void)hwnd; (void)callback; return nullptr;
#else
    if (!hwnd) return nullptr;
    HWND h = (HWND)hwnd;
    DropTargetData* dd = (DropTargetData*)calloc(1, sizeof(DropTargetData));
    if (!dd) return nullptr;
    dd->hwnd      = h;
    dd->orig_proc = (WNDPROC)GetWindowLongPtrW(h, GWLP_WNDPROC);
    dd->callback  = callback;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)dd);
    SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)DropWndProc);
    DragAcceptFiles(h, TRUE);
    return dd;
#endif
}

void aurora_desktop_drop_target_destroy(void* target) {
#ifndef _WIN32
    (void)target;
#else
    if (!target) return;
    DropTargetData* dd = (DropTargetData*)target;
    DragAcceptFiles(dd->hwnd, FALSE);
    SetWindowLongPtrW(dd->hwnd, GWLP_WNDPROC, (LONG_PTR)dd->orig_proc);
    SetWindowLongPtrW(dd->hwnd, GWLP_USERDATA, 0);
    free(dd);
#endif
}

int aurora_desktop_assoc_register(const char* ext, const char* prog_id, const char* desc, const char* command) {
#ifndef _WIN32
    (void)ext; (void)prog_id; (void)desc; (void)command; return -1;
#else
    if (!ext || !prog_id || !command) return -1;
    wchar_t key[512];
    wchar_t* we = to_wide_alloc(ext);
    wchar_t* wp = to_wide_alloc(prog_id);
    wchar_t* wd = to_wide_alloc(desc);
    wchar_t* wc = to_wide_alloc(command);
    HKEY hk;
    wsprintfW(key, L"Software\\Classes\\%s", we);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hk, nullptr, 0, REG_SZ, (const BYTE*)wp, (int)((wcslen(wp) + 1) * 2));
        RegCloseKey(hk);
    }
    wsprintfW(key, L"Software\\Classes\\%s", wp);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        if (wd) RegSetValueExW(hk, nullptr, 0, REG_SZ, (const BYTE*)wd, (int)((wcslen(wd) + 1) * 2));
        RegCloseKey(hk);
    }
    wsprintfW(key, L"Software\\Classes\\%s\\shell\\open\\command", wp);
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(hk, nullptr, 0, REG_SZ, (const BYTE*)wc, (int)((wcslen(wc) + 1) * 2));
        RegCloseKey(hk);
    }
    free(we); free(wp); free(wd); free(wc);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return 0;
#endif
}

int aurora_desktop_assoc_unregister(const char* ext, const char* prog_id) {
#ifndef _WIN32
    (void)ext; (void)prog_id; return -1;
#else
    wchar_t key[512];
    if (ext) {
        wchar_t* we = to_wide_alloc(ext);
        wsprintfW(key, L"Software\\Classes\\%s", we);
        RegDeleteTreeW(HKEY_CURRENT_USER, key);
        free(we);
    }
    if (prog_id) {
        wchar_t* wp = to_wide_alloc(prog_id);
        wsprintfW(key, L"Software\\Classes\\%s", wp);
        RegDeleteTreeW(HKEY_CURRENT_USER, key);
        free(wp);
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return 0;
#endif
}

int aurora_desktop_assoc_is_registered(const char* ext) {
#ifndef _WIN32
    (void)ext; return 0;
#else
    if (!ext) return 0;
    wchar_t key[512];
    wchar_t* we = to_wide_alloc(ext);
    wsprintfW(key, L"Software\\Classes\\%s", we);
    free(we);
    HKEY hk;
    LONG ret = RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hk);
    if (ret == ERROR_SUCCESS) { RegCloseKey(hk); return 1; }
    return 0;
#endif
}

int aurora_desktop_startup_set(const char* app_name, const char* command, int enable) {
#ifndef _WIN32
    (void)app_name; (void)command; (void)enable; return -1;
#else
    if (!app_name) return -1;
    HKEY hk;
    LONG ret = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hk);
    if (ret != ERROR_SUCCESS) return -1;
    wchar_t* wn = to_wide_alloc(app_name);
    if (enable && command) {
        wchar_t* wc = to_wide_alloc(command);
        RegSetValueExW(hk, wn, 0, REG_SZ, (const BYTE*)wc, (int)((wcslen(wc) + 1) * 2));
        free(wc);
    } else {
        RegDeleteValueW(hk, wn);
    }
    free(wn);
    RegCloseKey(hk);
    return 0;
#endif
}

int aurora_desktop_startup_is_enabled(const char* app_name) {
#ifndef _WIN32
    (void)app_name; return 0;
#else
    HKEY hk;
    LONG ret = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hk);
    if (ret != ERROR_SUCCESS) return 0;
    wchar_t* wn = to_wide_alloc(app_name);
    ret = RegQueryValueExW(hk, wn, nullptr, nullptr, nullptr, nullptr);
    free(wn);
    RegCloseKey(hk);
    return (ret == ERROR_SUCCESS) ? 1 : 0;
#endif
}

int aurora_desktop_window_set_effect(void* hwnd, int effect) {
#ifndef _WIN32
    (void)hwnd; (void)effect; return -1;
#else
    if (!hwnd) return -1;
    HWND h = (HWND)hwnd;
    if (effect == AURORA_WINDOW_EFFECT_MICA) {
        int bt = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute(h, DWMWA_SYSTEMBACKDROP_TYPE, &bt, sizeof(bt));
    } else if (effect == AURORA_WINDOW_EFFECT_ACRYLIC) {
        int bt = DWMSBT_ACRYLIC;
        DwmSetWindowAttribute(h, DWMWA_SYSTEMBACKDROP_TYPE, &bt, sizeof(bt));
    } else if (effect == AURORA_WINDOW_EFFECT_BLUR) {
        DWM_BLURBEHIND bb = { TRUE, FALSE, nullptr, 0 };
        DwmEnableBlurBehindWindow(h, &bb);
    } else {
        DWM_BLURBEHIND bb = { FALSE, FALSE, nullptr, 0 };
        DwmEnableBlurBehindWindow(h, &bb);
        int bt = DWMSBT_NONE;
        DwmSetWindowAttribute(h, DWMWA_SYSTEMBACKDROP_TYPE, &bt, sizeof(bt));
    }
    return 0;
#endif
}

int aurora_desktop_window_set_dark_mode(void* hwnd, int enable) {
#ifndef _WIN32
    (void)hwnd; (void)enable; return -1;
#else
    if (!hwnd) return -1;
    BOOL dark = enable ? TRUE : FALSE;
    DwmSetWindowAttribute((HWND)hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    return 0;
#endif
}

int aurora_desktop_window_set_round_corners(void* hwnd, int enable) {
#ifndef _WIN32
    (void)hwnd; (void)enable; return -1;
#else
    if (!hwnd) return -1;
    int pref = enable ? DWMWCP_ROUND : DWMWCP_DEFAULT;
    DwmSetWindowAttribute((HWND)hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    return 0;
#endif
}

int aurora_desktop_hotkey_register(int id, int ctrl, int alt, int shift, int key, void* callback) {
#ifndef _WIN32
    (void)id; (void)ctrl; (void)alt; (void)shift; (void)key; (void)callback; return -1;
#else
    HWND hwnd = ensure_tray_hwnd();
    if (!hwnd || !callback) return -1;
    UINT mod = 0;
    if (ctrl)  mod |= MOD_CONTROL;
    if (alt)   mod |= MOD_ALT;
    if (shift) mod |= MOD_SHIFT;
    if (!RegisterHotKey(hwnd, id, mod, (UINT)key)) return -1;
    if (g_hotkey_count < 64) {
        g_hotkeys[g_hotkey_count].id       = id;
        g_hotkeys[g_hotkey_count].callback = callback;
        g_hotkey_count++;
    }
    return 0;
#endif
}

void aurora_desktop_hotkey_unregister(int id) {
#ifndef _WIN32
    (void)id;
#else
    UnregisterHotKey(g_tray_hwnd, id);
    for (int i = 0; i < g_hotkey_count; i++) {
        if (g_hotkeys[i].id == id) {
            for (int j = i; j + 1 < g_hotkey_count; j++) g_hotkeys[j] = g_hotkeys[j + 1];
            g_hotkey_count--;
            break;
        }
    }
#endif
}
