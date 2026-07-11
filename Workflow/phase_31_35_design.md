# Phase 31–35: Web & App Development Completion

**Goal:** Aurora v2.0.0 — 100% production-ready for both web and app development across all 5 platforms (Windows, Linux, macOS, Android, iOS).

**Current State (v1.0.0):** App 75% · Web 65% · Overall 70%

**Target State (v2.0.0):** App 100% · Web 100% · Overall 100%

---

## Table of Contents

1. [Phase 31: Web Framework DSL Completion (Web 65% → 85%)](#phase-31-web-framework-dsl-completion)
2. [Phase 32: Desktop GUI Completion — Linux & macOS (App 75% → 85%)](#phase-32-desktop-gui-completion-linux--macos)
3. [Phase 33: Complex Widgets & Developer Tools (App 85% → 92%)](#phase-33-complex-widgets--developer-tools)
4. [Phase 34: Full-Stack Web & Production Hardening (Web 85% → 100%)](#phase-34-full-stack-web--production-hardening)
5. [Phase 35: End-to-End Testing, CI/CD & Documentation (Both 92% → 100%)](#phase-35-end-to-end-testing-cicd--documentation)

---

# Phase 31: Web Framework DSL Completion

**Web 65% → 85% | App 75% → 75%**

**Core theme:** Complete the Aurora server DSL so that every feature accessible via C API is also accessible from Aurora code with clean syntax. No more `request("body")` string hacks — real typed accessors.

---

## 31.1 request.params — Path Parameter Access

**Problem:** Route patterns like `/user/:id` are parsed at runtime by `match_route_pattern()` in `server.cpp` and populated into `AuroraHttpRequest.param_names[]` / `param_values[]`, but no codegen path exists to access them from Aurora code. `aurora_http_get_param()` exists in `backend.hpp` line 100 and is fully implemented — just not reachable from the DSL.

### Implementation

**Files to modify:**
- `aurora/src/compiler/parser/parse_expr.cpp` — Add `request.params.id` expression parsing
- `aurora/src/compiler/codegen/codegen_expr.cpp` — Generate call to `aurora_http_get_param()`
- `aurora/src/compiler/ast.hpp` — Ensure `NodeType::Request` supports dot-access children
- `aurora/src/runtime/backend/backend.hpp` — Already declared (line 100), verify
- `aurora/src/runtime/backend/server.cpp` — Already implemented

**Grammar:**
```aura
route "GET" "/user/:id"
    name = request.params.id
    response("{\"user\": \"" + name + "\"}")
```

### Detailed Steps

1. **Parser (`parse_expr.cpp`):**
   - In `parse_primary_expr()`, when token is `request` followed by `.` and an identifier, parse as `NodeType::RequestParam` with `value = "params"` and `left = <identifier>`.
   - Alternative: Parse `request.params.id` as a chain: `request` → dot → `params` → dot → `id`. Create ast nodes: `DotAccess(Request, "params")` → `DotAccess(result, "id")`.

2. **Codegen (`codegen_expr.cpp`):**
   - Add a case for `request.params.X` that generates:
     ```cpp
     aurora_http_get_param(__http_req, "X")
     ```
   - Wraps the `const char*` result in `AuroraStr*` via `aurora_str_from_cstr()`.

3. **Files:** `parse_expr.cpp`, `codegen_expr.cpp`

---

## 31.2 request.query, request.form, request.cookie — Additional Request Accessors

**Problem:** `aurora_http_get_field()` supports `"method"`, `"path"`, `"query_string"`, `"body"`, `"content_type"` but there's no dedicated accessor for query parameters, form fields, or cookies.

### Implementation

**Files to create/modify:**
- `aurora/src/runtime/backend/server.cpp` — Add `aurora_http_get_query(name)`, `aurora_http_get_form(name)`, `aurora_http_get_cookie(name)` if not already present
- `aurora/src/runtime/backend/backend.hpp` — Declare new functions
- `aurora/src/compiler/parser/parse_expr.cpp` — Parse `request.query.X`, `request.form.X`, `request.cookie.X`
- `aurora/src/compiler/codegen/codegen_expr.cpp` — Generate calls to new functions

**Runtime API (server.cpp):**
```cpp
const char* aurora_http_get_query(AuroraHttpRequest* req, const char* name);
const char* aurora_http_get_form(AuroraHttpRequest* req, const char* name);
const char* aurora_http_get_cookie(AuroraHttpRequest* req, const char* name);
```

These parse `req->query_string`, `req->body` (form-urlencoded), and `req->cookies` respectively.

**DSL syntax:**
```aura
route "GET" "/search"
    q = request.query.q
    response("{\"query\": \"" + q + "\"}")
```

---

## 31.3 response.json(), response.status(), response.redirect() — Response Builder DSL

**Problem:** Currently `response(...)` sets body directly with content type `application/json`. No way to set status code, redirect, set cookies, or chain builders.

### Implementation

**Files to modify:**
- `aurora/src/compiler/parser/parse_expr.cpp` — Parse `response.json(...)`, `response.status(N)`, `response.redirect(url)`, `response.cookie(name, value)`
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Generate calls to response builder functions
- `aurora/src/runtime/backend/backend.hpp` — Already has `aurora_response_set_status()` etc.
- `aurora/src/runtime/backend/server.cpp` — Already has `aurora_response_set_status()`, `aurora_response_set_header()`, `aurora_response_redirect()`

**Required runtime additions (backend.hpp / server.cpp):**
```cpp
void aurora_response_set_status(AuroraHttpResponse* res, int status_code);       // exists
void aurora_response_set_header(AuroraHttpResponse* res, const char* name, const char* value); // exists
void aurora_response_redirect(AuroraHttpResponse* res, const char* url, int status_code); // exists
void aurora_response_json(AuroraHttpResponse* res, const char* body);        // exists (default)
void aurora_response_html(AuroraHttpResponse* res, const char* body);        // exists
void aurora_response_cookie(AuroraHttpResponse* res, const char* name, const char* value, int64_t ttl); // needs impl
```

**DSL syntax:**
```aura
route "GET" "/old-page"
    response.redirect("/new-page", 301)

route "GET" "/api/data"
    response.status(201)
    response.json("{\"created\": true}")
```

**Codegen approach:**
- `response.json(...)` → `aurora_response_json(__http_res, ...)` then return from handler
- `response.status(N)` → `aurora_response_set_status(__http_res, N)`
- `response.redirect(url, code)` → `aurora_response_redirect(__http_res, url, code)` then return
- Multiple `response.X()` calls accumulate state on `__http_res`

---

## 31.4 CORS Preflight & DSL Integration

**Problem:** CORS runtime API exists (`aurora_cors_apply()`) but:
1. No OPTIONS preflight handler in server accept loop
2. Per-route CORS configuration not possible
3. `builtin_cors()` only prints a message — doesn't call actual CORS functions

### Implementation

**Files to modify:**
- `aurora/src/runtime/backend/server.cpp` — Add `handle_options_request()` in accept loop that calls `aurora_cors_apply_default()` and returns 204
- `aurora/src/runtime/backend/backend_builtins.cpp` — Fix `builtin_cors()` to actually call CORS functions
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Allow `cors` keyword inside `server {}` blocks

**Server accept loop changes (server.cpp ~line 1600):**
```cpp
if (strcmp(req.method, "OPTIONS") == 0) {
    aurora_cors_apply_default(&res);
    // send 204 response
    continue;
}
```

**DSL syntax:**
```aura
server app
    cors                              # enables CORS with *
    cors "https://example.com"        # enables CORS with specific origin
    route "GET" "/api"
        response("OK")
```

---

## 31.5 Middleware DSL

**Problem:** Middleware chain works at C level (`aurora_server_add_middleware()`, `aurora_middleware_run_chain()`) but there's no DSL keyword for writing middleware in Aurora code.

### Implementation

**Files to modify:**
- `aurora/src/compiler/parser/parse_stmt.cpp` — Ensure `middleware` inside `server {}` is parsed correctly (already partially parsed)
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Generate middleware handler function and register it
- `aurora/src/runtime/backend/backend_builtins.cpp` — `builtin_next()` already exists

**DSL syntax:**
```aura
server app
    middleware auth
        token = request.header("Authorization")
        if token == ""
            response.status(401)
            response.json("{\"error\": \"unauthorized\"}")
        else
            next()
    route "GET" "/protected"
        response("{\"secret\": 42}")
```

**Middleware handler signature (C level):**
```cpp
int handler(AuroraHttpRequest* req, AuroraHttpResponse* res, void* userdata)
```
Returns 0 to continue chain, non-zero to stop.

**Codegen:**
- Each `middleware` block inside `server` generates a handler function
- The handler is registered via `aurora_server_add_middleware(srv, handler_fn_ptr)` after route registration
- `next()` inside middleware generates return 0 (continue chain)
- Early return before `next()` means the middleware blocked the request

---

## 31.6 Template Engine DSL Integration

**Problem:** Mustache-style template engine exists (`template.cpp`, 493 lines) with `aurora_template_compile()`, `aurora_template_render()` and `builtin_render()`. But no DSL syntax for templates.

### Implementation

**Files to modify:**
- `aurora/src/compiler/parser/parse_stmt.cpp` — Parse `template` keyword inside `server`
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Generate template compile + render calls
- `aurora/src/runtime/backend/backend_builtins.cpp` — `builtin_template()` and `builtin_render()` already exist
- `libc/server.auf` — Add template extern declarations

**DSL syntax:**
```aura
server app
    template "layout" "<html><body>{{content}}</body></html>"

    route "GET" "/page"
        html = render("layout", "{\"content\": \"Hello World\"}")
        response.html(html)
```

**Load from file:**
```aura
template "index" from "./templates/index.html"
```

---

## 31.7 WebSocket & SSE DSL Integration

**Problem:** WebSocket (`websocket.cpp`, 296 lines) and SSE (in server.cpp) exist at C level but no DSL keywords.

### Implementation

**Files to modify:**
- `aurora/src/compiler/parser/parse_stmt.cpp` — Parse `websocket` and `sse` keywords
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Generate handler registration
- `aurora/src/runtime/backend/backend_builtins.cpp` — `builtin_sse()` already exists
- `aurora/src/runtime/backend/websocket.cpp` — Already has `aurora_ws_broadcast()` etc.

**DSL syntax:**
```aura
server app
    websocket "/ws"
        on_open
            print("client connected")
        on_message
            data = ws.message()
            ws.send("echo: " + data)
        on_close
            print("client disconnected")

    sse "/events"
        # pushes server-sent events to client
        send_event("update", "{\"status\": \"ok\"}")
```

---

## 31.8 Session/Auth DSL Keywords

**Problem:** Two separate session systems exist (one in `server.cpp` using `AuroraSession*`, another in `backend_builtins.cpp` using `g_session_data`). Neither is integrated with the middleware chain. Auth is minimal (env var login, XOR "hash").

### Implementation

**Files to modify:**
- `aurora/src/runtime/backend/server.cpp` — Unify session systems, add session middleware
- `aurora/src/runtime/backend/backend_builtins.cpp` — Add `builtin_login()`, `builtin_logout()`, `builtin_user()`
- `aurora/src/runtime/backend/backend.hpp` — Declare unified session API
- `aurora/src/compiler/parser/parse_stmt.cpp` — Parse `session`, `auth` keywords
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Generate auth middleware

**DSL syntax:**
```aura
server app
    session ttl=3600
    auth table="users"

    route "GET" "/profile"
        user = current_user()
        if user == ""
            response.redirect("/login", 302)
        else
            response.json("{\"user\": \"" + user + "\"}")

    route "POST" "/login"
        u = request.form.username
        p = request.form.password
        if auth.login(u, p)
            response.json("{\"status\": \"ok\"}")
        else
            response.status(401)
            response.json("{\"error\": \"invalid credentials\"}")
```

---

## 31.9 Summary of Phase 31 Deliverables

| Task | Files | Status |
|------|-------|--------|
| 31.1 request.params | `parse_expr.cpp`, `codegen_expr.cpp` | New |
| 31.2 request.query/form/cookie | `parse_expr.cpp`, `codegen_expr.cpp`, `server.cpp` | New |
| 31.3 response builder DSL | `parse_expr.cpp`, `codegen_stmt.cpp`, `server.cpp` | New |
| 31.4 CORS preflight + DSL | `server.cpp`, `backend_builtins.cpp`, `codegen_stmt.cpp` | Fix |
| 31.5 Middleware DSL | `parse_stmt.cpp`, `codegen_stmt.cpp`, `backend_builtins.cpp` | Fix |
| 31.6 Template DSL integration | `parse_stmt.cpp`, `codegen_stmt.cpp`, `server.auf` | New |
| 31.7 WebSocket/SSE DSL | `parse_stmt.cpp`, `codegen_stmt.cpp` | New |
| 31.8 Session/Auth DSL | `server.cpp`, `backend_builtins.cpp`, `parse_stmt.cpp` | Overhaul |

**Verification:**
- All existing web examples must still compile and JIT-run
- New test files: `test_web_dsl_full.aura` (tests all new DSL features)
- `aurorac --run examples/app/phase31_demo.aura` — comprehensive web demo

---

# Phase 32: Desktop GUI Completion — Linux & macOS

**App 75% → 85% | Web 85% → 85%**

**Core theme:** Bring Linux and macOS desktop GUI to parity with Windows. Currently Windows has a full Win32 implementation (819 lines) while:
- **Linux**: X11 implementation exists in `gui.cpp` (lines 8-639) — functional for basic widgets but complex widgets are stubs
- **macOS**: Entirely stubs — every function returns 0/nullptr (gui.cpp lines 641-886)

---

## 32.1 Linux X11 Advanced Widgets Completion

**Current state (gui.cpp lines 200-460):**
- Label: ✅ `XCreateSimpleWindow` + `XDrawString` (working)
- Button: ✅ `XCreateSimpleWindow` + `XDrawString` (working)
- Canvas: ✅ `XCreateSimpleWindow` + `XFillRectangle`/`XDrawLine`/etc (working)
- TreeView: ❌ Returns nullptr (lines 402-415)
- Table: ❌ Returns nullptr (lines 418-431)
- TabView: ❌ Returns nullptr (lines 434-442)
- SplitView: ❌ Returns nothing (lines 452-460)
- Image: ❌ Returns nullptr
- WebView: ❌ Returns nullptr (lines 848-872)
- Media: ❌ Returns nullptr
- Map: ❌ Returns nullptr

### Implementation

**Files to modify:**
- `aurora/src/std/gui.cpp` — Implement TreeView, Table, TabView, SplitView, Image, GroupBox, Menu, ToolBar, StatusBar, Dialog

**TreeView:**
```cpp
// Each tree item = XCreateSimpleWindow with indented text
// Store tree structure in a vector<AuroraX11TreeItem>
// Handle ButtonPress to expand/collapse
// Draw lines/arrows using XDrawLine
void* aurora_gui_treeview_new(void* parent, int x, int y, int w, int h) {
    // Create window, store in g_widgets, initialize tree data (empty root)
}
void aurora_gui_treeview_add_item(void* tv, const char* text) {
    // Add item under root
}
void aurora_gui_treeview_expand(void* tv, int index) {
    // Expand/collapse node
}
```

**Table:**
```cpp
// Grid of cells using XCreateSimpleWindow per cell
// Header row with different background color
// Scroll via child windows
// Column resize via mouse drag
```

**Image:**
```cpp
// Use XCreatePixmap + XCopyArea for rendering
// Load via stb_image (already bundled)
// Support PNG, JPEG, GIF
```

**WebView (X11):**
- **Option A**: Embed WebKitGTK (`webkit2gtk-4.1`). Load `libwebkit2gtk` dynamically.
- **Option B**: Embed a minimal Chromium via CEF (Chromium Embedded Framework).
- **Recommendation**: WebKitGTK is lighter and more commonly available on Linux.

```cpp
// Dynamic load of webkit2gtk functions
// Create GtkWidget, embed via XEmbed (XReparentWindow)
void* aurora_gui_webview_new(void* parent, int x, int y, int w, int h) {
    // dlopen("libwebkit2gtk-4.1.so")
    // Create WebKitWebView, get its XID, reparent
}
void aurora_gui_webview_navigate(void* wv, const char* url) {
    // webkit_web_view_load_uri(webview, url)
}
```

**Media (X11):**
- Use FFmpeg + OpenGL for video rendering
- `dlopen("libavformat.so")`, `libavcodec`, `libswscale`, `libavutil`
- Render frames to an X11 window via `XPutImage` or OpenGL texture

**Theme integration (X11):**
- Use `XRender` for alpha compositing
- Detect GTK theme colors via `gsettings` or reading `~/.config/gtk-3.0/settings.ini`
- System font detection via `fc-match` (fontconfig)

**Map (X11):**
- Offscreen WebKitGTK WebView with OpenStreetMap/Leaflet HTML loaded
- Intercept JavaScript callbacks for map events
- Alternatively: use a simple tile renderer that downloads tiles via HTTP

---

## 32.2 macOS Cocoa GUI Backend — Full Implementation (New File)

**Current state:** Pure stubs. Need to create a full Cocoa/AppKit implementation.

### Files to create
- `aurora/src/runtime/ui/ui_mac.mm` — Full Cocoa implementation (target: 800+ lines)
- `aurora/src/runtime/ui/ui_mac.hpp` — Header (if needed)

### Widget Implementation Plan

Each `aurora_gui_*` function maps to a Cocoa NSView/NSControl subclass.

**App lifecycle:**
```objc
// ui_mac.mm
static NSApplication* g_app;
static NSMutableArray* g_windows;
static int g_widget_counter = 0;
static NSMutableDictionary* g_widget_map; // id -> NSView*

void aurora_gui_app_init() {
    g_app = [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    g_windows = [[NSMutableArray alloc] init];
    g_widget_map = [[NSMutableDictionary alloc] init];
}

void aurora_gui_app_run() {
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
}
```

**Window:**
```objc
void* aurora_gui_window_new(const char* title, int w, int h) {
    NSRect frame = NSMakeRect(0, 0, w, h);
    NSWindow* win = [[NSWindow alloc] initWithContentRect:frame
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    [win setTitle:[NSString stringWithUTF8String:title]];
    [win center];
    int wid = g_widget_counter++;
    g_widget_map[@(wid)] = win;
    return (void*)(intptr_t)wid; // return ID, not pointer
}
```

**Widget table (all functions):**

| Function | Cocoa Implementation |
|----------|---------------------|
| `aurora_gui_label_new` | `NSTextField` (label style, non-editable, bezeled = NO) |
| `aurora_gui_button_new` | `NSButton` (NSButtonTypeMomentaryPushIn) |
| `aurora_gui_textbox_new` | `NSTextField` (editable, bezeled) |
| `aurora_gui_password_new` | `NSSecureTextField` |
| `aurora_gui_listbox_new` | `NSTableView` (single-column) |
| `aurora_gui_combobox_new` | `NSComboBox` |
| `aurora_gui_checkbox_new` | `NSButton` (NSButtonTypeSwitch) |
| `aurora_gui_radio_new` | `NSButton` (NSButtonTypeRadio) |
| `aurora_gui_slider_new` | `NSSlider` |
| `aurora_gui_progress_new` | `NSProgressIndicator` |
| `aurora_gui_canvas_new` | Custom `NSView` subclass with `drawRect:` |
| `aurora_gui_image_new` | `NSImageView` |
| `aurora_gui_treeview_new` | `NSOutlineView` |
| `aurora_gui_table_new` | `NSTableView` (multi-column) |
| `aurora_gui_tabview_new` | `NSTabView` |
| `aurora_gui_webview_new` | `WKWebView` (WebKit framework) |
| `aurora_gui_media_new` | `AVPlayerView` (AVKit framework) |
| `aurora_gui_scrollview_new` | `NSScrollView` |
| `aurora_gui_splitview_new` | `NSSplitView` |
| `aurora_gui_groupbox_new` | `NSBox` |
| `aurora_gui_menu_new` | `NSMenu` |
| `aurora_gui_toolbar_new` | `NSToolbar` |
| `aurora_gui_statusbar_new` | Custom NSView |
| `aurora_gui_dialog_new` | `NSAlert` |

**Event handling:**
```objc
// Target-action pattern for controls
// NSButton: setTarget:self, setAction:@selector(buttonClicked:)
// NSSlider: setTarget:self, setAction:@selector(sliderChanged:)
// NSTextField delegate: controlTextDidEndEditing:
```

**Canvas rendering:**
```objc
@interface AuroraCanvasView : NSView
@property (assign) int widget_id;
@end

@implementation AuroraCanvasView
- (void)drawRect:(NSRect)dirtyRect {
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    // Draw commands queued via aurora_gui_canvas_draw_*
    // Commands stored in a per-widget deque
}
@end
```

**Event callbacks to Aurora:**
```objc
- (void)buttonClicked:(id)sender {
    int wid = [self tag]; // widget ID stored in tag
    // Call registered Aurora callback
    if (g_callbacks[wid]) {
        g_callbacks[wid](wid, AURORA_EVENT_CLICK);
    }
}
```

**WebView (macOS):**
```objc
#import <WebKit/WebKit.h>

void* aurora_gui_webview_new(void* parent, int x, int y, int w, int h) {
    WKWebView* wv = [[WKWebView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
    // Add to parent view
    return (__bridge_retained void*)wv;
}
```

**Build integration (CMakeLists.txt):**
```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_sources(aurora_runtime PRIVATE
        aurora/src/runtime/ui/ui_mac.mm
    )
    target_link_libraries(aurora_runtime PUBLIC
        "-framework AppKit"
        "-framework WebKit"
        "-framework AVKit"
        "-framework AVFoundation"
    )
endif()
```

---

## 32.3 Desktop Integration — Linux & macOS

**Current state:** `desktop.cpp` (573 lines) is entirely `#ifdef _WIN32` guarded. On Linux/macOS every function returns -1 or 0.

### Linux Desktop Integration (`desktop_linux.cpp`)

**Files to create:**
- `aurora/src/std/desktop_linux.cpp`

**Implementations:**

| Feature | Linux Implementation |
|---------|---------------------|
| System tray | `libayatana-appindicator3` or `libdbus` for StatusNotifierItem spec |
| Notifications | `libnotify` (`notify-send` via DBus) |
| Clipboard | X11 `XSetSelectionOwner` + `XConvertSelection` or `wl-clipboard` (Wayland) |
| File associations | `~/.local/share/applications/*.desktop` + `xdg-mime` |
| Startup | `~/.config/autostart/*.desktop` |
| Global hotkeys | X11 `XGrabKey` |
| DnD | X11 `Xdnd` protocol |

**Load dynamic libraries where possible:**
```cpp
// libnotify
void* libnotify = dlopen("libnotify.so.4", RTLD_LAZY);
if (libnotify) {
    notify_init_func = dlsym(libnotify, "notify_init");
    notify_notification_new_func = dlsym(libnotify, "notify_notification_new");
    // ...
}
```

### macOS Desktop Integration (`desktop_mac.mm`)

**Files to create:**
- `aurora/src/std/desktop_mac.mm`

**Implementations:**

| Feature | macOS Implementation |
|---------|---------------------|
| System tray | `NSStatusBar` + `NSStatusItem` |
| Notifications | `NSUserNotification` (or `UNUserNotificationCenter` for 10.14+) |
| Clipboard | `NSPasteboard generalPasteboard` |
| File associations | `LSHandler` in Info.plist + `LSSetDefaultRoleHandlerForContentType` |
| Startup | `LSSharedFileList` (Login Items) |
| Global hotkeys | `CGEventTap` or `MASShortcut` |
| DnD | `NSDraggingDestination` protocol on main window |

---

## 32.4 Summary of Phase 32 Deliverables

| Task | Files | Platform | Status |
|------|-------|----------|--------|
| 32.1 Linux advanced widgets | `gui.cpp` | Linux | Extend |
| 32.2 macOS GUI backend | `ui_mac.mm` (new), `CMakeLists.txt` | macOS | New |
| 32.3a Linux desktop integration | `desktop_linux.cpp` (new) | Linux | New |
| 32.3b macOS desktop integration | `desktop_mac.mm` (new) | macOS | New |
| 32.4 Update CMakeLists.txt | `CMakeLists.txt` | All | Modify |

**Verification:**
- All existing 14 GUI widget tests compile and JIT-run on Linux and macOS (in addition to Windows)
- New test: `test_mac_gui.aura` (tests all 21+ widget types on macOS)
- Desktop integration tests: `test_desktop_notify.aura`, `test_desktop_clipboard.aura`
- `examples/app/cross_platform.aura` renders correct native GUI on all 3 desktop platforms
- Build zero errors on Linux, macOS, Windows

---

# Phase 33: Complex Widgets & Developer Tools

**App 85% → 92% | Web 85% → 85%**

**Core theme:** Finish what's been left as stubs — WebView, Media, Map widgets + real developer tools (Formatter, Linter, Debugger, Profiler).

---

## 33.1 WebView Widget — Full Implementation

**Current state:** Stub on all platforms (returns nullptr, methods do nothing).

### Implementation by Platform

**Windows (ui_win32.cpp):**
- Embed WebView2 (`Microsoft.Web.WebView2.Core`)
- Load `webview2.dll` dynamically at runtime
- Create child HWND with `CreateWindowEx` + `WebView2_Create`
- Navigate, GoBack, GoForward, Reload, ExecuteScript
- Events: NavigationStarting, NavigationCompleted, WebMessageReceived

**Linux X11 (gui.cpp):**
- WebKitGTK embedded via `XReparentWindow` (as described in 32.1)
- Event bridge: JavaScript ↔ Aurora via `webkit_web_view_run_javascript` + `window.webkit.messageHandlers`

**macOS (ui_mac.mm):**
- `WKWebView` (native, part of WebKit framework)
- `evaluateJavaScript:completionHandler:` for JS ↔ Aurora bridge
- `navigationDelegate` for load events

**Mobile:**
- **Android**: `WebView` via JNI
- **iOS**: `WKWebView` via UIKit

**Cross-platform C API (widgets_advanced.hpp):**
```cpp
void* aurora_widget_webview_create(void* parent, int x, int y, int w, int h);
void  aurora_widget_webview_navigate(void* wv, const char* url);
void  aurora_widget_webview_go_back(void* wv);
void  aurora_widget_webview_go_forward(void* wv);
void  aurora_widget_webview_reload(void* wv);
void  aurora_widget_webview_execute_js(void* wv, const char* js);
void  aurora_widget_webview_set_on_message(void* wv, void (*cb)(const char*));
```

**Files to modify:**
- `aurora/src/std/widgets_advanced.cpp` — Replace stubs with platform dispatch
- `aurora/src/std/widgets_advanced.hpp` — Declare webview functions
- `aurora/src/runtime/ui/ui_win32.cpp` — WebView2 integration
- `aurora/src/std/gui.cpp` — WebKitGTK integration (Linux)
- `aurora/src/runtime/ui/ui_mac.mm` — WKWebView integration (macOS)
- `aurora/src/mobile/android/android_renderer.cpp` — WebView JNI
- `aurora/src/mobile/ios/ios_renderer_widgets.mm` — WKWebView UIKit
- `libc/widgets_advanced.auf` — Add webview externs + wrappers

---

## 33.2 Media Player Widget

**Current state:** Stub on all platforms.

### Implementation

**Windows (ui_win32.cpp):**
- Windows Media Player ActiveX control or MFPlay (Media Foundation)
- `IMFMediaEngine` for video playback
- HWND-based video rendering

**Linux X11 (gui.cpp):**
- FFmpeg (`libavformat`, `libavcodec`, `libswscale`) for decoding
- X11/XRender or OpenGL for video frame display
- PulseAudio for audio output

**macOS (ui_mac.mm):**
- `AVPlayerView` (native AVKit view)
- Supports H.264, H.265, ProRes, etc.
- Native controls (play/pause/seek/volume)

**Mobile:**
- **Android**: `VideoView` or `ExoPlayer` via JNI
- **iOS**: `AVPlayerViewController` via UIKit

**Cross-platform C API:**
```cpp
void* aurora_widget_media_create(void* parent, int x, int y, int w, int h);
void  aurora_widget_media_load(void* m, const char* uri);
void  aurora_widget_media_play(void* m);
void  aurora_widget_media_pause(void* m);
void  aurora_widget_media_stop(void* m);
void  aurora_widget_media_seek(void* m, double position);
double aurora_widget_media_get_duration(void* m);
double aurora_widget_media_get_position(void* m);
void  aurora_widget_media_set_volume(void* m, double vol);
void  aurora_widget_media_set_on_end(void* m, void (*cb)());
```

**Files to modify:**
- Same files as WebView (33.1) — add parallel media functions

---

## 33.3 Map Widget

**Current state:** Stub on all platforms.

### Implementation

**Approach:** Offscreen WebView (from 33.1) with embedded Leaflet.js or MapLibre GL JS. C API sends commands via JavaScript evaluation.

**Cross-platform C API:**
```cpp
void* aurora_widget_map_create(void* parent, int x, int y, int w, int h);
void  aurora_widget_map_set_center(void* m, double lat, double lon, int zoom);
void  aurora_widget_map_add_marker(void* m, double lat, double lon, const char* title);
void  aurora_widget_map_clear_markers(void* m);
void  aurora_widget_map_set_on_click(void* m, void (*cb)(double lat, double lon));
```

**Implementation:**
```cpp
// On create, load embedded HTML with Leaflet:
// <html><head><link rel="stylesheet" href="https://unpkg.com/leaflet@1.9/dist/leaflet.css"/>
// <script src="https://unpkg.com/leaflet@1.9/dist/leaflet.js"></script></head>
// <body><div id="map" style="width:100%;height:100%"></div>
// <script>
//   var map = L.map('map').setView([0, 0], 2);
//   L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png').addTo(map);
//   map.on('click', function(e) { window.aurora.onMapClick(e.latlng.lat, e.latlng.lng); });
// </script></body></html>
void aurora_widget_map_set_center(void* m, double lat, double lon, int zoom) {
    char js[256];
    snprintf(js, sizeof(js), "map.setView([%f, %f], %d);", lat, lon, zoom);
    aurora_widget_webview_execute_js(get_webview(m), js);
}
void aurora_widget_map_add_marker(void* m, double lat, double lon, const char* title) {
    char js[512];
    snprintf(js, sizeof(js), "L.marker([%f, %f]).addTo(map).bindPopup('%s');", lat, lon, title);
    aurora_widget_webview_execute_js(get_webview(m), js);
}
```

Since the Map widget sits on top of WebView, the implementation is shared. Requires a working WebView (33.1) first.

---

## 33.4 Code Formatter

**Current state:** Does not exist. Only declared in `runtime_exports.hpp` as a no-op stub.

### Implementation

**Files to create:**
- `aurora/src/std/formatter.cpp`
- `aurora/include/std/formatter.hpp`

**Functionality:**
```cpp
// Entry point
AuroraStr* aurora_format_code(const char* source, const char* options_json);

// Internal engine
class AuroraFormatter {
public:
    std::string format(const std::string& source);
private:
    std::string indent(int level);
    // Token-based formatting:
    // - Track indent level based on braces/brackets/parens
    // - Place operators consistently (spaces around binary, none around unary)
    // - Align chained method calls
    // - Break long lines (> 100 chars)
    // - Sort imports
    // - Remove trailing whitespace
};
```

**Formatting rules:**
1. Indent: 4 spaces per level
2. Braces: same line for functions/if/for (`{` on same line)
3. Spaces: around binary operators, after comma, after `if`/`for`/`while`
4. Newlines: between top-level declarations, after `}` 
5. Max line length: configurable (default 100)
6. Trailing whitespace: removed
7. Blank lines: max 2 consecutive

**Integration:**
- CLI flag `voss format <file.aura>`
- `libc/dev.auf` — `dev_format(source, options)` function
- Editor integration (LSP `textDocument/formatting`)

**Files to modify:**
- `aurora/src/std/formatter.cpp` — New
- `aurora/include/std/formatter.hpp` — New
- `libc/dev.auf` — Add format extern
- `aurora/tools/voss/` — Add `voss format` command
- `CMakeLists.txt` — Add formatter.cpp
- `runtime_exports.hpp` — Wire up real exports (replace stubs)

---

## 33.5 Code Linter

**Current state:** Does not exist. Only declared as no-op stub.

### Implementation

**Files to create:**
- `aurora/src/std/linter.cpp`
- `aurora/include/std/linter.hpp`

**Lint rules:**

| Rule ID | Description | Severity |
|---------|-------------|----------|
| `no-unused-variable` | Unused local variables | Warning |
| `no-unused-param` | Unused function parameters | Warning |
| `no-shadow` | Variable shadows outer scope | Warning |
| `no-global-mut` | Mutable global variables | Warning |
| `no-var-init` | Variable used before initialization | Error |
| `no-constant-condition` | If/while with constant condition | Warning |
| `prefer-const` | Variable never reassigned should be const | Suggestion |
| `no-empty-block` | Empty if/for/while body | Warning |
| `max-function-lines` | Function exceeds line limit | Warning |
| `no-debug-print` | `print()` left in production code | Warning |
| `consistent-return` | Function sometimes returns, sometimes doesn't | Warning |
| `no-nested-ternary` | Ternary inside ternary | Suggestion |

**Implementation approach:**
```cpp
class AuroraLinter {
public:
    std::vector<LintDiagnostic> lint(const std::string& source);
private:
    // Parse tokens, walk AST, check each rule
    // Return list of (line, col, severity, message, rule_id)
};
```

**Integration:**
- CLI flag `voss lint <file.aura>` or `voss check`
- LSP `textDocument/publishDiagnostics`
- Pre-commit hook integration

---

## 33.6 Debugger

**Current state:** Does not exist. Only declared as no-op stub.

### Implementation

**Files to create:**
- `aurora/src/std/debugger.cpp`
- `aurora/include/std/debugger.hpp`

**Architecture:**
```cpp
// Debugger uses a separate process/thread that communicates via pipe/socket
// JIT mode: LLVM JIT with debug info (DWARF)
// Compiled mode: uses platform debugger (GDB, LLDB, WinDbg)

class AuroraDebugger {
public:
    bool attach(const char* process_id);
    bool detach();
    bool set_breakpoint(const char* file, int line);
    bool remove_breakpoint(int id);
    bool continue_execution();
    bool step_over();
    bool step_into();
    bool step_out();
    AuroraStr* get_stack_trace();
    AuroraStr* get_variables();
    AuroraStr* evaluate(const char* expression);
};
```

**JIT mode debugging:**
```cpp
// In LLVM JIT, set flag to emit debug info (DIBuilder)
// Generate DWARF metadata in JIT'd code
// Signal handler catches breakpoint (int3 on x86)
// Inspect registers, map back to source lines
// Read local variables from stack frame
```

**Integration:**
- `voss debug <file.aura>` — Launch with debugger
- `libc/dev.auf` — `dev_debug_attach()`, `dev_debug_step()`, etc.
- Inspector integration: click widget → break in code

---

## 33.7 Profiler

**Current state:** Does not exist. Only declared as no-op stub.

### Implementation

**Files to create:**
- `aurora/src/std/profiler.cpp`
- `aurora/include/std/profiler.hpp`

**Architecture:**
```cpp
class AuroraProfiler {
public:
    void start_session(const char* name);
    void stop_session();
    void begin_frame(const char* label);
    void end_frame();
    // Sampling profiler: timer interrupt every 1ms, record PC
    // Map PC to source function name
    // Output: flame graph JSON, top-N hot functions

    AuroraStr* get_report();         // JSON report
    AuroraStr* get_flamegraph_svg(); // Interactive SVG
    AuroraStr* get_hotspots(int n);  // Top N functions
};
```

**Implementation in JIT mode:**
- Register a signal handler for `SIGPROF` (or Windows timer)
- Set `setitimer(ITIMER_PROF, ...)` for 1ms intervals
- On each signal, record current PC from JIT'd code
- Map PC → function name (LLVM JIT symbol table)
- Aggregate samples per function

**Output format (JSON):**
```json
{
    "session": "my_app",
    "duration_ms": 5234,
    "samples": 5234,
    "functions": [
        {"name": "main", "samples": 1200, "self_ms": 1200, "total_ms": 5234},
        {"name": "render_frame", "samples": 3000, "self_ms": 800, "total_ms": 3800},
        {"name": "physics_update", "samples": 2000, "self_ms": 2000, "total_ms": 2000}
    ]
}
```

**Integration:**
- `voss profile <file.aura>` — Run with profiling
- `libc/dev.auf` — `dev_profiler_start()`, `dev_profiler_stop()`, `dev_profiler_report()`

---

## 33.8 Summary of Phase 33 Deliverables

| Task | Files | Status |
|------|-------|--------|
| 33.1 WebView widget | `widgets_advanced.cpp`, `ui_win32.cpp`, `gui.cpp`, `ui_mac.mm`, `android_renderer.cpp`, `ios_renderer_widgets.mm`, `widgets_advanced.auf` | New |
| 33.2 Media Player widget | Same files as 33.1 + platform media APIs | New |
| 33.3 Map widget | Same files as 33.1 + Leaflet.js bundle | New |
| 33.4 Formatter | `formatter.cpp`/`.hpp` (new), `dev.auf`, `voss` CLI | New |
| 33.5 Linter | `linter.cpp`/`.hpp` (new), `dev.auf`, `voss` CLI | New |
| 33.6 Debugger | `debugger.cpp`/`.hpp` (new), `dev.auf`, LLVM DIBuilder | New |
| 33.7 Profiler | `profiler.cpp`/`.hpp` (new), `dev.auf` | New |

**Verification:**
- WebView demo: `examples/app/webview_demo.aura`
- Media demo: `examples/app/media_demo.aura`
- Map demo: `examples/app/map_demo.aura`
- `voss format examples/app/*.aura` — formats all example files in-place
- `voss lint examples/app/*.aura` — produces diagnostics
- `voss debug examples/app/calc.aura` — launches debug shell
- `voss profile examples/app/calc.aura` — produces profile report
- All existing tests continue to pass

---

# Phase 34: Full-Stack Web & Production Hardening

**Web 85% → 100% | App 92% → 92%**

**Core theme:** Connect all the pieces into a production-ready full-stack web framework. CORS, CSRF, rate limiting, request validation, auth middleware, database integration at DSL level, and a comprehensive demo.

---

## 34.1 Unified CORS Middleware

**Problem:** CORS exists at C level but no per-route config, no preflight, no credentials/origin whitelist.

### Implementation

**Files to modify:**
- `aurora/src/runtime/backend/server.cpp` — Add `aurora_cors_configure()`, `aurora_cors_preflight_handler()`
- `aurora/src/runtime/backend/backend.hpp` — Declare CORS config struct
- `aurora/src/runtime/backend/backend_builtins.cpp` — Replace stub with real implementation
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Wire `cors` keyword to config

**CORS Config Struct:**
```cpp
typedef struct {
    char** allowed_origins;
    int    allowed_origins_count;
    char** allowed_methods;
    int    allowed_methods_count;
    char** allowed_headers;
    int    allowed_headers_count;
    bool   allow_credentials;
    int64_t max_age;
} AuroraCorsConfig;
```

**DSL syntax:**
```aura
server app
    cors
        origin "*"
        methods "GET", "POST", "PUT", "DELETE"
        headers "Content-Type", "Authorization"
        credentials true
        max_age 3600

    route "GET" "/public"
        response("{\"public\": true}")

    route "GET" "/private"
        cors origin "https://myapp.com"
        response("{\"private\": true}")
```

---

## 34.2 CSRF Protection at DSL Level

**Problem:** `builtin_csrf()` and `builtin_csrf_verify()` exist in `backend_builtins.cpp` but aren't integrated into any middleware or DSL.

### Implementation

**Files to modify:**
- `aurora/src/runtime/backend/backend_builtins.cpp` — Complete CSRF implementation
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Wire `csrf` keyword

**DSL syntax:**
```aura
server app
    csrf secret="my-secret-key"

    route "POST" "/form"
        if csrf.verify(request.header("X-CSRF-Token"))
            response("{\"status\": \"ok\"}")
        else
            response.status(403)
            response.json("{\"error\": \"invalid csrf\"}")

    route "GET" "/token"
        response.json("{\"token\": \"" + csrf.token() + "\"}")
```

---

## 34.3 Rate Limiting at DSL Level

**Problem:** Token-bucket rate limiter exists in `gateway.cpp` but has no DSL integration.

### Implementation

**Files to modify:**
- `aurora/src/runtime/backend/gateway.cpp` — Refactor rate limiter into reusable component
- `aurora/src/runtime/backend/backend.hpp` — Declare `aurora_rate_limit_init()`, `aurora_rate_limit_check()`
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Wire `rate_limit` keyword
- `libc/server.auf` — Add rate limit externs

**DSL syntax:**
```aura
server app
    rate_limit
        requests 100
        window 60           # 100 requests per 60 seconds
        key request.ip      # per-IP rate limiting

    route "GET" "/api"
        response("{\"ok\": true}")

    route "GET" "/api/admin"
        rate_limit requests 10 window 60 key request.ip
        response("{\"admin\": true}")
```

---

## 34.4 Request Validation DSL

**Problem:** No way to validate request body, query params, or headers in the DSL.

### Implementation

**Files to modify:**
- `aurora/src/compiler/parser/parse_stmt.cpp` — Parse `validate` block
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Generate validation checks
- `aurora/src/runtime/backend/backend_builtins.cpp` — Add validation helpers

**DSL syntax:**
```aura
route "POST" "/user"
    validate
        required name: string
        required email: string pattern=".+@.+\\..+"
        optional age: int min=0 max=150
        tags: string[]
    # if validation fails, auto-responds with 400 + error details
    response("{\"ok\": true}")
```

**Validation rules:**
| Rule | Description |
|------|-------------|
| `required` | Field must be present and non-empty |
| `optional` | Field may be absent |
| `type` | Must match type (string, int, float, bool, array, object) |
| `min`/`max` | Numeric or string length bounds |
| `pattern` | Regex pattern match |
| `enum` | Must be one of specified values |
| `email` | Must be valid email format |
| `url` | Must be valid URL format |

---

## 34.5 Session/Auth Middleware Unification

**Problem:** Two separate session systems. Need to unify and integrate with middleware chain.

### Implementation

**Files to modify:**
- `aurora/src/runtime/backend/server.cpp` — Remove old g_session_data, use AuroraSession everywhere
- `aurora/src/runtime/backend/backend_builtins.cpp` — Rewrite session builtins on top of AuroraSession
- `aurora/src/runtime/backend/backend.hpp` — Clean up session API

**Architecture:**
```cpp
// Per-request middleware:
// 1. Extract session_id from Cookie header
// 2. Look up AuroraSession* from session store
// 3. If valid, attach to request (req->session)
// 4. If expired or invalid, create new session
// 5. Set Set-Cookie header in response

// Auth middleware:
// 1. Check Authorization header for Bearer token
// 2. Verify token via aurora_auth_verify_token()
// 3. Attach user identity to session
// 4. If invalid, return 401
```

**DSL syntax (integrated):**
```aura
server app
    session ttl=86400               # 24 hour sessions
    auth
        provider jwt
        secret "my-jwt-secret"
        table users                 # SQLite users table

    middleware require_auth
        if current_user() == ""
            response.status(401)
            response.json("{\"error\": \"login required\"}")
            next() false            # stop chain
        else
            next()

    route "GET" "/profile" require_auth
        u = current_user()
        response.json("{\"user\": \"" + u + "\"}")
```

**Password hashing (replace XOR "hash"):**
- Use bundled SHA-256 + salt (or bcrypt if available)
- `aurora_auth_hash_password(password) → salted_hash`
- `aurora_auth_verify_password(password, stored_hash) → bool`

---

## 34.6 Database DSL Integration

**Problem:** Database access is via C API only (`aurora_db_*`). No DSL integration.

### Implementation

**Files to modify:**
- `aurora/src/compiler/parser/parse_stmt.cpp` — Parse `database` and `query` blocks
- `aurora/src/compiler/codegen/codegen_stmt.cpp` — Generate query execution
- `aurora/include/std/db.hpp` — Ensure all needed functions are exported
- `libc/database.auf` — Already has unified interface

**DSL syntax:**
```aura
server app
    database db sqlite://app.db
    model User
        id: int primary_key auto_increment
        name: string
        email: string unique
        created_at: datetime default now()

    route "GET" "/users"
        users = db.query("SELECT * FROM users")
        response.json(users)

    route "POST" "/users"
        u = request.body
        db.execute("INSERT INTO users (name, email) VALUES (?, ?)",
                   u.name, u.email)
        response.status(201)
        response.json("{\"created\": true}")

    route "GET" "/users/:id"
        user = db.query_one("SELECT * FROM users WHERE id = ?",
                           request.params.id)
        if user == ""
            response.status(404)
            response.json("{\"error\": \"not found\"}")
        else
            response.json(user)
```

**Auto-generated model endpoints:**
```aura
server app
    database db sqlite://app.db
    model User
        id: int primary_key auto_increment
        name: string
        email: string unique

    api "/api/users" for User      # auto-generates CRUD routes
    # GET /api/users — list all
    # GET /api/users/:id — get one
    # POST /api/users — create
    # PUT /api/users/:id — update
    # DELETE /api/users/:id — delete
```

---

## 34.7 Comprehensive Full-Stack Demo

**Files to create:**
- `examples/app/fullstack_demo.aura` — Single-file full-stack app
- `examples/app/fullstack_demo/` — Multi-file full-stack app

**Demo app features:**
1. User registration + login (session + auth)
2. Todo CRUD (database-backed)
3. Public API with rate limiting
4. Admin panel with CSRF protection
5. Server-side rendered pages (template engine)
6. WebSocket real-time updates
7. CORS-enabled for third-party clients
8. Input validation on all endpoints
9. Proper error handling (404, 400, 401, 403, 500)
10. Logging middleware

**Expected Aurora code structure:**
```aura
import "server"
import "database"
import "template"

server app
    cors origin "*"
    rate_limit requests 100 window 60
    session ttl=86400
    database db sqlite://todos.db
    template "layout" from "./templates/layout.html"

    model User
        id: int primary_key auto_increment
        username: string unique
        password_hash: string
        created_at: datetime default now()

    model Todo
        id: int primary_key auto_increment
        user_id: int
        title: string
        done: bool default false
        created_at: datetime default now()

    middleware auth
        if current_user() == "" && request.path != "/login" && request.path != "/register"
            response.redirect("/login", 302)
        next()

    route "GET" "/login"
        html = render("layout", "{\"content\": \"<form>...</form>\"}")
        response.html(html)

    route "POST" "/login"
        u = request.form.username
        p = request.form.password
        if auth.login(u, p)
            response.redirect("/todos", 302)
        else
            response.status(401)
            response.html("<h1>Invalid credentials</h1>")

    route "GET" "/api/todos"
        user = current_user()
        todos = db.query("SELECT * FROM todos WHERE user_id = ?", user.id)
        response.json(todos)

    route "POST" "/api/todos"
        validate
            required title: string min=1 max=200
        user = current_user()
        db.execute("INSERT INTO todos (user_id, title) VALUES (?, ?)",
                   user.id, request.body.title)
        response.status(201)
        response.json("{\"created\": true}")

    websocket "/ws"
        on_message
            data = ws.message()
            ws.broadcast("all", data)   # broadcast to all connected clients

    print("Server running at http://localhost:8080")
```

---

## 34.8 Summary of Phase 34 Deliverables

| Task | Files | Status |
|------|-------|--------|
| 34.1 Unified CORS | `server.cpp`, `backend_builtins.cpp`, `codegen_stmt.cpp` | Overhaul |
| 34.2 CSRF DSL | `backend_builtins.cpp`, `codegen_stmt.cpp` | Fix |
| 34.3 Rate limiting DSL | `gateway.cpp`, `backend.hpp`, `codegen_stmt.cpp`, `server.auf` | New |
| 34.4 Request validation | `parse_stmt.cpp`, `codegen_stmt.cpp`, `backend_builtins.cpp` | New |
| 34.5 Session/auth unification | `server.cpp`, `backend_builtins.cpp`, `backend.hpp` | Overhaul |
| 34.6 Database DSL | `parse_stmt.cpp`, `codegen_stmt.cpp` | New |
| 34.7 Full-stack demo | `examples/app/fullstack_demo.aura` | New |

**Verification:**
- `aurorac --run examples/app/fullstack_demo.aura` — full-stack app running
- `curl http://localhost:8080/api/todos` — returns JSON
- `curl -X POST http://localhost:8080/api/todos -H "Content-Type: application/json" -d '{"title":"test"}'` — creates todo
- `curl http://localhost:8080/login` — returns HTML
- All existing tests pass

---

# Phase 35: End-to-End Testing, CI/CD & Documentation

**Web 100% → 100% | App 92% → 100%**

**Core theme:** Comprehensive testing, CI/CD pipeline updates, and documentation to cement everything. This phase ensures all previous work is validated, documented, and maintainable.

---

## 35.1 Web Framework Test Suite

**Files to create:**
- `Workflow/tests/test_web_server.aura` — Server lifecycle (init, start, stop)
- `Workflow/tests/test_web_routes.aura` — Route registration, method+path matching
- `Workflow/tests/test_web_params.aura` — Path params, query params, form data
- `Workflow/tests/test_web_cors.aura` — CORS headers, preflight OPTIONS
- `Workflow/tests/test_web_csrf.aura` — CSRF token generation + verification
- `Workflow/tests/test_web_session.aura` — Session create, read, destroy, TTL
- `Workflow/tests/test_web_auth.aura` — Login, token, middleware
- `Workflow/tests/test_web_websocket.aura` — WebSocket connect, send, recv, broadcast
- `Workflow/tests/test_web_template.aura` — Template compile + render
- `Workflow/tests/test_web_validation.aura` — Request validation rules
- `Workflow/tests/test_web_rate_limit.aura` — Rate limiter thresholds
- `Workflow/tests/test_web_middleware.aura` — Middleware chain ordering + blocking

**Test approach:**
- Start server on a random port (or port 0)
- Use `net.auf` HTTP client to make requests
- Assert response status, headers, body
- Test error cases (404, 400, 401, 403, 500)
- Test edge cases (empty body, malformed JSON, concurrent requests)

---

## 35.2 Desktop GUI Test Suite — Linux & macOS

**Files to create:**
- `Workflow/tests/test_linux_gui.aura` — Linux X11 widget tests (all basic + advanced)
- `Workflow/tests/test_mac_gui.aura` — macOS Cocoa widget tests (all basic + advanced)
- `Workflow/tests/test_linux_desktop.aura` — Linux desktop integration tests
- `Workflow/tests/test_mac_desktop.aura` — macOS desktop integration tests

**Test approach for Linux:**
```bash
# Requires Xvfb (virtual framebuffer) for headless CI
xvfb-run aurorac --run test_linux_gui.aura
```

**Test approach for macOS:**
```bash
# Requires a window server (available on macOS CI runners)
aurorac --run test_mac_gui.aura
```

---

## 35.3 Complex Widget Test Suite

**Files to create:**
- `Workflow/tests/test_webview.aura` — WebView create, navigate, execute JS
- `Workflow/tests/test_media.aura` — Media player create, load, play, pause
- `Workflow/tests/test_map.aura` — Map create, center, marker, click

---

## 35.4 Developer Tools Test Suite

**Files to create:**
- `Workflow/tests/test_formatter.aura` — Format code, verify output
- `Workflow/tests/test_linter.aura` — Lint code, verify diagnostics
- `Workflow/tests/test_debugger.aura` — Set breakpoint, step, inspect variables
- `Workflow/tests/test_profiler.aura` — Profile code, verify report

---

## 35.5 CI/CD Pipeline Updates

**Files to modify:**
- `.github/workflows/build.yml` — Add full-stack web test, GUI tests on all platforms
- `.github/workflows/nightly.yml` — Add performance benchmarks
- `scripts/regression.ps1` — Add web tests, formatter/linter/debugger/profiler tests
- `scripts/pre-commit.ps1` — Add linter to pre-commit hook

**CI additions:**
```yaml
# New job: web-framework-tests
- name: Web Framework Tests
  run: |
    aurorac --run Workflow/tests/test_web_server.aura
    aurorac --run Workflow/tests/test_web_routes.aura
    aurorac --run Workflow/tests/test_web_params.aura
    # ... all web tests

# New job: linux-gui-tests
- name: Linux GUI Tests
  run: |
    xvfb-run aurorac --run Workflow/tests/test_linux_gui.aura

# New job: mac-gui-tests
- name: macOS GUI Tests
  if: runner.os == 'macOS'
  run: |
    aurorac --run Workflow/tests/test_mac_gui.aura
```

---

## 35.6 Documentation

### Web Framework Documentation

**Files to create/modify:**
- `docs/web_framework.md` — Comprehensive web framework guide
- `docs/api_reference.md` — Add all new web API functions
- `docs/cookbook.md` — Add web recipes
- `docs/getting_started.md` — Add web quick start

**`docs/web_framework.md` contents:**
```markdown
# Aurora Web Framework (Phase 31-34)

## 1. Quick Start
## 2. Routing
## 3. Request Accessors
## 4. Response Builders
## 5. CORS
## 6. CSRF
## 7. Rate Limiting
## 8. Sessions & Authentication
## 9. Middleware
## 10. Templates
## 11. WebSocket & SSE
## 12. Database Integration
## 13. Request Validation
## 14. Production Deployment
## 15. API Reference
```

### Desktop GUI Documentation (Linux & macOS)

**Files to modify:**
- `docs/platform_guides.md` — Add Linux and macOS GUI specifics
- `docs/app_development.md` — Update with new platform support details

### Developer Tools Documentation

**Files to create/modify:**
- `docs/developer_tools.md` — Formatter, Linter, Debugger, Profiler guide
- `docs/editor_integration.md` — LSP, editor setup

### Release & Deployment

**Files to modify:**
- `docs/app_deployment.md` — Add desktop deployment (Linux AppImage, macOS DMG)
- `docs/web_deployment.md` — New: deployment guide for web apps

---

## 35.7 Version Bump

- `VERSION` → `2.0.0`
- `aurora/include/common/aurora_version.hpp` → `AURORA_VERSION_STRING "2.0.0"`
- `release/setup.iss` → Update version
- `CHANGELOG.md` — Document all Phase 31-35 changes

---

## 35.8 Summary of Phase 35 Deliverables

| Task | Files | Status |
|------|-------|--------|
| 35.1 Web test suite | `Workflow/tests/test_web_*.aura` (12 files) | New |
| 35.2 Desktop GUI tests | `Workflow/tests/test_{linux,mac}_gui.aura` | New |
| 35.3 Complex widget tests | `Workflow/tests/test_{webview,media,map}.aura` | New |
| 35.4 Dev tools tests | `Workflow/tests/test_{formatter,linter,debugger,profiler}.aura` | New |
| 35.5 CI/CD updates | `.github/workflows/build.yml`, `nightly.yml`, `scripts/` | Modify |
| 35.6 Documentation | `docs/web_framework.md` (new), `docs/developer_tools.md` (new), others | New |
| 35.7 Version bump | `VERSION`, `aurora_version.hpp`, `setup.iss`, `CHANGELOG.md` | Modify |

**Verification:**
- `scripts/regression.ps1` — all tests pass (100+ test files)
- `scripts/check_release_readiness.ps1` — green across all checks
- CI all green on 3 platforms (Windows, Linux, macOS)
- `aurorac --version` → `Aurora v2.0.0`

---

## Appendix A: File Inventory by Phase

### Phase 31 — Web DSL (New/Modified Files)

| File | Action | Purpose |
|------|--------|---------|
| `aurora/src/compiler/parser/parse_expr.cpp` | Modify | `request.params.X`, `request.query.X`, `response.json()` |
| `aurora/src/compiler/parser/parse_stmt.cpp` | Modify | `middleware`, `websocket`, `sse`, `template`, `session`, `auth`, `cors`, `database`, `model` keywords |
| `aurora/src/compiler/codegen/codegen_expr.cpp` | Modify | `request.params` → `aurora_http_get_param()` |
| `aurora/src/compiler/codegen/codegen_stmt.cpp` | Modify | Server block codegen for all new keywords |
| `aurora/src/compiler/ast.hpp` | Modify | Ensure all new AST node types exist |
| `aurora/src/runtime/backend/server.cpp` | Modify | CORS preflight, unified session, password hashing |
| `aurora/src/runtime/backend/backend_builtins.cpp` | Modify | Fix CORS, unify session, validation helpers |
| `aurora/src/runtime/backend/backend.hpp` | Modify | New function declarations |
| `libc/server.auf` | Modify | New externs |
| `libc/template.auf` | Create | Template extern bindings |
| `Workflow/tests/test_web_dsl_full.aura` | Create | DSL integration test |
| `examples/app/phase31_demo.aura` | Create | Comprehensive web demo |

### Phase 32 — Desktop GUI (New/Modified Files)

| File | Action | Purpose |
|------|--------|---------|
| `aurora/src/std/gui.cpp` | Modify | Linux X11 advanced widgets (TreeView, Table, Image, etc.) |
| `aurora/src/runtime/ui/ui_mac.mm` | Create | macOS Cocoa GUI backend (800+ lines) |
| `aurora/src/runtime/ui/ui_mac.hpp` | Create | macOS header |
| `aurora/src/std/desktop_linux.cpp` | Create | Linux desktop integration |
| `aurora/src/std/desktop_mac.mm` | Create | macOS desktop integration |
| `aurora/include/std/desktop.hpp` | Modify | Non-Windows declarations |
| `CMakeLists.txt` | Modify | Add macOS sources, link Cocoa/WebKit/AVKit |
| `libc/desktop.auf` | Modify | Add Linux/macOS externs |
| `Workflow/tests/test_linux_gui.aura` | Create | Linux GUI test |
| `Workflow/tests/test_mac_gui.aura` | Create | macOS GUI test |
| `Workflow/tests/test_desktop_notify.aura` | Create | Desktop notification test |
| `Workflow/tests/test_desktop_clipboard.aura` | Create | Clipboard test |

### Phase 33 — Complex Widgets & Dev Tools (New/Modified Files)

| File | Action | Purpose |
|------|--------|---------|
| `aurora/src/std/widgets_advanced.cpp` | Modify | WebView/Media/Map dispatch |
| `aurora/src/std/widgets_advanced.hpp` | Modify | New function declarations |
| `aurora/src/runtime/ui/ui_win32.cpp` | Modify | WebView2 + MFPlay integration |
| `aurora/src/std/gui.cpp` | Modify | WebKitGTK + FFmpeg integration |
| `aurora/src/runtime/ui/ui_mac.mm` | Modify | WKWebView + AVPlayerView |
| `aurora/src/mobile/android/android_renderer.cpp` | Modify | Android WebView + ExoPlayer |
| `aurora/src/mobile/ios/ios_renderer_widgets.mm` | Modify | iOS WKWebView + AVPlayer |
| `libc/widgets_advanced.auf` | Modify | New externs |
| `aurora/src/std/formatter.cpp` | Create | Code formatter |
| `aurora/include/std/formatter.hpp` | Create | Formatter header |
| `aurora/src/std/linter.cpp` | Create | Code linter |
| `aurora/include/std/linter.hpp` | Create | Linter header |
| `aurora/src/std/debugger.cpp` | Create | Debugger |
| `aurora/include/std/debugger.hpp` | Create | Debugger header |
| `aurora/src/std/profiler.cpp` | Create | Profiler |
| `aurora/include/std/profiler.hpp` | Create | Profiler header |
| `libc/dev.auf` | Modify | Add format/lint/debug/profile externs |
| `aurora/tools/voss/commands_dev.cpp` | Create | `voss format/lint/debug/profile` CLI |
| `CMakeLists.txt` | Modify | Add 4 new .cpp files |
| `runtime_exports.hpp` | Modify | Replace stubs with real exports |
| `Workflow/tests/test_webview.aura` | Create | WebView test |
| `Workflow/tests/test_media.aura` | Create | Media test |
| `Workflow/tests/test_map.aura` | Create | Map test |
| `Workflow/tests/test_formatter.aura` | Create | Formatter test |
| `Workflow/tests/test_linter.aura` | Create | Linter test |
| `Workflow/tests/test_debugger.aura` | Create | Debugger test |
| `Workflow/tests/test_profiler.aura` | Create | Profiler test |
| `examples/app/webview_demo.aura` | Create | WebView demo |
| `examples/app/media_demo.aura` | Create | Media demo |
| `examples/app/map_demo.aura` | Create | Map demo |

### Phase 34 — Full-Stack Web (New/Modified Files)

| File | Action | Purpose |
|------|--------|---------|
| `aurora/src/compiler/parser/parse_stmt.cpp` | Modify | `validate`, `api` keywords |
| `aurora/src/compiler/codegen/codegen_stmt.cpp` | Modify | Validation, auto-CRUD codegen |
| `aurora/src/compiler/ast.hpp` | Modify | Validate, ApiModel nodes |
| `aurora/src/runtime/backend/server.cpp` | Modify | Unified session, password hashing, CSRF, rate limit integration |
| `aurora/src/runtime/backend/backend_builtins.cpp` | Modify | Validation helpers, rate limit wrappers |
| `aurora/src/runtime/backend/backend.hpp` | Modify | New declarations |
| `aurora/src/runtime/backend/gateway.cpp` | Modify | Rate limiter clean API |
| `libc/server.auf` | Modify | New externs |
| `examples/app/fullstack_demo.aura` | Create | Full-stack demo |
| `examples/app/fullstack_demo/templates/layout.html` | Create | Template |
| `examples/app/fullstack_demo/static/style.css` | Create | Styles |
| `Workflow/tests/test_web_server.aura` | Create | Server test |
| `Workflow/tests/test_web_routes.aura` | Create | Routes test |
| `Workflow/tests/test_web_params.aura` | Create | Params test |
| `Workflow/tests/test_web_cors.aura` | Create | CORS test |
| `Workflow/tests/test_web_csrf.aura` | Create | CSRF test |
| `Workflow/tests/test_web_session.aura` | Create | Session test |
| `Workflow/tests/test_web_auth.aura` | Create | Auth test |
| `Workflow/tests/test_web_websocket.aura` | Create | WebSocket test |
| `Workflow/tests/test_web_template.aura` | Create | Template test |
| `Workflow/tests/test_web_validation.aura` | Create | Validation test |
| `Workflow/tests/test_web_rate_limit.aura` | Create | Rate limit test |
| `Workflow/tests/test_web_middleware.aura` | Create | Middleware test |

### Phase 35 — Testing, CI/CD & Docs (New/Modified Files)

| File | Action | Purpose |
|------|--------|---------|
| `docs/web_framework.md` | Create | Web framework guide |
| `docs/developer_tools.md` | Create | Dev tools guide |
| `docs/editor_integration.md` | Create | Editor integration |
| `docs/web_deployment.md` | Create | Web deployment |
| `docs/api_reference.md` | Modify | Add all new API functions |
| `docs/cookbook.md` | Modify | Add web recipes |
| `docs/getting_started.md` | Modify | Add web quick start |
| `docs/platform_guides.md` | Modify | Linux/macOS GUI details |
| `docs/app_development.md` | Modify | Updated platform support |
| `docs/app_deployment.md` | Modify | Linux AppImage, macOS DMG |
| `.github/workflows/build.yml` | Modify | Add web + GUI test jobs |
| `.github/workflows/nightly.yml` | Modify | Add performance benchmarks |
| `scripts/regression.ps1` | Modify | Add all new tests |
| `scripts/pre-commit.ps1` | Modify | Add linter hook |
| `VERSION` | Modify | `2.0.0` |
| `aurora/include/common/aurora_version.hpp` | Modify | Version string |
| `release/setup.iss` | Modify | Version update |
| `CHANGELOG.md` | Modify | Phase 31-35 entries |

---

## Appendix B: Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| macOS Cocoa implementation too large for a single phase | Medium | High | Split into 32.2a (basic widgets) + 32.2b (advanced widgets) |
| WebView2 on Windows requires WebView2 runtime | Low | Medium | Check for runtime, provide clear installation instructions |
| WebKitGTK not available on minimal Linux systems | Medium | Low | Fall back to stub with clear error message |
| Full-stack demo becomes too complex to maintain | Medium | Low | Keep demo single-file; multi-file version optional |
| Linter/debugger/profiler scope creep | High | Medium | Ship v1 with core features; advanced features can come later |
| Breaking changes in existing API | Low | High | All new features are additive; no existing API changed |
| CI time increases significantly | Medium | Low | Parallelize test jobs, use test matrix |

---

## Appendix C: Build Targets

After Phase 31-35, build targets remain 23+ (same structure as v1.0.0):

| Target | Platform | Phase 31 | Phase 32 | Phase 33 | Phase 34 | Phase 35 |
|--------|----------|----------|----------|----------|----------|----------|
| `aurora_runtime.lib` | All | +server.cpp changes | +ui_mac.mm, +desktop_linux.cpp, +desktop_mac.mm | +widgets_advanced.cpp, +formatter/linter/debugger/profiler | +server.cpp changes | Unchanged |
| `aurorac.exe` | All | +parser/codegen changes | Unchanged | Unchanged | +parser/codegen changes | Unchanged |
| `test_crossplatform` | All | +web tests | +gui tests | +complex widget tests | Unchanged | +full test suite |

**Build verification:**
```
scripts/check_release_readiness.ps1
# Output: ✅ All checks passed — Aurora v2.0.0 ready for release
```

---

## Appendix D: Migration Guide (v1 → v2)

For existing Aurora v1.0.0 users:
- No breaking changes to any existing API
- New DSL syntax is additive only
- All existing `.aura` and `.auf` files continue to work unchanged
- Upgrade path: simply recompile with `aurorac` v2.0.0
