/*
  PSP: musica de PvZ pre-renderizada (tools/pcport/render_music.py).

  Sustituye a libopenmpt, que con el .mo3 decodificado necesitaria ~26 MB. Imita lo que el juego le pide
  a un modulo: saltar a un orden, decir en que orden va y el volumen por canal. Cada melodia es un
  archivo music/tune_XX.pvzm (XX = orden de inicio) con varias pistas IMA ADPCM mono que se mezclan
  segun el volumen de sus canales.

  El modulo se "carga" con un bloque de memoria que empieza por "PVZD" seguido de la carpeta.
*/

#ifdef MUSIC_PVZM

#include "SDL_rwops.h"
#include "music_pvzm.h"
#ifdef PSP_MUSIC_LOG
#include <stdio.h>
#endif

#define PVZM_BLOCK 1024
#define PVZM_MAX_STEMS 8
#define PVZM_MAX_ORDERS 256

typedef struct {
    Uint32 pos;
    Uint16 order;
} PVZM_Order;

typedef struct {
    char dir[64];
    SDL_RWops *file;
    int start;                      /* orden de inicio del archivo abierto, -1 = ninguno */
    int nstems;
    Uint32 nsamples, loop;
    int norders;
    PVZM_Order orders[PVZM_MAX_ORDERS];
    int stem_channel[PVZM_MAX_STEMS];  /* primer canal de cada pista: su volumen es el de la pista */
    Uint32 data_offset;
    Uint32 pos;                     /* muestra actual */
    int block;                      /* bloque decodificado en `pcm`, -1 = ninguno */
    Sint16 pcm[PVZM_MAX_STEMS][PVZM_BLOCK];
    Uint8 raw[PVZM_MAX_STEMS * (PVZM_BLOCK / 2 + 4)];
    int chanvol[64];
    int volume;
    int play_count;
    SDL_AudioStream *stream;
    Sint16 out[PVZM_BLOCK];
} PVZM_Music;

static const int pvzm_step[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
    449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
    9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};
static const int pvzm_index[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static void PVZM_Close(PVZM_Music *music)
{
    if (music->file) {
        SDL_RWclose(music->file);
        music->file = NULL;
    }
    music->start = -1;
    music->nsamples = 0;
    music->block = -1;
}

/* abre music/tune_XX.pvzm; devuelve 0 si existe */
static int PVZM_Open(PVZM_Music *music, int start)
{
    char path[96];
    Uint8 head[24];
    SDL_RWops *f;
    int i;

    if (music->file && music->start == start)
        return 0;
    SDL_snprintf(path, sizeof(path), "%stune_%02X.pvzm", music->dir, start);
    f = SDL_RWFromFile(path, "rb");
    if (!f)
        return -1;
    if (SDL_RWread(f, head, 1, 24) != 24 || SDL_memcmp(head, "PVZM", 4) != 0) {
        SDL_RWclose(f);
        return -1;
    }
    PVZM_Close(music);
#ifdef PSP_MUSIC_LOG
    { FILE *lf = fopen("mem.log", "a"); if (lf) { fprintf(lf, "MUSICA abre %s pistas=%d\n", path, SDL_SwapLE16(*(Uint16 *)(head + 6))); fclose(lf); } }
#endif
    music->file = f;
    music->start = start;
    music->nstems = SDL_SwapLE16(*(Uint16 *)(head + 6));
    music->nsamples = SDL_SwapLE32(*(Uint32 *)(head + 12));
    music->loop = SDL_SwapLE32(*(Uint32 *)(head + 16));
    music->norders = (int)SDL_SwapLE32(*(Uint32 *)(head + 20));
    if (music->nstems > PVZM_MAX_STEMS)
        music->nstems = PVZM_MAX_STEMS;
    for (i = 0; i < music->nstems; i++) {
        Uint32 mask = SDL_ReadLE32(f);
        int ch = 0;
        while (ch < 31 && !(mask & (1u << ch)))
            ch++;
        music->stem_channel[i] = ch;
    }
    for (i = 0; i < music->norders; i++) {
        Uint32 pos = SDL_ReadLE32(f);
        Uint16 ord = SDL_ReadLE16(f);
        SDL_ReadLE16(f);
        if (i < PVZM_MAX_ORDERS) {
            music->orders[i].pos = pos;
            music->orders[i].order = ord;
        }
    }
    if (music->norders > PVZM_MAX_ORDERS)
        music->norders = PVZM_MAX_ORDERS;
    music->data_offset = (Uint32)SDL_RWtell(f);
    music->pos = 0;
    music->block = -1;
    return 0;
}

static void PVZM_Decode(PVZM_Music *music, int block)
{
    const int size = PVZM_BLOCK / 2 + 4;
    int s, k;

    music->block = block;
    SDL_RWseek(music->file, music->data_offset + (Sint64)block * size * music->nstems, RW_SEEK_SET);
    if (SDL_RWread(music->file, music->raw, size, music->nstems) != (size_t)music->nstems) {
        SDL_memset(music->pcm, 0, sizeof(music->pcm));
        return;
    }
    for (s = 0; s < music->nstems; s++) {
        const Uint8 *p = music->raw + s * size;
        int pred = (Sint16)(p[0] | (p[1] << 8));
        int idx = p[2] > 88 ? 88 : p[2];
        Sint16 *out = music->pcm[s];
        p += 4;
        for (k = 0; k < PVZM_BLOCK; k++) {
            int code = (k & 1) ? (p[k >> 1] >> 4) : (p[k >> 1] & 15);
            int step = pvzm_step[idx];
            int delta = step >> 3;
            if (code & 4) delta += step;
            if (code & 2) delta += step >> 1;
            if (code & 1) delta += step >> 2;
            pred = (code & 8) ? pred - delta : pred + delta;
            if (pred < -32768) pred = -32768; else if (pred > 32767) pred = 32767;
            idx += pvzm_index[code & 7];
            if (idx < 0) idx = 0; else if (idx > 88) idx = 88;
            out[k] = (Sint16)pred;
        }
    }
}

static int PVZM_Open_(const SDL_AudioSpec *spec)
{
    (void)spec;
    return 0;
}

static void PVZM_Delete(void *context);
static int PVZM_GetOrder(void *context, int *order);

static void *PVZM_CreateFromRW(SDL_RWops *src, int freesrc)
{
    PVZM_Music *music;
    char head[68];
    size_t n;
    int i;

    SDL_memset(head, 0, sizeof(head));
    n = SDL_RWread(src, head, 1, sizeof(head) - 1);
    if (n < 4 || SDL_memcmp(head, "PVZD", 4) != 0)
        return NULL;
    music = (PVZM_Music *)SDL_calloc(1, sizeof(PVZM_Music));
    if (!music) {
        SDL_OutOfMemory();
        return NULL;
    }
    for (i = 4; i < (int)n && head[i] > ' '; i++)
        ;
    head[i] = '\0';
    SDL_strlcpy(music->dir, head + 4, sizeof(music->dir));
    music->start = -1;
    music->block = -1;
    music->volume = MIX_MAX_VOLUME;
    for (i = 0; i < 64; i++)
        music->chanvol[i] = 128;
    music->stream = SDL_NewAudioStream(AUDIO_S16SYS, 1, 22050, music_spec.format, music_spec.channels, music_spec.freq);
    if (!music->stream) {
        PVZM_Delete(music);
        return NULL;
    }
    if (freesrc)
        SDL_RWclose(src);
    return music;
}

static void PVZM_SetVolume(void *context, int volume)
{
    ((PVZM_Music *)context)->volume = volume;
}

static int PVZM_GetVolume(void *context)
{
    return ((PVZM_Music *)context)->volume;
}

static int PVZM_Play(void *context, int play_count)
{
    PVZM_Music *music = (PVZM_Music *)context;
    music->play_count = play_count;
    music->pos = 0;
    SDL_AudioStreamClear(music->stream);
    return 0;
}

static void PVZM_Stop(void *context)
{
    SDL_AudioStreamClear(((PVZM_Music *)context)->stream);
}

static int PVZM_GetSome(void *context, void *data, int bytes, SDL_bool *done)
{
    PVZM_Music *music = (PVZM_Music *)context;
    int filled, i, s, count;

    filled = SDL_AudioStreamGet(music->stream, data, bytes);
    if (filled != 0)
        return filled;
    if (!music->play_count) {
        *done = SDL_TRUE;
        return 0;
    }
    if (!music->file || music->nsamples == 0) {
        /* sin melodia (p. ej. los tambores de la noche antes de entrar): silencio */
        SDL_memset(music->out, 0, sizeof(music->out));
        SDL_AudioStreamPut(music->stream, music->out, 256 * 2);
        return 0;
    }
    if (music->pos >= music->nsamples)
        music->pos = music->loop < music->nsamples ? music->loop : 0;
    if ((int)(music->pos / PVZM_BLOCK) != music->block)
        PVZM_Decode(music, (int)(music->pos / PVZM_BLOCK));
    i = (int)(music->pos % PVZM_BLOCK);
    count = PVZM_BLOCK - i;
    if ((Uint32)count > music->nsamples - music->pos)
        count = (int)(music->nsamples - music->pos);
    {
        int vol[PVZM_MAX_STEMS];
        for (s = 0; s < music->nstems; s++)
            vol[s] = music->chanvol[music->stem_channel[s]];
        for (filled = 0; filled < count; filled++) {
            int v = 0;
            for (s = 0; s < music->nstems; s++)
                v += music->pcm[s][i + filled] * vol[s];
            v >>= 7;
            music->out[filled] = (Sint16)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
        }
    }
    music->pos += count;
#ifdef PSP_MUSIC_LOG
    if ((music->pos / PVZM_BLOCK) % 200 == 0 && i == 0) {
        int o; FILE *lf; PVZM_GetOrder(music, &o); lf = fopen("mem.log", "a");
        if (lf) { fprintf(lf, "MUSICA %02X pos=%u orden=%d vol=%d v0=%d pico=%d\n", music->start, (unsigned)music->pos, o, music->volume, music->chanvol[music->stem_channel[0]], music->out[100]); fclose(lf); }
    }
#endif
    if (SDL_AudioStreamPut(music->stream, music->out, count * 2) < 0)
        return -1;
    return 0;
}

static int PVZM_GetAudio(void *context, void *data, int bytes)
{
    PVZM_Music *music = (PVZM_Music *)context;
    return music_pcm_getaudio(context, data, bytes, music->volume, PVZM_GetSome);
}

/* orden empaquetado (16 bits bajos); busca un archivo que empiece ahi o el orden en el abierto */
static int PVZM_Jump(void *context, int order)
{
    PVZM_Music *music = (PVZM_Music *)context;
    int ord = order & 0xFFFF, i;

    SDL_AudioStreamClear(music->stream);
    if (PVZM_Open(music, ord) == 0) {
        music->pos = 0;
        return 0;
    }
    for (i = 0; i < music->norders; i++) {
        if (music->orders[i].order == ord) {
            music->pos = music->orders[i].pos;
            return 0;
        }
    }
    /* el orden no esta en ninguna melodia (p. ej. tambores en espera): silencio */
    PVZM_Close(music);
    return 0;
}

static int PVZM_GetOrder(void *context, int *order)
{
    PVZM_Music *music = (PVZM_Music *)context;
    int i, ord = music->start < 0 ? 0 : music->start;
    for (i = 0; i < music->norders && music->orders[i].pos <= music->pos; i++)
        ord = music->orders[i].order;
    *order = ord;
    return 0;
}

static int PVZM_SetChannelVolume(void *context, int channel, int volume)
{
    PVZM_Music *music = (PVZM_Music *)context;
    if (channel >= 0 && channel < 64)
        music->chanvol[channel] = volume;
    return 0;
}

static double PVZM_Tell(void *context)
{
    return ((PVZM_Music *)context)->pos / 22050.0;
}

static void PVZM_Delete(void *context)
{
    PVZM_Music *music = (PVZM_Music *)context;
    PVZM_Close(music);
    if (music->stream)
        SDL_FreeAudioStream(music->stream);
    SDL_free(music);
}

Mix_MusicInterface Mix_MusicInterface_PVZM =
{
    "PVZM",
    MIX_MUSIC_OPENMPT,
    MUS_MOD,
    SDL_FALSE,
    SDL_FALSE,

    NULL,   /* Load */
    PVZM_Open_,
    PVZM_CreateFromRW,
    NULL,   /* CreateFromRWex [MIXER-X]*/
    NULL,   /* CreateFromFile */
    NULL,   /* CreateFromFileEx [MIXER-X]*/
    PVZM_SetVolume,
    PVZM_GetVolume,
    PVZM_Play,
    NULL,   /* IsPlaying */
    PVZM_GetAudio,
    PVZM_Jump,
    PVZM_GetOrder,
    NULL,   /* MuteChannel */
    PVZM_SetChannelVolume,
    NULL,   /* Seek */
    PVZM_Tell,
    NULL,   /* Duration */
    NULL,   /* SetTempo [MIXER-X] */
    NULL,   /* GetTempo [MIXER-X] */
    NULL,   /* SetSpeed [MIXER-X] */
    NULL,   /* GetSpeed [MIXER-X] */
    NULL,   /* SetPitch [MIXER-X] */
    NULL,   /* GetPitch [MIXER-X] */
    NULL,   /* GetTracksCount [MIXER-X] */
    NULL,   /* SetTrackMute [MIXER-X] */
    NULL,   /* LoopStart */
    NULL,   /* LoopEnd */
    NULL,   /* LoopLength */
    NULL,   /* GetMetaTag */
    NULL,   /* GetNumTracks */
    NULL,   /* StartTrack */
    NULL,   /* Pause */
    NULL,   /* Resume */
    PVZM_Stop,
    PVZM_Delete,
    NULL,   /* Close */
    NULL    /* Unload */
};

#endif /* MUSIC_PVZM */
