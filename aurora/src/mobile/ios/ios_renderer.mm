/* ════════════════════════════════════════════════════════════
   ios_renderer.mm — Metal & OpenGL ES renderer for iOS
   ════════════════════════════════════════════════════════════ */

#include "mobile/ios.hpp"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <cstdio>

/* ── Metal state ── */
static id<MTLDevice>       g_device = nil;
static id<MTLCommandQueue> g_cmd_queue = nil;
static CAMetalLayer*       g_metal_layer = nil;
static float g_render_w = 0;
static float g_render_h = 0;

extern "C" {

int aurora_ios_metal_init(void* metal_layer) {
    g_metal_layer = (__bridge CAMetalLayer*)metal_layer;

    g_device = MTLCreateSystemDefaultDevice();
    if (!g_device) {
        fprintf(stderr, "[ios-metal] failed to create MTLDevice\n");
        return -1;
    }

    g_cmd_queue = [g_device newCommandQueue];
    if (!g_cmd_queue) {
        fprintf(stderr, "[ios-metal] failed to create command queue\n");
        return -1;
    }

    g_metal_layer.device = g_device;
    g_metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    g_metal_layer.framebufferOnly = YES;

    printf("[ios-metal] initialized: device=%s\n",
           [[g_device name] UTF8String]);
    return 0;
}

int aurora_ios_gles_init(void* gl_layer) {
    (void)gl_layer;
    /* OpenGL ES init would use EAGLContext + GLKView */
    printf("[ios-gles] placeholder: EAGL context init\n");
    return 0;
}

void aurora_ios_render_resize(float width, float height, float scale) {
    g_render_w = width;
    g_render_h = height;
    if (g_metal_layer) {
        g_metal_layer.drawableSize = CGSizeMake(width * scale, height * scale);
    }
}

int aurora_ios_render_frame() {
    if (!g_metal_layer || !g_cmd_queue) return -1;

    @autoreleasepool {
        id<CAMetalDrawable> drawable = [g_metal_layer nextDrawable];
        if (!drawable) return -1;

        id<MTLCommandBuffer> cmd_buf = [g_cmd_queue commandBuffer];
        if (!cmd_buf) return -1;

        /* Simple clear pass */
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        if (!pass) return -1;

        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.1, 0.1, 0.2, 1.0);

        id<MTLRenderCommandEncoder> enc = [cmd_buf renderCommandEncoderWithDescriptor:pass];
        if (enc) {
            [enc endEncoding];
        }

        [cmd_buf presentDrawable:drawable];
        [cmd_buf commit];
    }

    return 0;
}

} /* extern "C" */
