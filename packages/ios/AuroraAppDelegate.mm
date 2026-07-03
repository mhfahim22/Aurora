/* ════════════════════════════════════════════════════════════
   Aurora iOS App — AppDelegate implementation
   ════════════════════════════════════════════════════════════ */

#import "AuroraAppDelegate.h"
#import "AuroraViewController.h"

#include "mobile/ios.hpp"

@implementation AuroraAppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    (void)application; (void)launchOptions;

    aurora_ios_init();

    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    self.viewController = [[AuroraViewController alloc] init];
    self.window.rootViewController = self.viewController;
    [self.window makeKeyAndVisible];

    aurora_ios_set_view_controller((__bridge void*)self.viewController);
    aurora_ios_on_did_finish_launching();

    return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
    (void)application;
    aurora_ios_on_will_resign_active();
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
    (void)application;
    aurora_ios_on_did_enter_background();
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
    (void)application;
    aurora_ios_on_will_enter_foreground();
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
    (void)application;
    aurora_ios_on_did_become_active();
}

- (void)applicationWillTerminate:(UIApplication*)application {
    (void)application;
    aurora_ios_on_will_terminate();
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
    (void)application;
    aurora_ios_on_memory_warning();
}

@end
