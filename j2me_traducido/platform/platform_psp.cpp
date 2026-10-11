// Native PSP backend: framebuffer output, sceCtrl input, sceAudio output, Memory Stick saves.
#include "platform.h"

#include <pspaudio.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psputils.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

PSP_MODULE_INFO("PvZ", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_MAIN_THREAD_STACK_SIZE_KB(512);
// Leave a little memory for thread stacks and the audio thread; everything else is heap.
PSP_HEAP_THRESHOLD_SIZE_KB(1024);

static volatile bool g_quit = false;
static const char* SAVE_DIR = "saves";

// ---- exit callback (HOME button) ----

static int exit_callback(int, int, void*) {
    g_quit = true;
    return 0;
}

static int callback_thread(SceSize, void*) {
    int cbid = sceKernelCreateCallback("exit_cb", exit_callback, nullptr);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

// ---- display (GE) ----
//
// Frames are drawn by the PSP's graphics engine: the game canvas (in main memory, native ABGR
// layout) is used directly as a texture and scaled to 480x272 with bilinear filtering, so the
// CPU never touches the framebuffer.

static const int FB_STRIDE = 512;
static unsigned int __attribute__((aligned(16))) g_list[16384];
static void* g_draw_fb;  // VRAM offset of the buffer being drawn

struct TexVertex {
    float u, v;
    float x, y, z;
};

void platform_init() {
    int th = sceKernelCreateThread("cb_thread", callback_thread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, nullptr);
    if (th >= 0) sceKernelStartThread(th, 0, nullptr);

    scePowerSetClockFrequency(333, 333, 166);

    // Two 512-stride 32-bit framebuffers at the start of VRAM.
    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, FB_STRIDE);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void*)(FB_STRIDE * SCREEN_H * 4), FB_STRIDE);
    sceGuOffset(2048 - SCREEN_W / 2, 2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    g_draw_fb = (void*)0;

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceIoMkdir(SAVE_DIR, 0777);
}

void platform_shutdown() {
    sceKernelExitGame();
}

void platform_fatal(const char* msg) {
    pspDebugScreenInit();
    pspDebugScreenPrintf("Plants vs. Zombies - fatal error:\n\n%s\n\nPress X to exit.", msg);
    SceCtrlData pad;
    for (;;) {
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_CROSS) break;
        if (g_quit) break;
    }
    sceKernelExitGame();
    for (;;) {}
}

int64_t platform_time_ms() {
    return (int64_t)(sceKernelGetSystemTimeWide() / 1000);
}

int64_t platform_time_us() {
    return (int64_t)sceKernelGetSystemTimeWide();
}

void platform_sleep_ms(int ms) {
    if (ms > 0) sceKernelDelayThread(ms * 1000);
}

#ifdef PVZ_PROFILE
// Test builds: "autoplay.txt" next to the EBOOT feeds scripted input (same format as the PC
// build: "<ms> down X Y", "<ms> up X Y", "<ms> press CROSS", "<ms> shot NAME"...) and
// "shot" writes the current frame as a raw 480x272 ARGB file.
#include <vector>
struct ScriptCmd {
    int64_t t;
    char cmd[16];
    char arg[32];
    int x, y;
};
static std::vector<ScriptCmd> g_script;
static size_t g_script_pos;
static PadState g_script_pad;
static bool g_script_loaded;
static const uint32_t* g_last_frame;
static void capture_frame();

static uint32_t button_by_name(const char* n) {
    static const struct { const char* n; uint32_t b; } T[] = {
        {"UP", PAD_UP}, {"DOWN", PAD_DOWN}, {"LEFT", PAD_LEFT}, {"RIGHT", PAD_RIGHT}, {"CROSS", PAD_CROSS},
        {"CIRCLE", PAD_CIRCLE}, {"SQUARE", PAD_SQUARE}, {"TRIANGLE", PAD_TRIANGLE}, {"L", PAD_L}, {"R", PAD_R},
        {"START", PAD_START}, {"SELECT", PAD_SELECT},
    };
    for (auto& e : T)
        if (!strcmp(n, e.n)) return e.b;
    return 0;
}

static void script_step(PadState* pad) {
    if (!g_script_loaded) {
        g_script_loaded = true;
        FILE* f = fopen("autoplay.txt", "r");
        if (f) {
            char line[128];
            while (fgets(line, sizeof line, f)) {
                ScriptCmd c;
                memset(&c, 0, sizeof c);
                long long t;
                if (sscanf(line, "%lld %15s", &t, c.cmd) < 2) continue;
                c.t = t;
                if (sscanf(line, "%lld %15s %d %d", &t, c.cmd, &c.x, &c.y) < 4) sscanf(line, "%lld %15s %31s", &t, c.cmd, c.arg);
                g_script.push_back(c);
            }
            fclose(f);
        }
    }
    if (g_script.empty()) return;
    while (g_script_pos < g_script.size() && g_script[g_script_pos].t <= platform_time_ms()) {
        ScriptCmd& c = g_script[g_script_pos++];
        if (!strcmp(c.cmd, "down") || !strcmp(c.cmd, "up") || !strcmp(c.cmd, "move")) {
            g_script_pad.mouse_valid = true;
            g_script_pad.mouse_x = c.x;
            g_script_pad.mouse_y = c.y;
            if (!strcmp(c.cmd, "down")) g_script_pad.mouse_down = true;
            if (!strcmp(c.cmd, "up")) g_script_pad.mouse_down = false;
        } else if (!strcmp(c.cmd, "press")) {
            g_script_pad.buttons |= button_by_name(c.arg);
        } else if (!strcmp(c.cmd, "release")) {
            g_script_pad.buttons &= ~button_by_name(c.arg);
        } else if (!strcmp(c.cmd, "shot") && g_last_frame) {
            capture_frame();
            char name[64];
            snprintf(name, sizeof name, "%s.raw", c.arg);
            FILE* f = fopen(name, "wb");
            if (f) {
                fwrite(g_last_frame, 4, SCREEN_W * SCREEN_H, f);
                fclose(f);
            }
        } else if (!strcmp(c.cmd, "quit")) {
            g_quit = true;
        }
    }
    pad->buttons |= g_script_pad.buttons;
    pad->mouse_valid = g_script_pad.mouse_valid;
    pad->mouse_x = g_script_pad.mouse_x;
    pad->mouse_y = g_script_pad.mouse_y;
    pad->mouse_down = g_script_pad.mouse_down;
}
#endif

void platform_read_pad(PadState* pad) {
    memset(pad, 0, sizeof *pad);
    SceCtrlData d;
    sceCtrlPeekBufferPositive(&d, 1);
    struct { unsigned psp; uint32_t b; } map[] = {
        {PSP_CTRL_UP, PAD_UP}, {PSP_CTRL_DOWN, PAD_DOWN}, {PSP_CTRL_LEFT, PAD_LEFT},
        {PSP_CTRL_RIGHT, PAD_RIGHT}, {PSP_CTRL_CROSS, PAD_CROSS}, {PSP_CTRL_CIRCLE, PAD_CIRCLE},
        {PSP_CTRL_SQUARE, PAD_SQUARE}, {PSP_CTRL_TRIANGLE, PAD_TRIANGLE}, {PSP_CTRL_LTRIGGER, PAD_L},
        {PSP_CTRL_RTRIGGER, PAD_R}, {PSP_CTRL_START, PAD_START}, {PSP_CTRL_SELECT, PAD_SELECT},
    };
    for (auto& m : map)
        if (d.Buttons & m.psp) pad->buttons |= m.b;
    pad->ax = (int)d.Lx - 128;
    pad->ay = (int)d.Ly - 128;
#ifdef PVZ_PROFILE
    script_step(pad);
#endif
    pad->quit = g_quit;
}

static void draw_sprite(float u0, float v0, float u1, float v1, float x0, float y0, float x1, float y1) {
    TexVertex* v = (TexVertex*)sceGuGetMemory(2 * sizeof(TexVertex));
    v[0].u = u0; v[0].v = v0; v[0].x = x0; v[0].y = y0; v[0].z = 0;
    v[1].u = u1; v[1].v = v1; v[1].x = x1; v[1].y = y1; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, nullptr, v);
}

#ifdef PVZ_PROFILE
static void remember_frame();
#endif

bool platform_present_canvas(const uint32_t* pixels, int stride, int view_w, int h, const PresentCursor* cursor) {
    sceKernelDcacheWritebackRange(pixels, stride * h * 4);
    sceGuStart(GU_DIRECT, g_list);
    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuDisable(GU_BLEND);
    bool scaled = view_w != SCREEN_W || h != SCREEN_H;
    sceGuTexFilter(scaled ? GU_LINEAR : GU_NEAREST, scaled ? GU_LINEAR : GU_NEAREST);
    // Textures are at most 512 texels wide: draw the canvas in vertical slices that start on
    // 16-byte boundaries. Each slice keeps the whole canvas row stride, so bilinear filtering
    // across slice borders reads the real neighbouring pixels.
    const int SLICE = 128;
    float sx = (float)SCREEN_W / view_w, sy = (float)SCREEN_H / h;
    for (int x0 = 0; x0 < view_w; x0 += SLICE) {
        int w = view_w - x0 < SLICE ? view_w - x0 : SLICE;
        sceGuTexImage(0, 512, 512, stride, pixels + x0);
        sceGuTexFlush();
        draw_sprite(0, 0, (float)w, (float)h, x0 * sx, 0, (x0 + w) * sx, h * sy);
    }
    if (cursor && cursor->visible) {
        sceKernelDcacheWritebackRange(cursor->image, CURSOR_W * CURSOR_H * 4);
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
        sceGuTexImage(0, CURSOR_W, CURSOR_H, CURSOR_W, cursor->image);
        sceGuTexFlush();
        draw_sprite(0, 0, CURSOR_W, CURSOR_H, cursor->x, cursor->y, cursor->x + CURSOR_W, cursor->y + CURSOR_H);
    }
    sceGuFinish();
    sceGuSync(0, 0);
#ifdef PVZ_PROFILE
    remember_frame();
#endif
    sceDisplayWaitVblankStart();
    g_draw_fb = sceGuSwapBuffers();
    return true;
}

void platform_present(const uint32_t* pixels) {
    platform_present_canvas(pixels, SCREEN_W, SCREEN_W, SCREEN_H, nullptr);
}

#ifdef PVZ_PROFILE
// The last frame shown, read back from VRAM for autoplay screenshots.
static uint32_t g_shot[SCREEN_W * SCREEN_H];
static void* g_shown_fb;
static void remember_frame() {
    g_shown_fb = g_draw_fb;
    g_last_frame = g_shot;
}
static void capture_frame() {
    const uint32_t* fb = (const uint32_t*)(0x44000000 | (uintptr_t)g_shown_fb);
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++) g_shot[y * SCREEN_W + x] = PIX_TO_ARGB(fb[y * FB_STRIDE + x]);
}
#endif

// ---- saves ----

static std::string save_path(const char* name) {
    return std::string(SAVE_DIR) + "/" + name + ".rms";
}

bool platform_load_save(const char* name, std::vector<uint8_t>& out) {
    FILE* f = fopen(save_path(name).c_str(), "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return true;
}

void platform_store_save(const char* name, const uint8_t* data, size_t size) {
    // Write to a temporary file first so a power-off never leaves a truncated save.
    std::string path = save_path(name), tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return;
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    if (!ok) return;
    sceIoRemove(path.c_str());
    sceIoRename(tmp.c_str(), path.c_str());
}

void platform_delete_save(const char* name) {
    sceIoRemove(save_path(name).c_str());
}

// ---- audio ----

static AudioMixFn g_mix;
static SceUID g_audio_sema = -1;
static const int AUDIO_SAMPLES = 1024;
static int16_t g_audio_buf[2][AUDIO_SAMPLES * 2] __attribute__((aligned(64)));

static int audio_thread(SceSize, void*) {
    int ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, AUDIO_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
    if (ch < 0) return 0;
    int cur = 0;
    while (!g_quit) {
        sceKernelWaitSema(g_audio_sema, 1, nullptr);
        g_mix(g_audio_buf[cur], AUDIO_SAMPLES);
        sceKernelSignalSema(g_audio_sema, 1);
        sceAudioOutputBlocking(ch, PSP_AUDIO_VOLUME_MAX, g_audio_buf[cur]);
        cur ^= 1;
    }
    sceAudioChRelease(ch);
    return 0;
}

void platform_audio_start(AudioMixFn fn) {
    g_mix = fn;
    g_audio_sema = sceKernelCreateSema("audio", 0, 1, 1, nullptr);
    int th = sceKernelCreateThread("audio", audio_thread, 0x12, 0x10000, PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU,
                                   nullptr);
    if (th >= 0) sceKernelStartThread(th, 0, nullptr);
}

void platform_audio_lock() {
    if (g_audio_sema >= 0) sceKernelWaitSema(g_audio_sema, 1, nullptr);
}

void platform_audio_unlock() {
    if (g_audio_sema >= 0) sceKernelSignalSema(g_audio_sema, 1);
}
