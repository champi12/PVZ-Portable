// PSP-specific changes to Plants vs. Zombies (J2ME 4.6.0, 320x480 touch build).
//
// The phone build renders every frame into a 480x320 landscape image and then rotates it
// pixel by pixel into the 320x480 portrait screen. The port presents the landscape image
// directly on the 480x272 PSP screen instead.
#include "classes.h"
#include "platform.h"
#include "port.h"

enum { GAME_W = 480, GAME_H = 320 };

typedef J_javax_microedition_lcdui_Graphics Gfx;
typedef J_javax_microedition_lcdui_Image Img;

static void present_frame(JObject* screen_graphics) {
    Img* src = (Img*)S_cc__a_Ljavax_microedition_lcdui_Image_;
    Img* dst = (Img*)((Gfx*)screen_graphics)->f_img_Ljavax_microedition_lcdui_Image_;
    const uint32_t* s = jadata<uint32_t>(src->f_pixels_AI);
    uint32_t* d = jadata<uint32_t>(dst->f_pixels_AI);
    // Vertical resample 320 -> 272 with linear filtering.
    for (int y = 0; y < SCREEN_H; y++) {
        int fy = ((y * 2 + 1) * GAME_H * 128) / (SCREEN_H * 2) - 64;  // 1/128 px
        if (fy < 0) fy = 0;
        int y0 = fy >> 7, w1 = fy & 127, w0 = 128 - w1;
        int y1 = y0 + 1 < GAME_H ? y0 + 1 : y0;
        const uint32_t* r0 = s + y0 * GAME_W;
        const uint32_t* r1 = s + y1 * GAME_W;
        uint32_t* o = d + y * SCREEN_W;
        for (int x = 0; x < SCREEN_W; x++) {
            uint32_t a = r0[x], b = r1[x];
            uint32_t rb = (((a & 0xFF00FF) * w0 + (b & 0xFF00FF) * w1) >> 7) & 0xFF00FF;
            uint32_t g = (((a & 0x00FF00) * w0 + (b & 0x00FF00) * w1) >> 7) & 0x00FF00;
            o[x] = 0xFF000000u | rb | g;
        }
    }
}

void M_cc__paint__Ljavax_microedition_lcdui_Graphics__V(JObject* self, JObject* g) {
    (void)self;
    if (!S_cc__d_Z) return;
    if (S_Game__a_Z) {
        M_t__a__Ljavax_microedition_lcdui_Graphics__V(S_cc__a_Ljavax_microedition_lcdui_Graphics_);
        present_frame(g);
    }
    if (S_cc__b_Z && !S_cc__a_Z) {
        S_cc__b_Z = 0;
    }
}

void game_screen_to_pointer(int sx, int sy, int* px, int* py) {
    int gx = sx;
    int gy = (sy * GAME_H + SCREEN_H / 2) / SCREEN_H;
    // The phone build rotates touch coordinates: portrait (px, py) == landscape (py, 319 - px).
    *px = GAME_H - 1 - gy;
    *py = gx;
}

bool game_wants_cursor() { return true; }
