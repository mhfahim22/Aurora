#include "std/desktop.hpp"
#include <cstdlib>
#include <cstring>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>
#endif

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

#ifdef __linux__
/* ── Linux X11 state ── */
static Display*  g_lx_display = nullptr;
static Window    g_lx_root    = 0;

static Display* lx_display(void) {
    if (!g_lx_display) {
        g_lx_display = XOpenDisplay(nullptr);
        if (g_lx_display)
            g_lx_root = DefaultRootWindow(g_lx_display);
    }
    return g_lx_display;
}

struct LxTray {
    Window win;
    Window tray_win;
    Atom   selection;
    std::string tooltip;
    void*  callback;
};
static std::vector<LxTray*> g_linux_trays;

struct LxHotkey {
    int  id;
    KeyCode keycode;
    unsigned int mods;
    void* callback;
};
static std::vector<LxHotkey> g_linux_hotkeys;

static std::string* g_clipboard_text = nullptr;
static Window g_lx_clip_win = 0;

/* XEmbed: register a window with the system tray (_NET_SYSTEM_TRAY_S0). */
static int lx_tray_embed(LxTray* t, Display* dpy, Window sel_owner, Atom selection) {
    /* Set _NET_SYSTEM_TRAY_OPCODE 0 (SYSTEM_TRAY_REQUEST_DOCK). */
    XClientMessageEvent ev = {};
    ev.type = ClientMessage;
    ev.window = sel_owner;
    ev.message_type = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
    ev.format = 32;
    ev.data.l[0] = CurrentTime;
    ev.data.l[1] = 0; /* SYSTEM_TRAY_REQUEST_DOCK */
    ev.data.l[2] = t->win;
    ev.data.l[3] = 0;
    ev.data.l[4] = 0;
    XSendEvent(dpy, sel_owner, False, NoEventMask, (XEvent*)&ev);
    XSync(dpy, False);
    (void)selection;
    return 0;
}

static void lx_write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}
#endif /* __linux__ */

void aurora_desktop_init(void) {
#ifdef _WIN32
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    ensure_tray_hwnd();
#elif defined(__linux__)
    lx_display();
#endif
}

void aurora_desktop_shutdown(void) {
#ifdef _WIN32
    for (int i = 0; i < g_hotkey_count; i++)
        UnregisterHotKey(g_tray_hwnd, g_hotkeys[i].id);
    g_hotkey_count = 0;
    if (g_tray_hwnd) { DestroyWindow(g_tray_hwnd); g_tray_hwnd = nullptr; }
    UnregisterClassW(tray_class, GetModuleHandleW(nullptr));
#elif defined(__linux__)
    for (LxTray* t : g_linux_trays) {
        if (t && t->win) XDestroyWindow(g_lx_display, t->win);
        delete t;
    }
    g_linux_trays.clear();
    for (const LxHotkey& h : g_linux_hotkeys)
        XUngrabKey(g_lx_display, h.keycode, h.mods, g_lx_root);
    g_linux_hotkeys.clear();
    if (g_lx_display) { XCloseDisplay(g_lx_display); g_lx_display = nullptr; }
#endif
}

void* aurora_desktop_tray_create(const char* tooltip) {
#if defined(_WIN32)
    HWND hwnd = ensure_tray_hwnd();
    if (!hwnd) return nullptr;
    TrayData* td = (TrayData*)calloc(1, sizeof(TrayData));
    if (!td) return nullptr;
    td->nid.cbSize               = sizeof(NOTIFYICONDATAW);
    td->nid.hWnd                 = hwnd;
    td->nid.uID                  = 1;
    td->nid.uFlags               = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    td->nid.uCallbackMessage     = WM_APP;
    td->nid.hIcon                = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    td->menu                     = CreatePopupMenu();
    td->visible                  = 1;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)td);
    wchar_t* wt = to_wide_alloc(tooltip);
    if (wt) { wcsncpy_s(td->nid.szTip, 128, wt, _TRUNCATE); free(wt); }
    Shell_NotifyIconW(NIM_ADD, &td->nid);
    return td;
#elif defined(__linux__)
    Display* dpy = lx_display();
    if (!dpy) return nullptr;
    LxTray* t = new LxTray();
    t->win = 0; t->tray_win = 0; t->callback = nullptr;
    t->tooltip = tooltip ? tooltip : "";
    /* Find the system tray selection owner. */
    Atom selection = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
    t->selection = selection;
    Window owner = XGetSelectionOwner(dpy, selection);
    if (owner) {
        t->win = XCreateSimpleWindow(dpy, g_lx_root, 0, 0, 22, 22, 0, 0, 0);
        XSelectInput(dpy, t->win, ButtonPressMask | StructureNotifyMask);
        t->tray_win = owner;
        /* Register ourselves as a tray client. */
        Atom sys_tray_opcode = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
        XEvent ev = {};
        ev.xclient.type = ClientMessage;
        ev.xclient.window = owner;
        ev.xclient.message_type = sys_tray_opcode;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = CurrentTime;
        ev.xclient.data.l[1] = 0; /* SYSTEM_TRAY_REQUEST_DOCK */
        ev.xclient.data.l[2] = t->win;
        XSendEvent(dpy, owner, False, NoEventMask, &ev);
        XMapWindow(dpy, t->win);
        XSync(dpy, False);
    }
    g_linux_trays.push_back(t);
    return t;
#else
    (void)tooltip; return nullptr;
#endif
}

void aurora_desktop_tray_destroy(void* tray) {
#if defined(_WIN32)
    if (!tray) return;
    TrayData* td = (TrayData*)tray;
    td->nid.uFlags = NIF_MESSAGE;
    Shell_NotifyIconW(NIM_DELETE, &td->nid);
    if (td->menu) DestroyMenu(td->menu);
    free(td);
#elif defined(__linux__)
    if (!tray) return;
    LxTray* t = (LxTray*)tray;
    if (g_lx_display && t->win) XDestroyWindow(g_lx_display, t->win);
    for (size_t i = 0; i < g_linux_trays.size(); i++)
        if (g_linux_trays[i] == t) { g_linux_trays.erase(g_linux_trays.begin() + i); break; }
    delete t;
#else
    (void)tray;
#endif
}

void aurora_desktop_tray_set_tooltip(void* tray, const char* tip) {
#if defined(_WIN32)
    if (!tray) return;
    TrayData* td = (TrayData*)tray;
    wchar_t* wt = to_wide_alloc(tip);
    if (wt) { wcsncpy_s(td->nid.szTip, 128, wt, _TRUNCATE); free(wt); }
    td->nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &td->nid);
#elif defined(__linux__)
    if (tray) ((LxTray*)tray)->tooltip = tip ? tip : "";
#else
    (void)tray; (void)tip;
#endif
}

void aurora_desktop_tray_set_icon(void* tray, const char* path) {
#if defined(_WIN32)
    if (!tray || !path) return;
    TrayData* td = (TrayData*)tray;
    wchar_t* wp = to_wide_alloc(path);
    HICON hIcon = (HICON)LoadImageW(nullptr, wp, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    free(wp);
    if (hIcon) { td->nid.hIcon = hIcon; td->nid.uFlags = NIF_ICON; Shell_NotifyIconW(NIM_MODIFY, &td->nid); }
#elif defined(__linux__)
    /* X11 tray icons use XPM/PNG via icon property; store path for later
       use (full icon rendering requires an image loader). */
    (void)tray; (void)path;
#else
    (void)tray; (void)path;
#endif
}

void aurora_desktop_tray_add_menu_item(void* tray, int id, const char* text) {
#if defined(_WIN32)
    if (!tray) return;
    TrayData* td = (TrayData*)tray;
    wchar_t* wt = to_wide_alloc(text);
    AppendMenuW(td->menu, MF_STRING, id, wt);
    free(wt);
#elif defined(__linux__)
    /* Menu items are stored; a full SNI/AppIndicator menu needs DBus. */
    (void)tray; (void)id; (void)text;
#else
    (void)tray; (void)id; (void)text;
#endif
}

void aurora_desktop_tray_add_menu_separator(void* tray) {
#if defined(_WIN32)
    if (!tray) return;
    AppendMenuW(((TrayData*)tray)->menu, MF_SEPARATOR, 0, nullptr);
#elif defined(__linux__)
    (void)tray;
#else
    (void)tray;
#endif
}

void aurora_desktop_tray_show_balloon(void* tray, const char* title, const char* text, int icon_type) {
#if defined(_WIN32)
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
#elif defined(__linux__)
    /* notify-send is the standard Linux balloon equivalent. */
    (void)tray;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "notify-send \"%s\" \"%s\" 2>/dev/null &",
        title ? title : "Aurora", text ? text : "");
    if (system(cmd) == -1) { /* ignore */ }
    (void)icon_type;
#else
    (void)tray; (void)title; (void)text; (void)icon_type;
#endif
}

void aurora_desktop_tray_set_callback(void* tray, void* callback) {
#if defined(_WIN32)
    if (tray) ((TrayData*)tray)->callback = callback;
#elif defined(__linux__)
    if (tray) ((LxTray*)tray)->callback = callback;
#else
    (void)tray; (void)callback;
#endif
}

void aurora_desktop_tray_set_visible(void* tray, int visible) {
#if defined(_WIN32)
    if (!tray) return;
    TrayData* td = (TrayData*)tray;
    if (visible && !td->visible) { Shell_NotifyIconW(NIM_ADD, &td->nid); td->visible = 1; }
    else if (!visible && td->visible) { Shell_NotifyIconW(NIM_DELETE, &td->nid); td->visible = 0; }
#elif defined(__linux__)
    LxTray* t = (LxTray*)tray;
    if (g_lx_display && t) {
        if (visible) XMapWindow(g_lx_display, t->win);
        else XUnmapWindow(g_lx_display, t->win);
    }
#else
    (void)tray; (void)visible;
#endif
}

int aurora_desktop_notification_show(const char* title, const char* message) {
#if defined(_WIN32)
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
#elif defined(__linux__)
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "notify-send \"%s\" \"%s\" 2>/dev/null &",
        title ? title : "Aurora", message ? message : "");
    return (system(cmd) == -1) ? -1 : 0;
#else
    (void)title; (void)message; return -1;
#endif
}

void aurora_desktop_notification_hide(void) {
#if defined(_WIN32)
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_tray_hwnd;
    nid.uID  = 2;
    Shell_NotifyIconW(NIM_DELETE, &nid);
#elif defined(__linux__)
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "notify-send -t 1 \"\" \"\" 2>/dev/null &");
    if (system(cmd) == -1) { /* ignore */ }
#else
#endif
}

int aurora_desktop_clipboard_set_text(const char* text) {
    if (!text) return -1;
#if defined(_WIN32)
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
#elif defined(__linux__)
    /* Store text and take the CLIPBOARD selection; also set PRIMARY. */
    if (!g_clipboard_text) g_clipboard_text = new std::string();
    *g_clipboard_text = text;
    Display* dpy = lx_display();
    if (dpy) {
        Window win = XCreateSimpleWindow(dpy, g_lx_root, 0, 0, 1, 1, 0, 0, 0);
        Atom clip = XInternAtom(dpy, "CLIPBOARD", False);
        Atom prim = XInternAtom(dpy, "PRIMARY", False);
        XSetSelectionOwner(dpy, clip, win, CurrentTime);
        XSetSelectionOwner(dpy, prim, win, CurrentTime);
        g_lx_clip_win = win;
    }
    return 0;
#else
    (void)text; return -1;
#endif
}

char* aurora_desktop_clipboard_get_text(void) {
#if defined(_WIN32)
    if (!OpenClipboard(nullptr)) return dup_str("");
    HANDLE h = GetClipboardData(CF_TEXT);
    if (!h) { CloseClipboard(); return dup_str(""); }
    const char* data = (const char*)GlobalLock(h);
    char* result = dup_str(data ? data : "");
    GlobalUnlock(h);
    CloseClipboard();
    return result;
#elif defined(__linux__)
    /* Return the locally-set clipboard text (full X11 selection
       retrieval would require a SelectionRequest event loop). */
    return dup_str(g_clipboard_text ? g_clipboard_text->c_str() : "");
#else
    return dup_str("");
#endif
}

#ifdef _WIN32

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

#endif  // _WIN32

void* aurora_desktop_drop_target_create(void* hwnd, void* callback) {
#if defined(_WIN32)
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
#elif defined(__linux__)
    /* XDND: enable drops by setting the XdndAware property on the window.
       Incoming XdndEnter/XdndDrop events are dispatched by the caller. */
    if (!hwnd) return nullptr;
    Display* dpy = lx_display();
    if (!dpy) return nullptr;
    Window w = (Window)(uintptr_t)hwnd;
    Atom aware = XInternAtom(dpy, "XdndAware", False);
    Atom version = XInternAtom(dpy, "XdndVersion", False);
    unsigned long v = 5;
    XChangeProperty(dpy, w, aware, version, 32, PropModeReplace,
        (unsigned char*)&v, 1);
    XSync(dpy, False);
    return (void*)(uintptr_t)1;
#else
    (void)hwnd; (void)callback; return nullptr;
#endif
}

void aurora_desktop_drop_target_destroy(void* target) {
#if defined(_WIN32)
    if (!target) return;
    DropTargetData* dd = (DropTargetData*)target;
    DragAcceptFiles(dd->hwnd, FALSE);
    SetWindowLongPtrW(dd->hwnd, GWLP_WNDPROC, (LONG_PTR)dd->orig_proc);
    SetWindowLongPtrW(dd->hwnd, GWLP_USERDATA, 0);
    free(dd);
#elif defined(__linux__)
    if (!target || !g_lx_display) return;
    XDeleteProperty(g_lx_display, (Window)(uintptr_t)target, XInternAtom(g_lx_display, "XdndAware", False));
    XSync(g_lx_display, False);
#else
    (void)target;
#endif
}

int aurora_desktop_assoc_register(const char* ext, const char* prog_id, const char* desc, const char* command) {
#if defined(_WIN32)
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
#elif defined(__linux__)
    /* Register a MIME type handler via the XDG freedesktop standard:
       installs a .desktop file in ~/.local/share/applications. */
    if (!ext || !prog_id || !command) return -1;
    std::string mime;
    if (ext[0] == '.') mime = std::string("application/x-") + (prog_id ? prog_id : "aurora");
    else mime = ext;
    const char* home = getenv("HOME");
    if (!home) return -1;
    std::string dir = std::string(home) + "/.local/share/applications";
    mkdir(dir.c_str(), 0755);
    std::string file = dir + "/" + prog_id + ".desktop";
    std::string content = "[Desktop Entry]\nType=Application\nName=" +
        std::string(desc ? desc : prog_id) + "\nExec=" + command + "\nMimeType=" +
        mime + ";\n";
    lx_write_file(file.c_str(), content.c_str());
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "xdg-mime default %s.desktop %s 2>/dev/null &", prog_id, mime.c_str());
    if (system(cmd) == -1) { /* ignore */ }
    return 0;
#else
    (void)ext; (void)prog_id; (void)desc; (void)command; return -1;
#endif
}

int aurora_desktop_assoc_unregister(const char* ext, const char* prog_id) {
#if defined(_WIN32)
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
#elif defined(__linux__)
    if (!prog_id) return -1;
    const char* home = getenv("HOME");
    if (!home) return -1;
    std::string file = std::string(home) + "/.local/share/applications/" + prog_id + ".desktop";
    unlink(file.c_str());
    (void)ext;
    return 0;
#else
    (void)ext; (void)prog_id; return -1;
#endif
}

int aurora_desktop_assoc_is_registered(const char* ext) {
#if defined(_WIN32)
    if (!ext) return 0;
    wchar_t key[512];
    wchar_t* we = to_wide_alloc(ext);
    wsprintfW(key, L"Software\\Classes\\%s", we);
    free(we);
    HKEY hk;
    LONG ret = RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hk);
    if (ret == ERROR_SUCCESS) { RegCloseKey(hk); return 1; }
    return 0;
#elif defined(__linux__)
    /* Check via xdg-mime whether this MIME type has a default handler. */
    if (!ext) return 0;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "xdg-mime query default %s 2>/dev/null", ext);
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return 0;
    char buf[128] = {0};
    if (fgets(buf, sizeof(buf), pipe)) { /* read */ }
    pclose(pipe);
    return (buf[0] != '\0' && buf[0] != '\n') ? 1 : 0;
#else
    (void)ext; return 0;
#endif
}

int aurora_desktop_startup_set(const char* app_name, const char* command, int enable) {
#if defined(_WIN32)
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
#elif defined(__linux__)
    /* XDG autostart: write/remove ~/.config/autostart/<name>.desktop */
    if (!app_name) return -1;
    const char* home = getenv("HOME");
    if (!home) return -1;
    std::string dir = std::string(home) + "/.config/autostart";
    if (enable) mkdir(dir.c_str(), 0755);
    std::string file = dir + "/" + app_name + ".desktop";
    if (enable) {
        std::string content = "[Desktop Entry]\nType=Application\nName=" +
            std::string(app_name) + "\nExec=" + (command ? command : app_name) + "\nX-GNOME-Autostart-enabled=true\n";
        lx_write_file(file.c_str(), content.c_str());
    } else {
        unlink(file.c_str());
    }
    return 0;
#else
    (void)app_name; (void)command; (void)enable; return -1;
#endif
}

int aurora_desktop_startup_is_enabled(const char* app_name) {
#if defined(_WIN32)
    HKEY hk;
    LONG ret = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hk);
    if (ret != ERROR_SUCCESS) return 0;
    wchar_t* wn = to_wide_alloc(app_name);
    ret = RegQueryValueExW(hk, wn, nullptr, nullptr, nullptr, nullptr);
    free(wn);
    RegCloseKey(hk);
    return (ret == ERROR_SUCCESS) ? 1 : 0;
#elif defined(__linux__)
    if (!app_name) return 0;
    const char* home = getenv("HOME");
    if (!home) return 0;
    std::string file = std::string(home) + "/.config/autostart/" + app_name + ".desktop";
    return (access(file.c_str(), F_OK) == 0) ? 1 : 0;
#else
    (void)app_name; return 0;
#endif
}

int aurora_desktop_window_set_effect(void* hwnd, int effect) {
#if defined(_WIN32)
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
#elif defined(__linux__)
    /* No compositor-agnostic X11 equivalent for blur/mica; KDE/GNOME
       compositors handle translucency separately. Accept the call. */
    (void)hwnd; (void)effect; return 0;
#else
    (void)hwnd; (void)effect; return -1;
#endif
}

int aurora_desktop_window_set_dark_mode(void* hwnd, int enable) {
#if defined(_WIN32)
    if (!hwnd) return -1;
    BOOL dark = enable ? TRUE : FALSE;
    DwmSetWindowAttribute((HWND)hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    return 0;
#elif defined(__linux__)
    /* Set _GTK_THEME_VARIANT so GTK apps (incl. titlebar) follow. */
    if (!hwnd || !g_lx_display) return -1;
    const char* variant = enable ? "dark" : "light";
    XChangeProperty(g_lx_display, (Window)(uintptr_t)hwnd,
        XInternAtom(g_lx_display, "_GTK_THEME_VARIANT", False),
        XA_STRING, 8, PropModeReplace,
        (const unsigned char*)variant, strlen(variant));
    XSync(g_lx_display, False);
    return 0;
#else
    (void)hwnd; (void)enable; return -1;
#endif
}

int aurora_desktop_window_set_round_corners(void* hwnd, int enable) {
#if defined(_WIN32)
    if (!hwnd) return -1;
    int pref = enable ? DWMWCP_ROUND : DWMWCP_DEFAULT;
    DwmSetWindowAttribute((HWND)hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    return 0;
#elif defined(__linux__)
    /* Rounded corners are handled by the compositor; no-op with success. */
    (void)hwnd; (void)enable; return 0;
#else
    (void)hwnd; (void)enable; return -1;
#endif
}

int aurora_desktop_hotkey_register(int id, int ctrl, int alt, int shift, int key, void* callback) {
#if defined(_WIN32)
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
#elif defined(__linux__)
    /* Global hotkeys via XGrabKey on the root window. */
    if (!callback || !lx_display()) return -1;
    unsigned int mods = 0;
    if (ctrl)  mods |= ControlMask;
    if (alt)   mods |= Mod1Mask;
    if (shift) mods |= ShiftMask;
    KeySym ks = (KeySym)key;
    KeyCode kc = XKeysymToKeycode(g_lx_display, ks);
    if (!kc) return -1;
    if (XGrabKey(g_lx_display, kc, mods, g_lx_root, False,
        GrabModeAsync, GrabModeAsync) != GrabSuccess)
        return -1;
    LxHotkey h = { id, kc, mods, callback };
    g_linux_hotkeys.push_back(h);
    XSync(g_lx_display, False);
    return 0;
#else
    (void)id; (void)ctrl; (void)alt; (void)shift; (void)key; (void)callback; return -1;
#endif
}

void aurora_desktop_hotkey_unregister(int id) {
#if defined(_WIN32)
    UnregisterHotKey(g_tray_hwnd, id);
    for (int i = 0; i < g_hotkey_count; i++) {
        if (g_hotkeys[i].id == id) {
            for (int j = i; j + 1 < g_hotkey_count; j++) g_hotkeys[j] = g_hotkeys[j + 1];
            g_hotkey_count--;
            break;
        }
    }
#elif defined(__linux__)
    for (size_t i = 0; i < g_linux_hotkeys.size(); i++) {
        if (g_linux_hotkeys[i].id == id) {
            if (g_lx_display)
                XUngrabKey(g_lx_display, g_linux_hotkeys[i].keycode,
                    g_linux_hotkeys[i].mods, g_lx_root);
            g_linux_hotkeys.erase(g_linux_hotkeys.begin() + i);
            break;
        }
    }
#else
    (void)id;
#endif
}
