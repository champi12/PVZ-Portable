// Lookup of the game's data files, packed by tools/pack_resources.py and linked in with .incbin.
//
// Blob layout (little endian): u32 count, then per entry {u16 name_len, name bytes, u32 offset,
// u32 size}; offsets are relative to the start of the blob.
#include "platform.h"

#include <cstring>

extern "C" const uint8_t pvz_resources[];

#ifndef RESOURCE_BLOB
#error RESOURCE_BLOB must point to the packed resource file
#endif

#define STR2(x) #x
#define STR(x) STR2(x)
__asm__(
    ".section .rodata\n"
    ".balign 16\n"
    ".global pvz_resources\n"
    "pvz_resources:\n"
    ".incbin \"" STR(RESOURCE_BLOB) "\"\n"
    ".previous\n");

static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

const uint8_t* resource_find(const char* name, size_t* size) {
    const uint8_t* p = pvz_resources;
    uint32_t count = rd32(p);
    p += 4;
    size_t nl = strlen(name);
    for (uint32_t i = 0; i < count; i++) {
        uint16_t len = p[0] | (p[1] << 8);
        const uint8_t* nm = p + 2;
        uint32_t off = rd32(nm + len), sz = rd32(nm + len + 4);
        if (len == nl && memcmp(nm, name, nl) == 0) {
            *size = sz;
            return pvz_resources + off;
        }
        p = nm + len + 8;
    }
    return nullptr;
}
