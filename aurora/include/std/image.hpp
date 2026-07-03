#pragma once
#include <cstdint>
#include "common/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AuroraImage AuroraImage;

/* ════════════════════════════════════════════════════════════
   Lifecycle (new struct-based API)
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraImage*  aurora_img_new(int width, int height, int channels);
AURORA_EXPORT AuroraImage*  aurora_img_load(const char* path);
AURORA_EXPORT AuroraImage*  aurora_img_load_mem(const unsigned char* data, int len);
AURORA_EXPORT void          aurora_img_free(AuroraImage* img);
AURORA_EXPORT AuroraImage*  aurora_img_copy(AuroraImage* img);

/* ════════════════════════════════════════════════════════════
   Properties
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int           aurora_img_width(AuroraImage* img);
AURORA_EXPORT int           aurora_img_height(AuroraImage* img);
AURORA_EXPORT int           aurora_img_channels(AuroraImage* img);
AURORA_EXPORT unsigned char* aurora_img_data(AuroraImage* img);
AURORA_EXPORT int           aurora_img_detect_format(const char* path);

/* ════════════════════════════════════════════════════════════
   Save (encode)
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT int aurora_img_save_png(AuroraImage* img, const char* path);
AURORA_EXPORT int aurora_img_save_jpg(AuroraImage* img, const char* path, int quality);
AURORA_EXPORT int aurora_img_save_bmp(AuroraImage* img, const char* path);
AURORA_EXPORT int aurora_img_save_tga(AuroraImage* img, const char* path);

/* ════════════════════════════════════════════════════════════
   Processing
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraImage*  aurora_img_resize(AuroraImage* img, int w, int h);
AURORA_EXPORT AuroraImage*  aurora_img_crop(AuroraImage* img, int x, int y, int w, int h);
AURORA_EXPORT AuroraImage*  aurora_img_rotate(AuroraImage* img, float angle);
AURORA_EXPORT AuroraImage*  aurora_img_blur(AuroraImage* img, float radius);
AURORA_EXPORT void          aurora_img_brightness(AuroraImage* img, float factor);
AURORA_EXPORT void          aurora_img_contrast(AuroraImage* img, float factor);
AURORA_EXPORT void          aurora_img_saturation(AuroraImage* img, float factor);
AURORA_EXPORT AuroraImage*  aurora_img_flip_h(AuroraImage* img);
AURORA_EXPORT AuroraImage*  aurora_img_flip_v(AuroraImage* img);
AURORA_EXPORT AuroraImage*  aurora_img_grayscale(AuroraImage* img);
AURORA_EXPORT AuroraImage*  aurora_img_invert(AuroraImage* img);

/* ════════════════════════════════════════════════════════════
   SVG / ICO
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT AuroraImage*  aurora_img_load_svg(const char* path, int width, int height);
AURORA_EXPORT AuroraImage*  aurora_img_load_ico(const char* path);

/* ════════════════════════════════════════════════════════════
   Legacy raw-pixel API (backward compat, used by ai_builtins)
   ════════════════════════════════════════════════════════════ */
AURORA_EXPORT void*         aurora_image_load(const char* path, int* width, int* height, int* channels);
AURORA_EXPORT void          aurora_image_free(void* data);
AURORA_EXPORT unsigned int  aurora_image_create_gl_texture(const char* path);

#ifdef __cplusplus
}
#endif
