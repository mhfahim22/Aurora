/* ════════════════════════════════════════════════════════════
   image.cpp — Image Library (Phase 9)
   Load, save, process (resize, crop, rotate, blur, color)
   Formats: PNG, JPEG, BMP, TGA, GIF, SVG, ICO
   ════════════════════════════════════════════════════════════ */

#include "../../include/std/image.hpp"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT(x) ((void)0)
#include "../../deps/stb_image_write.h"

#ifndef AURORA_STB_IMAGE_LOADED
#define AURORA_STB_IMAGE_LOADED
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "../../deps/stb_image.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

/* ── Public image handle ── */
struct AuroraImage {
    unsigned char* pixels;
    int w, h, ch;
};

/* ════════════════════════════════════════════════════════════
   Helpers
   ════════════════════════════════════════════════════════════ */

static AuroraImage* img_alloc(int w, int h, int ch) {
    AuroraImage* img = (AuroraImage*)malloc(sizeof(AuroraImage));
    if (!img) return nullptr;
    img->w = w; img->h = h; img->ch = ch;
    img->pixels = (unsigned char*)calloc(1, (size_t)w * h * ch);
    if (!img->pixels) { free(img); return nullptr; }
    return img;
}

static unsigned char clamp_byte(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

/* ════════════════════════════════════════════════════════════
   Lifecycle
   ════════════════════════════════════════════════════════════ */

AuroraImage* aurora_img_new(int width, int height, int channels) {
    if (width < 1 || height < 1 || channels < 1 || channels > 4) return nullptr;
    return img_alloc(width, height, channels);
}

AuroraImage* aurora_img_load(const char* path) {
    if (!path) return nullptr;
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load(path, &w, &h, &ch, 0);
    if (!pixels) return nullptr;
    AuroraImage* img = (AuroraImage*)malloc(sizeof(AuroraImage));
    if (!img) { stbi_image_free(pixels); return nullptr; }
    img->pixels = pixels;
    img->w = w; img->h = h; img->ch = ch;
    return img;
}

AuroraImage* aurora_img_load_mem(const unsigned char* data, int len) {
    if (!data || len < 1) return nullptr;
    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load_from_memory(data, len, &w, &h, &ch, 0);
    if (!pixels) return nullptr;
    AuroraImage* img = (AuroraImage*)malloc(sizeof(AuroraImage));
    if (!img) { stbi_image_free(pixels); return nullptr; }
    img->pixels = pixels;
    img->w = w; img->h = h; img->ch = ch;
    return img;
}

void aurora_img_free(AuroraImage* img) {
    if (!img) return;
    if (img->pixels) stbi_image_free(img->pixels);
    free(img);
}

AuroraImage* aurora_img_copy(AuroraImage* img) {
    if (!img || !img->pixels) return nullptr;
    AuroraImage* cpy = img_alloc(img->w, img->h, img->ch);
    if (!cpy) return nullptr;
    memcpy(cpy->pixels, img->pixels, (size_t)img->w * img->h * img->ch);
    return cpy;
}

/* ════════════════════════════════════════════════════════════
   Properties
   ════════════════════════════════════════════════════════════ */

int aurora_img_width(AuroraImage* img) { return img ? img->w : 0; }
int aurora_img_height(AuroraImage* img) { return img ? img->h : 0; }
int aurora_img_channels(AuroraImage* img) { return img ? img->ch : 0; }
unsigned char* aurora_img_data(AuroraImage* img) { return img ? img->pixels : nullptr; }

int aurora_img_detect_format(const char* path) {
    if (!path) return 0;
    const char* ext = strrchr(path, '.');
    if (!ext) return 0;
    auto ci = [](char c) -> char { return (c >= 'A' && c <= 'Z') ? (c + 32) : c; };
    std::string e;
    for (const char* p = ext; *p; ++p) e.push_back(ci(*p));
    if (e == ".png") return 1;
    if (e == ".jpg" || e == ".jpeg") return 2;
    if (e == ".bmp") return 3;
    if (e == ".gif") return 4;
    if (e == ".tga") return 5;
    if (e == ".svg") return 6;
    if (e == ".ico") return 7;
    if (e == ".webp") return 8;
    return 0;
}

/* ════════════════════════════════════════════════════════════
   Save
   ════════════════════════════════════════════════════════════ */

int aurora_img_save_png(AuroraImage* img, const char* path) {
    if (!img || !img->pixels || !path) return 0;
    return stbi_write_png(path, img->w, img->h, img->ch, img->pixels, img->w * img->ch);
}

int aurora_img_save_jpg(AuroraImage* img, const char* path, int quality) {
    if (!img || !img->pixels || !path) return 0;
    return stbi_write_jpg(path, img->w, img->h, img->ch, img->pixels, quality);
}

int aurora_img_save_bmp(AuroraImage* img, const char* path) {
    if (!img || !img->pixels || !path) return 0;
    return stbi_write_bmp(path, img->w, img->h, img->ch, img->pixels);
}

int aurora_img_save_tga(AuroraImage* img, const char* path) {
    if (!img || !img->pixels || !path) return 0;
    return stbi_write_tga(path, img->w, img->h, img->ch, img->pixels);
}

/* ════════════════════════════════════════════════════════════
   Processing — Resize (bilinear)
   ════════════════════════════════════════════════════════════ */

AuroraImage* aurora_img_resize(AuroraImage* img, int nw, int nh) {
    if (!img || !img->pixels || nw < 1 || nh < 1) return nullptr;
    AuroraImage* out = img_alloc(nw, nh, img->ch);
    if (!out) return nullptr;
    int ch = img->ch;
    float ratio_x = (float)img->w / nw;
    float ratio_y = (float)img->h / nh;
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            float gx = (x + 0.5f) * ratio_x - 0.5f;
            float gy = (y + 0.5f) * ratio_y - 0.5f;
            int ix = (int)gx, iy = (int)gy;
            float fx = gx - ix, fy = gy - iy;
            if (ix < 0) ix = 0; if (ix >= img->w - 1) ix = img->w - 2;
            if (iy < 0) iy = 0; if (iy >= img->h - 1) iy = img->h - 2;
            for (int c = 0; c < ch; ++c) {
                float v = (1 - fx) * (1 - fy) * img->pixels[(iy * img->w + ix) * ch + c]
                        + fx * (1 - fy) * img->pixels[(iy * img->w + ix + 1) * ch + c]
                        + (1 - fx) * fy * img->pixels[((iy + 1) * img->w + ix) * ch + c]
                        + fx * fy * img->pixels[((iy + 1) * img->w + ix + 1) * ch + c];
                out->pixels[(y * nw + x) * ch + c] = clamp_byte((int)(v + 0.5f));
            }
        }
    }
    return out;
}

/* ════════════════════════════════════════════════════════════
   Processing — Crop
   ════════════════════════════════════════════════════════════ */

AuroraImage* aurora_img_crop(AuroraImage* img, int x, int y, int w, int h) {
    if (!img || !img->pixels || w < 1 || h < 1) return nullptr;
    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x + w > img->w) w = img->w - x;
    if (y + h > img->h) h = img->h - y;
    if (w < 1 || h < 1) return nullptr;
    AuroraImage* out = img_alloc(w, h, img->ch);
    if (!out) return nullptr;
    int ch = img->ch;
    for (int row = 0; row < h; ++row) {
        memcpy(out->pixels + row * w * ch,
               img->pixels + ((y + row) * img->w + x) * ch,
               (size_t)w * ch);
    }
    return out;
}

/* ════════════════════════════════════════════════════════════
   Processing — Rotate
   ════════════════════════════════════════════════════════════ */

AuroraImage* aurora_img_rotate(AuroraImage* img, float angle) {
    if (!img || !img->pixels) return nullptr;
    float rad = angle * 3.14159265f / 180.0f;
    float c = cosf(rad), s = sinf(rad);
    int ch = img->ch;
    int nw = img->w, nh = img->h;
    AuroraImage* out = img_alloc(nw, nh, ch);
    if (!out) return nullptr;
    float cx = nw / 2.0f, cy = nh / 2.0f;
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            float dx = x - cx, dy = y - cy;
            float sx = c * dx + s * dy + cx;
            float sy = -s * dx + c * dy + cy;
            int ix = (int)(sx + 0.5f);
            int iy = (int)(sy + 0.5f);
            if (ix < 0 || ix >= img->w || iy < 0 || iy >= img->h) continue;
            memcpy(out->pixels + (y * nw + x) * ch,
                   img->pixels + (iy * img->w + ix) * ch, ch);
        }
    }
    return out;
}

/* ════════════════════════════════════════════════════════════
   Processing — Blur (box blur, 3-pass)
   ════════════════════════════════════════════════════════════ */

static void blur_pass(unsigned char* src, unsigned char* dst, int w, int h, int ch, int radius, int horiz) {
    float* acc = (float*)calloc(1, (size_t)ch * sizeof(float));
    if (!acc) return;
    for (int y = 0; y < h; ++y) {
        for (int c = 0; c < ch; ++c) acc[c] = 0;
        int count = 0;
        for (int x = -radius; x < (horiz ? w : h); ++x) {
            if (x + radius < (horiz ? w : h)) {
                int idx = horiz ? (y * w + (x + radius)) * ch : ((x + radius) * w + y) * ch;
                for (int c2 = 0; c2 < ch; ++c2) acc[c2] += src[idx + c2];
                ++count;
            }
            if (x - radius - 1 >= 0) {
                int idx = horiz ? (y * w + (x - radius - 1)) * ch : ((x - radius - 1) * w + y) * ch;
                for (int c2 = 0; c2 < ch; ++c2) acc[c2] -= src[idx + c2];
                --count;
            }
            if (x >= 0) {
                int px = horiz ? x : y;
                int py = horiz ? y : x;
                for (int c2 = 0; c2 < ch; ++c2)
                    dst[(py * w + px) * ch + c2] = clamp_byte((int)(acc[c2] / count + 0.5f));
            }
        }
    }
    free(acc);
}

AuroraImage* aurora_img_blur(AuroraImage* img, float radius) {
    if (!img || !img->pixels || radius < 0.5f) return aurora_img_copy(img);
    int r = (int)(radius + 0.5f);
    if (r < 1) r = 1;
    int ch = img->ch;
    size_t sz = (size_t)img->w * img->h * ch;
    unsigned char* tmp1 = (unsigned char*)malloc(sz);
    unsigned char* tmp2 = (unsigned char*)malloc(sz);
    if (!tmp1 || !tmp2) { free(tmp1); free(tmp2); return nullptr; }
    AuroraImage* out = (AuroraImage*)malloc(sizeof(AuroraImage));
    if (!out) { free(tmp1); free(tmp2); return nullptr; }
    out->w = img->w; out->h = img->h; out->ch = ch;
    out->pixels = (unsigned char*)malloc(sz);
    if (!out->pixels) { free(tmp1); free(tmp2); free(out); return nullptr; }
    memcpy(out->pixels, img->pixels, sz);
    for (int i = 0; i < 3; ++i) {
        blur_pass(out->pixels, tmp1, img->w, img->h, ch, r, 1);
        blur_pass(tmp1, tmp2, img->w, img->h, ch, r, 0);
        memcpy(out->pixels, tmp2, sz);
    }
    free(tmp1); free(tmp2);
    return out;
}

/* ════════════════════════════════════════════════════════════
   Processing — Color ops
   ════════════════════════════════════════════════════════════ */

void aurora_img_brightness(AuroraImage* img, float factor) {
    if (!img || !img->pixels) return;
    int ch = img->ch;
    int n = img->w * img->h * ch;
    for (int i = 0; i < n; i += ch)
        for (int c = 0; c < 3 && c < ch; ++c)
            img->pixels[i + c] = clamp_byte((int)(img->pixels[i + c] * factor));
}

void aurora_img_contrast(AuroraImage* img, float factor) {
    if (!img || !img->pixels) return;
    int ch = img->ch, n = img->w * img->h;
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 3 && c < ch; ++c) {
            float v = (img->pixels[(i * ch) + c] - 128.0f) * factor + 128.0f;
            img->pixels[(i * ch) + c] = clamp_byte((int)(v + 0.5f));
        }
}

void aurora_img_saturation(AuroraImage* img, float factor) {
    if (!img || !img->pixels || img->ch < 3) return;
    int n = img->w * img->h;
    for (int i = 0; i < n; ++i) {
        unsigned char* p = img->pixels + i * img->ch;
        float gray = 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
        p[0] = clamp_byte((int)(gray + (p[0] - gray) * factor));
        p[1] = clamp_byte((int)(gray + (p[1] - gray) * factor));
        p[2] = clamp_byte((int)(gray + (p[2] - gray) * factor));
    }
}

AuroraImage* aurora_img_flip_h(AuroraImage* img) {
    if (!img || !img->pixels) return nullptr;
    AuroraImage* out = aurora_img_copy(img);
    if (!out) return nullptr;
    int ch = out->ch, row_sz = out->w * ch;
    for (int y = 0; y < out->h; ++y) {
        unsigned char* row = out->pixels + y * row_sz;
        for (int x = 0; x < out->w / 2; ++x)
            for (int c = 0; c < ch; ++c)
                std::swap(row[x * ch + c], row[(out->w - 1 - x) * ch + c]);
    }
    return out;
}

AuroraImage* aurora_img_flip_v(AuroraImage* img) {
    if (!img || !img->pixels) return nullptr;
    AuroraImage* out = aurora_img_copy(img);
    if (!out) return nullptr;
    int ch = out->ch, row_sz = out->w * ch;
    std::vector<unsigned char> tmp(row_sz);
    for (int y = 0; y < out->h / 2; ++y) {
        unsigned char* top = out->pixels + y * row_sz;
        unsigned char* bot = out->pixels + (out->h - 1 - y) * row_sz;
        memcpy(tmp.data(), top, row_sz);
        memcpy(top, bot, row_sz);
        memcpy(bot, tmp.data(), row_sz);
    }
    return out;
}

AuroraImage* aurora_img_grayscale(AuroraImage* img) {
    if (!img || !img->pixels) return nullptr;
    AuroraImage* out = img_alloc(img->w, img->h, 1);
    if (!out) return nullptr;
    int n = img->w * img->h;
    for (int i = 0; i < n; ++i) {
        unsigned char* p = img->pixels + i * img->ch;
        out->pixels[i] = (unsigned char)(0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2] + 0.5f);
    }
    return out;
}

AuroraImage* aurora_img_invert(AuroraImage* img) {
    if (!img || !img->pixels) return nullptr;
    AuroraImage* out = aurora_img_copy(img);
    if (!out) return nullptr;
    int ch = out->ch, n = out->w * out->h * ch;
    for (int i = 0; i < n; i += ch)
        for (int c = 0; c < 3 && c < ch; ++c)
            out->pixels[i + c] = 255 - out->pixels[i + c];
    return out;
}

/* ════════════════════════════════════════════════════════════
   SVG — Simple rasterizer
   ════════════════════════════════════════════════════════════ */

static int svg_parse_color(const char* s) {
    if (!s) return 0;
    while (*s && *s != '#' && *s != '(') ++s;
    if (*s == '#') { unsigned v = 0; sscanf(s + 1, "%x", &v); return (int)v; }
    if (*s == '(') { int r = 0, g = 0, b = 0; sscanf(s + 1, "%d,%d,%d", &r, &g, &b); return (r << 16) | (g << 8) | b; }
    /* named colors */
    while (*s == ' ') ++s;
    std::string n; for (; *s && *s != '"' && *s != ' '; ++s) n.push_back(*s);
    if (n == "black") return 0x000000; if (n == "white") return 0xFFFFFF;
    if (n == "red") return 0xFF0000; if (n == "green") return 0x00FF00;
    if (n == "blue") return 0x0000FF; if (n == "yellow") return 0xFFFF00;
    if (n == "cyan") return 0x00FFFF; if (n == "magenta") return 0xFF00FF;
    if (n == "gray" || n == "grey") return 0x808080;
    if (n == "none") return -1;
    return 0;
}

static void svg_fill_rect(unsigned char* p, int w, int h, int x, int y, int rw, int rh, int color) {
    if (color < 0) return;
    for (int row = y; row < y + rh && row < h; ++row) {
        if (row < 0) continue;
        for (int col = x; col < x + rw && col < w; ++col) {
            if (col < 0) continue;
            p[(row * w + col) * 4 + 0] = (unsigned char)((color >> 16) & 0xFF);
            p[(row * w + col) * 4 + 1] = (unsigned char)((color >> 8) & 0xFF);
            p[(row * w + col) * 4 + 2] = (unsigned char)(color & 0xFF);
            p[(row * w + col) * 4 + 3] = 255;
        }
    }
}

static void svg_fill_circle(unsigned char* p, int w, int h, int cx, int cy, int r, int color) {
    if (color < 0) return;
    for (int row = cy - r; row <= cy + r; ++row) {
        if (row < 0 || row >= h) continue;
        for (int col = cx - r; col <= cx + r; ++col) {
            if (col < 0 || col >= w) continue;
            if ((col - cx) * (col - cx) + (row - cy) * (row - cy) <= r * r) {
                p[(row * w + col) * 4 + 0] = (unsigned char)((color >> 16) & 0xFF);
                p[(row * w + col) * 4 + 1] = (unsigned char)((color >> 8) & 0xFF);
                p[(row * w + col) * 4 + 2] = (unsigned char)(color & 0xFF);
                p[(row * w + col) * 4 + 3] = 255;
            }
        }
    }
}

static void svg_fill_line(unsigned char* p, int w, int h, int x1, int y1, int x2, int y2, int color) {
    if (color < 0) return;
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    for (int x = x1, y = y1;;) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            p[(y * w + x) * 4 + 0] = (unsigned char)((color >> 16) & 0xFF);
            p[(y * w + x) * 4 + 1] = (unsigned char)((color >> 8) & 0xFF);
            p[(y * w + x) * 4 + 2] = (unsigned char)(color & 0xFF);
            p[(y * w + x) * 4 + 3] = 255;
        }
        if (x == x2 && y == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

static int svg_next_int(const char*& p) {
    while (*p && ((*p < '0' || *p > '9') && *p != '-')) ++p;
    if (!*p) return 0;
    return (int)strtol(p, (char**)&p, 10);
}

AuroraImage* aurora_img_load_svg(const char* path, int width, int height) {
    if (!path || width < 1 || height < 1) return nullptr;
    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    if (len < 1) { fclose(f); return nullptr; }
    std::string xml((size_t)len, '\0');
    fread(&xml[0], 1, (size_t)len, f);
    fclose(f);

    AuroraImage* img = img_alloc(width, height, 4);
    if (!img) return nullptr;
    memset(img->pixels, 255, (size_t)width * height * 4);

    const char* p = xml.c_str();
    while (*p) {
        if (*p == '<') {
            ++p;
            if (*p == '/') { while (*p && *p != '>') ++p; if (*p) ++p; continue; }
            char tag[32]; int ti = 0;
            while (*p && *p != '>' && *p != ' ' && ti < 31) tag[ti++] = *p++;
            tag[ti] = 0;
            std::string attrs;
            while (*p && *p != '>') attrs.push_back(*p++);
            if (*p == '>') ++p;

            auto find_attr = [&](const char* name) -> std::string {
                const char* q = strstr(attrs.c_str(), name);
                if (!q) return {};
                q = strchr(q, '='); if (!q) return {};
                ++q; while (*q == '"' || *q == '\'') ++q;
                std::string r; while (*q && *q != '"' && *q != '\'' && *q != '>') r.push_back(*q++);
                return r;
            };

            std::string fs = find_attr("fill");
            int fill = fs.empty() ? 0 : svg_parse_color(fs.c_str());

            if (strcmp(tag, "rect") == 0) {
                int x = svg_next_int(p), y = svg_next_int(p);
                int rw = svg_next_int(p), rh = svg_next_int(p);
                svg_fill_rect(img->pixels, width, height, x, y, rw, rh, fill);
            } else if (strcmp(tag, "circle") == 0) {
                int cx = svg_next_int(p), cy = svg_next_int(p), r = svg_next_int(p);
                svg_fill_circle(img->pixels, width, height, cx, cy, r, fill);
            } else if (strcmp(tag, "ellipse") == 0) {
                int cx = svg_next_int(p), cy = svg_next_int(p);
                int rx = svg_next_int(p), ry = svg_next_int(p);
                svg_fill_circle(img->pixels, width, height, cx, cy, rx < ry ? rx : ry, fill);
            } else if (strcmp(tag, "line") == 0) {
                int x1 = svg_next_int(p), y1 = svg_next_int(p);
                int x2 = svg_next_int(p), y2 = svg_next_int(p);
                svg_fill_line(img->pixels, width, height, x1, y1, x2, y2, fill);
            } else if (strcmp(tag, "polygon") == 0 || strcmp(tag, "polyline") == 0) {
                std::string pts = find_attr("points");
                const char* pp = pts.c_str();
                int prev_x = -1, prev_y = -1, first_x = -1, first_y = -1;
                bool first = true;
                while (*pp) {
                    int px = svg_next_int(pp), py = svg_next_int(pp);
                    if (first) { first_x = px; first_y = py; first = false; }
                    if (prev_x >= 0) svg_fill_line(img->pixels, width, height, prev_x, prev_y, px, py, fill);
                    prev_x = px; prev_y = py;
                }
                if (strcmp(tag, "polygon") == 0 && prev_x >= 0 && first_x >= 0)
                    svg_fill_line(img->pixels, width, height, prev_x, prev_y, first_x, first_y, fill);
            }
        } else ++p;
    }
    return img;
}

/* ════════════════════════════════════════════════════════════
   ICO — Win32 GDI
   ════════════════════════════════════════════════════════════ */

#ifdef _WIN32
AuroraImage* aurora_img_load_ico(const char* path) {
    if (!path) return nullptr;
    HANDLE h = LoadImageA(nullptr, path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    if (!h) return nullptr;
    ICONINFO ii = {0};
    if (!GetIconInfo((HICON)h, &ii)) { DestroyIcon((HICON)h); return nullptr; }
    BITMAP bm = {0};
    GetObject(ii.hbmColor, sizeof(bm), &bm);
    int w = bm.bmWidth, hpx = bm.bmHeight;
    HDC dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(dc);
    SelectObject(mem_dc, ii.hbmColor);
    AuroraImage* img = img_alloc(w, hpx, 4);
    if (img) {
        BITMAPINFO bi = {0};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -hpx;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        GetDIBits(mem_dc, ii.hbmColor, 0, hpx, img->pixels, &bi, DIB_RGB_COLORS);
    }
    DeleteDC(mem_dc); ReleaseDC(nullptr, dc);
    DeleteObject(ii.hbmColor); DeleteObject(ii.hbmMask);
    DestroyIcon((HICON)h);
    return img;
}
#else
AuroraImage* aurora_img_load_ico(const char*) { return nullptr; }
#endif

/* ════════════════════════════════════════════════════════════
   Legacy raw-pixel API
   ════════════════════════════════════════════════════════════ */

void* aurora_image_load(const char* path, int* width, int* height, int* channels) {
    return stbi_load(path, width, height, channels, 0);
}

void aurora_image_free(void* data) {
    if (data) stbi_image_free(data);
}

unsigned int aurora_image_create_gl_texture(const char* path) {
    (void)path;
    return 0;
}
