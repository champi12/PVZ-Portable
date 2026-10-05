// Small software synthesizer for the game's General MIDI music.
//
// The PSP has no MIDI hardware, so the MIDI files of the original game are rendered here with
// simple synthesis: Karplus-Strong plucked strings (harp, pizzicato, guitar, piano-ish),
// subtractive-style oscillators for bass / pads / leads, additive bells and synthesized drums.
#include "midi.h"
#include "platform.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

const int SAMPLE_RATE = 44100;
const int MAX_VOICES = 32;
const int MAX_PLAYERS = 8;
const int KS_MAX = 2048;

enum EventType { EV_NOTE_ON, EV_NOTE_OFF, EV_PROGRAM, EV_CONTROL, EV_PITCH, EV_TEMPO, EV_END };

struct Event {
    uint32_t tick;
    uint8_t type, ch, a, b;
    uint32_t tempo;  // EV_TEMPO: microseconds per quarter note
};

struct Song {
    std::vector<Event> events;
    int tpb = 96;
};

struct Channel {
    int program = 0;
    float volume = 100 / 127.0f;
    float expression = 1.0f;
    float pan = 0.5f;
    float bend = 1.0f;  // frequency multiplier
    bool sustain = false;
};

struct Player {
    bool used = false;
    bool playing = false;
    Song song;
    size_t pos = 0;
    double tick = 0;
    double ticks_per_sample = 0;
    int loops_left = 0;  // -1 = forever
    float volume = 1.0f;
    Channel ch[16];
};

enum VoiceKind { VK_PLUCK, VK_BASS, VK_PAD, VK_LEAD, VK_BELL, VK_ORGAN, VK_KICK, VK_SNARE, VK_HAT, VK_TOM, VK_CYMBAL, VK_NOISE };

struct Voice {
    bool active;
    int player, ch, note;
    VoiceKind kind;
    float freq;
    double phase, phase2, phase3;
    float amp;
    float env;
    int stage;      // 0 attack, 1 decay/sustain, 2 release
    float attack, decay, sustain, release;  // per-sample rates / level
    bool held;      // key down
    bool sustained; // released while sustain pedal down
    float lp;       // one-pole filter state
    uint32_t noise;
    int age;
    int ks_len, ks_pos;
    float ks_damp;
    float sweep;    // drums: pitch sweep state
    float ks[KS_MAX];  // must stay last: note_on() clears everything before it
};

Player g_players[MAX_PLAYERS];
Voice g_voices[MAX_VOICES];
bool g_started = false;

inline float frand(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return (float)(int32_t)s / 2147483648.0f;
}

uint32_t read_vlq(const uint8_t* d, size_t size, size_t& p) {
    uint32_t v = 0;
    for (int i = 0; i < 4 && p < size; i++) {
        uint8_t c = d[p++];
        v = (v << 7) | (c & 0x7F);
        if (!(c & 0x80)) break;
    }
    return v;
}

bool parse_midi(const uint8_t* d, size_t size, Song& song) {
    size_t p = 0;
    // The game's sound bank may carry a few bytes before the header.
    while (p + 4 <= size && memcmp(d + p, "MThd", 4) != 0) p++;
    if (p + 14 > size) return false;
    uint32_t hl = (d[p + 4] << 24) | (d[p + 5] << 16) | (d[p + 6] << 8) | d[p + 7];
    int ntracks = (d[p + 10] << 8) | d[p + 11];
    int div = (d[p + 12] << 8) | d[p + 13];
    if (div & 0x8000) return false;  // SMPTE timing not supported
    song.tpb = div ? div : 96;
    p += 8 + hl;
    for (int t = 0; t < ntracks && p + 8 <= size; t++) {
        if (memcmp(d + p, "MTrk", 4) != 0) break;
        uint32_t len = (d[p + 4] << 24) | (d[p + 5] << 16) | (d[p + 6] << 8) | d[p + 7];
        size_t q = p + 8, end = q + len;
        if (end > size) end = size;
        uint32_t tick = 0;
        uint8_t running = 0;
        while (q < end) {
            tick += read_vlq(d, end, q);
            if (q >= end) break;
            uint8_t st = d[q];
            if (st & 0x80) {
                q++;
                if (st < 0xF0) running = st;
            } else {
                st = running;
            }
            Event e;
            memset(&e, 0, sizeof e);
            e.tick = tick;
            if (st == 0xFF) {
                if (q >= end) break;
                uint8_t type = d[q++];
                uint32_t ml = read_vlq(d, end, q);
                if (type == 0x51 && ml == 3 && q + 3 <= end) {
                    e.type = EV_TEMPO;
                    e.tempo = (d[q] << 16) | (d[q + 1] << 8) | d[q + 2];
                    song.events.push_back(e);
                }
                q += ml;
                if (type == 0x2F) break;
                continue;
            }
            if (st == 0xF0 || st == 0xF7) {
                uint32_t ml = read_vlq(d, end, q);
                q += ml;
                continue;
            }
            uint8_t hi = st & 0xF0;
            e.ch = st & 0x0F;
            uint8_t a = q < end ? d[q] : 0;
            if (hi == 0xC0 || hi == 0xD0) {
                q += 1;
                if (hi == 0xC0) {
                    e.type = EV_PROGRAM;
                    e.a = a;
                    song.events.push_back(e);
                }
                continue;
            }
            uint8_t b = q + 1 < end ? d[q + 1] : 0;
            q += 2;
            switch (hi) {
                case 0x90: e.type = b ? EV_NOTE_ON : EV_NOTE_OFF; e.a = a; e.b = b; break;
                case 0x80: e.type = EV_NOTE_OFF; e.a = a; break;
                case 0xB0: e.type = EV_CONTROL; e.a = a; e.b = b; break;
                case 0xE0: e.type = EV_PITCH; e.a = a; e.b = b; break;
                default: continue;
            }
            song.events.push_back(e);
        }
        p = 8 + p + len;
    }
    // merge tracks by time (stable keeps note-off before note-on at equal ticks per track order)
    std::vector<Event>& ev = song.events;
    for (size_t i = 1; i < ev.size(); i++) {
        Event x = ev[i];
        size_t j = i;
        while (j > 0 && ev[j - 1].tick > x.tick) {
            ev[j] = ev[j - 1];
            j--;
        }
        ev[j] = x;
    }
    Event endev;
    memset(&endev, 0, sizeof endev);
    endev.type = EV_END;
    endev.tick = ev.empty() ? 0 : ev.back().tick;
    ev.push_back(endev);
    return true;
}

float note_freq(int note) { return 440.0f * powf(2.0f, (note - 69) / 12.0f); }

VoiceKind kind_for_program(int prog) {
    if (prog <= 7) return VK_PLUCK;                    // pianos
    if (prog >= 8 && prog <= 15) return VK_BELL;       // chromatic percussion
    if (prog >= 16 && prog <= 23) return VK_ORGAN;     // organs
    if (prog >= 24 && prog <= 31) return VK_PLUCK;     // guitars
    if (prog >= 32 && prog <= 39) return VK_BASS;      // basses
    if (prog == 45 || prog == 46 || prog == 47) return VK_PLUCK;  // pizzicato, harp, timpani
    if (prog >= 40 && prog <= 55) return VK_PAD;       // strings / ensemble / choir
    if (prog >= 56 && prog <= 79) return VK_LEAD;      // brass, reeds, pipes
    if (prog >= 80 && prog <= 87) return VK_LEAD;      // synth leads
    if (prog >= 88 && prog <= 103) return VK_PAD;      // synth pads / fx
    if (prog >= 104 && prog <= 111) return VK_PLUCK;   // ethnic
    if (prog >= 112 && prog <= 119) return VK_BELL;    // percussive
    return VK_NOISE;                                   // sound effects
}

Voice* alloc_voice() {
    Voice* best = nullptr;
    for (Voice& v : g_voices)
        if (!v.active) return &v;
    // steal the oldest released voice, else the oldest one
    for (Voice& v : g_voices)
        if (v.stage == 2 && (!best || v.age > best->age)) best = &v;
    if (best) return best;
    for (Voice& v : g_voices)
        if (!best || v.age > best->age) best = &v;
    return best;
}

inline float rate_for(float seconds) { return seconds <= 0 ? 1.0f : 1.0f / (seconds * SAMPLE_RATE); }

void note_on(int pi, int ch, int note, int vel) {
    Player& pl = g_players[pi];
    Voice* v = alloc_voice();
    memset(v, 0, offsetof(Voice, ks));
    v->active = true;
    v->player = pi;
    v->ch = ch;
    v->note = note;
    v->held = true;
    v->noise = 0x12345u + note * 977u + vel;
    float velf = vel / 127.0f;
    v->amp = velf * velf;
    if (ch == 9) {
        switch (note) {
            case 35: case 36: v->kind = VK_KICK; v->amp *= 1.0f; break;
            case 37: case 38: case 39: case 40: v->kind = VK_SNARE; v->amp *= 0.55f; break;
            case 42: case 44: case 46: v->kind = VK_HAT; v->amp *= note == 46 ? 0.22f : 0.16f; break;
            case 41: case 43: case 45: case 47: case 48: case 50: v->kind = VK_TOM; v->amp *= 0.6f; break;
            case 49: case 51: case 52: case 53: case 55: case 57: case 59: v->kind = VK_CYMBAL; v->amp *= 0.15f; break;
            default: v->kind = VK_HAT; v->amp *= 0.12f; break;
        }
        v->freq = v->kind == VK_TOM ? 80.0f + (note - 41) * 12.0f : 55.0f;
        v->sweep = 1.0f;
        v->stage = 1;
        v->env = 1.0f;
        float dec = 0.25f;
        switch (v->kind) {
            case VK_KICK: dec = 0.35f; break;
            case VK_SNARE: dec = 0.18f; break;
            case VK_HAT: dec = note == 46 ? 0.25f : 0.05f; break;
            case VK_TOM: dec = 0.3f; break;
            case VK_CYMBAL: dec = 1.2f; break;
            default: break;
        }
        v->decay = rate_for(dec);
        v->sustain = 0;
        v->release = v->decay;
        v->held = false;
        return;
    }
    v->kind = kind_for_program(pl.ch[ch].program);
    v->freq = note_freq(note);
    switch (v->kind) {
        case VK_PLUCK: {
            v->ks_len = (int)(SAMPLE_RATE / v->freq + 0.5f);
            if (v->ks_len < 2) v->ks_len = 2;
            if (v->ks_len > KS_MAX) v->ks_len = KS_MAX;
            for (int i = 0; i < v->ks_len; i++) v->ks[i] = frand(v->noise);
            // pre-smooth the excitation for a rounder tone
            for (int k = 0; k < 2; k++)
                for (int i = 1; i < v->ks_len; i++) v->ks[i] = 0.5f * (v->ks[i] + v->ks[i - 1]);
            int prog = pl.ch[ch].program;
            v->ks_damp = prog == 45 ? 0.985f : (prog <= 7 ? 0.9975f : 0.996f);
            v->attack = 1.0f;
            v->env = 1.0f;
            v->stage = 1;
            v->decay = 0;
            v->sustain = 1.0f;
            v->release = rate_for(prog == 46 ? 0.6f : 0.15f);
            v->amp *= 0.9f;
            break;
        }
        case VK_BASS:
            v->attack = rate_for(0.005f);
            v->decay = rate_for(0.6f);
            v->sustain = 0.5f;
            v->release = rate_for(0.08f);
            v->amp *= 0.8f;
            break;
        case VK_PAD:
            v->attack = rate_for(0.25f);
            v->decay = rate_for(1.0f);
            v->sustain = 0.8f;
            v->release = rate_for(0.5f);
            v->amp *= 0.30f;
            break;
        case VK_LEAD:
            v->attack = rate_for(0.02f);
            v->decay = rate_for(0.3f);
            v->sustain = 0.7f;
            v->release = rate_for(0.12f);
            v->amp *= 0.3f;
            break;
        case VK_ORGAN:
            v->attack = rate_for(0.01f);
            v->decay = rate_for(0.1f);
            v->sustain = 0.9f;
            v->release = rate_for(0.08f);
            v->amp *= 0.3f;
            break;
        case VK_BELL:
            v->attack = rate_for(0.002f);
            v->decay = rate_for(1.2f);
            v->sustain = 0.0f;
            v->release = rate_for(0.6f);
            v->amp *= 0.45f;
            break;
        default:
            v->attack = rate_for(0.01f);
            v->decay = rate_for(0.4f);
            v->sustain = 0.0f;
            v->release = rate_for(0.2f);
            v->amp *= 0.2f;
            break;
    }
}

void note_off(int pi, int ch, int note) {
    for (Voice& v : g_voices) {
        if (v.active && v.player == pi && v.ch == ch && v.note == note && v.held) {
            v.held = false;
            if (g_players[pi].ch[ch].sustain) v.sustained = true;
            else v.stage = 2;
        }
    }
}

void handle_event(int pi, const Event& e) {
    Player& pl = g_players[pi];
    Channel& c = pl.ch[e.ch];
    switch (e.type) {
        case EV_NOTE_ON: note_on(pi, e.ch, e.a, e.b); break;
        case EV_NOTE_OFF: note_off(pi, e.ch, e.a); break;
        case EV_PROGRAM: c.program = e.a; break;
        case EV_CONTROL:
            switch (e.a) {
                case 7: c.volume = e.b / 127.0f; break;
                case 10: c.pan = e.b / 127.0f; break;
                case 11: c.expression = e.b / 127.0f; break;
                case 64:
                    c.sustain = e.b >= 64;
                    if (!c.sustain) {
                        for (Voice& v : g_voices)
                            if (v.active && v.player == pi && v.ch == e.ch && v.sustained) {
                                v.sustained = false;
                                v.stage = 2;
                            }
                    }
                    break;
                case 120: case 123:
                    for (Voice& v : g_voices)
                        if (v.active && v.player == pi && v.ch == e.ch) v.stage = 2;
                    break;
                case 121:
                    c.volume = 100 / 127.0f;
                    c.expression = 1.0f;
                    c.pan = 0.5f;
                    c.bend = 1.0f;
                    break;
            }
            break;
        case EV_PITCH: {
            int v = ((e.b << 7) | e.a) - 8192;
            c.bend = powf(2.0f, (v / 8192.0f) * 2.0f / 12.0f);
            break;
        }
        case EV_TEMPO:
            pl.ticks_per_sample = (double)pl.song.tpb * 1e6 / ((double)e.tempo * SAMPLE_RATE);
            break;
        default:
            break;
    }
}

void kill_player_voices(int pi) {
    for (Voice& v : g_voices)
        if (v.active && v.player == pi) v.active = false;
}

void reset_player(Player& pl) {
    pl.pos = 0;
    pl.tick = 0;
    pl.ticks_per_sample = (double)pl.song.tpb * 1e6 / (500000.0 * SAMPLE_RATE);
    for (Channel& c : pl.ch) c = Channel();
}

// Advances the sequencer of player pi by one sample.
void sequence(int pi) {
    Player& pl = g_players[pi];
    std::vector<Event>& ev = pl.song.events;
    while (pl.playing && pl.pos < ev.size() && ev[pl.pos].tick <= pl.tick) {
        const Event& e = ev[pl.pos++];
        if (e.type == EV_END) {
            if (pl.loops_left < 0 || pl.loops_left > 1) {
                if (pl.loops_left > 0) pl.loops_left--;
                for (Voice& v : g_voices)
                    if (v.active && v.player == pi) v.stage = 2;
                double over = pl.tick - e.tick;
                reset_player(pl);
                pl.tick = over > 0 ? over : 0;
            } else {
                pl.playing = false;
            }
            return;
        }
        handle_event(pi, e);
    }
    pl.tick += pl.ticks_per_sample;
}

inline float voice_sample(Voice& v, float freq_mul) {
    float f = v.freq * freq_mul;
    float out = 0;
    switch (v.kind) {
        case VK_PLUCK: {
            int i = v.ks_pos, j = i + 1 == v.ks_len ? 0 : i + 1;
            out = v.ks[i];
            v.ks[i] = (v.ks[i] + v.ks[j]) * 0.5f * v.ks_damp;
            v.ks_pos = j;
            break;
        }
        case VK_BASS: {
            v.phase += f / SAMPLE_RATE;
            v.phase -= floor(v.phase);
            float s = sinf((float)(v.phase * 2 * M_PI));
            float tri = 4.0f * fabsf((float)v.phase - 0.5f) - 1.0f;
            out = 0.6f * s + 0.4f * tri;
            break;
        }
        case VK_PAD: {
            v.phase += f * 1.003f / SAMPLE_RATE;
            v.phase2 += f * 0.997f / SAMPLE_RATE;
            v.phase -= floor(v.phase);
            v.phase2 -= floor(v.phase2);
            float a = 2.0f * (float)v.phase - 1.0f, b = 2.0f * (float)v.phase2 - 1.0f;
            float raw = 0.5f * (a + b);
            v.lp += (raw - v.lp) * 0.08f;  // soften the saws
            out = v.lp * 1.6f;
            break;
        }
        case VK_LEAD: {
            v.phase += f / SAMPLE_RATE;
            v.phase -= floor(v.phase);
            float sq = v.phase < 0.5 ? 1.0f : -1.0f;
            v.lp += (sq - v.lp) * 0.15f;
            out = v.lp;
            break;
        }
        case VK_ORGAN: {
            v.phase += f / SAMPLE_RATE;
            v.phase -= floor(v.phase);
            float p = (float)(v.phase * 2 * M_PI);
            out = 0.6f * sinf(p) + 0.3f * sinf(2 * p) + 0.15f * sinf(4 * p);
            break;
        }
        case VK_BELL: {
            v.phase += f / SAMPLE_RATE;
            v.phase2 += f * 2.76f / SAMPLE_RATE;
            v.phase3 += f * 5.4f / SAMPLE_RATE;
            v.phase -= floor(v.phase);
            v.phase2 -= floor(v.phase2);
            v.phase3 -= floor(v.phase3);
            float d2 = expf(-v.age / (0.25f * SAMPLE_RATE));
            out = sinf((float)(v.phase * 2 * M_PI)) + 0.4f * d2 * sinf((float)(v.phase2 * 2 * M_PI)) +
                  0.2f * d2 * d2 * sinf((float)(v.phase3 * 2 * M_PI));
            out *= 0.7f;
            break;
        }
        case VK_KICK: {
            v.sweep *= 0.9993f;
            float fk = 45.0f + 110.0f * v.sweep;
            v.phase += fk / SAMPLE_RATE;
            v.phase -= floor(v.phase);
            out = sinf((float)(v.phase * 2 * M_PI));
            break;
        }
        case VK_SNARE: {
            v.phase += 190.0f / SAMPLE_RATE;
            v.phase -= floor(v.phase);
            float n = frand(v.noise);
            float tone = sinf((float)(v.phase * 2 * M_PI)) * expf(-v.age / (0.04f * SAMPLE_RATE));
            out = 0.7f * n + 0.5f * tone;
            break;
        }
        case VK_HAT:
        case VK_CYMBAL: {
            float n = frand(v.noise);
            float hp = n - v.lp;  // crude high-pass
            v.lp += (n - v.lp) * 0.3f;
            out = hp;
            break;
        }
        case VK_TOM: {
            v.sweep *= 0.99985f;
            v.phase += v.freq * (0.7f + 0.3f * v.sweep) / SAMPLE_RATE;
            v.phase -= floor(v.phase);
            out = sinf((float)(v.phase * 2 * M_PI));
            break;
        }
        default:
            out = frand(v.noise) * 0.5f;
            break;
    }
    // envelope
    switch (v.stage) {
        case 0:
            v.env += v.attack;
            if (v.env >= 1.0f) {
                v.env = 1.0f;
                v.stage = 1;
            }
            break;
        case 1:
            if (v.env > v.sustain) {
                v.env -= v.decay * (v.kind == VK_PLUCK ? 0 : 1);
                if (v.env < v.sustain) v.env = v.sustain;
            }
            if (v.env <= 0.0005f && v.sustain <= 0) v.active = false;
            break;
        case 2:
            v.env -= v.release;
            if (v.env <= 0) {
                v.env = 0;
                v.active = false;
            }
            break;
    }
    if (v.kind == VK_PLUCK && v.age > SAMPLE_RATE * 6) v.active = false;
    v.age++;
    return out * v.env;
}

void mix(int16_t* out, int frames) {
    for (int i = 0; i < frames; i++) {
        for (int p = 0; p < MAX_PLAYERS; p++)
            if (g_players[p].used && g_players[p].playing) sequence(p);
        float l = 0, r = 0;
        for (Voice& v : g_voices) {
            if (!v.active) continue;
            Player& pl = g_players[v.player];
            Channel& c = pl.ch[v.ch];
            float s = voice_sample(v, c.bend) * v.amp * c.volume * c.expression * pl.volume;
            l += s * (1.0f - c.pan);
            r += s * c.pan;
        }
        // soft clip
        l *= 0.85f;
        r *= 0.85f;
        l = l / (1.0f + fabsf(l));
        r = r / (1.0f + fabsf(r));
        out[i * 2] = (int16_t)(l * 32000.0f);
        out[i * 2 + 1] = (int16_t)(r * 32000.0f);
    }
}

void ensure_started() {
    if (!g_started) {
        g_started = true;
        platform_audio_start(mix);
    }
}

}  // namespace

void midi_init() { ensure_started(); }

int midi_load(const uint8_t* data, size_t size) {
    Song song;
    if (!parse_midi(data, size, song)) return -1;
    ensure_started();
    platform_audio_lock();
    int h = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!g_players[i].used) {
            g_players[i] = Player();
            g_players[i].used = true;
            g_players[i].song = song;
            reset_player(g_players[i]);
            h = i;
            break;
        }
    }
    platform_audio_unlock();
    return h;
}

void midi_play(int h, int loops) {
    if (h < 0 || h >= MAX_PLAYERS || !g_players[h].used) return;
    platform_audio_lock();
    Player& pl = g_players[h];
    kill_player_voices(h);
    reset_player(pl);
    pl.loops_left = loops == 0 ? 1 : loops;
    pl.playing = true;
    platform_audio_unlock();
}

void midi_stop(int h) {
    if (h < 0 || h >= MAX_PLAYERS) return;
    platform_audio_lock();
    g_players[h].playing = false;
    kill_player_voices(h);
    platform_audio_unlock();
}

void midi_free(int h) {
    if (h < 0 || h >= MAX_PLAYERS) return;
    platform_audio_lock();
    kill_player_voices(h);
    g_players[h] = Player();
    platform_audio_unlock();
}

bool midi_playing(int h) {
    if (h < 0 || h >= MAX_PLAYERS) return false;
    return g_players[h].used && g_players[h].playing;
}

void midi_volume(int h, int level) {
    if (h < 0 || h >= MAX_PLAYERS) return;
    g_players[h].volume = level / 100.0f;
}
