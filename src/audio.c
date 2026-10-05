/* audio.c
 * - Musica: decodificacion MP3 por hardware (Media Engine via sceMp3), streaming
 *   desde la Memory Stick con buffers pequenos (~20 KB de RAM). Casi 0% de CPU.
 * - Efectos: IMA-ADPCM 4 bits 22050 Hz mono decodificado al vuelo en un hilo
 *   mezclador (8 voces) hacia un canal estereo 44100 Hz.
 */
#include <pspkernel.h>
#include <pspaudio.h>
#include <pspmp3.h>
#include <psputility.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "audio.h"

/* ---------------- efectos ---------------- */
#define MAX_VOICES 8
#define MIX_SAMPLES 1024

typedef struct { u8 *data; u32 n; } Sfx;
typedef struct {
    const u8 *data; u32 n, pos;   /* pos en muestras 22050 Hz */
    int pred, idx, half, vol, prev, cur;
    volatile int active;
    int gen;
} Voice;

static const short step_tab[89] = {7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const signed char idx_tab[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};

static Sfx sfx_tab[SFX_COUNT];
static u32 sfx_offs[SFX_COUNT], sfx_sizes[SFX_COUNT];
static SceUID sfx_fd = -1;
static Voice voices[MAX_VOICES];
static short __attribute__((aligned(64))) mixbuf[2][MIX_SAMPLES * 2];
static volatile int audio_running;
static SceUID mix_thid = -1, mus_thid = -1;
static int sfx_master = 256;
static unsigned int sfx_mem;

static inline int adpcm_step(Voice *v)
{
    int code = (v->data[v->pos >> 1] >> ((v->pos & 1) << 2)) & 15;
    int st = step_tab[v->idx];
    int dq = st >> 3;
    if (code & 4) dq += st;
    if (code & 2) dq += st >> 1;
    if (code & 1) dq += st >> 2;
    v->pred += (code & 8) ? -dq : dq;
    if (v->pred > 32767) v->pred = 32767; else if (v->pred < -32768) v->pred = -32768;
    v->idx += idx_tab[code];
    if (v->idx < 0) v->idx = 0; else if (v->idx > 88) v->idx = 88;
    v->pos++;
    return v->pred;
}

static int mixer_thread(SceSize args, void *argp)
{
    int ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, MIX_SAMPLES, PSP_AUDIO_FORMAT_STEREO);
    int cur = 0;
    static int acc[MIX_SAMPLES];
    while (audio_running) {
        memset(acc, 0, sizeof(acc));
        for (int vi = 0; vi < MAX_VOICES; vi++) {
            Voice *v = &voices[vi];
            if (!v->active) continue;
            int vol = (v->vol * sfx_master) >> 8;
            for (int i = 0; i < MIX_SAMPLES; i++) {
                int s;
                if (!v->half) {               /* nueva muestra 22050 Hz */
                    if (v->pos >= v->n) { v->active = 0; break; }
                    v->prev = v->cur;
                    v->cur = adpcm_step(v);
                    s = (v->prev + v->cur) >> 1;  /* interpolacion lineal x2 */
                } else s = v->cur;
                v->half ^= 1;
                acc[i] += (s * vol) >> 8;
            }
        }
        short *out = mixbuf[cur];
        for (int i = 0; i < MIX_SAMPLES; i++) {
            int s = acc[i];
            if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
            out[i * 2] = out[i * 2 + 1] = (short)s;
        }
        sceAudioOutputBlocking(ch, PSP_AUDIO_VOLUME_MAX, out);
        cur ^= 1;
    }
    sceAudioChRelease(ch);
    return 0;
}

int sfx_preload(int id)
{
    if (id < 0 || id >= SFX_COUNT || sfx_fd < 0) return -1;
    if (sfx_tab[id].data) return 0;
    u32 size = sfx_sizes[id];
    u8 *buf = (u8 *)malloc(size);
    if (!buf) return -2;
    sceIoLseek32(sfx_fd, sfx_offs[id], PSP_SEEK_SET);
    sceIoRead(sfx_fd, buf, size);
    if (memcmp(buf, "SFX1", 4) != 0) { free(buf); return -3; }
    u32 n; memcpy(&n, buf + 4, 4);
    sfx_tab[id].data = buf;
    sfx_tab[id].n = n;
    sfx_mem += size;
    return 0;
}

void sfx_unload_all(void)
{
    for (int i = 0; i < MAX_VOICES; i++) voices[i].active = 0;
    sceKernelDelayThread(30000); /* deja terminar el bloque de mezcla en curso */
    for (int i = 0; i < SFX_COUNT; i++) if (sfx_tab[i].data) { free(sfx_tab[i].data); sfx_tab[i].data = NULL; }
    sfx_mem = 0;
}

static int sfx_last_handle;
void sfx_play_vol(int id, int vol)
{
    if (sfx_preload(id) != 0) return;
    int best = -1;
    for (int i = 0; i < MAX_VOICES; i++) if (!voices[i].active) { best = i; break; }
    if (best < 0) { /* roba la voz mas avanzada */
        u32 far = 0;
        for (int i = 0; i < MAX_VOICES; i++) if (voices[i].pos >= far) { far = voices[i].pos; best = i; }
    }
    Voice *v = &voices[best];
    v->active = 0;
    v->data = sfx_tab[id].data + 12;
    v->n = sfx_tab[id].n;
    v->pos = 0; v->pred = 0; v->idx = 0; v->half = 0; v->prev = 0; v->cur = 0;
    v->vol = vol;
    v->gen = (v->gen + 1) & 0xFFFF;
    sfx_last_handle = best | (v->gen << 8);
    v->active = 1;
}

int sfx_play(int id) { sfx_last_handle = -1; sfx_play_vol(id, 256); return sfx_last_handle; }
void sfx_stop(int h)
{
    if (h < 0) return;
    Voice *v = &voices[h & 0xFF];
    if (((h >> 8) & 0xFFFF) == v->gen) v->active = 0;
}
void sfx_stop_all(void) { for (int i = 0; i < MAX_VOICES; i++) voices[i].active = 0; }
void sfx_set_volume(int vol) { sfx_master = vol; }
unsigned int sfx_mem_used(void) { return sfx_mem; }

/* ---------------- musica (sceMp3) ---------------- */
static unsigned char __attribute__((aligned(64))) mp3_buf[16 * 1024];
static short __attribute__((aligned(64))) pcm_buf[16 * (1152 / 2)];
static char mus_req_path[256];
static volatile int mus_req, mus_req_loop, mus_stop_req, mus_playing;
static volatile int mus_vol = 256, mus_master = 256;
static int mp3_ok;

static int fill_stream(SceUID fd, int handle)
{
    unsigned char *dst; SceInt32 write, pos;
    if (sceMp3GetInfoToAddStreamData(handle, &dst, &write, &pos) < 0) return -1;
    if (sceIoLseek32(fd, pos, PSP_SEEK_SET) < 0) return -1;
    int rd = sceIoRead(fd, dst, write);
    if (rd < 0) return -1;
    sceMp3NotifyAddStreamData(handle, rd);
    return rd;
}

static int music_thread(SceSize args, void *argp)
{
    SceUID fd = -1; int handle = -1, src_ch = -1, last_samples = 0;
    while (audio_running) {
        if (mus_req || mus_stop_req) {
            if (handle >= 0) { sceMp3ReleaseMp3Handle(handle); handle = -1; }
            if (fd >= 0) { sceIoClose(fd); fd = -1; }
            mus_playing = 0;
            if (mus_stop_req) { mus_stop_req = 0; mus_req = 0; }
            if (mus_req && mp3_ok) {
                mus_req = 0;
                fd = sceIoOpen(mus_req_path, PSP_O_RDONLY, 0);
                if (fd >= 0) {
                    SceMp3InitArg a;
                    a.mp3StreamStart = 0;
                    a.mp3StreamEnd = sceIoLseek32(fd, 0, PSP_SEEK_END);
                    a.mp3Buf = mp3_buf; a.mp3BufSize = sizeof(mp3_buf);
                    a.pcmBuf = (SceUChar8 *)pcm_buf; a.pcmBufSize = sizeof(pcm_buf);
                    handle = sceMp3ReserveMp3Handle(&a);
                    if (handle >= 0 && fill_stream(fd, handle) >= 0 && sceMp3Init(handle) >= 0) {
                        sceMp3SetLoopNum(handle, mus_req_loop ? -1 : 0);
                        mus_playing = 1;
                    } else {
                        if (handle >= 0) sceMp3ReleaseMp3Handle(handle);
                        handle = -1; sceIoClose(fd); fd = -1;
                    }
                }
            }
        }
        if (handle < 0) { sceKernelDelayThread(10000); continue; }
        if (sceMp3CheckStreamDataNeeded(handle) > 0) fill_stream(fd, handle);
        short *buf; int bytes = sceMp3Decode(handle, &buf);
        if (bytes <= 0) { /* fin de la cancion (sin loop) o error */
            sceMp3ReleaseMp3Handle(handle); handle = -1; sceIoClose(fd); fd = -1; mus_playing = 0;
            continue;
        }
        int samples = bytes / 4;  /* estereo 16 bits */
        if (src_ch < 0 || samples != last_samples) {
            if (src_ch >= 0) sceAudioSRCChRelease();
            src_ch = sceAudioSRCChReserve(samples, 44100, 2);
            last_samples = samples;
        }
        sceAudioSRCOutputBlocking((((mus_vol * mus_master) >> 8) * PSP_AUDIO_VOLUME_MAX) >> 8, buf);
    }
    if (handle >= 0) sceMp3ReleaseMp3Handle(handle);
    if (fd >= 0) sceIoClose(fd);
    if (src_ch >= 0) sceAudioSRCChRelease();
    return 0;
}

void music_play(const char *path, int loop)
{
    /* En PSP los hilos creados por el juego no tienen directorio de trabajo: el hilo de
     * musica necesita la ruta completa (ms0:/PSP/GAME/PVZ/data/music/...). */
    char full[256];
    if (!strchr(path, ':')) {
        char cwd[200];
        if (getcwd(cwd, sizeof(cwd))) {
            snprintf(full, sizeof(full), "%s/%s", cwd, path);
            path = full;
        }
    }
    while (mus_req) sceKernelDelayThread(1000);   /* que el hilo recoja la peticion anterior */
    snprintf(mus_req_path, sizeof(mus_req_path), "%s", path);
    mus_req_loop = loop;
    mus_req = 1;
}
void music_stop(void) { mus_stop_req = 1; }
void music_set_volume(int vol) { mus_vol = vol; }
void music_set_master(int vol) { mus_master = vol; }
int music_is_playing(void) { return mus_playing; }

int audio_init(const char *sfx_pak)
{
    sfx_fd = sceIoOpen(sfx_pak, PSP_O_RDONLY, 0);
    if (sfx_fd >= 0) {
        char hdr[8]; int n;
        sceIoRead(sfx_fd, hdr, 8);
        memcpy(&n, hdr + 4, 4);
        if (n > SFX_COUNT) n = SFX_COUNT;
        for (int i = 0; i < n; i++) {
            u32 e[2]; sceIoRead(sfx_fd, e, 8);
            sfx_offs[i] = e[0]; sfx_sizes[i] = e[1];
        }
    }
    /* modulos del Media Engine para MP3 */
    sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
    sceUtilityLoadModule(PSP_MODULE_AV_MP3);
    mp3_ok = sceMp3InitResource() >= 0;
    audio_running = 1;
    mix_thid = sceKernelCreateThread("sfx_mixer", mixer_thread, 0x12, 0x4000, PSP_THREAD_ATTR_USER, NULL);
    if (mix_thid >= 0) sceKernelStartThread(mix_thid, 0, NULL);
    mus_thid = sceKernelCreateThread("music", music_thread, 0x12, 0x4000, PSP_THREAD_ATTR_USER, NULL);
    if (mus_thid >= 0) sceKernelStartThread(mus_thid, 0, NULL);
    return 0;
}

void audio_shutdown(void)
{
    audio_running = 0;
    if (mix_thid >= 0) { sceKernelWaitThreadEnd(mix_thid, NULL); sceKernelDeleteThread(mix_thid); }
    if (mus_thid >= 0) { sceKernelWaitThreadEnd(mus_thid, NULL); sceKernelDeleteThread(mus_thid); }
    if (mp3_ok) sceMp3TermResource();
    sfx_unload_all();
    if (sfx_fd >= 0) sceIoClose(sfx_fd);
}
