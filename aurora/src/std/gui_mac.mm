/* gui_mac.mm — macOS Cocoa/AppKit native GUI backend for Aurora.
   All aurora_gui_* functions implemented using native NSView/NSControl widgets.
   AuroraWidget is an integer ID cast to void*. */

#include "../../include/std/gui.hpp"
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import <AVKit/AVKit.h>
#import <AVFoundation/AVFoundation.h>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <string>

/* ── TreeView data source — NSOutlineView data source ── */
@interface AuroraTreeDataSource : NSObject <NSOutlineViewDataSource> {
@public
    int widget_id;
}
- (instancetype)initWithWidgetId:(int)wid;
@end

/* ── WebView navigation delegate — WKNavigationDelegate ── */
@interface AuroraWebViewDelegate : NSObject <WKNavigationDelegate> {
@public
    int widget_id;
}
- (instancetype)initWithWidgetId:(int)wid;
@end

/* ── Widget type constants ── */
#define WIDGET_WINDOW     1
#define WIDGET_LABEL      2
#define WIDGET_BUTTON     3
#define WIDGET_TEXTBOX    4
#define WIDGET_PASSWORD   5
#define WIDGET_LISTBOX    6
#define WIDGET_COMBOBOX   7
#define WIDGET_CHECKBOX   8
#define WIDGET_RADIO      9
#define WIDGET_SLIDER    10
#define WIDGET_PROGRESS  11
#define WIDGET_TREEVIEW  13
#define WIDGET_TABLE     14
#define WIDGET_TABVIEW   15
#define WIDGET_SCROLL    16
#define WIDGET_CANVAS    17
#define WIDGET_IMAGE     18
#define WIDGET_STATUSBAR 19
#define WIDGET_SWITCH    20
#define WIDGET_SPLIT     21
#define WIDGET_GROUPBOX  22
#define WIDGET_TOOLBAR   23
#define WIDGET_WEBVIEW   24
#define WIDGET_MEDIA     25
#define WIDGET_MAP       26

#define EVENT_CLICK      1
#define EVENT_CHANGE     2
#define EVENT_FOCUS      3
#define EVENT_BLUR       4

/* ── Global state ── */
static int g_next_id = 1;
static std::map<int, NSView*> g_widget_map;
static std::map<int, int> g_widget_types;
static std::map<int, AuroraEventCallback> g_callbacks;
static std::map<int, std::string> g_widget_texts;
static std::map<int, std::vector<std::string>> g_list_items;
static std::map<int, int> g_radio_group;
static NSWindow* g_main_window = nil;
static bool g_running = false;

/* ── TreeView data model ── */
struct TreeNode {
    int id;
    int parent_id;
    std::string text;
};
static std::map<int, std::vector<TreeNode>> g_tree_nodes;   // widget_id → nodes
static std::map<int, int> g_tree_next_node_id;               // widget_id → next node id
static std::map<int, std::map<int, int>> g_tree_node_index;  // widget_id → { node_id → index }

static int alloc_id() { return g_next_id++; }

static NSView* view_for_id(int wid) {
    auto it = g_widget_map.find(wid);
    return it != g_widget_map.end() ? it->second : nil;
}

static void set_callback(int wid, AuroraEventCallback cb) {
    if (cb) g_callbacks[wid] = cb;
    else g_callbacks.erase(wid);
}

static void fire_event(int wid, int event, int p1, int p2) {
    auto it = g_callbacks.find(wid);
    if (it != g_callbacks.end() && it->second)
        it->second(wid, event, p1, p2);
}

static NSString* to_ns(const char* s) {
    return s ? [NSString stringWithUTF8String:s] : @"";
}

static unsigned int ns_color_to_uint(NSColor* c) {
    c = [c colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    CGFloat r = [c redComponent] * 255;
    CGFloat g = [c greenComponent] * 255;
    CGFloat b = [c blueComponent] * 255;
    return ((unsigned int)r << 16) | ((unsigned int)g << 8) | (unsigned int)b;
}

static NSColor* uint_to_ns_color(unsigned int hex) {
    CGFloat r = ((hex >> 16) & 0xFF) / 255.0;
    CGFloat g = ((hex >> 8) & 0xFF) / 255.0;
    CGFloat b = (hex & 0xFF) / 255.0;
    return [NSColor colorWithSRGBRed:r green:g blue:b alpha:1.0];
}

/* ── Helper: add subview to parent widget ── */
static void add_to_parent(NSView* child, AuroraWidget parent) {
    if (!parent) return;
    int pid = (int)(intptr_t)parent;
    NSView* pv = view_for_id(pid);
    if (!pv) return;
    int ptype = g_widget_types[pid];
    if (ptype == WIDGET_WINDOW)
        [[(NSWindow*)pv contentView] addSubview:child];
    else
        [pv addSubview:child];
}

extern "C" {

/* ════════════════════════════════════════════════════════════
   App Lifecycle
   ════════════════════════════════════════════════════════════ */
int aurora_gui_app_init(void) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    }
    return 0;
}

void aurora_gui_app_run(void) {
    @autoreleasepool {
        g_running = true;
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];
    }
}

void aurora_gui_app_quit(void) {
    g_running = false;
    [NSApp stop:nil];
    [NSApp abortModal];
}

/* ════════════════════════════════════════════════════════════
   Window
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_window_new(const char* title, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSRect frame = NSMakeRect(0, 0, w, h);
        NSWindow* win = [[NSWindow alloc] initWithContentRect:frame
            styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable|NSWindowStyleMaskMiniaturizable
            backing:NSBackingStoreBuffered defer:NO];
        [win setTitle:to_ns(title)];
        [win center];
        g_widget_map[wid] = (NSView*)win;
        g_widget_types[wid] = WIDGET_WINDOW;
        g_main_window = win;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_window_set_title(AuroraWidget w, const char* t) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    [win setTitle:to_ns(t)];
}

void aurora_gui_window_resize(AuroraWidget w, int w_, int h_) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    NSRect r = [win frame];
    r.size = NSMakeSize(w_, h_);
    [win setFrame:r display:YES];
}

void aurora_gui_window_show(AuroraWidget w) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    [win makeKeyAndOrderFront:nil];
}

void aurora_gui_window_hide(AuroraWidget w) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    [win orderOut:nil];
}

void aurora_gui_window_destroy(AuroraWidget w) {
    int wid = (int)(intptr_t)w;
    NSWindow* win = (NSWindow*)view_for_id(wid);
    [win close];
    g_widget_map.erase(wid);
    g_widget_types.erase(wid);
    g_callbacks.erase(wid);
}

void aurora_gui_window_maximize(AuroraWidget w) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    [win zoom:nil];
}

void aurora_gui_window_minimize(AuroraWidget w) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    [win miniaturize:nil];
}

void aurora_gui_window_restore(AuroraWidget w) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    [win deminiaturize:nil];
}

int aurora_gui_window_get_width(AuroraWidget w) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    return (int)[win frame].size.width;
}

int aurora_gui_window_get_height(AuroraWidget w) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    return (int)[win frame].size.height;
}

void aurora_gui_window_set_min_size(AuroraWidget w, int mw, int mh) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    [win setContentMinSize:NSMakeSize(mw, mh)];
}

void aurora_gui_window_set_max_size(AuroraWidget w, int mw, int mh) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    [win setContentMaxSize:NSMakeSize(mw, mh)];
}

void aurora_gui_window_set_resizable(AuroraWidget w, int r) {
    NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)w);
    [win setStyleMask:r ? (NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable|NSWindowStyleMaskMiniaturizable)
                        : (NSWindowStyleMaskTitled|NSWindowStyleMaskClosable)];
}

/* ════════════════════════════════════════════════════════════
   Common
   ════════════════════════════════════════════════════════════ */
void aurora_gui_set_callback(AuroraWidget widget, AuroraEventCallback cb) {
    int wid = (int)(intptr_t)widget;
    set_callback(wid, cb);
}

void aurora_gui_set_enabled(AuroraWidget w, int e) {
    NSView* v = view_for_id((int)(intptr_t)w);
    if ([v respondsToSelector:@selector(setEnabled:)])
        [(id)v setEnabled:(BOOL)e];
}

int aurora_gui_get_enabled(AuroraWidget w) {
    NSView* v = view_for_id((int)(intptr_t)w);
    if ([v respondsToSelector:@selector(isEnabled)])
        return [(id)v isEnabled] ? 1 : 0;
    return 1;
}

void aurora_gui_set_visible(AuroraWidget w, int v) {
    NSView* view = view_for_id((int)(intptr_t)w);
    [view setHidden:(BOOL)!v];
}

int aurora_gui_get_visible(AuroraWidget w) {
    NSView* view = view_for_id((int)(intptr_t)w);
    return [view isHidden] ? 0 : 1;
}

void aurora_gui_set_focus(AuroraWidget w) {
    NSView* view = view_for_id((int)(intptr_t)w);
    [[view window] makeFirstResponder:view];
}

void aurora_gui_move(AuroraWidget w, int x, int y, int w_, int h_) {
    NSView* view = view_for_id((int)(intptr_t)w);
    [view setFrame:NSMakeRect(x, y, w_, h_)];
}

void* aurora_gui_get_native_handle(AuroraWidget w) {
    return (__bridge void*)view_for_id((int)(intptr_t)w);
}

/* ════════════════════════════════════════════════════════════
   Label
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_label_new(AuroraWidget p, const char* text, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSTextField* lbl = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [lbl setStringValue:to_ns(text)];
        [lbl setBezeled:NO];
        [lbl setDrawsBackground:NO];
        [lbl setEditable:NO];
        [lbl setSelectable:NO];
        [lbl setBordered:NO];
        add_to_parent(lbl, p);
        g_widget_map[wid] = lbl;
        g_widget_types[wid] = WIDGET_LABEL;
        g_widget_texts[wid] = text ? text : "";
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_label_set_text(AuroraWidget w, const char* t) {
    NSTextField* lbl = (NSTextField*)view_for_id((int)(intptr_t)w);
    [lbl setStringValue:to_ns(t)];
    g_widget_texts[(int)(intptr_t)w] = t ? t : "";
}

const char* aurora_gui_label_get_text(AuroraWidget w) {
    static std::string result;
    result = g_widget_texts[(int)(intptr_t)w];
    return result.c_str();
}

void aurora_gui_label_set_font_size(AuroraWidget w, int size) {
    NSTextField* lbl = (NSTextField*)view_for_id((int)(intptr_t)w);
    [lbl setFont:[NSFont systemFontOfSize:(CGFloat)size]];
}

void aurora_gui_label_set_color(AuroraWidget w, unsigned int color) {
    NSTextField* lbl = (NSTextField*)view_for_id((int)(intptr_t)w);
    [lbl setTextColor:uint_to_ns_color(color)];
}

void aurora_gui_label_set_align(AuroraWidget w, int align) {
    NSTextField* lbl = (NSTextField*)view_for_id((int)(intptr_t)w);
    NSTextAlignment a = NSTextAlignmentLeft;
    if (align == 1) a = NSTextAlignmentCenter;
    else if (align == 2) a = NSTextAlignmentRight;
    [lbl setAlignment:a];
}

/* ════════════════════════════════════════════════════════════
   Text (alias for label)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_text_new(AuroraWidget p, const char* t, int x, int y, int w, int h) {
    return aurora_gui_label_new(p, t, x, y, w, h);
}
void aurora_gui_text_set_text(AuroraWidget w, const char* t) { aurora_gui_label_set_text(w, t); }
const char* aurora_gui_text_get_text(AuroraWidget w) { return aurora_gui_label_get_text(w); }

/* ════════════════════════════════════════════════════════════
   Button
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_button_new(AuroraWidget p, const char* text, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSButton* btn = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [btn setTitle:to_ns(text)];
        [btn setBezelStyle:NSBezelStyleRounded];
        [btn setButtonType:NSButtonTypeMomentaryPushIn];
        [btn setTarget:btn];
        [btn setAction:@selector(buttonClicked:)];
        add_to_parent(btn, p);
        g_widget_map[wid] = btn;
        g_widget_types[wid] = WIDGET_BUTTON;
        return (AuroraWidget)(intptr_t)wid;
    }
}

@interface AuroraButtonTarget : NSObject
@end
static AuroraButtonTarget* g_btn_target = nil;

@implementation AuroraButtonTarget
- (void)buttonClicked:(id)sender {
    NSButton* btn = (NSButton*)sender;
    for (auto& pair : g_widget_map) {
        if ((NSView*)pair.second == (NSView*)btn) {
            fire_event(pair.first, EVENT_CLICK, 0, 0);
            return;
        }
    }
}
@end

void aurora_gui_button_set_text(AuroraWidget w, const char* t) {
    NSButton* btn = (NSButton*)view_for_id((int)(intptr_t)w);
    [btn setTitle:to_ns(t)];
}

const char* aurora_gui_button_get_text(AuroraWidget w) {
    NSButton* btn = (NSButton*)view_for_id((int)(intptr_t)w);
    static std::string result;
    result = [[btn title] UTF8String];
    return result.c_str();
}

/* ════════════════════════════════════════════════════════════
   Checkbox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_checkbox_new(AuroraWidget p, const char* text, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSButton* cb = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [cb setTitle:to_ns(text)];
        [cb setButtonType:NSButtonTypeSwitch];
        [cb setTarget:cb];
        [cb setAction:@selector(checkChanged:)];
        add_to_parent(cb, p);
        g_widget_map[wid] = cb;
        g_widget_types[wid] = WIDGET_CHECKBOX;
        return (AuroraWidget)(intptr_t)wid;
    }
}

@interface AuroraCheckTarget : NSObject
@end
static AuroraCheckTarget* g_check_target = nil;
@implementation AuroraCheckTarget
- (void)checkChanged:(id)sender {
    NSButton* cb = (NSButton*)sender;
    for (auto& pair : g_widget_map) {
        if ((NSView*)pair.second == (NSView*)cb) {
            fire_event(pair.first, EVENT_CHANGE, [cb state] == NSControlStateValueOn ? 1 : 0, 0);
            return;
        }
    }
}
@end

void aurora_gui_checkbox_set_text(AuroraWidget w, const char* t) {
    NSButton* cb = (NSButton*)view_for_id((int)(intptr_t)w);
    [cb setTitle:to_ns(t)];
}

const char* aurora_gui_checkbox_get_text(AuroraWidget w) {
    NSButton* cb = (NSButton*)view_for_id((int)(intptr_t)w);
    static std::string result;
    result = [[cb title] UTF8String];
    return result.c_str();
}

int aurora_gui_checkbox_is_checked(AuroraWidget w) {
    NSButton* cb = (NSButton*)view_for_id((int)(intptr_t)w);
    return [cb state] == NSControlStateValueOn ? 1 : 0;
}

void aurora_gui_checkbox_set_checked(AuroraWidget w, int c) {
    NSButton* cb = (NSButton*)view_for_id((int)(intptr_t)w);
    [cb setState:(c ? NSControlStateValueOn : NSControlStateValueOff)];
}

/* ════════════════════════════════════════════════════════════
   RadioButton
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_radiobutton_new(AuroraWidget p, const char* text, int x, int y, int w, int h, int gid) {
    @autoreleasepool {
        int wid = alloc_id();
        NSButton* rb = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [rb setTitle:to_ns(text)];
        [rb setButtonType:NSButtonTypeRadio];
        [rb setTarget:rb];
        [rb setAction:@selector(radioClicked:)];
        add_to_parent(rb, p);
        g_widget_map[wid] = rb;
        g_widget_types[wid] = WIDGET_RADIO;
        g_radio_group[wid] = gid;
        return (AuroraWidget)(intptr_t)wid;
    }
}

@interface AuroraRadioTarget : NSObject
@end
static AuroraRadioTarget* g_radio_target = nil;
@implementation AuroraRadioTarget
- (void)radioClicked:(id)sender {
    NSButton* rb = (NSButton*)sender;
    for (auto& pair : g_widget_map) {
        if ((NSView*)pair.second == (NSView*)rb) {
            fire_event(pair.first, EVENT_CLICK, 0, 0);
            return;
        }
    }
}
@end

void aurora_gui_radiobutton_set_text(AuroraWidget w, const char* t) {
    NSButton* rb = (NSButton*)view_for_id((int)(intptr_t)w);
    [rb setTitle:to_ns(t)];
}

const char* aurora_gui_radiobutton_get_text(AuroraWidget w) {
    NSButton* rb = (NSButton*)view_for_id((int)(intptr_t)w);
    static std::string result;
    result = [[rb title] UTF8String];
    return result.c_str();
}

int aurora_gui_radiobutton_is_checked(AuroraWidget w) {
    NSButton* rb = (NSButton*)view_for_id((int)(intptr_t)w);
    return [rb state] == NSControlStateValueOn ? 1 : 0;
}

void aurora_gui_radiobutton_set_checked(AuroraWidget w, int c) {
    NSButton* rb = (NSButton*)view_for_id((int)(intptr_t)w);
    [rb setState:(c ? NSControlStateValueOn : NSControlStateValueOff)];
}

/* ════════════════════════════════════════════════════════════
   Switch
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_switch_new(AuroraWidget p, const char* text, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSButton* sw = [[NSButton alloc] initWithFrame:NSMakeRect(x, y, w > 0 ? w : 60, h > 0 ? h : 24)];
        [sw setTitle:to_ns(text)];
        [sw setButtonType:NSButtonTypeSwitch];
        [sw setTarget:sw];
        [sw setAction:@selector(switchChanged:)];
        add_to_parent(sw, p);
        g_widget_map[wid] = sw;
        g_widget_types[wid] = WIDGET_SWITCH;
        return (AuroraWidget)(intptr_t)wid;
    }
}

@interface AuroraSwitchTarget : NSObject
@end
static AuroraSwitchTarget* g_switch_target = nil;
@implementation AuroraSwitchTarget
- (void)switchChanged:(id)sender {
    NSButton* sw = (NSButton*)sender;
    for (auto& pair : g_widget_map) {
        if ((NSView*)pair.second == (NSView*)sw) {
            fire_event(pair.first, EVENT_CHANGE, [sw state] == NSControlStateValueOn ? 1 : 0, 0);
            return;
        }
    }
}
@end

int aurora_gui_switch_is_on(AuroraWidget w) {
    NSButton* sw = (NSButton*)view_for_id((int)(intptr_t)w);
    return [sw state] == NSControlStateValueOn ? 1 : 0;
}

void aurora_gui_switch_set_on(AuroraWidget w, int on) {
    NSButton* sw = (NSButton*)view_for_id((int)(intptr_t)w);
    [sw setState:(on ? NSControlStateValueOn : NSControlStateValueOff)];
}

/* ════════════════════════════════════════════════════════════
   TextBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_textbox_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSTextField* tb = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [tb setBezeled:YES];
        [tb setEditable:YES];
        [tb setSelectable:YES];
        [tb setBordered:YES];
        add_to_parent(tb, p);
        g_widget_map[wid] = tb;
        g_widget_types[wid] = WIDGET_TEXTBOX;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_textbox_set_text(AuroraWidget w, const char* t) {
    NSTextField* tb = (NSTextField*)view_for_id((int)(intptr_t)w);
    [tb setStringValue:to_ns(t)];
}

const char* aurora_gui_textbox_get_text(AuroraWidget w) {
    NSTextField* tb = (NSTextField*)view_for_id((int)(intptr_t)w);
    static std::string result;
    result = [[tb stringValue] UTF8String];
    return result.c_str();
}

void aurora_gui_textbox_set_readonly(AuroraWidget w, int r) {
    NSTextField* tb = (NSTextField*)view_for_id((int)(intptr_t)w);
    [tb setEditable:!r];
    [tb setSelectable:!r];
}

void aurora_gui_textbox_set_placeholder(AuroraWidget w, const char* t) {
    NSTextField* tb = (NSTextField*)view_for_id((int)(intptr_t)w);
    if ([[tb cell] respondsToSelector:@selector(setPlaceholderString:)])
        [[tb cell] setPlaceholderString:to_ns(t)];
}

void aurora_gui_textbox_set_multiline(AuroraWidget w, int ml) {
    (void)w; (void)ml;
}

int aurora_gui_textbox_get_line_count(AuroraWidget w) { (void)w; return 1; }

/* ════════════════════════════════════════════════════════════
   PasswordBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_passwordbox_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSSecureTextField* pt = [[NSSecureTextField alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        add_to_parent(pt, p);
        g_widget_map[wid] = pt;
        g_widget_types[wid] = WIDGET_PASSWORD;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_passwordbox_set_text(AuroraWidget w, const char* t) {
    NSSecureTextField* pt = (NSSecureTextField*)view_for_id((int)(intptr_t)w);
    [pt setStringValue:to_ns(t)];
}

const char* aurora_gui_passwordbox_get_text(AuroraWidget w) {
    NSSecureTextField* pt = (NSSecureTextField*)view_for_id((int)(intptr_t)w);
    static std::string result;
    result = [[pt stringValue] UTF8String];
    return result.c_str();
}

/* ════════════════════════════════════════════════════════════
   Slider
   ════════════════════════════════════════════════════════════ */
@interface AuroraSliderTarget : NSObject
@end
static AuroraSliderTarget* g_slider_target = nil;
@implementation AuroraSliderTarget
- (void)sliderChanged:(id)sender {
    NSSlider* sl = (NSSlider*)sender;
    for (auto& pair : g_widget_map) {
        if ((NSView*)pair.second == (NSView*)sl) {
            fire_event(pair.first, EVENT_CHANGE, (int)[sl integerValue], 0);
            return;
        }
    }
}
@end

AuroraWidget aurora_gui_slider_new(AuroraWidget p, int x, int y, int w, int h, int min, int max) {
    @autoreleasepool {
        int wid = alloc_id();
        NSSlider* sl = [[NSSlider alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [sl setMinValue:(double)min];
        [sl setMaxValue:(double)max];
        [sl setDoubleValue:(double)((min + max) / 2)];
        [sl setTarget:sl];
        [sl setAction:@selector(sliderChanged:)];
        add_to_parent(sl, p);
        g_widget_map[wid] = sl;
        g_widget_types[wid] = WIDGET_SLIDER;
        return (AuroraWidget)(intptr_t)wid;
    }
}

int aurora_gui_slider_get_value(AuroraWidget w) {
    NSSlider* sl = (NSSlider*)view_for_id((int)(intptr_t)w);
    return (int)[sl integerValue];
}

void aurora_gui_slider_set_value(AuroraWidget w, int v) {
    NSSlider* sl = (NSSlider*)view_for_id((int)(intptr_t)w);
    [sl setIntegerValue:v];
}

void aurora_gui_slider_set_range(AuroraWidget w, int min, int max) {
    NSSlider* sl = (NSSlider*)view_for_id((int)(intptr_t)w);
    [sl setMinValue:(double)min];
    [sl setMaxValue:(double)max];
}

/* ════════════════════════════════════════════════════════════
   ProgressBar
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_progressbar_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSProgressIndicator* pi = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [pi setIndeterminate:NO];
        [pi setMinValue:0];
        [pi setMaxValue:100];
        [pi setDoubleValue:0];
        [pi setStyle:NSProgressIndicatorStyleBar];
        add_to_parent(pi, p);
        g_widget_map[wid] = pi;
        g_widget_types[wid] = WIDGET_PROGRESS;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_progressbar_set_value(AuroraWidget w, int v) {
    NSProgressIndicator* pi = (NSProgressIndicator*)view_for_id((int)(intptr_t)w);
    [pi setDoubleValue:(double)v];
}

int aurora_gui_progressbar_get_value(AuroraWidget w) {
    NSProgressIndicator* pi = (NSProgressIndicator*)view_for_id((int)(intptr_t)w);
    return (int)[pi doubleValue];
}

void aurora_gui_progressbar_set_range(AuroraWidget w, int min, int max) {
    NSProgressIndicator* pi = (NSProgressIndicator*)view_for_id((int)(intptr_t)w);
    [pi setMinValue:(double)min];
    [pi setMaxValue:(double)max];
}

void aurora_gui_progressbar_set_marquee(AuroraWidget w, int on) {
    NSProgressIndicator* pi = (NSProgressIndicator*)view_for_id((int)(intptr_t)w);
    [pi setIndeterminate:(BOOL)on];
    if (on) [pi startAnimation:nil];
    else [pi stopAnimation:nil];
}

/* ════════════════════════════════════════════════════════════
   ComboBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_combobox_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSComboBox* cb = [[NSComboBox alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        add_to_parent(cb, p);
        g_widget_map[wid] = cb;
        g_widget_types[wid] = WIDGET_COMBOBOX;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_combobox_add_item(AuroraWidget w, const char* item) {
    NSComboBox* cb = (NSComboBox*)view_for_id((int)(intptr_t)w);
    [cb addItemWithObjectValue:to_ns(item)];
}

void aurora_gui_combobox_clear(AuroraWidget w) {
    NSComboBox* cb = (NSComboBox*)view_for_id((int)(intptr_t)w);
    [cb removeAllItems];
}

int aurora_gui_combobox_get_selected(AuroraWidget w) {
    NSComboBox* cb = (NSComboBox*)view_for_id((int)(intptr_t)w);
    return (int)[cb indexOfSelectedItem];
}

void aurora_gui_combobox_set_selected(AuroraWidget w, int idx) {
    NSComboBox* cb = (NSComboBox*)view_for_id((int)(intptr_t)w);
    [cb selectItemAtIndex:idx];
}

int aurora_gui_combobox_count(AuroraWidget w) {
    NSComboBox* cb = (NSComboBox*)view_for_id((int)(intptr_t)w);
    return (int)[cb numberOfItems];
}

const char* aurora_gui_combobox_get_item(AuroraWidget w, int idx) {
    NSComboBox* cb = (NSComboBox*)view_for_id((int)(intptr_t)w);
    static std::string result;
    id obj = [cb itemObjectValueAtIndex:idx];
    result = obj ? [[obj description] UTF8String] : "";
    return result.c_str();
}

/* ════════════════════════════════════════════════════════════
   DropDown (NSPopUpButton)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_dropdown_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSPopUpButton* pb = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(x, y, w, h) pullsDown:NO];
        add_to_parent(pb, p);
        g_widget_map[wid] = pb;
        g_widget_types[wid] = WIDGET_COMBOBOX;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_dropdown_add_item(AuroraWidget w, const char* item) {
    NSPopUpButton* pb = (NSPopUpButton*)view_for_id((int)(intptr_t)w);
    [pb addItemWithTitle:to_ns(item)];
}

void aurora_gui_dropdown_clear(AuroraWidget w) {
    NSPopUpButton* pb = (NSPopUpButton*)view_for_id((int)(intptr_t)w);
    [pb removeAllItems];
}

int aurora_gui_dropdown_get_selected(AuroraWidget w) {
    NSPopUpButton* pb = (NSPopUpButton*)view_for_id((int)(intptr_t)w);
    return (int)[pb indexOfSelectedItem];
}

void aurora_gui_dropdown_set_selected(AuroraWidget w, int idx) {
    NSPopUpButton* pb = (NSPopUpButton*)view_for_id((int)(intptr_t)w);
    [pb selectItemAtIndex:idx];
}

int aurora_gui_dropdown_count(AuroraWidget w) {
    NSPopUpButton* pb = (NSPopUpButton*)view_for_id((int)(intptr_t)w);
    return (int)[pb numberOfItems];
}

const char* aurora_gui_dropdown_get_item(AuroraWidget w, int idx) {
    NSPopUpButton* pb = (NSPopUpButton*)view_for_id((int)(intptr_t)w);
    static std::string result;
    result = [[[pb itemAtIndex:idx] title] UTF8String];
    return result.c_str();
}

/* ════════════════════════════════════════════════════════════
   ListBox (NSTableView)
   ════════════════════════════════════════════════════════════ */
@interface AuroraListDataSource : NSObject <NSTableViewDataSource> {
    int _widget_id;
}
- (instancetype)initWithWidgetId:(int)wid;
@end
static std::map<int, AuroraListDataSource*> g_list_sources;

@implementation AuroraListDataSource
- (instancetype)initWithWidgetId:(int)wid {
    self = [super init];
    if (self) _widget_id = wid;
    return self;
}
- (NSInteger)numberOfRowsInTableView:(NSTableView*)tv {
    (void)tv;
    auto it = g_list_items.find(_widget_id);
    return it != g_list_items.end() ? (NSInteger)it->second.size() : 0;
}
- (id)tableView:(NSTableView*)tv objectValueForTableColumn:(NSTableColumn*)col row:(NSInteger)row {
    (void)tv; (void)col;
    auto it = g_list_items.find(_widget_id);
    if (it == g_list_items.end() || row < 0 || row >= (NSInteger)it->second.size())
        return nil;
    return [NSString stringWithUTF8String:it->second[(size_t)row].c_str()];
}
@end

AuroraWidget aurora_gui_listbox_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        NSTableView* tv = [[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
        NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:@"main"];
        [tv addTableColumn:col];
        [tv setHeaderView:nil];
        AuroraListDataSource* ds = [[AuroraListDataSource alloc] initWithWidgetId:wid];
        [tv setDataSource:ds];
        g_list_sources[wid] = ds;
        [scroll setDocumentView:tv];
        [scroll setHasVerticalScroller:YES];
        add_to_parent(scroll, p);
        g_widget_map[wid] = tv;
        g_widget_types[wid] = WIDGET_LISTBOX;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_listbox_add_item(AuroraWidget w, const char* item) {
    int wid = (int)(intptr_t)w;
    g_list_items[wid].push_back(item ? item : "");
    NSTableView* tv = (NSTableView*)view_for_id(wid);
    [tv reloadData];
}

void aurora_gui_listbox_clear(AuroraWidget w) {
    int wid = (int)(intptr_t)w;
    g_list_items[wid].clear();
    NSTableView* tv = (NSTableView*)view_for_id(wid);
    [tv reloadData];
}

int aurora_gui_listbox_get_selected(AuroraWidget w) {
    NSTableView* tv = (NSTableView*)view_for_id((int)(intptr_t)w);
    return (int)[tv selectedRow];
}

const char* aurora_gui_listbox_get_item(AuroraWidget w, int idx) {
    int wid = (int)(intptr_t)w;
    auto it = g_list_items.find(wid);
    static std::string result;
    if (it != g_list_items.end() && idx >= 0 && idx < (int)it->second.size())
        result = it->second[idx];
    else result = "";
    return result.c_str();
}

int aurora_gui_listbox_count(AuroraWidget w) {
    int wid = (int)(intptr_t)w;
    auto it = g_list_items.find(wid);
    return it != g_list_items.end() ? (int)it->second.size() : 0;
}

/* ════════════════════════════════════════════════════════════
   TreeView (NSOutlineView with data source)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_treeview_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        NSOutlineView* ov = [[NSOutlineView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
        NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:@"main"];
        [ov addTableColumn:col];
        [ov setHeaderView:nil];
        [ov setOutlineTableColumn:col];
        [scroll setDocumentView:ov];
        [scroll setHasVerticalScroller:YES];
        AuroraTreeDataSource* ds = [[AuroraTreeDataSource alloc] initWithWidgetId:wid];
        [ov setDataSource:ds];
        add_to_parent(scroll, p);
        g_widget_map[wid] = ov;
        g_widget_types[wid] = WIDGET_TREEVIEW;
        g_tree_nodes[wid] = {};
        g_tree_next_node_id[wid] = 1;
        g_tree_node_index[wid] = {};
        return (AuroraWidget)(intptr_t)wid;
    }
}

static TreeNode* tree_find_node(int wid, int node_id) {
    auto it = g_tree_node_index[wid].find(node_id);
    if (it == g_tree_node_index[wid].end()) return nullptr;
    int idx = it->second;
    if (idx < 0 || idx >= (int)g_tree_nodes[wid].size()) return nullptr;
    return &g_tree_nodes[wid][idx];
}

AuroraTreeItem aurora_gui_treeview_add_item(AuroraWidget w, const char* s, AuroraTreeItem p) {
    int wid = (int)(intptr_t)w;
    int pid = p ? *(int*)p : -1;
    int nid = g_tree_next_node_id[wid]++;
    TreeNode node;
    node.id = nid;
    node.parent_id = pid;
    node.text = s ? s : "";
    g_tree_node_index[wid][nid] = (int)g_tree_nodes[wid].size();
    g_tree_nodes[wid].push_back(node);
    NSOutlineView* ov = (NSOutlineView*)view_for_id(wid);
    if (ov) {
        [ov reloadData];
        if (pid >= 0) [ov expandItem:@(pid)];
    }
    return (AuroraTreeItem)(intptr_t)nid;
}

void aurora_gui_treeview_remove_item(AuroraWidget w, AuroraTreeItem i) {
    int wid = (int)(intptr_t)w;
    int nid = (int)(intptr_t)i;
    auto& nodes = g_tree_nodes[wid];
    auto& idx_map = g_tree_node_index[wid];
    auto it = idx_map.find(nid);
    if (it == idx_map.end()) return;
    nodes.erase(nodes.begin() + it->second);
    idx_map.clear();
    for (int k = 0; k < (int)nodes.size(); k++)
        idx_map[nodes[k].id] = k;
    NSOutlineView* ov = (NSOutlineView*)view_for_id(wid);
    if (ov) [ov reloadData];
}

void aurora_gui_treeview_clear(AuroraWidget w) {
    int wid = (int)(intptr_t)w;
    g_tree_nodes[wid].clear();
    g_tree_node_index[wid].clear();
    g_tree_next_node_id[wid] = 1;
    NSOutlineView* ov = (NSOutlineView*)view_for_id(wid);
    if (ov) [ov reloadData];
}

AuroraTreeItem aurora_gui_treeview_get_selected(AuroraWidget w) {
    int wid = (int)(intptr_t)w;
    NSOutlineView* ov = (NSOutlineView*)view_for_id(wid);
    if (!ov) return nullptr;
    id item = [ov itemAtRow:[ov selectedRow]];
    if (!item) return nullptr;
    int nid = (int)[(NSNumber*)item integerValue];
    return (AuroraTreeItem)(intptr_t)nid;
}

void aurora_gui_treeview_expand(AuroraWidget w, AuroraTreeItem i) {
    int wid = (int)(intptr_t)w;
    int nid = (int)(intptr_t)i;
    NSOutlineView* ov = (NSOutlineView*)view_for_id(wid);
    if (ov) [ov expandItem:@(nid)];
}

void aurora_gui_treeview_collapse(AuroraWidget w, AuroraTreeItem i) {
    int wid = (int)(intptr_t)w;
    int nid = (int)(intptr_t)i;
    NSOutlineView* ov = (NSOutlineView*)view_for_id(wid);
    if (ov) [ov collapseItem:@(nid)];
}

void aurora_gui_treeview_set_item_text(AuroraWidget w, AuroraTreeItem i, const char* s) {
    int wid = (int)(intptr_t)w;
    int nid = (int)(intptr_t)i;
    TreeNode* node = tree_find_node(wid, nid);
    if (node) {
        node->text = s ? s : "";
        NSOutlineView* ov = (NSOutlineView*)view_for_id(wid);
        if (ov) [ov reloadData];
    }
}

/* ════════════════════════════════════════════════════════════
   Table
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_table_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        NSTableView* tv = [[NSTableView alloc] initWithFrame:NSMakeRect(0, 0, w, h)];
        [scroll setDocumentView:tv];
        [scroll setHasVerticalScroller:YES];
        [scroll setHasHorizontalScroller:YES];
        add_to_parent(scroll, p);
        g_widget_map[wid] = tv;
        g_widget_types[wid] = WIDGET_TABLE;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_table_add_column(AuroraWidget w, const char* s, int wid_) {
    NSTableView* tv = (NSTableView*)view_for_id((int)(intptr_t)w);
    NSTableColumn* col = [[NSTableColumn alloc] initWithIdentifier:to_ns(s)];
    [[col headerCell] setStringValue:to_ns(s)];
    [col setWidth:(CGFloat)wid_];
    [tv addTableColumn:col];
}

int aurora_gui_table_column_count(AuroraWidget w) {
    NSTableView* tv = (NSTableView*)view_for_id((int)(intptr_t)w);
    return (int)[[tv tableColumns] count];
}

AuroraTableItem aurora_gui_table_add_row(AuroraWidget w) { (void)w; return nullptr; }
void aurora_gui_table_set_cell(AuroraWidget w, int r, int c, const char* s) { (void)w;(void)r;(void)c;(void)s; }
const char* aurora_gui_table_get_cell(AuroraWidget w, int r, int c) { (void)w;(void)r;(void)c; return ""; }
void aurora_gui_table_remove_row(AuroraWidget w, int r) { (void)w;(void)r; }
void aurora_gui_table_clear(AuroraWidget w) { (void)w; }
int aurora_gui_table_get_selected(AuroraWidget w) {
    NSTableView* tv = (NSTableView*)view_for_id((int)(intptr_t)w);
    return (int)[tv selectedRow];
}
int aurora_gui_table_row_count(AuroraWidget w) { (void)w; return 0; }

/* ════════════════════════════════════════════════════════════
   TabView
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_tabview_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSTabView* tv = [[NSTabView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        add_to_parent(tv, p);
        g_widget_map[wid] = tv;
        g_widget_types[wid] = WIDGET_TABVIEW;
        return (AuroraWidget)(intptr_t)wid;
    }
}

AuroraWidget aurora_gui_tabview_add_page(AuroraWidget w, const char* s) {
    NSTabView* tv = (NSTabView*)view_for_id((int)(intptr_t)w);
    NSTabViewItem* item = [[NSTabViewItem alloc] initWithIdentifier:nil];
    [item setLabel:to_ns(s)];
    [tv addTabViewItem:item];
    return (__bridge AuroraWidget)[item view];
}

int aurora_gui_tabview_get_selected(AuroraWidget w) {
    NSTabView* tv = (NSTabView*)view_for_id((int)(intptr_t)w);
    return (int)[tv indexOfTabViewItem:[tv selectedTabViewItem]];
}

void aurora_gui_tabview_set_selected(AuroraWidget w, int i) {
    NSTabView* tv = (NSTabView*)view_for_id((int)(intptr_t)w);
    if (i >= 0 && i < (int)[tv numberOfTabViewItems])
        [tv selectTabViewItemAtIndex:i];
}

int aurora_gui_tabview_page_count(AuroraWidget w) {
    NSTabView* tv = (NSTabView*)view_for_id((int)(intptr_t)w);
    return (int)[tv numberOfTabViewItems];
}

/* ════════════════════════════════════════════════════════════
   ScrollView
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_scrollview_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSScrollView* sv = [[NSScrollView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [sv setHasVerticalScroller:YES];
        [sv setHasHorizontalScroller:YES];
        [sv setAutohidesScrollers:YES];
        add_to_parent(sv, p);
        g_widget_map[wid] = sv;
        g_widget_types[wid] = WIDGET_SCROLL;
        return (AuroraWidget)(intptr_t)wid;
    }
}

/* ════════════════════════════════════════════════════════════
   SplitView
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_splitview_new(AuroraWidget p, int x, int y, int w, int h, int o) {
    @autoreleasepool {
        int wid = alloc_id();
        NSSplitView* sv = [[NSSplitView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [sv setVertical:(BOOL)o];
        add_to_parent(sv, p);
        g_widget_map[wid] = sv;
        g_widget_types[wid] = WIDGET_SPLIT;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_splitview_set_position(AuroraWidget w, int p) {
    NSSplitView* sv = (NSSplitView*)view_for_id((int)(intptr_t)w);
    NSArray* subviews = [sv subviews];
    if ([subviews count] > 0) {
        NSView* first = [subviews objectAtIndex:0];
        NSRect r = [first frame];
        if ([sv isVertical]) r.size.width = (CGFloat)p;
        else r.size.height = (CGFloat)p;
        [first setFrame:r];
    }
}

int aurora_gui_splitview_get_position(AuroraWidget w) {
    NSSplitView* sv = (NSSplitView*)view_for_id((int)(intptr_t)w);
    NSArray* subviews = [sv subviews];
    if ([subviews count] > 0) {
        NSView* first = [subviews objectAtIndex:0];
        NSRect r = [first frame];
        return (int)([sv isVertical] ? r.size.width : r.size.height);
    }
    return 0;
}

AuroraWidget aurora_gui_splitview_get_pane1(AuroraWidget w) { (void)w; return nullptr; }
AuroraWidget aurora_gui_splitview_get_pane2(AuroraWidget w) { (void)w; return nullptr; }

/* ════════════════════════════════════════════════════════════
   GroupBox
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_groupbox_new(AuroraWidget p, const char* s, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSBox* box = [[NSBox alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [box setTitle:to_ns(s)];
        [box setBoxType:NSBoxPrimary];
        [box setBorderType:NSLineBorder];
        add_to_parent(box, p);
        g_widget_map[wid] = box;
        g_widget_types[wid] = WIDGET_GROUPBOX;
        return (AuroraWidget)(intptr_t)wid;
    }
}

/* ════════════════════════════════════════════════════════════
   Image
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_image_new(AuroraWidget p, const char* path, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSImageView* iv = [[NSImageView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        if (path) [iv setImage:[[NSImage alloc] initWithContentsOfFile:to_ns(path)]];
        [iv setImageScaling:NSImageScaleProportionallyUpOrDown];
        add_to_parent(iv, p);
        g_widget_map[wid] = iv;
        g_widget_types[wid] = WIDGET_IMAGE;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_image_load(AuroraWidget w, const char* s) {
    NSImageView* iv = (NSImageView*)view_for_id((int)(intptr_t)w);
    if (s) [iv setImage:[[NSImage alloc] initWithContentsOfFile:to_ns(s)]];
}

void aurora_gui_image_set_data(AuroraWidget w, const unsigned char* d, int l) { (void)w;(void)d;(void)l; }

/* ════════════════════════════════════════════════════════════
   Canvas
   ════════════════════════════════════════════════════════════ */
@interface AuroraCanvasView : NSView
@property (assign) int widget_id;
@end
@implementation AuroraCanvasView
- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    int wid = self.widget_id;
    auto it = g_callbacks.find(wid);
    if (it != g_callbacks.end() && it->second)
        it->second(wid, 0, 0, 0);
}
@end

AuroraWidget aurora_gui_canvas_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        AuroraCanvasView* cv = [[AuroraCanvasView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [cv setWidget_id:wid];
        add_to_parent(cv, p);
        g_widget_map[wid] = cv;
        g_widget_types[wid] = WIDGET_CANVAS;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_canvas_set_paint_callback(AuroraWidget w, AuroraPaintCallback cb, void* u) { (void)w;(void)cb;(void)u; }

void aurora_gui_canvas_repaint(AuroraWidget w) {
    AuroraCanvasView* cv = (AuroraCanvasView*)view_for_id((int)(intptr_t)w);
    [cv setNeedsDisplay:YES];
}

/* ════════════════════════════════════════════════════════════
   Menu
   ════════════════════════════════════════════════════════════ */
AuroraMenu aurora_gui_menu_bar_new(AuroraWidget p) {
    @autoreleasepool {
        (void)p;
        NSMenu* m = [[NSMenu alloc] initWithTitle:@""];
        [NSApp setMainMenu:m];
        return (__bridge AuroraMenu)m;
    }
}

AuroraMenu aurora_gui_menu_new(const char* s) {
    @autoreleasepool {
        return (__bridge AuroraMenu)[[NSMenu alloc] initWithTitle:to_ns(s)];
    }
}

void aurora_gui_menu_add_item(AuroraMenu m, const char* s, int i) {
    @autoreleasepool {
        NSMenu* menu = (__bridge NSMenu*)m;
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:to_ns(s) action:nil keyEquivalent:@""];
        [item setTag:i];
        [menu addItem:item];
    }
}

void aurora_gui_menu_add_separator(AuroraMenu m) {
    @autoreleasepool {
        NSMenu* menu = (__bridge NSMenu*)m;
        [menu addItem:[NSMenuItem separatorItem]];
    }
}

void aurora_gui_menu_add_submenu(AuroraMenu m, AuroraMenu s) {
    @autoreleasepool {
        NSMenu* parent = (__bridge NSMenu*)m;
        NSMenu* sub = (__bridge NSMenu*)s;
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[sub title] action:nil keyEquivalent:@""];
        [parent addItem:item];
        [parent setSubmenu:sub forItem:item];
    }
}

void aurora_gui_menu_bar_add_menu(AuroraMenu m, AuroraMenu s) {
    @autoreleasepool {
        NSMenu* bar = (__bridge NSMenu*)m;
        NSMenu* menu = (__bridge NSMenu*)s;
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[menu title] action:nil keyEquivalent:@""];
        [bar addItem:item];
        [bar setSubmenu:menu forItem:item];
    }
}

void aurora_gui_menu_set_checked(AuroraMenu m, int i, int c) {
    @autoreleasepool {
        NSMenu* menu = (__bridge NSMenu*)m;
        NSMenuItem* item = [menu itemWithTag:i];
        [item setState:(c ? NSControlStateValueOn : NSControlStateValueOff)];
    }
}

void aurora_gui_menu_set_enabled(AuroraMenu m, int i, int e) {
    @autoreleasepool {
        NSMenu* menu = (__bridge NSMenu*)m;
        NSMenuItem* item = [menu itemWithTag:i];
        [item setEnabled:(BOOL)e];
    }
}

/* ════════════════════════════════════════════════════════════
   ToolBar
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_toolbar_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSToolbar* tb = [[NSToolbar alloc] initWithIdentifier:@"toolbar"];
        NSView* container = [[NSView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        if (p) {
            NSWindow* win = (NSWindow*)view_for_id((int)(intptr_t)p);
            [win setToolbar:tb];
        }
        g_widget_map[wid] = container;
        g_widget_types[wid] = WIDGET_TOOLBAR;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_toolbar_add_button(AuroraWidget w, const char* s, int i) { (void)w;(void)s;(void)i; }
void aurora_gui_toolbar_add_separator(AuroraWidget w) { (void)w; }

/* ════════════════════════════════════════════════════════════
   StatusBar
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_statusbar_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSTextField* sb = [[NSTextField alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        [sb setBezeled:NO];
        [sb setDrawsBackground:NO];
        [sb setEditable:NO];
        [sb setSelectable:NO];
        add_to_parent(sb, p);
        g_widget_map[wid] = sb;
        g_widget_types[wid] = WIDGET_STATUSBAR;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_statusbar_set_text(AuroraWidget w, const char* t) {
    NSTextField* sb = (NSTextField*)view_for_id((int)(intptr_t)w);
    [sb setStringValue:to_ns(t)];
}

const char* aurora_gui_statusbar_get_text(AuroraWidget w) {
    NSTextField* sb = (NSTextField*)view_for_id((int)(intptr_t)w);
    static std::string result;
    result = [[sb stringValue] UTF8String];
    return result.c_str();
}

void aurora_gui_statusbar_set_parts(AuroraWidget w, const int* widths, int c) { (void)w;(void)widths;(void)c; }

/* ════════════════════════════════════════════════════════════
   Dialog
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_dialog_new(AuroraWidget p, const char* s, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        NSWindow* dlg = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, w, h)
            styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable
            backing:NSBackingStoreBuffered defer:NO];
        [dlg setTitle:to_ns(s)];
        g_widget_map[wid] = (NSView*)dlg;
        g_widget_types[wid] = WIDGET_WINDOW;
        return (AuroraWidget)(intptr_t)wid;
    }
}

int aurora_gui_dialog_show_modal(AuroraWidget w) {
    NSWindow* dlg = (NSWindow*)view_for_id((int)(intptr_t)w);
    [NSApp runModalForWindow:dlg];
    return 0;
}

void aurora_gui_dialog_close(AuroraWidget w) {
    NSWindow* dlg = (NSWindow*)view_for_id((int)(intptr_t)w);
    [NSApp stopModal];
    [dlg close];
}

/* ════════════════════════════════════════════════════════════
   MessageBox
   ════════════════════════════════════════════════════════════ */
int aurora_gui_messagebox_show(AuroraWidget p, const char* title, const char* message, int type) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setMessageText:to_ns(title)];
        [alert setInformativeText:to_ns(message)];
        [alert addButtonWithTitle:@"OK"];
        if (type == 1) [alert addButtonWithTitle:@"Cancel"];
        NSModalResponse resp = [alert runModal];
        return resp == NSAlertFirstButtonReturn ? 0 : 1;
    }
}

/* ════════════════════════════════════════════════════════════
   FilePicker
   ════════════════════════════════════════════════════════════ */
const char* aurora_gui_file_open_dialog(AuroraWidget p, const char* t, const char* f) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setTitle:to_ns(t)];
        [panel setAllowsMultipleSelection:NO];
        if (f) [panel setAllowedFileTypes:@[to_ns(f)]];
        if ([panel runModal] == NSModalResponseOK) {
            static std::string result;
            result = [[[panel URL] path] UTF8String];
            return result.c_str();
        }
        return nullptr;
    }
}

const char* aurora_gui_file_save_dialog(AuroraWidget p, const char* t, const char* f) {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        [panel setTitle:to_ns(t)];
        if (f) [panel setAllowedFileTypes:@[to_ns(f)]];
        if ([panel runModal] == NSModalResponseOK) {
            static std::string result;
            result = [[[panel URL] path] UTF8String];
            return result.c_str();
        }
        return nullptr;
    }
}

const char* aurora_gui_folder_select_dialog(AuroraWidget p, const char* t) {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setTitle:to_ns(t)];
        [panel setCanChooseDirectories:YES];
        [panel setCanChooseFiles:NO];
        if ([panel runModal] == NSModalResponseOK) {
            static std::string result;
            result = [[[panel URL] path] UTF8String];
            return result.c_str();
        }
        return nullptr;
    }
}

/* ════════════════════════════════════════════════════════════
   ColorPicker, FontPicker
   ════════════════════════════════════════════════════════════ */
int aurora_gui_color_picker_dialog(AuroraWidget p, unsigned int c) {
    @autoreleasepool {
        NSColorPanel* panel = [NSColorPanel sharedColorPanel];
        [panel setColor:uint_to_ns_color(c)];
        [NSApp runModalForWindow:panel];
        return (int)ns_color_to_uint([panel color]);
    }
}

int aurora_gui_font_picker_dialog(AuroraWidget p, AuroraFontInfo* f) { (void)p;(void)f; return 0; }

/* ════════════════════════════════════════════════════════════
   Notification
   ════════════════════════════════════════════════════════════ */
int aurora_gui_notification_show(AuroraWidget p, const char* t, const char* m, int i) {
    @autoreleasepool {
        NSUserNotification* note = [[NSUserNotification alloc] init];
        [note setTitle:to_ns(t)];
        [note setInformativeText:to_ns(m)];
        [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:note];
        return 0;
    }
}

void aurora_gui_notification_remove(AuroraWidget p) { (void)p; }

/* ════════════════════════════════════════════════════════════
   Clipboard
   ════════════════════════════════════════════════════════════ */
int aurora_gui_clipboard_set_text(const char* s) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        [pb clearContents];
        [pb setString:to_ns(s) forType:NSPasteboardTypeString];
        return 0;
    }
}

const char* aurora_gui_clipboard_get_text(void) {
    @autoreleasepool {
        NSPasteboard* pb = [NSPasteboard generalPasteboard];
        static std::string result;
        result = [[pb stringForType:NSPasteboardTypeString] UTF8String];
        return result.c_str();
    }
}

/* ════════════════════════════════════════════════════════════
   Cursor
   ════════════════════════════════════════════════════════════ */
void aurora_gui_cursor_set(int c) { (void)c; }
int aurora_gui_cursor_get(void) { return 0; }

/* ════════════════════════════════════════════════════════════
   Keyboard
   ════════════════════════════════════════════════════════════ */
int aurora_gui_keyboard_is_key_down(int k) { (void)k; return 0; }
int aurora_gui_keyboard_get_modifiers(void) { return 0; }

/* ════════════════════════════════════════════════════════════
   Mouse
   ════════════════════════════════════════════════════════════ */
int aurora_gui_mouse_get_x(void) { return 0; }
int aurora_gui_mouse_get_y(void) { return 0; }
int aurora_gui_mouse_button_down(int b) { (void)b; return 0; }
void aurora_gui_mouse_set_pos(int x, int y) { (void)x;(void)y; }

/* ════════════════════════════════════════════════════════════
   WebView (WKWebView)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_webview_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        WKWebView* wv = [[WKWebView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        add_to_parent(wv, p);
        g_widget_map[wid] = wv;
        g_widget_types[wid] = WIDGET_WEBVIEW;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_webview_navigate(AuroraWidget wv, const char* url) {
    WKWebView* wv_ = (WKWebView*)view_for_id((int)(intptr_t)wv);
    [wv_ loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:to_ns(url)]]];
}

void aurora_gui_webview_go_back(AuroraWidget wv) {
    WKWebView* wv_ = (WKWebView*)view_for_id((int)(intptr_t)wv);
    [wv_ goBack];
}

void aurora_gui_webview_go_forward(AuroraWidget wv) {
    WKWebView* wv_ = (WKWebView*)view_for_id((int)(intptr_t)wv);
    [wv_ goForward];
}

void aurora_gui_webview_reload(AuroraWidget wv) {
    WKWebView* wv_ = (WKWebView*)view_for_id((int)(intptr_t)wv);
    [wv_ reload];
}

void aurora_gui_webview_set_on_title(AuroraWidget wv, AuroraEventCallback cb) {
    int wid = (int)(intptr_t)wv;
    set_callback(wid, cb);
    WKWebView* wv_ = (WKWebView*)view_for_id(wid);
    if (wv_ && cb) {
        AuroraWebViewDelegate* d = [[AuroraWebViewDelegate alloc] initWithWidgetId:wid];
        [wv_ setNavigationDelegate:d];
        [wv_ addObserver:d forKeyPath:@"title" options:NSKeyValueObservingOptionNew context:nullptr];
    }
}

void aurora_gui_webview_set_on_navigate(AuroraWidget wv, AuroraEventCallback cb) {
    int wid = (int)(intptr_t)wv;
    set_callback(wid, cb);
    WKWebView* wv_ = (WKWebView*)view_for_id(wid);
    if (wv_ && cb) {
        AuroraWebViewDelegate* d = [[AuroraWebViewDelegate alloc] initWithWidgetId:wid];
        [wv_ setNavigationDelegate:d];
        [wv_ addObserver:d forKeyPath:@"title" options:NSKeyValueObservingOptionNew context:nullptr];
    }
}

/* ════════════════════════════════════════════════════════════
   Media Player (AVPlayerView)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_media_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        AVPlayerView* pv = [[AVPlayerView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        add_to_parent(pv, p);
        g_widget_map[wid] = pv;
        g_widget_types[wid] = WIDGET_MEDIA;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_media_play(AuroraWidget m) {
    AVPlayerView* pv = (AVPlayerView*)view_for_id((int)(intptr_t)m);
    [[pv player] play];
}

void aurora_gui_media_pause(AuroraWidget m) {
    AVPlayerView* pv = (AVPlayerView*)view_for_id((int)(intptr_t)m);
    [[pv player] pause];
}

void aurora_gui_media_stop(AuroraWidget m) {
    AVPlayerView* pv = (AVPlayerView*)view_for_id((int)(intptr_t)m);
    [[pv player] seekToTime:kCMTimeZero];
    [[pv player] pause];
}

void aurora_gui_media_load(AuroraWidget m, const char* src) {
    AVPlayerView* pv = (AVPlayerView*)view_for_id((int)(intptr_t)m);
    AVPlayer* player = [AVPlayer playerWithURL:[NSURL URLWithString:to_ns(src)]];
    [pv setPlayer:player];
}

/* ════════════════════════════════════════════════════════════
   Map (WKWebView with Leaflet/OpenStreetMap)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_map_new(AuroraWidget p, int x, int y, int w, int h) {
    @autoreleasepool {
        int wid = alloc_id();
        WKWebView* mv = [[WKWebView alloc] initWithFrame:NSMakeRect(x, y, w, h)];
        add_to_parent(mv, p);
        g_widget_map[wid] = mv;
        g_widget_types[wid] = WIDGET_MAP;
        return (AuroraWidget)(intptr_t)wid;
    }
}

void aurora_gui_map_set_center(AuroraWidget m, double lat, double lon) { (void)m;(void)lat;(void)lon; }
void aurora_gui_map_set_zoom(AuroraWidget m, int z) { (void)m;(void)z; }
void aurora_gui_map_add_marker(AuroraWidget m, double lat, double lon, const char* l) { (void)m;(void)lat;(void)lon;(void)l; }

/* ════════════════════════════════════════════════════════════
   Widget introspection
   ════════════════════════════════════════════════════════════ */
int aurora_gui_widget_get_type(void* widget) {
    int wid = (int)(intptr_t)widget;
    auto it = g_widget_types.find(wid);
    return it != g_widget_types.end() ? it->second : 0;
}

void* aurora_gui_widget_get_parent(void* widget) { (void)widget; return nullptr; }

const char* aurora_gui_widget_get_text(void* widget) {
    static std::string result;
    int wid = (int)(intptr_t)widget;
    auto it = g_widget_texts.find(wid);
    result = it != g_widget_texts.end() ? it->second : "";
    return result.c_str();
}

void aurora_gui_widget_get_bounds(void* widget, int* x, int* y, int* w, int* h) {
    NSView* view = view_for_id((int)(intptr_t)widget);
    if (view) {
        NSRect r = [view frame];
        if (x) *x = (int)r.origin.x;
        if (y) *y = (int)r.origin.y;
        if (w) *w = (int)r.size.width;
        if (h) *h = (int)r.size.height;
    }
}

int aurora_gui_widget_get_id(void* widget) { return (int)(intptr_t)widget; }

void* aurora_gui_widget_find_at(int x, int y) {
    for (auto& pair : g_widget_map) {
        NSView* view = pair.second;
        if (view && !view.hidden && view.window) {
            NSRect fr = [view frame];
            if (NSPointInRect(NSMakePoint(x, y), fr))
                return (void*)(intptr_t)pair.first;
        }
    }
    return nullptr;
}

int aurora_gui_widget_count(void) { return (int)g_widget_map.size(); }

void* aurora_gui_widget_get_by_index(int idx) {
    if (idx < 0 || idx >= (int)g_widget_map.size()) return nullptr;
    auto it = g_widget_map.begin();
    std::advance(it, idx);
    return (void*)(intptr_t)it->first;
}

/* ════════════════════════════════════════════════════════════
   Legacy aliases
   ════════════════════════════════════════════════════════════ */
void aurora_gui_run() { aurora_gui_app_run(); }
void aurora_gui_quit() { aurora_gui_app_quit(); }
void aurora_gui_layout_horizontal(AuroraWidget p, int m) { (void)p;(void)m; }
void aurora_gui_layout_vertical(AuroraWidget p, int m) { (void)p;(void)m; }

/* ════════════════════════════════════════════════════════════
   Layout stubs (handled by app_layout.cpp)
   ════════════════════════════════════════════════════════════ */
AuroraWidget aurora_gui_row_new(AuroraWidget p,int x,int y,int w,int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_column_new(AuroraWidget p,int x,int y,int w,int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_stack_new(AuroraWidget p,int x,int y,int w,int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_grid_new(AuroraWidget p,int x,int y,int w,int h,int c) { (void)p;(void)x;(void)y;(void)w;(void)h;(void)c; return nullptr; }
AuroraWidget aurora_gui_wrap_new(AuroraWidget p,int x,int y,int w,int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_flow_new(AuroraWidget p,int x,int y,int w,int h) { (void)p;(void)x;(void)y;(void)w;(void)h; return nullptr; }
void aurora_gui_layout_add_child(AuroraWidget l,AuroraWidget c) { (void)l;(void)c; }
void aurora_gui_layout_remove_child(AuroraWidget l,int i) { (void)l;(void)i; }
void aurora_gui_layout_clear(AuroraWidget l) { (void)l; }
int aurora_gui_layout_child_count(AuroraWidget l) { (void)l; return 0; }
void aurora_gui_layout_recalc(AuroraWidget l) { (void)l; }
void aurora_gui_layout_set_main_align(AuroraWidget l,int a) { (void)l;(void)a; }
void aurora_gui_layout_set_cross_align(AuroraWidget l,int a) { (void)l;(void)a; }
void aurora_gui_layout_set_spacing(AuroraWidget l,int s) { (void)l;(void)s; }
void aurora_gui_layout_child_set_flex(AuroraWidget l,AuroraWidget c,int f) { (void)l;(void)c;(void)f; }
void aurora_gui_layout_child_set_fit(AuroraWidget l,AuroraWidget c,int f) { (void)l;(void)c;(void)f; }
void aurora_gui_grid_set_child_pos(AuroraWidget g,AuroraWidget c,int col,int r,int cs,int rs) { (void)g;(void)c;(void)col;(void)r;(void)cs;(void)rs; }
AuroraWidget aurora_gui_spacer_new(AuroraWidget p,int f) { (void)p;(void)f; return nullptr; }
AuroraWidget aurora_gui_padding_new(AuroraWidget p,AuroraWidget c,int l,int t,int r,int b) { (void)p;(void)c;(void)l;(void)t;(void)r;(void)b; return nullptr; }
AuroraWidget aurora_gui_margin_new(AuroraWidget p,AuroraWidget c,int l,int t,int r,int b) { (void)p;(void)c;(void)l;(void)t;(void)r;(void)b; return nullptr; }
AuroraWidget aurora_gui_center_new(AuroraWidget p,AuroraWidget c) { (void)p;(void)c; return nullptr; }
AuroraWidget aurora_gui_align_new(AuroraWidget p,AuroraWidget c,int ax,int ay) { (void)p;(void)c;(void)ax;(void)ay; return nullptr; }
AuroraWidget aurora_gui_expand_new(AuroraWidget p,AuroraWidget c,int f) { (void)p;(void)c;(void)f; return nullptr; }
AuroraWidget aurora_gui_flexible_new(AuroraWidget p,AuroraWidget c,int f) { (void)p;(void)c;(void)f; return nullptr; }
AuroraWidget aurora_gui_container_new(AuroraWidget p,AuroraWidget c) { (void)p;(void)c; return nullptr; }
void aurora_gui_container_set_padding(AuroraWidget c,int l,int t,int r,int b) { (void)c;(void)l;(void)t;(void)r;(void)b; }
void aurora_gui_container_set_margin(AuroraWidget c,int l,int t,int r,int b) { (void)c;(void)l;(void)t;(void)r;(void)b; }
void aurora_gui_container_set_bg(AuroraWidget c,unsigned int clr) { (void)c;(void)clr; }
AuroraWidget aurora_gui_divider_new(AuroraWidget p,int o,int t,int x,int y,int w,int h) { (void)p;(void)o;(void)t;(void)x;(void)y;(void)w;(void)h; return nullptr; }
AuroraWidget aurora_gui_aspect_ratio_new(AuroraWidget p,AuroraWidget c,float r) { (void)p;(void)c;(void)r; return nullptr; }

} /* extern "C" */

/* ── TreeView Data Source ── */
@implementation AuroraTreeDataSource
- (instancetype)initWithWidgetId:(int)wid {
    self = [super init];
    if (self) widget_id = wid;
    return self;
}
- (NSInteger)outlineView:(NSOutlineView*)ov numberOfChildrenOfItem:(id)item {
    int pid = item ? (int)[(NSNumber*)item integerValue] : -1;
    int cnt = 0;
    auto& nodes = g_tree_nodes[widget_id];
    for (auto& n : nodes)
        if (n.parent_id == pid) cnt++;
    return cnt;
}
- (id)outlineView:(NSOutlineView*)ov child:(NSInteger)index ofItem:(id)item {
    int pid = item ? (int)[(NSNumber*)item integerValue] : -1;
    int cnt = 0;
    auto& nodes = g_tree_nodes[widget_id];
    for (auto& n : nodes) {
        if (n.parent_id == pid) {
            if (cnt == index) return @(n.id);
            cnt++;
        }
    }
    return nil;
}
- (BOOL)outlineView:(NSOutlineView*)ov isItemExpandable:(id)item {
    int nid = (int)[(NSNumber*)item integerValue];
    auto& nodes = g_tree_nodes[widget_id];
    for (auto& n : nodes)
        if (n.parent_id == nid) return YES;
    return NO;
}
- (id)outlineView:(NSOutlineView*)ov objectValueForTableColumn:(NSTableColumn*)col byItem:(id)item {
    int nid = (int)[(NSNumber*)item integerValue];
    auto& nodes = g_tree_nodes[widget_id];
    for (auto& n : nodes)
        if (n.id == nid) return [NSString stringWithUTF8String:n.text.c_str()];
    return @"";
}
@end

/* ── WebView Delegate ── */
@implementation AuroraWebViewDelegate
- (instancetype)initWithWidgetId:(int)wid {
    self = [super init];
    if (self) widget_id = wid;
    return self;
}
- (void)webView:(WKWebView*)wv decidePolicyForNavigationAction:(WKNavigationAction*)nav decisionHandler:(void(^)(WKNavigationActionPolicy))h {
    fire_event(widget_id, EVENT_CLICK, 0, 0);
    h(WKNavigationActionPolicyAllow);
}
- (void)observeValueForKeyPath:(NSString*)kp ofObject:(id)obj change:(NSDictionary*)ch context:(void*)ctx {
    if ([kp isEqualToString:@"title"]) {
        WKWebView* wv = (WKWebView*)obj;
        g_widget_texts[widget_id] = [[wv title] UTF8String];
        fire_event(widget_id, EVENT_CHANGE, 0, 0);
    }
}
@end
