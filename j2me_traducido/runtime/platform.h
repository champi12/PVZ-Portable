// Interface between the portable runtime and the hardware backend (PSP or PC).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Logical screen of the port. The PSP LCD is 480x272.
enum { SCREEN_W = 480, SCREEN_H = 272 };

enum PadButton : uint32_t {
    PAD_UP = 1u << 0,
    PAD_DOWN = 1u << 1,
    PAD_LEFT = 1u << 2,
    PAD_RIGHT = 1u << 3,
    PAD_CROSS = 1u << 4,
    PAD_CIRCLE = 1u << 5,
    PAD_SQUARE = 1u << 6,
    PAD_TRIANGLE = 1u << 7,
    PAD_L = 1u << 8,
    PAD_R = 1u << 9,
    PAD_START = 1u << 10,
    PAD_SELECT = 1u << 11,
};

struct PadState {
    uint32_t buttons;  // PadButton bits currently held
    int ax, ay;        // analog stick, -128..127 (0 = centred)
    // PC builds only: direct mouse input in screen coordinates.
    bool mouse_valid;
    int mouse_x, mouse_y;
    bool mouse_down;
    bool quit;
};

void platform_init();
void platform_shutdown();
[[noreturn]] void platform_fatal(const char* msg);

int64_t platform_time_ms();
int64_t platform_time_us();
void platform_sleep_ms(int ms);

void platform_read_pad(PadState* pad);
// Pixel format of all images: ARGB on PC, ABGR (the PSP's native 8888 layout) on the PSP.
#ifdef PIX_ABGR
#define PIX_FROM_ARGB(p) (((p) & 0xFF00FF00u) | (((p) >> 16) & 0xFFu) | (((p) & 0xFFu) << 16))
#else
#define PIX_FROM_ARGB(p) (p)
#endif
#define PIX_TO_ARGB(p) PIX_FROM_ARGB(p)

enum { CURSOR_W = 16, CURSOR_H = 32 };
struct PresentCursor {
    bool visible;
    int x, y;
    const uint32_t* image;  // CURSOR_W x CURSOR_H, 0 = transparent
};

// Shows a SCREEN_W x SCREEN_H frame.
void platform_present(const uint32_t* pixels);
// Shows the view_w x h top-left part of a canvas scaled to the whole screen, with the cursor.
// Returns false if the backend cannot scale (the runtime then scales on the CPU).
bool platform_present_canvas(const uint32_t* pixels, int stride, int view_w, int h, const PresentCursor* cursor);

// Save data (record stores).
bool platform_load_save(const char* name, std::vector<uint8_t>& out);
void platform_store_save(const char* name, const uint8_t* data, size_t size);
void platform_delete_save(const char* name);

// Audio: the backend pulls 44.1 kHz interleaved stereo samples from this callback
// on its own thread.
typedef void (*AudioMixFn)(int16_t* stereo, int frames);
void platform_audio_start(AudioMixFn fn);
void platform_audio_lock();
void platform_audio_unlock();

// Game resources (files of the original .jar), embedded in the executable.
const uint8_t* resource_find(const char* name, size_t* size);
