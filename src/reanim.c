/* reanim.c - Reproductor de animaciones por piezas con interpolacion entre frames
 * (el J2ME pre-rotaba imagenes porque MIDP no rota; aqui el GU rota/escala en tiempo real). */
#include <pspkernel.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include "reanim.h"
#include "gfx.h"

static SceUID fd = -1;
static u32 offs[RE_COUNT + 1];
static int re_count;
static ReDef defs[RE_COUNT];
int reanim_interp = 0;
int reanim_pc_smooth = 0;   /* 0: las animaciones del PC van a saltos como las del J2ME (6 fps, sin interpolar) */
#define PC_STEP 2           /* el PC anima a 12 fps: un frame de cada 2 */

/* frame que se dibuja: las del PC, a saltos de PC_STEP frames desde el principio del rango */
static float qframe(ReAnim *a, float fr)
{
    if (reanim_pc_smooth || !a->def || reanim_id(a->def) < RE_J2ME_COUNT || fr < a->start) return fr;
    return a->start + floorf((fr - a->start) / PC_STEP) * PC_STEP;
}

int reanim_init(const char *pak)
{
    fd = sceIoOpen(pak, PSP_O_RDONLY, 0);
    if (fd < 0) return -1;
    char hdr[8];
    sceIoRead(fd, hdr, 8);
    if (memcmp(hdr, "ANIM", 4) != 0) return -2;
    memcpy(&re_count, hdr + 4, 4);
    if (re_count > RE_COUNT) re_count = RE_COUNT;
    sceIoRead(fd, offs, (re_count + 1) * sizeof(u32));
    memset(defs, 0, sizeof(defs));
    return 0;
}

ReDef *reanim_get(int id)
{
    if (id < 0 || id >= re_count || fd < 0) return NULL;
    ReDef *d = &defs[id];
    if (d->block) return d;
    u32 size = offs[id + 1] - offs[id];
    u8 *b = (u8 *)memalign(16, size);
    if (!b) return NULL;
    sceIoLseek32(fd, offs[id], PSP_SEEK_SET);
    sceIoRead(fd, b, size);
    if (memcmp(b, "RNM1", 4) != 0) { free(b); return NULL; }
    unsigned short h[4];
    memcpy(h, b + 4, 8);
    d->ntracks = h[0]; d->nframes = h[1]; d->fps = h[2] ? h[2] : 4;
    memcpy(d->bbox, b + 12, 16);
    d->frames = (ReFrame *)(b + 28);
    d->block = b;
    /* precarga de las imagenes que usa */
    for (int i = 0; i < d->ntracks * d->nframes; i++)
        if (d->frames[i].img >= 0) img_load(d->frames[i].img);
    return d;
}

int reanim_id(ReDef *d) { return d ? (int)(d - defs) : -1; }

void reanim_fix_bbox(ReDef *d, int frame)
{
    if (!d || d->bbox_frame == frame + 1 || frame < 0 || frame >= d->nframes) return;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (int t = 0; t < d->ntracks; t++) {
        ReFrame *F = &d->frames[t * d->nframes + frame];
        if (!F->vis || F->img < 0) continue;
        float a = cosf(F->kx) * F->sx, b = -sinf(F->kx) * F->sx, c = sinf(F->ky) * F->sy, e = cosf(F->ky) * F->sy;
        float W = img_w(F->img), H = img_h(F->img);
        float us[4] = { 0, W, 0, W }, vs[4] = { 0, 0, H, H };
        for (int k = 0; k < 4; k++) {
            float x = F->x + a * us[k] + c * vs[k], y = F->y + b * us[k] + e * vs[k];
            x0 = fminf(x0, x); x1 = fmaxf(x1, x); y0 = fminf(y0, y); y1 = fmaxf(y1, y);
        }
    }
    if (x1 < x0) return;
    d->bbox[0] = x0; d->bbox[1] = y0; d->bbox[2] = x1; d->bbox[3] = y1;
    d->bbox_frame = frame + 1;
}

void reanim_unload_all(void)
{
    for (int i = 0; i < RE_COUNT; i++) if (defs[i].block) { free(defs[i].block); defs[i].block = NULL; }
}

static int is_control_track(ReDef *d, int t)
{
    for (int f = 0; f < d->nframes; f++) if (d->frames[t * d->nframes + f].img >= 0) return 0;
    return 1;
}

int reanim_range(ReDef *d, int track, int *start, int *end)
{
    int n = -1;
    for (int t = 0; t < d->ntracks; t++) {
        if (!is_control_track(d, t)) continue;
        if (++n != track) continue;
        int s = -1, e = -1;
        for (int f = 0; f < d->nframes; f++)
            if (d->frames[t * d->nframes + f].vis) { if (s < 0) s = f; e = f; }
        if (s < 0) return -1;
        *start = s; *end = e;
        return 0;
    }
    return -1;
}

void reanim_idle_range(ReDef *d, int *start, int *end)
{
    *start = 0; *end = d->nframes - 1;
    int n = -1;
    for (int t = 0; t < d->ntracks; t++) {
        if (!is_control_track(d, t)) continue;
        n++;
        if (d->frames[t * d->nframes].vis) { reanim_range(d, n, start, end); return; }
    }
}

void reanim_play(ReAnim *a, int id, int start, int end, int loop)
{
    memset(a, 0, sizeof(*a));
    a->def = reanim_get(id);
    if (!a->def) return;
    if (start < 0) reanim_idle_range(a->def, &a->start, &a->end);
    else { a->start = start; a->end = end; }
    a->frame = a->start;
    a->speed = 1.0f;
    a->loop = loop;
}

void reanim_update(ReAnim *a, float dt)
{
    if (!a->def) return;
    a->frame += dt * a->def->fps * a->speed;
    float len = (float)(a->end - a->start + 1);
    if (a->frame >= a->end + 1) {
        if (a->loop) { while (a->frame >= a->end + 1) a->frame -= len; }
        else a->frame = (float)a->end;
    }
}

int reanim_swap_track = -1, reanim_swap_img = -1;

int reanim_done(ReAnim *a) { return !a->loop && a->frame >= a->end; }

void reanim_draw(ReAnim *a, float x, float y, float scale, u32 color) { reanim_draw_flip(a, x, y, scale, 0, color); }

void reanim_draw_flip(ReAnim *a, float x, float y, float scale, int flipx, u32 color)
{
    ReDef *d = a->def;
    if (!d || (!d->block && !reanim_get(reanim_id(d)))) return;   /* liberada al cambiar de pantalla: se recarga */
    float fr = qframe(a, a->frame);
    int f0 = (int)fr;
    float t = fr - f0;
    int f1 = f0 + 1;
    if (f1 > a->end) f1 = a->loop ? a->start : a->end;
    for (int tr = 0; tr < d->ntracks; tr++) {
        if (tr < 64 && (a->hide_mask[tr >> 5] & (1u << (tr & 31)))) continue;
        ReFrame *A = &d->frames[tr * d->nframes + f0];
        if (!A->vis || A->img < 0) continue;
        ReFrame *B = &d->frames[tr * d->nframes + f1];
        float fx = A->x, fy = A->y, sx = A->sx, sy = A->sy, kx = A->kx, ky = A->ky;
        if ((reanim_interp || (a->interp && reanim_pc_smooth)) && B->vis && B->img == A->img && t > 0) {  /* interpolacion como el PC */
            fx += (B->x - A->x) * t; fy += (B->y - A->y) * t;
            sx += (B->sx - A->sx) * t; sy += (B->sy - A->sy) * t;
            kx += (B->kx - A->kx) * t; ky += (B->ky - A->ky) * t;
        }
        float ma = cosf(kx) * sx * scale, mb = -sinf(kx) * sx * scale;
        float mc = sinf(ky) * sy * scale, md = cosf(ky) * sy * scale;
        if (flipx) { ma = -ma; mc = -mc; fx = -fx; }
        gfx_draw_affine(tr == reanim_swap_track ? reanim_swap_img : A->img, x + fx * scale, y + fy * scale, ma, mb, mc, md, color);
    }
}

int reanim_track_info(ReAnim *a, int track, float *x, float *y, int *img)
{
    ReDef *d = a->def;
    if (!d || track < 0 || track >= d->ntracks) return 0;
    int f = (int)a->frame;
    if (f >= d->nframes) f = d->nframes - 1;
    ReFrame *F = &d->frames[track * d->nframes + f];
    *x = F->x; *y = F->y; *img = F->img;
    return F->vis && F->img >= 0;
}

/* ---- animaciones del PC: matrices de pista y enganches ---- */
void reanim_track_matrix(ReAnim *a, int track, float frame, float m[6])
{
    ReDef *d = a->def;
    m[0] = 1; m[1] = 0; m[2] = 0; m[3] = 1; m[4] = 0; m[5] = 0;
    if (d && !d->block && !reanim_get(reanim_id(d))) d = NULL;
    if (!d || track < 0 || track >= d->ntracks) return;
    if (!reanim_pc_smooth) frame = qframe(a, frame);
    int f0 = (int)frame;
    if (f0 < 0) f0 = 0;
    if (f0 >= d->nframes) f0 = d->nframes - 1;
    int f1 = f0 + 1 > a->end ? (a->loop ? a->start : a->end) : f0 + 1;
    if (f1 >= d->nframes) f1 = d->nframes - 1;
    float t = a->interp && !reanim_pc_smooth && reanim_id(d) >= RE_J2ME_COUNT ? 0 : frame - (int)frame;
    ReFrame *A = &d->frames[track * d->nframes + f0], *B = &d->frames[track * d->nframes + f1];
    float x = A->x + (B->x - A->x) * t, y = A->y + (B->y - A->y) * t;
    float sx = A->sx + (B->sx - A->sx) * t, sy = A->sy + (B->sy - A->sy) * t;
    float kx = A->kx + (B->kx - A->kx) * t, ky = A->ky + (B->ky - A->ky) * t;
    m[0] = cosf(kx) * sx; m[1] = -sinf(kx) * sx; m[2] = sinf(ky) * sy; m[3] = cosf(ky) * sy; m[4] = x; m[5] = y;
}

static void mat_mul(const float *p, const float *q, float *r)   /* r = p * q */
{
    float o[6];
    o[0] = p[0] * q[0] + p[2] * q[1]; o[1] = p[1] * q[0] + p[3] * q[1];
    o[2] = p[0] * q[2] + p[2] * q[3]; o[3] = p[1] * q[2] + p[3] * q[3];
    o[4] = p[0] * q[4] + p[2] * q[5] + p[4]; o[5] = p[1] * q[4] + p[3] * q[5] + p[5];
    memcpy(r, o, sizeof(o));
}

void reanim_attach_matrix(ReAnim *a, int track, float m[6])
{
    float cur[6], base[6], inv[6];
    reanim_track_matrix(a, track, a->frame, cur);
    reanim_track_matrix(a, track, (float)a->start, base);
    float det = base[0] * base[3] - base[2] * base[1];
    if (fabsf(det) < 1e-6f) { memcpy(m, cur, sizeof(cur)); return; }
    inv[0] = base[3] / det; inv[1] = -base[1] / det; inv[2] = -base[2] / det; inv[3] = base[0] / det;
    inv[4] = -(inv[0] * base[4] + inv[2] * base[5]); inv[5] = -(inv[1] * base[4] + inv[3] * base[5]);
    mat_mul(cur, inv, m);
}

void reanim_draw_m(ReAnim *a, float x, float y, float scale, const float *ov, u32 color)
{
    ReDef *d = a->def;
    if (!d || (!d->block && !reanim_get(reanim_id(d)))) return;   /* liberada al cambiar de pantalla: se recarga */
    int f0 = (int)qframe(a, a->frame);
    for (int tr = 0; tr < d->ntracks; tr++) {
        if (tr < 64 && (a->hide_mask[tr >> 5] & (1u << (tr & 31)))) continue;
        ReFrame *A = &d->frames[tr * d->nframes + f0];
        if (!A->vis || A->img < 0) continue;
        float m[6];
        reanim_track_matrix(a, tr, a->frame, m);
        if (ov) mat_mul(ov, m, m);
        gfx_draw_affine(A->img, x + m[4] * scale, y + m[5] * scale, m[0] * scale, m[1] * scale, m[2] * scale, m[3] * scale, color);
    }
}
