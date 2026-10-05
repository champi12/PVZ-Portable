/* Plants vs. Zombies - port nativo para PSP (base: version J2ME 4.6.0, audio de PC)
 * Objetivo: PSP-1000 (32 MB, 333 MHz). Sin SDL: sceGu + sceMp3 directamente. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <stdio.h>
#include "gfx.h"
#include "audio.h"
#include "input.h"
#include "game.h"
#include "reanim.h"

PSP_MODULE_INFO("PvZ PSP", PSP_MODULE_USER, 0, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);  /* todo el heap menos 1 MB para el sistema (PSP-1000 ~ 22 MB) */

static volatile int running = 1;

static int exit_cb(int a, int b, void *c) { running = 0; return 0; }
static int cb_thread(SceSize args, void *argp)
{
    int cbid = sceKernelCreateCallback("Exit Callback", exit_cb, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int main(int argc, char *argv[])
{
    SceUID th = sceKernelCreateThread("update_thread", cb_thread, 0x11, 0xFA0, 0, 0);
    if (th >= 0) sceKernelStartThread(th, 0, 0);

    scePowerSetClockFrequency(333, 333, 166);

    if (gfx_init("data/gfx.pak") != 0) {
        /* sin graficos no hay juego: sal limpio */
        sceKernelExitGame();
        return 0;
    }
    reanim_init("data/anim.pak");
    audio_init("data/sfx.pak");
    input_init();
    game_init();

    while (running && !game_quit_requested()) {
        input_update();
        game_update();
        gfx_begin(0xFF000000);
        game_draw();
        gfx_end();
    }

    game_shutdown();
    audio_shutdown();
    gfx_shutdown();
    sceKernelExitGame();
    return 0;
}
