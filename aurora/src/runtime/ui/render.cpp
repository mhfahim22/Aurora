#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

extern "C" {

/* ── Screen buffer for text-based rendering ── */
#define RENDER_MAX_W 160
#define RENDER_MAX_H 50

static char render_buffer[RENDER_MAX_H][RENDER_MAX_W + 1];
static int  render_width = 80;
static int  render_height = 25;
static int  initialized = 0;

static void ensure_init() {
    if (initialized) return;
    render_width = 80;
    render_height = 25;
    for (int y = 0; y < RENDER_MAX_H; y++) {
        memset(render_buffer[y], ' ', RENDER_MAX_W);
        render_buffer[y][RENDER_MAX_W] = '\0';
    }
    initialized = 1;
}

void aurora_ui_init() { ensure_init(); }

void aurora_ui_shutdown() {
    initialized = 0;
}

void aurora_ui_render() {
    if (!initialized) ensure_init();
    if (isatty(fileno(stdout)))
#ifdef _WIN32
    if (_isatty(_fileno(stdout)))
#else
    if (isatty(fileno(stdout)))
#endif
        printf("\033[H"); /* move cursor home */
    for (int y = 0; y < render_height; y++) {
        render_buffer[y][render_width] = '\0';
        printf("%s\n", render_buffer[y]);
        render_buffer[y][render_width] = ' ';
    }
    fflush(stdout);
}

/* ── Draw a character at (x, y) ── */
void aurora_render_char(int x, int y, char ch) {
    ensure_init();
    if (x >= 0 && x < render_width && y >= 0 && y < render_height)
        render_buffer[y][x] = ch;
}

/* ── Draw a string at (x, y) ── */
void aurora_render_text(int x, int y, const char* text) {
    ensure_init();
    if (!text) return;
    for (int i = 0; text[i] && (x + i) < render_width; i++)
        if (y >= 0 && y < render_height)
            render_buffer[y][x + i] = text[i];
}

/* ── Draw a line from (x1,y1) to (x2,y2) using ── */
void aurora_render_line(int x1, int y1, int x2, int y2, char ch) {
    ensure_init();
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        aurora_render_char(x1, y1, ch);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

/* ── Clear the render buffer ── */
void aurora_render_clear() {
    ensure_init();
    for (int y = 0; y < render_height; y++)
        memset(render_buffer[y], ' ', (size_t)render_width);
}

/* ── Set render resolution ── */
void aurora_render_resolution(int w, int h) {
    if (w > RENDER_MAX_W) w = RENDER_MAX_W;
    if (h > RENDER_MAX_H) h = RENDER_MAX_H;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    render_width = w;
    render_height = h;
    aurora_render_clear();
}

}
