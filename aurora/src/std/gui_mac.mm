/* ════════════════════════════════════════════════════════════
   gui_mac.mm — macOS Cocoa native GUI backend
   Compile as Objective-C++ (.mm) on Apple platforms.
   ════════════════════════════════════════════════════════════ */

#include "../../include/std/gui.hpp"
#import <Cocoa/Cocoa.h>
#import <cstdlib>
#import <cstring>
#import <cstdio>
#import <vector>
#import <map>
#import <string>

/* ── Internal widget state ── */
struct GuiWidget {
    int     id;
    int     type;
    int     x, y, w, h;
    std::string text;
    std::vector<std::string> items;
    int     selected_idx;
    AuroraEventCallback callback;
    GuiWidget* parent;
    /* Cocoa native handles */
    NSView*      nsview;
    NSWindow*    nswindow;
    NSButton*    nsbutton;
    NSTextField* nslabel;
    NSTextField* nstextbox;
    NSTableView* nstableview;
    bool visible;
};

/* ── Global state ── */
static std::vector<GuiWidget*> g_widgets;
static std::map<int, GuiWidget*> g_id_map;
static int g_next_id = 1;
static bool g_running = false;
static bool g_app_inited = false;

/* ── Forward declarations ── */
@interface AuroraAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
- (void)windowWillClose:(NSNotification*)note;
@end

@interface AuroraButtonTarget : NSObject
- (void)buttonClicked:(id)sender;
@end

/* ── Globals for delegates ── */
static AuroraAppDelegate* g_app_delegate = nil;
static AuroraButtonTarget* g_btn_target = nil;

/* ── Helper: create widget ── */
static GuiWidget* widget_new(int type, GuiWidget* parent) {
    GuiWidget* w = new GuiWidget();
    w->id = g_next_id++;
    w->type = type;
    w->x = w->y = w->w = w->h = 0;
    w->selected_idx = -1;
    w->callback = nullptr;
    w->parent = parent;
    w->nsview = nil;
    w->nswindow = nil;
    w->nsbutton = nil;
    w->nslabel = nil;
    w->nstextbox = nil;
    w->nstableview = nil;
    w->visible = false;
    g_widgets.push_back(w);
    g_id_map[w->id] = w;
    return w;
}

/* ── Helper: find widget by NSView ── */
static GuiWidget* widget_from_nsview(NSView* view) {
    for (auto* w : g_widgets)
        if (w->nsview == view || (w->nswindow && [w->nswindow contentView] == view))
            return w;
    return nullptr;
}

/* ── Helper: initialize NSApp ── */
static void ensure_app() {
    if (g_app_inited) return;
    g_app_inited = true;

    g_app_delegate = [[AuroraAppDelegate alloc] init];
    g_btn_target = [[AuroraButtonTarget alloc] init];

    [NSApplication sharedApplication];
    [NSApp setDelegate:g_app_delegate];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
}

/* ═══════════════════════════════════════════════════════════════
   AuroraAppDelegate — handles app-level events
   ═══════════════════════════════════════════════════════════════ */
@implementation AuroraAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification*)note {
    (void)note;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}
- (void)windowWillClose:(NSNotification*)note {
    NSWindow* win = [note object];
    for (auto* w : g_widgets) {
        if (w->nswindow == win && w->callback) {
            w->callback(w->id, AURORA_EVENT_CLOSE, 0, 0);
        }
    }
    g_running = false;
}
@end

/* ═══════════════════════════════════════════════════════════════
   AuroraButtonTarget — handles button clicks
   ═══════════════════════════════════════════════════════════════ */
@implementation AuroraButtonTarget
- (void)buttonClicked:(id)sender {
    NSButton* btn = (NSButton*)sender;
    for (auto* w : g_widgets) {
        if (w->nsbutton == btn && w->callback) {
            w->callback(w->id, AURORA_EVENT_CLICK, 0, 0);
            return;
        }
    }
}
@end

/* ═══════════════════════════════════════════════════════════════
   Public API — Cocoa implementations
   ═══════════════════════════════════════════════════════════════ */

extern "C" {

AuroraWidget aurora_gui_window_new(const char* title, int width, int height) {
    ensure_app();

    NSRect rect = NSMakeRect(0, 0, width, height);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                       NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    NSWindow* win = [[NSWindow alloc] initWithContentRect:rect
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [win setTitle:[NSString stringWithUTF8String:title ?: ""]];
    [win setDelegate:g_app_delegate];
    [win center];

    GuiWidget* w = widget_new(AURORA_WIDGET_WINDOW, nullptr);
    w->nswindow = win;
    w->nsview = [win contentView];
    w->w = width;
    w->h = height;
    w->text = title ? title : "";
    return (AuroraWidget)w;
}

void aurora_gui_window_set_title(AuroraWidget win, const char* title) {
    GuiWidget* w = (GuiWidget*)win;
    if (!w || !w->nswindow) return;
    w->text = title ? title : "";
    [w->nswindow setTitle:[NSString stringWithUTF8String:w->text.c_str()]];
}

void aurora_gui_window_resize(AuroraWidget win, int w_, int h_) {
    GuiWidget* w = (GuiWidget*)win;
    if (!w || !w->nswindow) return;
    w->w = w_; w->h = h_;
    NSRect rect = [w->nswindow contentRectForFrameRect:[w->nswindow frame]];
    rect.size.width = w_;
    rect.size.height = h_;
    [w->nswindow setFrame:[w->nswindow frameRectForContentRect:rect] display:YES animate:YES];
}

void aurora_gui_window_show(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (!w || !w->nswindow) return;
    [w->nswindow makeKeyAndOrderFront:nil];
    w->visible = true;
}

void aurora_gui_window_hide(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (!w || !w->nswindow) return;
    [w->nswindow orderOut:nil];
    w->visible = false;
}

void aurora_gui_window_destroy(AuroraWidget win) {
    GuiWidget* w = (GuiWidget*)win;
    if (!w) return;
    if (w->nswindow) {
        [w->nswindow close];
        [w->nswindow release];
        w->nswindow = nil;
    }
    w->nsview = nil;
}

AuroraWidget aurora_gui_button_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    if (!p || !p->nsview) return nullptr;

    NSRect rect = NSMakeRect(x, y, w_, h_);
    NSButton* btn = [[NSButton alloc] initWithFrame:rect];
    [btn setTitle:[NSString stringWithUTF8String:text ?: "Button"]];
    [btn setBezelStyle:NSBezelStyleRounded];
    [btn setTarget:g_btn_target];
    [btn setAction:@selector(buttonClicked:)];
    [p->nsview addSubview:btn];

    GuiWidget* w = widget_new(AURORA_WIDGET_BUTTON, p);
    w->nsbutton = btn;
    w->nsview = btn;
    w->text = text ? text : "";
    w->x = x; w->y = y; w->w = w_; w->h = h_;
    return (AuroraWidget)w;
}

void aurora_gui_button_set_text(AuroraWidget btn, const char* text) {
    GuiWidget* w = (GuiWidget*)btn;
    if (!w) return;
    w->text = text ? text : "";
    if (w->nsbutton)
        [w->nsbutton setTitle:[NSString stringWithUTF8String:w->text.c_str()]];
}

const char* aurora_gui_button_get_text(AuroraWidget btn) {
    GuiWidget* w = (GuiWidget*)btn;
    return w ? w->text.c_str() : "";
}

AuroraWidget aurora_gui_label_new(AuroraWidget parent, const char* text, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    if (!p || !p->nsview) return nullptr;

    NSRect rect = NSMakeRect(x, y, w_, h_);
    NSTextField* lbl = [[NSTextField alloc] initWithFrame:rect];
    [lbl setStringValue:[NSString stringWithUTF8String:text ?: ""]];
    [lbl setBezeled:NO];
    [lbl setDrawsBackground:NO];
    [lbl setEditable:NO];
    [lbl setSelectable:NO];
    [p->nsview addSubview:lbl];

    GuiWidget* w = widget_new(AURORA_WIDGET_LABEL, p);
    w->nslabel = lbl;
    w->nsview = lbl;
    w->text = text ? text : "";
    w->x = x; w->y = y; w->w = w_; w->h = h_;
    return (AuroraWidget)w;
}

void aurora_gui_label_set_text(AuroraWidget lbl, const char* text) {
    GuiWidget* w = (GuiWidget*)lbl;
    if (!w) return;
    w->text = text ? text : "";
    if (w->nslabel)
        [w->nslabel setStringValue:[NSString stringWithUTF8String:w->text.c_str()]];
}

const char* aurora_gui_label_get_text(AuroraWidget lbl) {
    GuiWidget* w = (GuiWidget*)lbl;
    return w ? w->text.c_str() : "";
}

AuroraWidget aurora_gui_textbox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    if (!p || !p->nsview) return nullptr;

    NSRect rect = NSMakeRect(x, y, w_, h_);
    NSTextField* tb = [[NSTextField alloc] initWithFrame:rect];
    [tb setStringValue:@""];
    [tb setBezeled:YES];
    [tb setDrawsBackground:YES];
    [tb setEditable:YES];
    [tb setSelectable:YES];
    [p->nsview addSubview:tb];

    GuiWidget* w = widget_new(AURORA_WIDGET_TEXTBOX, p);
    w->nstextbox = tb;
    w->nsview = tb;
    w->x = x; w->y = y; w->w = w_; w->h = h_;
    return (AuroraWidget)w;
}

void aurora_gui_textbox_set_text(AuroraWidget tb, const char* text) {
    GuiWidget* w = (GuiWidget*)tb;
    if (!w) return;
    w->text = text ? text : "";
    if (w->nstextbox)
        [w->nstextbox setStringValue:[NSString stringWithUTF8String:w->text.c_str()]];
}

const char* aurora_gui_textbox_get_text(AuroraWidget tb) {
    GuiWidget* w = (GuiWidget*)tb;
    if (!w) return "";
    if (w->nstextbox) {
        w->text = [[w->nstextbox stringValue] UTF8String];
    }
    return w->text.c_str();
}

AuroraWidget aurora_gui_listbox_new(AuroraWidget parent, int x, int y, int w_, int h_) {
    GuiWidget* p = (GuiWidget*)parent;
    if (!p || !p->nsview) return nullptr;

    /* NSScrollView containing NSTableView */
    NSRect scroll_rect = NSMakeRect(x, y, w_, h_);
    NSScrollView* sv = [[NSScrollView alloc] initWithFrame:scroll_rect];
    [sv setHasVerticalScroller:YES];
    [sv setHasHorizontalScroller:NO];
    [sv setAutohidesScrollers:YES];

    NSTableView* tv = [[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, w_, h_)];
    NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:@"main"];
    [col setWidth:w_ - 20];
    [tv addTableColumn:col];
    [tv setHeaderView:nil];
    [tv setAllowsMultipleSelection:NO];
    [sv setDocumentView:tv];
    [p->nsview addSubview:sv];

    GuiWidget* w = widget_new(AURORA_WIDGET_LISTBOX, p);
    w->nstableview = tv;
    w->nsview = sv;
    w->x = x; w->y = y; w->w = w_; w->h = h_;
    return (AuroraWidget)w;
}

void aurora_gui_listbox_add_item(AuroraWidget lb, const char* item) {
    GuiWidget* w = (GuiWidget*)lb;
    if (!w) return;
    w->items.push_back(item ? item : "");
    if (w->nstableview)
        [w->nstableview reloadData];
}

void aurora_gui_listbox_clear(AuroraWidget lb) {
    GuiWidget* w = (GuiWidget*)lb;
    if (!w) return;
    w->items.clear();
    w->selected_idx = -1;
    if (w->nstableview)
        [w->nstableview reloadData];
}

int aurora_gui_listbox_get_selected(AuroraWidget lb) {
    GuiWidget* w = (GuiWidget*)lb;
    if (!w || !w->nstableview) return -1;
    NSInteger row = [w->nstableview selectedRow];
    w->selected_idx = (row >= 0) ? (int)row : -1;
    return w->selected_idx;
}

const char* aurora_gui_listbox_get_item(AuroraWidget lb, int idx) {
    GuiWidget* w = (GuiWidget*)lb;
    if (!w || idx < 0 || idx >= (int)w->items.size()) return nullptr;
    return w->items[idx].c_str();
}

int aurora_gui_listbox_count(AuroraWidget lb) {
    GuiWidget* w = (GuiWidget*)lb;
    return w ? (int)w->items.size() : 0;
}

void aurora_gui_set_callback(AuroraWidget widget, AuroraEventCallback cb) {
    if (widget) ((GuiWidget*)widget)->callback = cb;
}

void aurora_gui_run() {
    ensure_app();
    g_running = true;
    [NSApp run];
}

void aurora_gui_quit() {
    g_running = false;
    [NSApp stop:nil];

    /* Post a fake event to unblock the run loop */
    NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSZeroPoint
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:NO];
}

void aurora_gui_layout_horizontal(AuroraWidget parent, int margin) {
    /* Auto-layout is not implemented — Cocoa provides Auto Layout natively.
       This is a no-op; users should use Cocoa Auto Layout constraints. */
    (void)parent; (void)margin;
}

void aurora_gui_layout_vertical(AuroraWidget parent, int margin) {
    (void)parent; (void)margin;
}

} /* extern "C" */
