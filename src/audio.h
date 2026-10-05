/* audio.h - Musica MP3 por hardware (sceMp3) + mezclador de efectos IMA-ADPCM */
#ifndef AUDIO_H
#define AUDIO_H
#include "sfx_ids.h"

int  audio_init(const char *sfx_pak);
void audio_shutdown(void);

/* Efectos: se cargan desde sfx.pak (4 bits/muestra en RAM) */
int  sfx_preload(int id);
void sfx_unload_all(void);
int  sfx_play(int id);                 /* precarga si hace falta; devuelve un manejador */
void sfx_stop(int handle);
void sfx_stop_all(void);
void sfx_play_vol(int id, int vol);    /* vol 0..256 */
void sfx_set_volume(int vol);          /* volumen general efectos 0..256 */
unsigned int sfx_mem_used(void);

/* Musica: ruta a .mp3 (44.1 kHz). loop=1 repite sin fin */
void music_play(const char *path, int loop);
void music_stop(void);
void music_set_volume(int vol);        /* 0..256 */
int  music_is_playing(void);

#endif
