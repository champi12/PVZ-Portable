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

static void present_frame() {
    Img* src = (Img*)S_cc__a_Ljavax_microedition_lcdui_Image_;
    g_view_w = choose_view_width(src->f_width_I);
    port_present_canvas(jadata<uint32_t>(src->f_pixels_AI), src->f_width_I, g_view_w, src->f_height_I);
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
    (void)g;
    if (!S_cc__d_Z) return;
    // Keep showing the previous frame instead of a loading screen.
    if (instant_loading_active()) {
        static const bool debug = getenv("PVZ_DEBUG") != nullptr;
        if (debug) fprintf(stderr, "[%lld] skip loading frame\n", (long long)platform_time_ms());
        return;
    }
    if (S_Game__a_Z) {
        g_menu_art_drawn = false;
        // Development aid: PVZ_LEVEL=n jumps to level n (0 = 1-1, 10 = 2-1...) on the main menu.
        static const char* jump = getenv("PVZ_LEVEL");
        if (jump && S_bt__b_I == 3) {
            S_bo__d_I = atoi(jump);
            jump = nullptr;
        }
        int64_t t0 = port_time_us();
        M_t__a__Ljavax_microedition_lcdui_Graphics__V(S_cc__a_Ljavax_microedition_lcdui_Graphics_);
        int64_t t1 = port_time_us();
        g_prof_us[PROF_RENDER] += t1 - t0;
        static const bool draw_stats = getenv("PVZ_DRAWSTATS") != nullptr;
        if (draw_stats) {
            extern int64_t g_stat_fill_calls, g_stat_fill_px, g_stat_blit_calls, g_stat_blit_px, g_stat_blit_tr_px;
            static int frames = 0;
            if (++frames % 30 == 0) {
                fprintf(stderr, "[%lld] per frame: fill %lld calls %lld px, blit %lld calls %lld px (%lld transformed)\n",
                        (long long)platform_time_ms(), g_stat_fill_calls / 30, g_stat_fill_px / 30,
                        g_stat_blit_calls / 30, g_stat_blit_px / 30, g_stat_blit_tr_px / 30);
                g_stat_fill_calls = g_stat_fill_px = g_stat_blit_calls = g_stat_blit_px = g_stat_blit_tr_px = 0;
            }
        }
        static const bool trace_state = getenv("PVZ_STATE") != nullptr;
        if (trace_state) {
            static int last = -12345;
            int key = S_bt__a_I * 10000 + S_bt__b_I * 10 + 0;
            if (key != last) {
                fprintf(stderr, "[%lld] state bt.a=%d bt.b=%d screen=%d\n", (long long)platform_time_ms(), S_bt__a_I,
                        S_bt__b_I, S_bt__b_I >= 0 ? M_bl__b__I_I(S_bt__b_I) : -1);
                last = key;
            }
        }
        int64_t t2 = port_time_us();
        present_frame();
        g_prof_us[PROF_SCALE] += port_time_us() - t2;
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

// Game logic step (profiling wrapper).
void M_t__b___V() {
    int64_t t0 = port_time_us();
    M_t__b___V__orig();
    g_prof_us[PROF_LOGIC] += port_time_us() - t0;
}
