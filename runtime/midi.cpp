#include "midi.h"
void midi_init() {}
int midi_load(const uint8_t*, size_t) { return -1; }
void midi_play(int, int) {}
void midi_stop(int) {}
void midi_free(int) {}
bool midi_playing(int) { return false; }
void midi_volume(int, int) {}
