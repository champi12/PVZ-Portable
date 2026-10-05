// Input translation (PSP buttons -> touch screen / phone keys) and frame presentation.
//
// The original game is a touch-screen MIDlet. On the PSP the analog stick (or D-pad) moves a
// cursor and X "taps" the screen; holding X while moving drags.
#include "classes.h"
#include "platform.h"
#include "port.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum { EV_KEY_DOWN = 1, EV_KEY_UP = 2, EV_PTR_DOWN = 3, EV_PTR_UP = 4, EV_PTR_DRAG = 5 };

static const int QUEUE_MAX = 64;
static int32_t g_queue[QUEUE_MAX * 3];
static int g_queue_len = 0;

static float g_cx = SCREEN_W / 2, g_cy = SCREEN_H / 2;
static uint32_t g_prev_buttons = 0;
static bool g_ptr_down = false;
static int g_last_px = -1, g_last_py = -1;
static int64_t g_last_update = 0;
static int g_dpad_hold_ms = 0;
static bool g_cursor_visible = true;
static int g_idle_ms = 0;

static int64_t g_prof_start = 0, g_prof_slept = 0;
int64_t g_prof_us[PROF_COUNT];

int64_t port_time_us() { return platform_time_us(); }

static void push(int type, int a, int b) {
    if (g_queue_len >= QUEUE_MAX) return;
    g_queue[g_queue_len * 3] = type;
    g_queue[g_queue_len * 3 + 1] = a;
    g_queue[g_queue_len * 3 + 2] = b;
    g_queue_len++;
}

struct KeyMap {
    uint32_t button;
    int keycode;
};

// Phone keys: -6 / -7 are the left / right soft keys, -5 fire.
static const KeyMap KEYS[] = {
    {PAD_CIRCLE, -7},
    {PAD_TRIANGLE, -6},
    {PAD_START, -7},
};

static void update_input() {
    PadState pad;
    memset(&pad, 0, sizeof pad);
    platform_read_pad(&pad);
    int64_t now = platform_time_ms();
    int dt = g_last_update ? (int)(now - g_last_update) : 16;
    if (dt > 100) dt = 100;
    if (dt < 0) dt = 0;
    g_last_update = now;

    if (pad.quit) S_javax_microedition_midlet_MIDlet__exitRequested_Z = 1;

    bool moved = false;
    if (pad.mouse_valid) {
        if ((int)g_cx != pad.mouse_x || (int)g_cy != pad.mouse_y) moved = true;
        g_cx = pad.mouse_x;
        g_cy = pad.mouse_y;
        if (pad.mouse_down) pad.buttons |= PAD_CROSS;
    }

    // analog stick
    float ax = pad.ax / 128.0f, ay = pad.ay / 128.0f;
    const float dead = 0.25f;
    float mag = sqrtf(ax * ax + ay * ay);
    if (mag > dead) {
        float n = (mag - dead) / (1.0f - dead);
        if (n > 1) n = 1;
        float speed = 60.0f + 340.0f * n * n;  // px per second
        if (pad.buttons & PAD_R) speed *= 1.8f;
        g_cx += ax / mag * speed * dt / 1000.0f;
        g_cy += ay / mag * speed * dt / 1000.0f;
        moved = true;
    }
    // D-pad with acceleration
    int dx = ((pad.buttons & PAD_RIGHT) ? 1 : 0) - ((pad.buttons & PAD_LEFT) ? 1 : 0);
    int dy = ((pad.buttons & PAD_DOWN) ? 1 : 0) - ((pad.buttons & PAD_UP) ? 1 : 0);
    if (dx || dy) {
        g_dpad_hold_ms += dt;
        float speed = 90.0f + (g_dpad_hold_ms > 600 ? 600 : g_dpad_hold_ms) * 0.45f;
        if (pad.buttons & PAD_R) speed *= 1.8f;
        g_cx += dx * speed * dt / 1000.0f;
        g_cy += dy * speed * dt / 1000.0f;
        moved = true;
    } else {
        g_dpad_hold_ms = 0;
    }
    if (g_cx < 0) g_cx = 0;
    if (g_cy < 0) g_cy = 0;
    if (g_cx > SCREEN_W - 1) g_cx = SCREEN_W - 1;
    if (g_cy > SCREEN_H - 1) g_cy = SCREEN_H - 1;

    if (moved) {
        g_idle_ms = 0;
        g_cursor_visible = true;
    }

    int px, py;
    game_screen_to_pointer((int)g_cx, (int)g_cy, &px, &py);
    bool down = (pad.buttons & PAD_CROSS) != 0;
    if (down && !g_ptr_down) {
        push(EV_PTR_DOWN, px, py);
        g_last_px = px;
        g_last_py = py;
    } else if (down && g_ptr_down && (px != g_last_px || py != g_last_py)) {
        push(EV_PTR_DRAG, px, py);
        g_last_px = px;
        g_last_py = py;
    } else if (!down && g_ptr_down) {
        push(EV_PTR_UP, px, py);
    }
    g_ptr_down = down;

    for (const KeyMap& k : KEYS) {
        bool now_down = (pad.buttons & k.button) != 0, was = (g_prev_buttons & k.button) != 0;
        if (now_down && !was) push(EV_KEY_DOWN, k.keycode, 0);
        if (!now_down && was) push(EV_KEY_UP, k.keycode, 0);
    }
    g_prev_buttons = pad.buttons;
}

int port_poll_events(int32_t* buf, int max) {
    update_input();
    int n = g_queue_len < max ? g_queue_len : max;
    // The game samples the touch state once per logic frame (166 ms), so only one press or
    // release is delivered per frame; otherwise quick taps would be lost.
    int transitions = 0;
    for (int i = 0; i < n; i++) {
        int t = g_queue[i * 3];
        if (t == EV_PTR_DOWN || t == EV_PTR_UP) {
            if (++transitions == 2) {
                n = i;
                break;
            }
        }
    }
    static int dbg = -1;
    if (dbg < 0) dbg = getenv("PVZ_INPUT_DEBUG") != nullptr;
    if (dbg)
        for (int i = 0; i < n; i++)
            fprintf(stderr, "[%lld] event %d %d %d\n", (long long)platform_time_ms(), g_queue[i * 3], g_queue[i * 3 + 1],
                    g_queue[i * 3 + 2]);
    memcpy(buf, g_queue, n * 3 * sizeof(int32_t));
    memmove(g_queue, g_queue + n * 3, (g_queue_len - n) * 3 * sizeof(int32_t));
    g_queue_len -= n;
    return n;
}

// 12x19 arrow cursor: '#' outline, '.' fill.
static const char* CURSOR[] = {
    "#           ", "##          ", "#.#         ", "#..#        ", "#...#       ", "#....#      ",
    "#.....#     ", "#......#    ", "#.......#   ", "#........#  ", "#.........# ", "#......#####",
    "#...#..#    ", "#..# #..#   ", "#.#  #..#   ", "##    #..#  ", "#     #..#  ", "       #..# ",
    "        ##  ",
};

// Cursor bitmap in the native pixel format (CURSOR_W x CURSOR_H, transparent background).
static uint32_t g_cursor_img[2][CURSOR_W * CURSOR_H] __attribute__((aligned(16)));

static void build_cursor() {
    static bool done = false;
    if (done) return;
    done = true;
    for (int k = 0; k < 2; k++) {
        uint32_t fill = k ? PIX_FROM_ARGB(0xFFFFE070u) : 0xFFFFFFFFu;
        for (int y = 0; y < CURSOR_H; y++)
            for (int x = 0; x < CURSOR_W; x++) {
                char c = (y < 19 && x < 12) ? CURSOR[y][x] : ' ';
                g_cursor_img[k][y * CURSOR_W + x] = c == '#' ? 0xFF000000u : c == '.' ? fill : 0;
            }
    }
}

// The last frame: either a game canvas (scaled to the screen) or a ready screen image.
static const uint32_t* g_canvas = nullptr;
static int g_canvas_stride = 0, g_canvas_view_w = 0, g_canvas_h = 0;
static bool g_have_frame = false;
static bool g_canvas_presented = false;
static uint32_t g_out[SCREEN_W * SCREEN_H];

// Bilinear resample of a view_w x h canvas region to SCREEN_W x SCREEN_H (fixed point 1/128).
static void scale_canvas(uint32_t* d, const uint32_t* s, int stride, int view_w, int h) {
    if (view_w == SCREEN_W && h == SCREEN_H) {
        for (int y = 0; y < SCREEN_H; y++) memcpy(d + y * SCREEN_W, s + y * stride, SCREEN_W * 4);
        return;
    }
    static int xs[SCREEN_W], xw[SCREEN_W];
    static int cached_w = -1;
    if (cached_w != view_w) {
        for (int x = 0; x < SCREEN_W; x++) {
            int fx = ((x * 2 + 1) * view_w * 128) / (SCREEN_W * 2) - 64;
            if (fx < 0) fx = 0;
            xs[x] = fx >> 7;
            xw[x] = fx & 127;
            if (xs[x] >= view_w - 1) {
                xs[x] = view_w - 2;
                xw[x] = 128;
            }
        }
        cached_w = view_w;
    }
    for (int y = 0; y < SCREEN_H; y++) {
        int fy = ((y * 2 + 1) * h * 128) / (SCREEN_H * 2) - 64;
        if (fy < 0) fy = 0;
        int y0 = fy >> 7, wy = fy & 127;
        int y1 = y0 + 1 < h ? y0 + 1 : y0;
        const uint32_t* r0 = s + y0 * stride;
        const uint32_t* r1 = s + y1 * stride;
        uint32_t* o = d + y * SCREEN_W;
        for (int x = 0; x < SCREEN_W; x++) {
            int sx = xs[x], wx = xw[x];
            uint32_t a = r0[sx], b = r0[sx + 1], c = r1[sx], e = r1[sx + 1];
            uint32_t t_rb = (((a & 0xFF00FF) * (128 - wx) + (b & 0xFF00FF) * wx) >> 7) & 0xFF00FF;
            uint32_t t_g = (((a & 0x00FF00) * (128 - wx) + (b & 0x00FF00) * wx) >> 7) & 0x00FF00;
            uint32_t b_rb = (((c & 0xFF00FF) * (128 - wx) + (e & 0xFF00FF) * wx) >> 7) & 0xFF00FF;
            uint32_t b_g = (((c & 0x00FF00) * (128 - wx) + (e & 0x00FF00) * wx) >> 7) & 0x00FF00;
            uint32_t rb = ((t_rb * (128 - wy) + b_rb * wy) >> 7) & 0xFF00FF;
            uint32_t g = ((t_g * (128 - wy) + b_g * wy) >> 7) & 0x00FF00;
            o[x] = 0xFF000000u | rb | g;
        }
    }
}

static int64_t g_last_present = 0;

static void compose_and_show() {
    g_last_present = platform_time_ms();
    build_cursor();
    bool cursor = g_cursor_visible && game_wants_cursor();
    PresentCursor pc = {cursor, (int)g_cx, (int)g_cy, g_cursor_img[g_ptr_down ? 1 : 0]};
    // Fast path: the backend scales the canvas itself (PSP: on the GPU).
    if (platform_present_canvas(g_canvas, g_canvas_stride, g_canvas_view_w, g_canvas_h, &pc)) return;
    scale_canvas(g_out, g_canvas, g_canvas_stride, g_canvas_view_w, g_canvas_h);
    if (cursor) {
        for (int y = 0; y < CURSOR_H; y++) {
            int yy = pc.y + y;
            if (yy < 0 || yy >= SCREEN_H) continue;
            for (int x = 0; x < CURSOR_W; x++) {
                int xx = pc.x + x;
                uint32_t p = pc.image[y * CURSOR_W + x];
                if (xx >= 0 && xx < SCREEN_W && p) g_out[yy * SCREEN_W + xx] = p;
            }
        }
    }
    platform_present(g_out);
}

void port_present_canvas(const uint32_t* pixels, int stride, int view_w, int h) {
    int64_t t0 = port_time_us();
    g_canvas = pixels;
    g_canvas_stride = stride;
    g_canvas_view_w = view_w;
    g_canvas_h = h;
    g_have_frame = true;
    g_canvas_presented = true;
    compose_and_show();
    g_prof_us[PROF_PRESENT] += port_time_us() - t0;
}

void port_present(uint32_t* pixels) {
    // Display.present() after Canvas.paint(): once the game presents its own canvas, the MIDP
    // screen image is not used any more (a skipped paint keeps the previous frame on screen).
    if (g_canvas_presented) return;
    port_present_canvas(pixels, SCREEN_W, SCREEN_W, SCREEN_H);
    g_canvas_presented = false;
}

// Prints the share of time the game spends working (not sleeping) every 5 s when profiling
// is enabled (PVZ_PROFILE=1 on PC; built in with -DPVZ_PROFILE on the PSP).
static void profile(int64_t slept) {
#ifndef PVZ_PROFILE
    static int enabled = -1;
    if (enabled < 0) enabled = getenv("PVZ_PROFILE") != nullptr;
    if (!enabled) return;
#endif
    int64_t now = platform_time_ms();
    if (!g_prof_start) g_prof_start = now;
    g_prof_slept += slept;
    if (now - g_prof_start >= 5000) {
        extern int64_t g_midi_mix_us;
        printf("[profile] cpu busy %d%% (heap %u KB) logic %d render %d (blit %d fill %d) present %d audio %d ms\n",
               (int)(100 - g_prof_slept * 100 / (now - g_prof_start)), (unsigned)(gc_heap_bytes() / 1024),
               (int)(g_prof_us[PROF_LOGIC] / 1000), (int)(g_prof_us[PROF_RENDER] / 1000),
               (int)(g_prof_us[PROF_BLIT] / 1000), (int)(g_prof_us[PROF_FILL] / 1000), (int)(g_prof_us[PROF_PRESENT] / 1000),
               (int)(g_midi_mix_us / 1000));
        g_midi_mix_us = 0;
        memset(g_prof_us, 0, sizeof g_prof_us);
        fflush(stdout);
        g_prof_start = now;
        g_prof_slept = 0;
    }
}

void port_sleep(int ms) {
    if (g_prev_buttons & PAD_L) ms /= 2;
    int64_t begin = platform_time_ms();
    int64_t end = begin + ms;
    for (;;) {
        int64_t now = platform_time_ms();
        int64_t left = end - now;
        if (left <= 0) break;
        float ox = g_cx, oy = g_cy;
        bool was_down = g_ptr_down;
        update_input();
        bool dirty = ox != g_cx || oy != g_cy || was_down != g_ptr_down;
        if (dirty && g_have_frame && now - g_last_present >= 15 && left > 12) {
            compose_and_show();
            continue;
        }
        int64_t t0 = platform_time_ms();
        platform_sleep_ms(left > 8 ? 8 : (int)left);
        profile(platform_time_ms() - t0);
    }
    profile(0);
    (void)begin;
}
