// Native PSP backend: framebuffer output, sceCtrl input, sceAudio output, Memory Stick saves.
#include "platform.h"

#include <pspaudio.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspge.h>
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

// ---- display ----

static const int FB_STRIDE = 512;
static uint32_t* g_fb[2];
static int g_back = 1;

void platform_init() {
    int th = sceKernelCreateThread("cb_thread", callback_thread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, nullptr);
    if (th >= 0) sceKernelStartThread(th, 0, nullptr);

    scePowerSetClockFrequency(333, 333, 166);

    // Two 512-stride 32-bit framebuffers at the start of VRAM (uncached mirror).
    uint32_t* vram = (uint32_t*)(0x40000000 | (uintptr_t)sceGeEdramGetAddr());
    g_fb[0] = vram;
    g_fb[1] = vram + FB_STRIDE * SCREEN_H;
    memset(g_fb[0], 0, FB_STRIDE * SCREEN_H * 4 * 2);
    sceDisplaySetMode(0, SCREEN_W, SCREEN_H);
    sceDisplaySetFrameBuf(g_fb[0], FB_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);

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

void platform_sleep_ms(int ms) {
    if (ms > 0) sceKernelDelayThread(ms * 1000);
}

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
    pad->quit = g_quit;
}

void platform_present(const uint32_t* argb) {
    uint32_t* dst = g_fb[g_back];
    for (int y = 0; y < SCREEN_H; y++) {
        const uint32_t* s = argb + y * SCREEN_W;
        uint32_t* d = dst + y * FB_STRIDE;
        for (int x = 0; x < SCREEN_W; x++) {
            uint32_t p = s[x];
            // ARGB -> ABGR (the PSP framebuffer stores R in the low byte)
            d[x] = 0xFF000000u | ((p & 0xFF) << 16) | (p & 0xFF00) | ((p >> 16) & 0xFF);
        }
    }
    sceDisplaySetFrameBuf(dst, FB_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    g_back ^= 1;
    // Let the flip happen before the next frame is drawn into the other buffer.
    sceDisplayWaitVblankStart();
}

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
