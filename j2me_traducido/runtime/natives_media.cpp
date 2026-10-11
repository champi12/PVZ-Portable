// javax.microedition.media natives: MIDI playback through the built-in synthesizer.
#include "classes.h"
#include "midi.h"

#include <cstdio>
#include <cstdlib>

static bool debug() {
    static int d = -1;
    if (d < 0) d = getenv("PVZ_DEBUG") != nullptr;
    return d;
}

int32_t M_javax_microedition_media_PlayerImpl__load__AB_I(JObject* data) {
    int h = midi_load(jadata<uint8_t>(JNN(data)), ((JArray*)data)->length);
    if (debug()) fprintf(stderr, "midi load %d bytes -> %d\n", ((JArray*)data)->length, h);
    return h;
}
void M_javax_microedition_media_PlayerImpl__play__II_V(int32_t h, int32_t loops) {
    if (debug()) fprintf(stderr, "midi play %d loops %d\n", h, loops);
    midi_play(h, loops);
}
void M_javax_microedition_media_PlayerImpl__halt__I_V(int32_t h) { midi_stop(h); }
void M_javax_microedition_media_PlayerImpl__free__I_V(int32_t h) { midi_free(h); }
int32_t M_javax_microedition_media_PlayerImpl__playing__I_Z(int32_t h) { return midi_playing(h) ? 1 : 0; }
void M_javax_microedition_media_PlayerImpl__volume__II_V(int32_t h, int32_t level) { midi_volume(h, level); }
