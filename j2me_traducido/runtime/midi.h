// Small General MIDI software synthesizer used for the game's music.
#pragma once

#include <cstddef>
#include <cstdint>

void midi_init();
int midi_load(const uint8_t* data, size_t size);   // returns a handle or -1
void midi_play(int handle, int loops);             // loops: -1 = forever
void midi_stop(int handle);
void midi_free(int handle);
bool midi_playing(int handle);
void midi_volume(int handle, int level);          // 0..100
