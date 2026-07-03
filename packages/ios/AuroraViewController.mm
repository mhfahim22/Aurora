/* ════════════════════════════════════════════════════════════
   Aurora iOS App — ViewController implementation
   ════════════════════════════════════════════════════════════ */

#import "AuroraViewController.h"

#include "mobile/ios.hpp"

@interface AuroraViewController () <MTKViewDelegate>
@end

@implementation AuroraViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    /* Create Metal view */
    self.metalView = [[MTKView alloc] initWithFrame:self.view.bounds];
    self.metalView.device = MTLCreateSystemDefaultDevice();
    self.metalView.delegate = self;
    self.metalView.clearColor = MTLClearColorMake(0.1, 0.1, 0.2, 1.0);
    self.metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:self.metalView];

    /* Initialize Metal renderer */
    CAMetalLayer* layer = (CAMetalLayer*)self.metalView.layer;
    aurora_ios_metal_init((__bridge void*)layer);
    aurora_ios_render_resize(self.view.bounds.size.width, self.view.bounds.size.height,
                             [UIScreen mainScreen].scale);
}

- (BOOL)prefersStatusBarHidden {
    return NO;
}

- (BOOL)shouldAutorotate {
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskAll;
}

/* ── MTKViewDelegate ── */

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
    aurora_ios_render_resize(size.width, size.height, 1.0);
}

- (void)drawInMTKView:(MTKView*)view {
    (void)view;
    aurora_ios_render_frame();
}

@end
