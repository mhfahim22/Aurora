/* ════════════════════════════════════════════════════════════
   ios_runtime.mm — iOS UIKit bridge implementation
   Compile as Objective-C++ on Apple platforms.
   ════════════════════════════════════════════════════════════ */

#include "mobile/ios.hpp"
#import <UIKit/UIKit.h>
#import <cstdio>
#import <cstdlib>
#import <cstring>
#import <string>
#import <vector>

/* ── Global state ── */
static UIViewController* g_view_controller = nil;
static float g_screen_w = 0;
static float g_screen_h = 0;
static float g_scale = 1.0f;
static int g_orientation = 2; /* auto */

/* Touch state */
static std::vector<AuroraIOSTouch> g_touches;

/* ── UIKit helper ── */
static void update_screen_size() {
    CGRect bounds = [UIScreen mainScreen].bounds;
    CGFloat scale = [UIScreen mainScreen].scale;
    g_scale = (float)scale;
    UIInterfaceOrientation orient = [UIApplication sharedApplication].statusBarOrientation;
    if (orient == UIInterfaceOrientationLandscapeLeft ||
        orient == UIInterfaceOrientationLandscapeRight) {
        g_screen_w = (float)bounds.size.height;
        g_screen_h = (float)bounds.size.width;
    } else {
        g_screen_w = (float)bounds.size.width;
        g_screen_h = (float)bounds.size.height;
    }
}

/* ════════════════════════════════════════════════════════════
   AuroraViewController — handles touch & rotation
   ════════════════════════════════════════════════════════════ */

@interface AuroraViewController : UIViewController
@end

@implementation AuroraViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.multipleTouchEnabled = YES;
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    update_screen_size();
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    for (UITouch* t in touches) {
        CGPoint pt = [t locationInView:self.view];
        AuroraIOSTouch at;
        at.phase = 0;
        at.tap_count = (int)t.tapCount;
        at.x = (float)pt.x; at.y = (float)pt.y;
        at.prev_x = at.x; at.prev_y = at.y;
        at.timestamp = t.timestamp;
        g_touches.push_back(at);
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    for (UITouch* t in touches) {
        CGPoint pt = [t locationInView:self.view];
        CGPoint prev = [t previousLocationInView:self.view];
        AuroraIOSTouch at;
        at.phase = 1;
        at.tap_count = (int)t.tapCount;
        at.x = (float)pt.x; at.y = (float)pt.y;
        at.prev_x = (float)prev.x; at.prev_y = (float)prev.y;
        at.timestamp = t.timestamp;
        g_touches.push_back(at);
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    for (UITouch* t in touches) {
        CGPoint pt = [t locationInView:self.view];
        AuroraIOSTouch at;
        at.phase = 3;
        at.tap_count = (int)t.tapCount;
        at.x = (float)pt.x; at.y = (float)pt.y;
        at.prev_x = at.x; at.prev_y = at.y;
        at.timestamp = t.timestamp;
        g_touches.push_back(at);
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    (void)event;
    for (UITouch* t in touches) {
        CGPoint pt = [t locationInView:self.view];
        AuroraIOSTouch at;
        at.phase = 4;
        at.tap_count = (int)t.tapCount;
        at.x = (float)pt.x; at.y = (float)pt.y;
        at.prev_x = at.x; at.prev_y = at.y;
        at.timestamp = t.timestamp;
        g_touches.push_back(at);
    }
}

- (BOOL)prefersStatusBarHidden { return NO; }
- (BOOL)shouldAutorotate { return YES; }

@end

/* ════════════════════════════════════════════════════════════
   Public API
   ════════════════════════════════════════════════════════════ */

extern "C" {

int aurora_ios_init() {
    @autoreleasepool {
        update_screen_size();
        printf("[ios] runtime initialized: %.0fx%.0f scale=%.1f\n",
               g_screen_w, g_screen_h, g_scale);
    }
    return 0;
}

void* aurora_ios_get_view_controller() {
    return (__bridge void*)g_view_controller;
}

void aurora_ios_set_view_controller(void* vc) {
    g_view_controller = (__bridge UIViewController*)vc;
}

float aurora_ios_screen_width() { return g_screen_w; }
float aurora_ios_screen_height() { return g_screen_h; }
float aurora_ios_screen_scale() { return g_scale; }

int aurora_ios_orientation() { return g_orientation; }

void aurora_ios_set_orientation(int orient) {
    g_orientation = orient;
    /* Orientation is typically set via Info.plist supported orientations */
}

/* ════════════════════════════════════════════════════════════
   Touch
   ════════════════════════════════════════════════════════════ */

int aurora_ios_touch_count() {
    return (int)g_touches.size();
}

int aurora_ios_touch_get(int index, AuroraIOSTouch* out) {
    if (index < 0 || index >= (int)g_touches.size() || !out)
        return -1;
    *out = g_touches[index];
    return 0;
}

/* ════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════ */

void aurora_ios_on_did_finish_launching() {
    printf("[ios] lifecycle: didFinishLaunching\n");
}

void aurora_ios_on_will_resign_active() {
    printf("[ios] lifecycle: willResignActive\n");
}

void aurora_ios_on_did_enter_background() {
    printf("[ios] lifecycle: didEnterBackground\n");
}

void aurora_ios_on_will_enter_foreground() {
    printf("[ios] lifecycle: willEnterForeground\n");
}

void aurora_ios_on_did_become_active() {
    printf("[ios] lifecycle: didBecomeActive\n");
}

void aurora_ios_on_will_terminate() {
    printf("[ios] lifecycle: willTerminate\n");
}

void aurora_ios_on_memory_warning() {
    printf("[ios] lifecycle: memoryWarning\n");
}

/* ════════════════════════════════════════════════════════════
   Bundle / Resources
   ════════════════════════════════════════════════════════════ */

const char* aurora_ios_path_for_resource(const char* name, const char* type) {
    static std::string path;
    NSString* nsname = [NSString stringWithUTF8String:name ?: ""];
    NSString* nstype = [NSString stringWithUTF8String:type ?: ""];
    NSString* result = [[NSBundle mainBundle] pathForResource:nsname ofType:nstype];
    if (result) {
        path = [result UTF8String];
    }
    return path.c_str();
}

const char* aurora_ios_documents_path() {
    static std::string path;
    NSArray* dirs = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory, NSUserDomainMask, YES);
    if ([dirs count] > 0) {
        path = [[dirs firstObject] UTF8String];
    }
    return path.c_str();
}

const char* aurora_ios_cache_path() {
    static std::string path;
    NSArray* dirs = NSSearchPathForDirectoriesInDomains(
        NSCachesDirectory, NSUserDomainMask, YES);
    if ([dirs count] > 0) {
        path = [[dirs firstObject] UTF8String];
    }
    return path.c_str();
}

/* ════════════════════════════════════════════════════════════
   Device Info
   ════════════════════════════════════════════════════════════ */

const char* aurora_ios_device_model() {
    static std::string model;
    UIDevice* dev = [UIDevice currentDevice];
    model = [[dev model] UTF8String];
    return model.c_str();
}

const char* aurora_ios_os_version() {
    static std::string version;
    UIDevice* dev = [UIDevice currentDevice];
    version = [[dev systemVersion] UTF8String];
    return version.c_str();
}

int aurora_ios_is_ipad() {
    return (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad) ? 1 : 0;
}

/* ════════════════════════════════════════════════════════════
   Haptics
   ════════════════════════════════════════════════════════════ */

void aurora_ios_haptic(int type) {
    UIImpactFeedbackStyle style = UIImpactFeedbackStyleLight;
    switch (type) {
        case 1: style = UIImpactFeedbackStyleMedium; break;
        case 2: style = UIImpactFeedbackStyleHeavy; break;
        case 3: {
            UISelectionFeedbackGenerator* gen = [[UISelectionFeedbackGenerator alloc] init];
            [gen prepare];
            [gen selectionChanged];
            return;
        }
        default: break;
    }
    UIImpactFeedbackGenerator* gen = [[UIImpactFeedbackGenerator alloc] initWithStyle:style];
    [gen prepare];
    [gen impactOccurred];
}

} /* extern "C" */
