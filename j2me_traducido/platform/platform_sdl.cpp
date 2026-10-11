// SDL2 backend, used for the PC build (development / testing).
//
// Environment variables (PC only):
//   PVZ_HEADLESS=1        run without a window on a virtual clock (as fast as possible)
//   PVZ_SCRIPT=file       scripted input, one command per line: "<ms> <cmd> [args]"
//                         cmds: move X Y | down X Y | up X Y | press BUTTON | release BUTTON |
//                               shot NAME | quit
//   PVZ_SHOTDIR=dir       where "shot" commands write PNG screenshots (default ".")
//   PVZ_SAVEDIR=dir       where record stores are written (default "saves")
#include "platform.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

static SDL_Window* g_window;
static SDL_Renderer* g_renderer;
static SDL_Texture* g_texture;
static SDL_GameController* g_pad;
static bool g_headless;
static int64_t g_virtual_ms;
static std::string g_savedir = "saves";
static std::string g_shotdir = ".";
static const uint32_t* g_last_frame;
static int g_scale = 2;

struct ScriptCmd {
    int64_t t;
    std::string cmd;
    int x, y;
    std::string arg;
};
static std::vector<ScriptCmd> g_script;
static size_t g_script_pos;
static PadState g_script_pad;

static uint32_t button_by_name(const std::string& n) {
    static const struct { const char* n; uint32_t b; } T[] = {
        {"UP", PAD_UP}, {"DOWN", PAD_DOWN}, {"LEFT", PAD_LEFT}, {"RIGHT", PAD_RIGHT},
        {"CROSS", PAD_CROSS}, {"CIRCLE", PAD_CIRCLE}, {"SQUARE", PAD_SQUARE},
        {"TRIANGLE", PAD_TRIANGLE}, {"L", PAD_L}, {"R", PAD_R}, {"START", PAD_START},
        {"SELECT", PAD_SELECT},
    };
    for (auto& e : T)
        if (n == e.n) return e.b;
    return 0;
}

static void load_script(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open script %s\n", path);
        exit(1);
    }
    char line[256];
    int64_t base = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char cmd[64] = {0}, arg[128] = {0};
        long long t = 0;
        int x = 0, y = 0;
        int n = sscanf(line, "%lld %63s %127s %d", &t, cmd, arg, &y);
        if (n < 2) continue;
        ScriptCmd c;
        // "+N" makes the time relative to the previous command
        if (line[0] == '+') base += t; else base = t;
        c.t = base;
        c.cmd = cmd;
        if (c.cmd == "move" || c.cmd == "down" || c.cmd == "up" || c.cmd == "tap") {
            sscanf(line, "%lld %63s %d %d", &t, cmd, &x, &y);
            c.x = x;
            c.y = y;
        } else {
            c.arg = arg;
        }
        g_script.push_back(c);
    }
    fclose(f);
}

static void save_png(const std::string& name) {
    if (!g_last_frame) return;
    std::vector<uint8_t> rgb(SCREEN_W * SCREEN_H * 3);
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint32_t p = g_last_frame[i];
        rgb[i * 3] = p >> 16;
        rgb[i * 3 + 1] = p >> 8;
        rgb[i * 3 + 2] = p;
    }
    std::string path = g_shotdir + "/" + name + ".png";
    stbi_write_png(path.c_str(), SCREEN_W, SCREEN_H, 3, rgb.data(), SCREEN_W * 3);
    fprintf(stderr, "[%lld ms] screenshot %s\n", (long long)g_virtual_ms, path.c_str());
}

static void run_script() {
    while (g_script_pos < g_script.size() && g_script[g_script_pos].t <= platform_time_ms()) {
        ScriptCmd& c = g_script[g_script_pos++];
        g_script_pad.mouse_valid = true;
        if (c.cmd == "move") {
            g_script_pad.mouse_x = c.x;
            g_script_pad.mouse_y = c.y;
        } else if (c.cmd == "down") {
            g_script_pad.mouse_x = c.x;
            g_script_pad.mouse_y = c.y;
            g_script_pad.mouse_down = true;
        } else if (c.cmd == "up") {
            g_script_pad.mouse_x = c.x;
            g_script_pad.mouse_y = c.y;
            g_script_pad.mouse_down = false;
        } else if (c.cmd == "press") {
            g_script_pad.buttons |= button_by_name(c.arg);
        } else if (c.cmd == "release") {
            g_script_pad.buttons &= ~button_by_name(c.arg);
        } else if (c.cmd == "shot") {
            save_png(c.arg);
        } else if (c.cmd == "quit") {
            g_script_pad.quit = true;
        }
    }
}

void platform_init() {
    if (const char* s = getenv("PVZ_SAVEDIR")) g_savedir = s;
    if (const char* s = getenv("PVZ_SHOTDIR")) g_shotdir = s;
    if (const char* s = getenv("PVZ_SCALE")) g_scale = atoi(s) > 0 ? atoi(s) : 1;
    g_headless = getenv("PVZ_HEADLESS") != nullptr;
    if (const char* s = getenv("PVZ_SCRIPT")) load_script(s);
#ifdef _WIN32
    std::string mk = "mkdir \"" + g_savedir + "\" 2>nul";
#else
    std::string mk = "mkdir -p '" + g_savedir + "'";
#endif
    if (system(mk.c_str())) {}
    if (g_headless) return;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) platform_fatal(SDL_GetError());
    g_window = SDL_CreateWindow("Plants vs. Zombies", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                SCREEN_W * g_scale, SCREEN_H * g_scale, 0);
    if (!g_window) platform_fatal(SDL_GetError());
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, 0);
    SDL_RenderSetLogicalSize(g_renderer, SCREEN_W, SCREEN_H);
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            g_pad = SDL_GameControllerOpen(i);
            break;
        }
    }
}

void platform_shutdown() {
    if (!g_headless) SDL_Quit();
}

void platform_fatal(const char* msg) {
    fprintf(stderr, "fatal: %s\n", msg);
    exit(1);
}

int64_t platform_time_ms() {
    if (g_headless) return g_virtual_ms;
    return (int64_t)SDL_GetTicks64();
}

int64_t platform_time_us() {
    if (g_headless) return g_virtual_ms * 1000;
    return (int64_t)(SDL_GetPerformanceCounter() * 1000000.0 / SDL_GetPerformanceFrequency());
}

void platform_sleep_ms(int ms) {
    if (g_headless) {
        g_virtual_ms += ms;
        return;
    }
    SDL_Delay(ms);
}

void platform_read_pad(PadState* pad) {
    memset(pad, 0, sizeof *pad);
    if (g_headless) {
        g_virtual_ms += 1;  // guarantee progress
        run_script();
        *pad = g_script_pad;
        if (g_script_pos >= g_script.size() && g_script.empty()) pad->quit = g_virtual_ms > 60000;
        return;
    }
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) pad->quit = true;
    }
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    struct { SDL_Scancode s; uint32_t b; } map[] = {
        {SDL_SCANCODE_UP, PAD_UP}, {SDL_SCANCODE_DOWN, PAD_DOWN}, {SDL_SCANCODE_LEFT, PAD_LEFT},
        {SDL_SCANCODE_RIGHT, PAD_RIGHT}, {SDL_SCANCODE_Z, PAD_CROSS}, {SDL_SCANCODE_X, PAD_CIRCLE},
        {SDL_SCANCODE_A, PAD_SQUARE}, {SDL_SCANCODE_S, PAD_TRIANGLE}, {SDL_SCANCODE_Q, PAD_L},
        {SDL_SCANCODE_W, PAD_R}, {SDL_SCANCODE_RETURN, PAD_START}, {SDL_SCANCODE_BACKSPACE, PAD_SELECT},
    };
    for (auto& m : map)
        if (k[m.s]) pad->buttons |= m.b;
    if (k[SDL_SCANCODE_ESCAPE]) pad->quit = true;
    if (g_pad) {
        struct { SDL_GameControllerButton c; uint32_t b; } bm[] = {
            {SDL_CONTROLLER_BUTTON_DPAD_UP, PAD_UP}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN, PAD_DOWN},
            {SDL_CONTROLLER_BUTTON_DPAD_LEFT, PAD_LEFT}, {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, PAD_RIGHT},
            {SDL_CONTROLLER_BUTTON_A, PAD_CROSS}, {SDL_CONTROLLER_BUTTON_B, PAD_CIRCLE},
            {SDL_CONTROLLER_BUTTON_X, PAD_SQUARE}, {SDL_CONTROLLER_BUTTON_Y, PAD_TRIANGLE},
            {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, PAD_L}, {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PAD_R},
            {SDL_CONTROLLER_BUTTON_START, PAD_START}, {SDL_CONTROLLER_BUTTON_BACK, PAD_SELECT},
        };
        for (auto& m : bm)
            if (SDL_GameControllerGetButton(g_pad, m.c)) pad->buttons |= m.b;
        pad->ax = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTX) / 256;
        pad->ay = SDL_GameControllerGetAxis(g_pad, SDL_CONTROLLER_AXIS_LEFTY) / 256;
    }
    int mx, my;
    Uint32 mb = SDL_GetMouseState(&mx, &my);
    if (SDL_GetMouseFocus() == g_window) {
        static int last_mx = -1, last_my = -1;
        static bool last_down = false;
        bool down = (mb & SDL_BUTTON_LMASK) != 0;
        // Only take over the cursor while the mouse is actually used.
        if (mx != last_mx || my != last_my || down != last_down) {
            pad->mouse_valid = true;
            pad->mouse_x = mx / g_scale;
            pad->mouse_y = my / g_scale;
            pad->mouse_down = down;
        }
        last_mx = mx;
        last_my = my;
        last_down = down;
        if (down) {
            pad->mouse_valid = true;
            pad->mouse_x = mx / g_scale;
            pad->mouse_y = my / g_scale;
            pad->mouse_down = true;
        }
    }
}

void platform_present(const uint32_t* argb) {
    g_last_frame = argb;
    if (g_headless) return;
    SDL_UpdateTexture(g_texture, nullptr, argb, SCREEN_W * 4);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);
    SDL_RenderPresent(g_renderer);
}

bool platform_present_canvas(const uint32_t*, int, int, int, const PresentCursor*) {
    return false;  // the runtime scales on the CPU
}

bool platform_load_save(const char* name, std::vector<uint8_t>& out) {
    std::string path = g_savedir + "/" + name + ".rms";
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return true;
}

void platform_store_save(const char* name, const uint8_t* data, size_t size) {
    std::string path = g_savedir + "/" + name + ".rms";
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    fwrite(data, 1, size, f);
    fclose(f);
}

void platform_delete_save(const char* name) {
    std::string path = g_savedir + "/" + name + ".rms";
    remove(path.c_str());
}

static AudioMixFn g_mix;
static SDL_AudioDeviceID g_audio_dev;

static void sdl_audio_cb(void*, Uint8* stream, int len) {
    g_mix((int16_t*)stream, len / 4);
}

void platform_audio_start(AudioMixFn fn) {
    if (g_headless) return;
    g_mix = fn;
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = sdl_audio_cb;
    g_audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (g_audio_dev) SDL_PauseAudioDevice(g_audio_dev, 0);
}

void platform_audio_lock() {
    if (g_audio_dev) SDL_LockAudioDevice(g_audio_dev);
}

void platform_audio_unlock() {
    if (g_audio_dev) SDL_UnlockAudioDevice(g_audio_dev);
}
