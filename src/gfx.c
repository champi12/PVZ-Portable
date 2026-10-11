/* gfx.c - Render 2D con sceGu. Texturas swizzled, T8+CLUT cuando es posible. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include "gfx.h"
#include "fonts_data.h"

#define BUF_W 512
#define MAX_IMAGES 3000
#define MAX_TILES 2

enum { FMT_T8 = 0, FMT_565 = 1, FMT_8888 = 2 };

typedef struct {
    u32 off, size;
    u16 w, h;
    u8 fmt, ntiles, flip, pad;
} PakEntry;

typedef struct {
    u16 x, y, w, h, tbw, rows;
    u8 l2w, l2h;
    u16 pad;
    u32 data_off;
} PakTile;

typedef struct {
    void *block;      /* bloque cargado (clut + headers + pixeles) */
    PakTile *tiles;
    void *clut;
} Image;

static unsigned int __attribute__((aligned(16))) dlist[128 * 1024];
static PakEntry entries[MAX_IMAGES];
static Image images[MAX_IMAGES];
static int pak_count;
static SceUID pak_fd = -1;
static u32 mem_used;
static const void *cur_tex;
static float vsy = 1.0f;     /* escala vertical global (vista J2ME 480x320 -> 480x272) */
void gfx_set_vscale(float s) { vsy = s; }
float gfx_vscale(void) { return vsy; }

typedef struct { float u, v; u32 color; float x, y, z; } Vtx;

int gfx_init(const char *pak_path)
{
    pak_fd = sceIoOpen(pak_path, PSP_O_RDONLY, 0);
    if (pak_fd < 0) return -1;
    char hdr[12];
    sceIoRead(pak_fd, hdr, 12);
    if (memcmp(hdr, "PVZP", 4) != 0) return -2;
    memcpy(&pak_count, hdr + 8, 4);
    if (pak_count > MAX_IMAGES) pak_count = MAX_IMAGES;
    sceIoRead(pak_fd, entries, sizeof(PakEntry) * pak_count);
    memset(images, 0, sizeof(images));

    sceGuInit();
    sceGuStart(GU_DIRECT, dlist);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)(BUF_W * SCREEN_H * 4), BUF_W);
    sceGuOffset(2048 - SCREEN_W / 2, 2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    return 0;
}

void gfx_shutdown(void)
{
    img_unload_all();
    if (pak_fd >= 0) sceIoClose(pak_fd);
    sceGuTerm();
}

void gfx_begin(u32 clear_color)
{
    sceGuStart(GU_DIRECT, dlist);
    sceGuClearColor(clear_color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    cur_tex = NULL;
}

#if defined(AUTOTEST) && defined(AT_SHOT_EVERY)
/* Pruebas: guarda el fotograma cada AT_SHOT_EVERY fotogramas como shotNNNNN.raw
 * (480x272, 32 bits ABGR) para revisarlo fuera de la consola. */
#include <stdio.h>
static void at_capture(void *fb)
{
    static int n;
#ifndef AT_SHOT_FROM
#define AT_SHOT_FROM 0
#endif
    if (++n % AT_SHOT_EVERY || n < AT_SHOT_FROM) return;
    char name[32];
    snprintf(name, sizeof(name), "shot%07d.raw", n);
    SceUID f = sceIoOpen(name, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (f < 0) return;
    const u32 *p = (const u32 *)(0x44000000 | (u32)fb);
    for (int y = 0; y < SCREEN_H; y++) sceIoWrite(f, p + y * BUF_W, SCREEN_W * 4);
    sceIoClose(f);
}
#endif

void gfx_end(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
#if defined(AUTOTEST) && defined(AT_SHOT_EVERY)
    static void *draw_buf = 0;
    at_capture(draw_buf);
#endif
    sceDisplayWaitVblankStart();
#if defined(AUTOTEST) && defined(AT_SHOT_EVERY)
    draw_buf = sceGuSwapBuffers();
#else
    sceGuSwapBuffers();
#endif
}

/* ---------- imagenes ---------- */
int img_load(int id)
{
    if (id < 0 || id >= pak_count) return -1;
    if (images[id].block) return 0;
    PakEntry *e = &entries[id];
    void *blk = memalign(16, e->size);
    if (!blk) return -2;
    sceIoLseek32(pak_fd, e->off, PSP_SEEK_SET);
    if (sceIoRead(pak_fd, blk, e->size) != (int)e->size) { free(blk); return -3; }
    sceKernelDcacheWritebackRange(blk, e->size);
    Image *im = &images[id];
    im->block = blk;
    u8 *p = (u8 *)blk;
    if (e->fmt == FMT_T8) { im->clut = p; p += 1024; } else im->clut = NULL;
    im->tiles = (PakTile *)p;
    mem_used += e->size;
    return 0;
}

void img_unload(int id)
{
    if (id < 0 || id >= pak_count || !images[id].block) return;
    sceGuSync(0, 0);
    free(images[id].block);
    mem_used -= entries[id].size;
    memset(&images[id], 0, sizeof(Image));
}

void img_unload_all(void)
{
    for (int i = 0; i < pak_count; i++) img_unload(i);
}

int img_w(int id) { return (id >= 0 && id < pak_count) ? entries[id].w : 0; }
int img_h(int id) { return (id >= 0 && id < pak_count) ? entries[id].h : 0; }
u32 gfx_mem_used(void) { return mem_used; }

static void bind_tile(int id, PakTile *t, int linear)
{
    PakEntry *e = &entries[id];
    Image *im = &images[id];
    const void *data = (u8 *)im->block + t->data_off;
    if (data != cur_tex) {
        int psm = e->fmt == FMT_T8 ? GU_PSM_T8 : (e->fmt == FMT_565 ? GU_PSM_5650 : GU_PSM_8888);
        if (e->fmt == FMT_T8) {
            sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
            sceGuClutLoad(256 / 8, im->clut);
        }
        sceGuTexMode(psm, 0, 0, GU_TRUE);
        sceGuTexImage(0, 1 << t->l2w, 1 << t->l2h, t->tbw, data);
        sceGuTexFlush();
        cur_tex = data;
    }
    sceGuTexFilter(linear ? GU_LINEAR : GU_NEAREST, linear ? GU_LINEAR : GU_NEAREST);
}

/* Dibuja el rectangulo de imagen (sx,sy,sw,sh) en (x,y) con escala 1. Recorre tiles. */
float text_scale = 1;   /* escala del texto (1 = tamano real) */
static float rect_sc = 1;

static void draw_src_rect(int id, int sx, int sy, int sw, int sh, float x, float y, u32 color, int flags)
{
    float sc = rect_sc;
    if (id < 0 || id >= pak_count) return;
    if (!images[id].block && img_load(id) != 0) return;
    PakEntry *e = &entries[id];
    Image *im = &images[id];
    flags ^= e->flip;
    for (int i = 0; i < e->ntiles; i++) {
        PakTile *t = &im->tiles[i];
        /* interseccion del rect pedido con el tile */
        int x0 = sx > t->x ? sx : t->x;
        int y0 = sy > t->y ? sy : t->y;
        int x1 = (sx + sw) < (t->x + t->w) ? (sx + sw) : (t->x + t->w);
        int y1 = (sy + sh) < (t->y + t->h) ? (sy + sh) : (t->y + t->h);
        if (x0 >= x1 || y0 >= y1) continue;
        bind_tile(id, t, vsy != 1.0f || sc != 1.0f);
        /* posicion en pantalla (con espejo respecto al rect pedido) */
        float dx0 = (flags & GFX_FLIPX) ? x + (sx + sw - x1) * sc : x + (x0 - sx) * sc;
        float dy0 = (flags & GFX_FLIPY) ? y + (sy + sh - y1) * sc : y + (y0 - sy) * sc;
        float u0 = x0 - t->x, u1 = x1 - t->x, v0 = y0 - t->y, v1 = y1 - t->y;
        if (flags & GFX_FLIPX) { float tmp = u0; u0 = u1; u1 = tmp; }
        if (flags & GFX_FLIPY) { float tmp = v0; v0 = v1; v1 = tmp; }
        /* tiras de 64 px de ancho: mas amigable con la cache de texturas */
        int total = x1 - x0;
        for (int s = 0; s < total; s += 64) {
            int sl = total - s < 64 ? total - s : 64;
            Vtx *v = (Vtx *)sceGuGetMemory(2 * sizeof(Vtx));
            float fu0, fu1;
            if (flags & GFX_FLIPX) { fu0 = u0 - s; fu1 = u0 - s - sl; }
            else { fu0 = u0 + s; fu1 = u0 + s + sl; }
            v[0].u = fu0; v[0].v = v0; v[0].color = color; v[0].x = dx0 + s * sc; v[0].y = dy0 * vsy; v[0].z = 0;
            v[1].u = fu1; v[1].v = v1; v[1].color = color; v[1].x = dx0 + (s + sl) * sc; v[1].y = (dy0 + (y1 - y0) * sc) * vsy; v[1].z = 0;
            sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
        }
    }
}

void gfx_draw(int id, float x, float y, u32 color, int flags)
{
    draw_src_rect(id, 0, 0, img_w(id), img_h(id), (int)x, (int)y, color, flags);
}

void gfx_draw_region(int id, int sx, int sy, int sw, int sh, float x, float y, u32 color)
{
    draw_src_rect(id, sx, sy, sw, sh, (int)x, (int)y, color, 0);
}

void gfx_draw_ex(int id, float x, float y, float ax, float ay,
                 float sx, float sy, float rot, u32 color, int flags)
{
    if (id < 0 || id >= pak_count) return;
    if (!images[id].block && img_load(id) != 0) return;
    if (sx == 1.0f && sy == 1.0f && rot == 0.0f && vsy == 1.0f) { gfx_draw(id, x - ax, y - ay, color, flags); return; }
    PakEntry *e = &entries[id];
    Image *im = &images[id];
    flags ^= e->flip;
    float c = cosf(rot), s = sinf(rot);
    for (int i = 0; i < e->ntiles; i++) {
        PakTile *t = &im->tiles[i];
        bind_tile(id, t, !(flags & GFX_NEAREST));
        float lx[4] = { t->x, t->x + t->w, t->x, t->x + t->w };
        float ly[4] = { t->y, t->y, t->y + t->h, t->y + t->h };
        float tu[4] = { 0, t->w, 0, t->w };
        float tv[4] = { 0, 0, t->h, t->h };
        Vtx *v = (Vtx *)sceGuGetMemory(4 * sizeof(Vtx));
        for (int k = 0; k < 4; k++) {
            float px = lx[k], py = ly[k];
            if (flags & GFX_FLIPX) px = e->w - px;
            if (flags & GFX_FLIPY) py = e->h - py;
            px = (px - ax) * sx; py = (py - ay) * sy;
            v[k].x = x + px * c - py * s;
            v[k].y = (y + px * s + py * c) * vsy;
            v[k].z = 0; v[k].u = tu[k]; v[k].v = tv[k]; v[k].color = color;
        }
        sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, 0, v);
    }
}

void gfx_rect(float x, float y, float w, float h, u32 color)
{
    sceGuDisable(GU_TEXTURE_2D);
    Vtx *v = (Vtx *)sceGuGetMemory(2 * sizeof(Vtx));
    v[0].u = v[0].v = 0; v[0].color = color; v[0].x = x; v[0].y = y * vsy; v[0].z = 0;
    v[1].u = v[1].v = 0; v[1].color = color; v[1].x = x + w; v[1].y = (y + h) * vsy; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
    sceGuEnable(GU_TEXTURE_2D);
}

void gfx_draw_affine(int id, float ox, float oy, float a, float b, float c, float d, u32 color)
{
    if (id < 0 || id >= pak_count) return;
    if (!images[id].block && img_load(id) != 0) return;
    PakEntry *e = &entries[id];
    Image *im = &images[id];
    int flip = e->flip;
    int lin = !(a == 1.0f && b == 0.0f && c == 0.0f && d == 1.0f) || vsy != 1.0f;
    for (int i = 0; i < e->ntiles; i++) {
        PakTile *t = &im->tiles[i];
        bind_tile(id, t, lin);
        float lu[4] = { t->x, t->x + t->w, t->x, t->x + t->w };
        float lv[4] = { t->y, t->y, t->y + t->h, t->y + t->h };
        float tu[4] = { 0, t->w, 0, t->w };
        float tv[4] = { 0, 0, t->h, t->h };
        Vtx *v = (Vtx *)sceGuGetMemory(4 * sizeof(Vtx));
        for (int k = 0; k < 4; k++) {
            float u = lu[k], w = lv[k];
            if (flip & GFX_FLIPX) u = e->w - u;
            if (flip & GFX_FLIPY) w = e->h - w;
            v[k].x = ox + a * u + c * w;
            v[k].y = (oy + b * u + d * w) * vsy;
            v[k].z = 0; v[k].u = tu[k]; v[k].v = tv[k]; v[k].color = color;
        }
        sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, 0, v);
    }
}

/* ---------- texto ---------- */
static int utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    int c = *p++;
    if (c >= 0xC0 && c < 0xE0 && *p) { c = ((c & 0x1F) << 6) | (*p++ & 0x3F); }
    else if (c >= 0xE0 && c < 0xF0 && p[0] && p[1]) { c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
    *s = (const char *)p;
    return c;
}

static int to_upper(int c)
{
    if (c >= 'a' && c <= 'z') return c - 32;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32; /* latin-1: á->Á, ñ->Ñ ... */
    return c;
}

static int glyph_index(const FontDef *f, int c)
{
    if (f->upper) c = to_upper(c);
    for (int i = 0; i < f->count; i++) if (f->chars[i] == c) return i;
    if (!f->upper) { /* fuentes sin minusculas: intenta en mayuscula */
        int u = to_upper(c);
        for (int i = 0; i < f->count; i++) if (f->chars[i] == u) return i;
    }
    return -1;
}

int font_load(int font) { return img_load(font_defs[font].img); }

int text_width(int font, const char *s)
{
    const FontDef *f = &font_defs[font];
    int w = 0;
    while (*s) {
        int c = utf8_next(&s);
        int g = glyph_index(f, c);
        w += g >= 0 ? f->glyphs[g * 3 + 2] : 4;
    }
    return (int)(w * text_scale + 0.5f);
}

void text_draw(int font, float x, float y, const char *s, u32 color)
{
    const FontDef *f = &font_defs[font];
    float px = (int)x;
    rect_sc = text_scale;
    while (*s) {
        int c = utf8_next(&s);
        int g = glyph_index(f, c);
        if (g < 0) { px += 4 * text_scale; continue; }
        int gx = f->glyphs[g * 3], gy = f->glyphs[g * 3 + 1], gw = f->glyphs[g * 3 + 2];
        draw_src_rect(f->img, gx, gy, gw, f->h, text_scale == 1 ? (int)px : px, (int)y, color, 0);
        px += gw * text_scale;
    }
    rect_sc = 1;
}

void text_draw_centered(int font, float cx, float y, const char *s, u32 color)
{
    text_draw(font, cx - text_width(font, s) / 2, y, s, color);
}

void gfx_clip(int x, int y, int w, int h)
{
    int x1 = x + w, y1 = (int)((y + h) * vsy + 0.5f);
    y = (int)(y * vsy);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    if (x1 <= x || y1 <= y) { x = y = 0; x1 = y1 = 1; }
    sceGuScissor(x, y, x1 - x, y1 - y);
}
void gfx_noclip(void) { sceGuScissor(0, 0, SCREEN_W, SCREEN_H); }
