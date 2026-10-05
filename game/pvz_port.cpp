// PSP-specific changes to Plants vs. Zombies (J2ME 4.6.0, 320x480 touch build).
//
// The phone build renders every frame into a 480x320 landscape image and then rotates it
// pixel by pixel into the 320x480 portrait screen. The port presents the landscape image
// directly on the 480x272 PSP screen instead.
#include "classes.h"
#include "platform.h"
#include "port.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

typedef J_javax_microedition_lcdui_Graphics Gfx;
typedef J_javax_microedition_lcdui_Image Img;

// Size of the game's landscape canvas (480x320 on the phone, see game/patches.txt).
static int game_w() {
    Img* src = (Img*)S_cc__a_Ljavax_microedition_lcdui_Image_;
    return src ? src->f_width_I : SCREEN_W;
}
static int game_h() {
    Img* src = (Img*)S_cc__a_Ljavax_microedition_lcdui_Image_;
    return src ? src->f_height_I : SCREEN_H;
}


// Part of the game canvas shown on screen. Most screens lay themselves out on the whole
// widened canvas; the main-menu artwork (drawn by bi.b()) is only 480 px wide, so frames that
// show it are taken from that part of the canvas.
static int g_view_w = 0;
static bool g_menu_art_drawn = false;

void M_bi__b__Ljavax_microedition_lcdui_Graphics__V(JObject* g) {
    g_menu_art_drawn = true;
    M_bi__b__Ljavax_microedition_lcdui_Graphics__V__orig(g);
}

static int choose_view_width(int canvas_w) {
    if (canvas_w > 480 && g_menu_art_drawn) return 480;
    return canvas_w;
}

static void present_frame(JObject* screen_graphics) {
    Img* src = (Img*)S_cc__a_Ljavax_microedition_lcdui_Image_;
    Img* dst = (Img*)((Gfx*)screen_graphics)->f_img_Ljavax_microedition_lcdui_Image_;
    const uint32_t* s = jadata<uint32_t>(src->f_pixels_AI);
    uint32_t* d = jadata<uint32_t>(dst->f_pixels_AI);
    const int SW = src->f_width_I, GH = src->f_height_I;
    const int GW = choose_view_width(SW);
    g_view_w = GW;
    if (SW == SCREEN_W && GH == SCREEN_H) {
        memcpy(d, s, SCREEN_W * SCREEN_H * 4);
        return;
    }
    // Bilinear resample GW x GH -> SCREEN_W x SCREEN_H (fixed point, 1/128 px).
    static int xs[SCREEN_W], xw[SCREEN_W];
    static int cached_gw = -1;
    if (cached_gw != GW) {
        for (int x = 0; x < SCREEN_W; x++) {
            int fx = ((x * 2 + 1) * GW * 128) / (SCREEN_W * 2) - 64;
            if (fx < 0) fx = 0;
            xs[x] = fx >> 7;
            xw[x] = fx & 127;
            if (xs[x] >= GW - 1) { xs[x] = GW - 2; xw[x] = 128; }
        }
        cached_gw = GW;
    }
    for (int y = 0; y < SCREEN_H; y++) {
        int fy = ((y * 2 + 1) * GH * 128) / (SCREEN_H * 2) - 64;
        if (fy < 0) fy = 0;
        int y0 = fy >> 7, wy = fy & 127;
        int y1 = y0 + 1 < GH ? y0 + 1 : y0;
        const uint32_t* r0 = s + y0 * SW;
        const uint32_t* r1 = s + y1 * SW;
        uint32_t* o = d + y * SCREEN_W;
        for (int x = 0; x < SCREEN_W; x++) {
            int sx = xs[x], wx = xw[x];
            uint32_t a = r0[sx], b = r0[sx + 1], c = r1[sx], e = r1[sx + 1];
            // horizontal
            uint32_t t_rb = (((a & 0xFF00FF) * (128 - wx) + (b & 0xFF00FF) * wx) >> 7) & 0xFF00FF;
            uint32_t t_g = (((a & 0x00FF00) * (128 - wx) + (b & 0x00FF00) * wx) >> 7) & 0x00FF00;
            uint32_t b_rb = (((c & 0xFF00FF) * (128 - wx) + (e & 0xFF00FF) * wx) >> 7) & 0xFF00FF;
            uint32_t b_g = (((c & 0x00FF00) * (128 - wx) + (e & 0x00FF00) * wx) >> 7) & 0x00FF00;
            // vertical
            uint32_t rb = ((t_rb * (128 - wy) + b_rb * wy) >> 7) & 0xFF00FF;
            uint32_t g = ((t_g * (128 - wy) + b_g * wy) >> 7) & 0x00FF00;
            o[x] = 0xFF000000u | rb | g;
        }
    }
}

// ---- Loading screens --------------------------------------------------------------------
//
// ce.a(steps, progress, flag, minMillis) starts a loading sequence. The phone build shows a
// loading screen for at least minMillis and runs one loading step per frame. The port drops
// the artificial minimum duration everywhere, and for every load after the initial one
// (starting a level, retrying, next level...) runs all steps at once inside a single frame,
// so no loading screen is shown at all.
static int g_load_count = 0;

void M_ce__a__Ljava_lang_String_Ljava_lang_String_ZJ_V(JObject* steps, JObject* progress, int32_t flag,
                                                       int64_t min_ms) {
    (void)min_ms;
    M_ce__a__Ljava_lang_String_Ljava_lang_String_ZJ_V__orig(steps, progress, flag, 0);
    if (g_load_count++ > 0) S_ce__d_Z = 1;  // load everything in one go
    if (getenv("PVZ_DEBUG")) fprintf(stderr, "[%lld] load #%d start\n", (long long)platform_time_ms(), g_load_count);
}

static bool instant_loading_active() { return S_ce__a_Z && S_ce__d_Z; }

void M_cc__paint__Ljavax_microedition_lcdui_Graphics__V(JObject* self, JObject* g) {
    (void)self;
    if (!S_cc__d_Z) return;
    // Keep showing the previous frame instead of a loading screen.
    if (instant_loading_active()) {
        if (getenv("PVZ_DEBUG")) fprintf(stderr, "[%lld] skip loading frame\n", (long long)platform_time_ms());
        return;
    }
    if (S_Game__a_Z) {
        g_menu_art_drawn = false;
        M_t__a__Ljavax_microedition_lcdui_Graphics__V(S_cc__a_Ljavax_microedition_lcdui_Graphics_);
        if (getenv("PVZ_STATE")) {
            static int last = -12345;
            int key = S_bt__a_I * 10000 + S_bt__b_I * 10 + 0;
            if (key != last) {
                fprintf(stderr, "[%lld] state bt.a=%d bt.b=%d screen=%d\n", (long long)platform_time_ms(), S_bt__a_I,
                        S_bt__b_I, S_bt__b_I >= 0 ? M_bl__b__I_I(S_bt__b_I) : -1);
                last = key;
            }
        }
        present_frame(g);
    }
    if (S_cc__b_Z && !S_cc__a_Z) {
        S_cc__b_Z = 0;
    }
}

void game_screen_to_pointer(int sx, int sy, int* px, int* py) {
    const int GAME_H = game_h();
    int vw = g_view_w ? g_view_w : game_w();
    int gx = (sx * vw + SCREEN_W / 2) / SCREEN_W;
    int gy = (sy * GAME_H + SCREEN_H / 2) / SCREEN_H;
    // The phone build rotates touch coordinates: portrait (px, py) == landscape (py, 319 - px).
    *px = GAME_H - 1 - gy;
    *py = gx;
}

bool game_wants_cursor() { return true; }
