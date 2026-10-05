// Glue between the MIDP runtime and the game-specific presentation / input mapping.
#pragma once

#include <cstdint>

// Fills up to max (type, a, b) triples with MIDP input events; returns the count.
int port_poll_events(int32_t* buf, int max);

// Presents a SCREEN_W x SCREEN_H frame (adds the cursor overlay).
void port_present(uint32_t* pixels);

// Sleeps while keeping input and the cursor alive (used by Thread.sleep).
void port_sleep(int ms);

// Blits an ARGB region with a MIDP transform, alpha blended, clipped to [cx0,cx1)x[cy0,cy1).
void port_blit(uint32_t* dst, int dw, int dh, int cx0, int cy0, int cx1, int cy1, const uint32_t* src, int srcw,
               int sx, int sy, int w, int h, int tr, int x, int y);

// Game hooks (implemented in game/).
// Converts a screen position into the coordinates passed to Canvas.pointer*().
void game_screen_to_pointer(int sx, int sy, int* px, int* py);
// Whether the analog cursor should be drawn this frame.
bool game_wants_cursor();
