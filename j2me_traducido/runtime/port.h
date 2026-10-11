// Glue between the MIDP runtime and the game-specific presentation / input mapping.
#pragma once

#include <cstdint>

struct JObject;

// Fills up to max (type, a, b) triples with MIDP input events; returns the count.
int port_poll_events(int32_t* buf, int max);

// Presents a SCREEN_W x SCREEN_H frame (adds the cursor overlay).
void port_present(uint32_t* pixels);
// Presents the view_w x h top-left part of a game canvas, scaled to the screen.
void port_present_canvas(const uint32_t* pixels, int stride, int view_w, int h);

// Sleeps while keeping input and the cursor alive (used by Thread.sleep).
void port_sleep(int ms);

// Blits an ARGB region with a MIDP transform, alpha blended, clipped to [cx0,cx1)x[cy0,cy1).
void port_blit(uint32_t* dst, int dw, int dh, int cx0, int cy0, int cx1, int cy1, const uint32_t* src, int srcw,
               int sx, int sy, int w, int h, int tr, int x, int y, bool opaque = false);

// Game hooks (implemented in game/).
// Converts a screen position into the coordinates passed to Canvas.pointer*().
void game_screen_to_pointer(int sx, int sy, int* px, int* py);
// Whether the analog cursor should be drawn this frame.
bool game_wants_cursor();

// Profiling counters (milliseconds accumulated per section, printed with PVZ_PROFILE).
enum { PROF_LOGIC, PROF_RENDER, PROF_SCALE, PROF_PRESENT, PROF_BLIT, PROF_FILL, PROF_COUNT };
extern int64_t g_prof_us[PROF_COUNT];
int64_t port_time_us();

// Optional development hook: called for every image blit (weak, may be null).
extern void game_log_blit(JObject* g, JObject* src_pixels, int sx, int sy, int w, int h, int tr, int x, int y)
    __attribute__((weak));
