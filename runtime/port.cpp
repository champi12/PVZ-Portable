// Input translation (PSP buttons -> touch screen / phone keys) and frame presentation.
//
// The original game is a touch-screen MIDlet. On the PSP the analog stick (or D-pad) moves a
// cursor and X "taps" the screen; holding X while moving drags.
#include "classes.h"
#include "platform.h"
#include "port.h"

#include <cmath>
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

static uint32_t g_frame[SCREEN_W * SCREEN_H];
static uint32_t g_out[SCREEN_W * SCREEN_H];
static bool g_have_frame = false;

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
    {PAD_START, -6},
    {PAD_SELECT, -7},
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

static void compose_and_show() {
    memcpy(g_out, g_frame, sizeof g_out);
    if (g_cursor_visible && game_wants_cursor()) {
        int x0 = (int)g_cx, y0 = (int)g_cy;
        for (int y = 0; y < 19; y++) {
            int yy = y0 + y;
            if (yy < 0 || yy >= SCREEN_H) continue;
            for (int x = 0; x < 12; x++) {
                int xx = x0 + x;
                if (xx < 0 || xx >= SCREEN_W) continue;
                char c = CURSOR[y][x];
                if (c == '#') g_out[yy * SCREEN_W + xx] = 0xFF000000u;
                else if (c == '.') g_out[yy * SCREEN_W + xx] = g_ptr_down ? 0xFFFFE070u : 0xFFFFFFFFu;
            }
        }
    }
    platform_present(g_out);
}

void port_present(uint32_t* pixels) {
    memcpy(g_frame, pixels, sizeof g_frame);
    g_have_frame = true;
    compose_and_show();
}

// Thread.sleep(): keep the cursor responsive while the game waits between frames.
void port_sleep(int ms) {
    int64_t end = platform_time_ms() + ms;
    for (;;) {
        int64_t left = end - platform_time_ms();
        if (left <= 0) break;
        int step = left > 16 ? 16 : (int)left;
        platform_sleep_ms(step);
        update_input();
        if (g_have_frame) compose_and_show();
    }
}
