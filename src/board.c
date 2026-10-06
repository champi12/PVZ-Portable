/* board.c - Tablero de juego: aventura completa del PvZ J2ME (50 niveles) con reglas del PC.
 * Logica a 100 ticks/s (centesimas, "cs"), como el PC.
 * Controles del PvZ J2ME de teclado (PvZBV): cursor por casillas, caja de semillas arriba/abajo,
 * X abre caja / elige / planta, soles se recogen solos alrededor del cursor, triangulo = pala. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include "gfx.h"
#include "audio.h"
#include "input.h"
#include "reanim.h"
#include "defs.h"
#include "board.h"
#include "ui.h"
#include "texts.h"

/* ---------------- imagenes ---------------- */
#define IMG_BG_DAY 147
#define IMG_BG_DIRT 305
#define IMG_BG_NIGHT 572
#define IMG_BG_POOL 8
#define IMG_BG_FOG 110
#define IMG_BG_ROOF 45
#define IMG_SOD_1ROW 238
#define IMG_SOD_3ROW 235
#define IMG_SUN 472
#define IMG_SUNBAR 509
#define IMG_SHOVEL 474
#define IMG_SHOVEL_SLOT 95
#define IMG_SHOVEL_BTN 328    /* boton de la pala del J2ME (30x23) */
#define IMG_CUR_OK 612        /* esquina del cursor del PvZBV (13x12) */
#define IMG_CUR_BAD 613       /* esquina roja: ahi no se puede plantar */
#define IMG_SEEDFRAME 614     /* marco del sobre (41x31) */
#define IMG_SMALLPACK 620     /* sobres pequenos del PvZBV (38x28), en el orden PL_* */
#define SLOT_Y(i) (2 + (i) * 33.0f)
#define IMG_MOWER 248
#define IMG_SHADOW 97
#define IMG_POWIE 83
#define IMG_SPUDOW 104
#define IMG_DOOM 590
#define IMG_BUSH 105
#define IMG_GRAVE1 336
#define IMG_GRAVE2 337
#define IMG_GRAVE3 338
#define IMG_CRATER 155
#define IMG_FLAMES 477
#define IMG_PEA 73           /* guisante del J2ME (la 544 es la col que sostiene la coltapulta) */
#define IMG_SEASPORE 444     /* espora de la seta marina */
#define IMG_SNOWPEA 587
#define IMG_FIREPEA 335
#define IMG_PUFF 324
#define IMG_SPIKE 109        /* espina del cactus (la 426 es su boca, pista 11) */
#define IMG_STAR 20
#define IMG_CABBAGE 544      /* col (la misma que sostiene, pista 6 de la coltapulta) */
#define IMG_KERNEL 188       /* grano de maiz (la 30 es la cesta de la catapulta) */
#define IMG_BUTTER 281
#define IMG_MELON 473        /* pista 7 de la melonpulta */
#define IMG_FIREBALL 349
#define IMG_ICEBALL 64
#define IMG_ROOFCLEANER 413
#define IMG_BALL 162
#define IMG_FLAG 543
#define IMG_NOTE 139
#define IMG_DIRT 400
#define IMG_LADDER 401
#define IMG_MASHED 564       /* patatapum aplastada (frame 8 de su animacion) */
#define IMG_ZHEAD 282        /* cabeza de zombi de la barra de progreso */
#define IMG_PFLAG 543        /* banderita de la barra de progreso */

/* hojas de particulas del J2ME (clase y): imagen y ancho de cada frame */
enum {
    PS_PEA, PS_SNOW, PS_YELLOW, PS_GREENBIT, PS_GREYBIT, PS_YELLOWBIT, PS_FLAKE, PS_PURPLEDOT, PS_CONEBIT, PS_CYANBIT,
    PS_BUCKETBIT, PS_DIRT, PS_STARW, PS_STARY, PS_PUFF, PS_FUMEBIT, PS_SKIN, PS_FLESH, PS_SMOKE, PS_CLOUD, PS_FLAME,
    PS_FIRE, PS_DOOMCLOUD, PS_FUMECLOUD, PS_ICE, PS_RED, PS_SPLASH, PS_ICECRYSTAL, PS_POWIE, PS_SPUDOW, PS_DOOM, PS_COUNT
};
static const short psheet[PS_COUNT][2] = {
    {352,12}, {279,12}, {287,12}, {372,6}, {394,6}, {221,6}, {218,9}, {66,4}, {70,6}, {74,6},
    {234,6}, {138,38}, {150,9}, {396,8}, {197,12}, {89,12}, {85,11}, {142,11}, {15,14}, {535,30}, {165,24},
    {477,44}, {5,30}, {232,40}, {10,30}, {325,44}, {52,61}, {395,34}, {83,54}, {104,53}, {590,60} };

/* ---------------- textos (traduccion oficial, tabla t-spa) ---------------- */
#define TXT_READY TXT[251]
#define TXT_SET TXT[252]
#define TXT_PLANT TXT[253]
#define TXT_HUGE TXT[76]
#define TXT_FINAL TXT[77]
#define TXT_LOSE TXT[263]

#define COLS 9
#define RMAX 6
#define HUD_H 40.0f
#define KPC(px) ((px) * G.cw / 80.0f)     /* px del PC -> px de este fondo */

/* ---------------- geometria por zona ---------------- */
typedef struct { int bg, rows, night, sky_sun, roof; float x0, y0, cw, rh, ws, camx, camy; const char *music; } Geo;
static Geo G;
static int lane[RMAX];          /* 0 tierra, 1 cesped, 2 agua, 3 tejado */
enum { LN_DIRT, LN_GRASS, LN_WATER, LN_ROOF };

int level_area(int lv) { return lv < 10 ? AR_DAY : lv < 20 ? AR_NIGHT : lv < 30 ? AR_POOL : lv < 40 ? AR_FOG : AR_ROOF; }
int level_is_conveyor(int lv) { return lv == 4 || lv == 9 || lv == 19 || lv == 29 || lv == 39 || lv == 49; }
int level_slots(int lv) { int s = 6 + (lv >= 19) + (lv >= 29); return s > 8 ? 8 : s; }

/* ---------------- entidades ---------------- */
typedef struct {
    int type, alive, hp, timer, state, shot_at, glow, sleeping, aux, ladder, snd;
    float dx, dy;             /* desplazamiento de dibujo (salto de la apisonaflor) */
    ReAnim anim;
} Plant;
enum { ZS_WALK, ZS_EAT, ZS_JUMP, ZS_SPECIAL, ZS_DYING, ZS_CHARRED };
typedef struct {
    int alive, type, row, state, dir;
    float x, speed, yoff;
    int hp, helm, maxhelm, shield, maxshield;
    int chill, freeze, butter, arm_lost, fade, chew, hypno, timer, jumped, under, balloon, ammo, swim, angry, summoned;
    int inwater;              /* nadando (en las filas de agua, ya dentro de la piscina) */
    int kelped, jsnd;         /* agarrado por la zampalga; sonido del salto (+1) */
    float jx0;                /* x al empezar un salto */
    ReAnim anim;
} Zombie;
enum { PJ_PEA, PJ_SNOW, PJ_FIRE, PJ_PUFF, PJ_SPIKE, PJ_STAR, PJ_CABBAGE, PJ_KERNEL, PJ_BUTTER, PJ_MELON, PJ_BALL, PJ_BOWL, PJ_SEA, PJ_BOSSFIRE, PJ_BOSSICE };
typedef struct {
    int alive, kind, row, dmg, target, plant_r, plant_c, bounced;
    float x, y, vx, vy, x0, y0, tx, ty, t, dur, maxx;
} Proj;
typedef struct { int alive; float x, y, vy, ty; int ttl, from_plant, collecting, value; } Sun;
typedef struct { int state; float x, sink; } Mower;   /* state 0 quieto, 1 avanzando, 2 gastado, 3 hundiendose */
typedef struct { int alive, sheet, loop, img; float x, y, vx, vy, g, age, ttl, sc, rot, vr, floor; } Part;   /* img>=0: pieza suelta que cae */

#define MAXZ 64
#define MAXP 96
#define MAXS 32
#define MAXPT 160
static Plant P[RMAX][COLS][2];          /* [0] base (nenufar/maceta), [1] planta */
static int grave[RMAX][COLS], crater[RMAX][COLS];
static Zombie Z[MAXZ];
static Proj PJ[MAXP];
static Sun S[MAXS];
static Mower M[RMAX];
static Part PT[MAXPT];

static int pause_confirm;
static int lv, state, state_timer, frame, paused, pause_sel, sun, sky_timer, suns_fallen, result;
static int seeds[10], nseeds, refresh[10], shovel_ok;
static int mode, cur_r, cur_c, bank_sel, held;      /* held = indice del sobre o -1 */
static int wave, nwaves, wave_timer, huge_timer, wave_hp_trig, msg_timer, msg_kind;
static int conveyor, belt[12], belt_n, belt_timer; static float belt_x[12];
static int reward_type, reward_alive; static float reward_x, reward_y;
static float tick_acc;
static float curx = -1, cury, curw, curh;    /* cursor que se desliza hacia su casilla (como el PvZBV) */
enum { ST_INTRO, ST_PLAY, ST_WONWAIT, ST_LOST };
/* intro del J2ME: espera, la camara va a la calle (zombis de la oleada), eleccion de plantas,
 * vuelve, entran los cortacesped y la barra de semillas, y "preparados, listos, a plantar" */
enum { IP_WAIT, IP_OUT, IP_HOLD, IP_CHOOSE, IP_CHOOSE_OUT, IP_BACK, IP_MOW, IP_READY };
#define JF (100.0f / 6.0f)          /* un frame logico del J2ME (6 por segundo) en cs */
static int intro_phase, choosing, ch_list[PL_COUNT], ch_n, ch_cur, ch_scroll, slots, has_shovel_flag;
static float intro_t, hud_off;
static Zombie PZ[12]; static int npz;   /* zombis de muestra en la calle */
enum { MODE_LAWN, MODE_BANK, MODE_SHOVEL };

/* ---------------- utilidades ---------------- */
static int rnd(int a, int b) { return a + rand() % (b - a + 1); }
static float frnd(float a, float b) { return a + (b - a) * (rand() % 10000) / 10000.0f; }
static float cell_x(int c) { return G.x0 + c * G.cw; }
static float cell_y(int r) { return G.y0 + r * G.rh; }
/* tejado: las 5 primeras columnas van bajando 8.3 px cada una hacia la izquierda (medido en el J2ME) */
static float slope_x(float x)
{
    if (!G.roof) return 0;
    float k = 5.5f - (x - G.x0) / G.cw;
    return (k < 0 ? 0 : k > 5 ? 5 : k) * 8.3f;
}
static float cy_x(int r, float x) { return cell_y(r) + slope_x(x); }
static float cy_c(int r, int c) { return cell_y(r) + slope_x(G.x0 + (c + 0.5f) * G.cw); }
static float sxw(float x) { return (x - G.camx) * G.ws; }
static float syw(float y) { return (y - G.camy) * G.ws; }
static float view_right(void) { return G.camx + SCREEN_W / G.ws; }
static float bush_x(void) { return G.x0 + COLS * G.cw + 4; }
static void draw_world(int img, float wx, float wy, u32 col) { gfx_draw_ex(img, sxw(wx), syw(wy), 0, 0, G.ws, G.ws, 0, col, 0); }
static void draw_world_c(int img, float wx, float wy, float sc, u32 col) {
    gfx_draw_ex(img, sxw(wx), syw(wy), img_w(img) / 2.0f, img_h(img) / 2.0f, G.ws * sc, G.ws * sc, 0, col, 0);
}
static int row_ok(int r) { return r >= 0 && r < G.rows && lane[r] != LN_DIRT; }
static int first_row(void) { for (int r = 0; r < G.rows; r++) if (row_ok(r)) return r; return 0; }
static int last_row(void) { for (int r = G.rows - 1; r >= 0; r--) if (row_ok(r)) return r; return 0; }
static int col_of(float x) { int c = (int)((x - G.x0) / G.cw); return c < 0 ? -1 : c >= COLS ? COLS : c; }
static Plant *top_plant(int r, int c) {
    if (r < 0 || r >= G.rows || c < 0 || c >= COLS) return NULL;
    if (P[r][c][1].alive) return &P[r][c][1];
    if (P[r][c][0].alive) return &P[r][c][0];
    return NULL;
}
static const PlantDef *pdef(int t) { return &plant_defs[t == PL_BOWLNUT ? PL_WALLNUT : t]; }
static int seed_type(int i) { return conveyor ? belt[i] : seeds[i]; }

/* ---------------- efectos (particulas del J2ME: hojas de sprites que se reproducen en su vida) ---------------- */
static Part *part(int sheet, float x, float y, float vx, float vy, float g, int ttl)
{
    for (int i = 0; i < MAXPT; i++) if (!PT[i].alive) {
        Part *p = &PT[i]; memset(p, 0, sizeof(*p));
        p->img = -1; p->alive = 1; p->sheet = sheet; p->x = x; p->y = y; p->vx = vx; p->vy = vy; p->g = g; p->ttl = ttl; p->sc = 1;
        return p;
    }
    return NULL;
}
/* trocitos que saltan en todas direcciones (cascos, patata, col...) */
static void burst(int sheet, float x, float y, int n, float speed)
{
    for (int k = 0; k < n; k++) {
        float a = frnd(3.4f, 6.0f), v = frnd(0.4f, 1.0f) * speed;
        part(sheet, x + frnd(-4, 4), y + frnd(-4, 4), cosf(a) * v, sinf(a) * v, 0.035f, rnd(45, 75));
    }
}
static void splat(int sheet, float x, float y) { part(sheet, x, y, 0, 0, 0, 24); }
/* pieza que se suelta del zombi (brazo, cabeza, cono, cubo, puerta): cae girando y rebota en el suelo */
static void drop_part(int img, float x, float y, float floor, float vx)
{
    Part *p = part(0, x, y, vx * 0.4f, -1.1f, 0.06f, 170);
    if (!p) return;
    p->img = img; p->floor = floor; p->vr = frnd(-0.12f, 0.12f) + (vx < 0 ? -0.06f : 0.06f);
}
static void update_parts(void)
{
    for (int i = 0; i < MAXPT; i++) {
        Part *p = &PT[i];
        if (!p->alive) continue;
        p->x += p->vx; p->y += p->vy; p->vy += p->g;
        if (p->img >= 0) {
            p->rot += p->vr;
            if (p->y > p->floor) {               /* rebota una vez y luego se desliza un poco hacia atras */
                p->y = p->floor;
                if (p->vy > 0.9f) { p->vy = -p->vy * 0.3f; p->vr *= 0.4f; }
                else { p->vy = 0; p->g = 0; p->vr = 0; if (p->loop == 0) { p->loop = 2; p->vx = 0.55f; } }
            }
            if (p->loop == 2) p->vx *= 0.95f;
        }
        if (++p->age >= p->ttl) p->alive = 0;
    }
}
static void draw_parts(void)
{
    for (int i = 0; i < MAXPT; i++) {
        Part *p = &PT[i];
        if (!p->alive) continue;
        if (p->img >= 0) {
            float left = p->ttl - p->age;
            u32 col = left < 30 ? ((u32)(left * 255 / 30) << 24) | 0xFFFFFF : WHITE;
            gfx_draw_ex(p->img, sxw(p->x), syw(p->y), img_w(p->img) / 2.0f, img_h(p->img) / 2.0f, G.ws, G.ws, p->rot, col, 0);
            continue;
        }
        int img = psheet[p->sheet][0], fw = psheet[p->sheet][1], h = img_h(img);
        int nf = img_w(img) / fw; if (nf < 1) nf = 1;
        int f = p->loop ? ((int)(p->age / 8)) % nf : (int)(p->age * nf / p->ttl);
        if (f >= nf) f = nf - 1;
        u32 col = WHITE;
        float left = p->ttl - p->age;
        if ((p->loop || nf == 1) && left < 25) col = ((u32)(left * 255 / 25) << 24) | 0xFFFFFF;
        if (p->sc != 1.0f || nf == 1) {
            float sc = p->sc;
            gfx_draw_ex(img, sxw(p->x), syw(p->y), fw / 2.0f, h / 2.0f, sc * G.ws, sc * G.ws, 0, col, 0);
        } else gfx_draw_region(img, f * fw, 0, fw, h, sxw(p->x) - fw / 2, syw(p->y) - h / 2, col);
    }
}

/* petacereza / caja sorpresa: "POWIE!!" con humo naranja y nubes */
static void boom_fx(float x, float y)
{
    for (int k = 0; k < 10; k++) {
        float a = k * 0.628f, v = frnd(0.25f, 0.6f);
        part(k & 1 ? PS_CLOUD : PS_SMOKE, x + cosf(a) * 10, y + sinf(a) * 8, cosf(a) * v, sinf(a) * v * 0.7f, 0, rnd(60, 90));
    }
    Part *t = part(PS_POWIE, x, y - 6, 0, -0.08f, 0, 110); if (t) t->sc = 1.3f;
}
/* patatapum: "SPUDOW!!" y trozos de patata (bk.d del J2ME: 10-15 particulas 12/13) */
static void spud_fx(float x, float y)
{
    int n = rnd(10, 15);
    for (int k = 0; k < n; k++) {
        float a = frnd(3.3f, 6.1f), v = frnd(0.5f, 1.3f);
        part(k > n / 2 ? PS_SKIN : PS_FLESH, x + frnd(-6, 6), y + frnd(-6, 6), cosf(a) * v, sinf(a) * v, 0.04f, rnd(55, 80));
    }
    part(PS_SMOKE, x, y, 0, -0.1f, 0, 50);
    Part *t = part(PS_SPUDOW, x, y - 12, 0, -0.1f, 0, 110); if (t) t->sc = 1.2f;
}

/* ---------------- soles ---------------- */
static void spawn_sun(float x, float y, float ty, int from_plant, int value)
{
    for (int i = 0; i < MAXS; i++) if (!S[i].alive) {
        Sun *s = &S[i]; memset(s, 0, sizeof(*s));
        s->alive = 1; s->x = x; s->y = y; s->ty = ty; s->vy = from_plant ? -1.6f : 0.5f;
        s->ttl = 1500; s->from_plant = from_plant; s->value = value;
        return;
    }
}

static void update_suns(void)
{
    if (G.sky_sun && !conveyor && --sky_timer <= 0) {          /* Board::UpdateSunSpawning */
        int cd = 425 + suns_fallen * 10; if (cd > 950) cd = 950;
        sky_timer = cd + rnd(0, 274); suns_fallen++;
        int c = rnd(0, COLS - 1), r = rnd(first_row(), last_row());
        spawn_sun(cell_x(c) + G.cw / 2, G.camy - 20, cell_y(r) + G.rh / 2, 0, 25);
    }
    for (int i = 0; i < MAXS; i++) {
        Sun *s = &S[i];
        if (!s->alive) continue;
        if (s->collecting) {
            float tx = G.camx + 14 / G.ws, ty = G.camy + 12 / G.ws;
            s->x += (tx - s->x) * 0.08f; s->y += (ty - s->y) * 0.08f;
            if (fabsf(s->x - tx) < 4 && fabsf(s->y - ty) < 4) { s->alive = 0; sun += s->value; if (sun > 9990) sun = 9990; }
            continue;
        }
        if (s->y < s->ty || s->vy < 0) {
            if (s->from_plant) s->vy += 0.06f;
            s->y += s->vy * (s->from_plant ? 1.0f : 0.6f);
            if (s->y > s->ty && s->vy > 0) s->y = s->ty;
        } else if (--s->ttl <= 0) s->alive = 0;
    }
}

static void auto_collect(void)
{
    if (mode == MODE_BANK) return;
    int r0 = cur_r > 0 ? cur_r - 1 : 0, c0 = cur_c > 0 ? cur_c - 1 : 0;
    int r1 = cur_r < G.rows - 1 ? cur_r + 1 : G.rows - 1, c1 = cur_c < COLS - 1 ? cur_c + 1 : COLS - 1;
    float x0 = cell_x(c0), y0 = cell_y(r0), x1 = cell_x(c1 + 1), y1 = cell_y(r1 + 1);
    for (int i = 0; i < MAXS; i++) {
        Sun *s = &S[i];
        if (!s->alive || s->collecting) continue;
        if (s->x + 10 > x0 && s->x - 10 < x1 && s->y + 10 > y0 && s->y - 10 < y1) { s->collecting = 1; sfx_play(SFX_POINTS); }
    }
    (void)x0; (void)x1; (void)y0; (void)y1;
}

/* ---------------- zombis ---------------- */
#define ZAX 27.7f            /* origen de la animacion 0 respecto a los pies del zombi */
#define ZAY 72.2f
static const ZombieDef *zd(Zombie *z) { return &zombie_defs[z->type]; }
static int zhp(Zombie *z) { return z->hp + z->helm + z->shield; }
static int z_hittable(Zombie *z) { return z->alive && z->state < ZS_DYING && !z->under && !z->hypno; }
/* buzo bajo el agua: solo se le puede dar cuando sale a comer */
static int z_submerged(Zombie *z) { return z->type == ZT_SNORKEL && z->inwater && z->state != ZS_EAT; }

static void z_parts(Zombie *z)
{
    if (zd(z)->re != RE_ZOMBIE) { z->anim.hide_mask[0] = 0; return; }
    u32 m = (7u << 11) | (3u << 16) | (0x1FFu << 21);
    m &= ~zd(z)->show;
    if (z->type == ZT_FLAG) m &= ~((1u << 11) | (1u << 12));
    if (z->swim) m &= ~((1u << 16) | (1u << 17));
    if (z->helm > 0) {
        int base = z->type == ZT_CONE ? 21 : 24;
        int st = z->helm * 3 > z->maxhelm * 2 ? 0 : (z->helm * 3 > z->maxhelm ? 1 : 2);
        m &= ~(1u << (base + st));
    }
    if (z->shield > 0 && z->type == ZT_DOOR) {
        int st = z->shield * 3 > z->maxshield * 2 ? 0 : (z->shield * 3 > z->maxshield ? 1 : 2);
        m &= ~(1u << (27 + st));
    }
    if (z->arm_lost) m |= 1u << 20;
    if (z->state == ZS_DYING) m |= 1u << 19;
    z->anim.hide_mask[0] = m;
}

static void z_anim(Zombie *z, int s, int e, int loop)
{
    if (s < 0) return;
    if (z->anim.def && z->anim.start == s && z->anim.end == e && z->anim.loop == loop) return;
    reanim_play(&z->anim, zd(z)->re, s, e, loop);
    z_parts(z);
}

/* rango de andar/comer segun el estado (pertiga sin pertiga, periodico roto, nadando...) */
static void z_walk(Zombie *z)
{
    const ZombieDef *d = zd(z);
    int s = d->walk_s, e = d->walk_e;
    if (z->under) { s = 32; e = 36; }
    else if (z->type == ZT_POLE && z->jumped) { s = 24; e = 34; }
    else if (z->type == ZT_NEWSPAPER && z->angry) { s = 30; e = 33; }
    else if (z->type == ZT_POGO && !z->jumped) { s = 39; e = 41; }
    else if (z->balloon) { s = 4; e = 7; }
    else if (z->type == ZT_SNORKEL && z->inwater) { s = 20; e = 21; }
    else if (z->inwater && d->re == RE_ZOMBIE) { s = 40; e = 49; }
    z_anim(z, s, e, 1);
}
static void z_eat(Zombie *z)
{
    const ZombieDef *d = zd(z);
    int s = d->eat_s, e = d->eat_e;
    if (z->inwater && d->re == RE_ZOMBIE) { s = 40; e = 49; }
    z_anim(z, s, e, 1);
}
/* posicion en el mundo de una pista de la animacion 0 (para soltar brazo, cabeza, cono...) */
static int z_track_pos(Zombie *z, int track, float *x, float *y, int *img)
{
    float hx, hy;
    if (zd(z)->re != RE_ZOMBIE || !reanim_track_info(&z->anim, track, &hx, &hy, img)) return 0;
    *x = z->x - ZAX + hx + img_w(*img) / 2.0f;
    *y = cy_x(z->row, z->x) + G.rh - 2 + z->yoff - ZAY + hy + img_h(*img) / 2.0f;
    return 1;
}
static void z_drop(Zombie *z, int track, int fallback_img)
{
    float x, y; int img = fallback_img;
    if (!z_track_pos(z, track, &x, &y, &img)) { x = z->x; y = cell_y(z->row) + G.rh * 0.3f + z->yoff; img = fallback_img; }
    if (img < 0) return;
    if (z->inwater) { part(PS_SPLASH, x, cell_y(z->row) + G.rh * 0.8f, 0, 0, 0, 30); return; }
    drop_part(img, x, y, cell_y(z->row) + G.rh - 6, frnd(0.2f, 0.7f));
}

static Zombie *spawn_zombie(int type, int row, float x)
{
    for (int i = 0; i < MAXZ; i++) if (!Z[i].alive) {
        Zombie *z = &Z[i]; memset(z, 0, sizeof(*z));
        const ZombieDef *d = &zombie_defs[type];
        z->alive = 1; z->type = type; z->row = row; z->dir = -1;
        z->x = x; z->speed = d->speed * frnd(0.88f, 1.12f);
        z->hp = d->hp; z->helm = z->maxhelm = d->helm; z->shield = z->maxshield = d->shield;
        z->swim = lane[row] == LN_WATER;
        if (type == ZT_BALLOON) z->balloon = 1;
        if (type == ZT_DIGGER) z->under = 1;
        if (type == ZT_CATAPULT) z->ammo = 20;
        if (type == ZT_POLE || type == ZT_POGO) z->jumped = 0;
        reanim_get(d->re);
        z_walk(z);
        z->anim.frame += frnd(0, z->anim.end - z->anim.start);
        z_parts(z);
        return z;
    }
    return NULL;
}

enum { BS_ENTER, BS_IDLE, BS_SUMMON, BS_STOMP, BS_HEADDOWN, BS_HEAD, BS_HEADUP, BS_DIE };
static void boss_set(Zombie *z, int st);
static void z_kill(Zombie *z, int charred)
{
    if (z->state >= ZS_DYING) return;
    if (z->jsnd) { sfx_stop(z->jsnd - 1); z->jsnd = 0; }          /* aplastado en pleno salto: sin el "hup" */
    if (z->type == ZT_BOSS) { z->state = ZS_DYING; z->fade = 500; boss_set(z, BS_DIE); sfx_play(SFX_BOSSEXPLOSION); return; }
    if (charred && zd(z)->re == RE_ZOMBIE && reanim_get(RE_ZOMBIE_CHARRED)) {
        z->state = ZS_CHARRED;
        reanim_play(&z->anim, RE_ZOMBIE_CHARRED, 0, reanim_get(RE_ZOMBIE_CHARRED)->nframes - 1, 0);
        z->anim.speed = 1.5f; z->fade = 260;
    } else {
        z->state = ZS_DYING; z->fade = 300;
        if (zd(z)->re == RE_ZOMBIE && !z->balloon) {
            if (!z->arm_lost) { z_drop(z, 20, 350); z->arm_lost = 1; }
            z_drop(z, 19, 78);                        /* la cabeza cae rodando */
        }
        if (z->inwater && zd(z)->re == RE_ZOMBIE) { reanim_play(&z->anim, RE_ZOMBIE, 50, 55, 0); z_parts(z); }
        else if (zd(z)->die_s >= 0) { reanim_play(&z->anim, zd(z)->re, zd(z)->die_s, zd(z)->die_e, 0); z_parts(z); }
        else z->fade = 100;
        sfx_play(z->type == ZT_GARGANTUAR ? SFX_GARGANTUDEATH : SFX_LIMBS_POP);
    }
    z->anim.hide_mask[0] |= 0; z_parts(z);
}

/* dano: from_front=1 si viene de frente (la puerta/periodico lo absorbe) */
static int quiet_dmg;
static void z_damage(Zombie *z, int dmg, int from_front, int snow)
{
    if (!z->alive || z->state >= ZS_DYING) return;
    if (z->type == ZT_BOSS) { dmg = z->angry == 5 ? dmg * 2 : dmg / 2; from_front = 0; snow = 0; }
    if (z->shield > 0 && from_front) {
        z->shield -= dmg;
        if (!quiet_dmg) sfx_play(z->type == ZT_DOOR ? (rand() & 1 ? SFX_SHIELDHIT : SFX_SHIELDHIT2) : SFX_PAPER);
        if (z->shield <= 0) {
            dmg = -z->shield; z->shield = 0;
            if (z->type == ZT_DOOR) z_drop(z, 29, 515);
            if (z->type == ZT_NEWSPAPER) {         /* se enfada: rompe el periodico (26-29) y corre (30-33) */
                z->angry = 1; z->speed *= 2.5f; sfx_play(SFX_NEWSPAPER_RARRGH); sfx_play(SFX_NEWSPAPER_RIP);
                if (z->state < ZS_DYING) { z->state = ZS_SPECIAL; z_anim(z, 26, 29, 0); }
            }
        } else { z_parts(z); return; }
    }
    if (z->helm > 0) {
        int metal = z->type == ZT_BUCKET || z->type == ZT_FOOTBALL;
        float hx = z->x + 2, hy = cell_y(z->row) + G.rh * 0.15f + z->yoff;
        if ((z->type == ZT_CONE || metal) && !quiet_dmg) part(metal ? PS_BUCKETBIT : PS_CONEBIT, hx, hy, frnd(-0.4f, 0.6f), -0.5f, 0.04f, 40);
        int had = z->helm;
        z->helm -= dmg;
        if (had > 0 && z->helm <= 0) {             /* el cono o el cubo se caen al suelo */
            if (z->type == ZT_CONE) z_drop(z, 23, 33);
            else if (z->type == ZT_BUCKET) z_drop(z, 26, 316);
        }
        if (!quiet_dmg) sfx_play(z->type == ZT_BUCKET || z->type == ZT_FOOTBALL ? (rand() & 1 ? SFX_SHIELDHIT : SFX_SHIELDHIT2) : SFX_PLASTICHIT);
        if (z->helm < 0) { z->hp += z->helm; z->helm = 0; }
    } else {
        z->hp -= dmg;
        static const int sp[] = { SFX_SPLAT, SFX_SPLAT2, SFX_SPLAT3 };
        if (!quiet_dmg) sfx_play_vol(sp[rand() % 3], 200);
    }
    if (snow && z->type != ZT_BOSS) { if (!z->chill) sfx_play(SFX_FROZEN); z->chill = 1000; }
    if (!z->arm_lost && z->hp * 3 < zd(z)->hp * 2 && z->hp > 0) { z_drop(z, 20, 350); z->arm_lost = 1; sfx_play_vol(SFX_LIMBS_POP, 160); }
    if (z->type == ZT_GARGANTUAR && !z->summoned && z->hp < zd(z)->hp / 2) {    /* lanza al zombidito */
        z->summoned = 1;
        Zombie *imp = spawn_zombie(ZT_IMP, z->row, cell_x(rnd(1, 3)));
        if (imp) sfx_play(SFX_IMP);
    }
    if (z->hp <= 0) z_kill(z, 0); else z_parts(z);
}

static void start_mower(int r) { if (M[r].state == 0) { M[r].state = 1; sfx_play(SFX_LAWNMOWER); } }

static void kill_plant(int r, int c, int layer) { P[r][c][layer].alive = 0; }

/* planta que un zombi que va hacia la izquierda tiene delante (o -1) */
static int z_front_col(Zombie *z)
{
    float fx = z->x - (z->dir < 0 ? 8 : -8);
    int c = col_of(fx);
    if (c < 0 || c >= COLS) return -1;
    Plant *p = top_plant(z->row, c);
    if (!p) return -1;
    if (pdef(p->type)->flags & PF_NOEAT) return -1;
    if (p->type == PL_POTATOMINE && p->state == 3) return -1;     /* pure de patata */
    float pc = cell_x(c) + G.cw / 2;
    if (fabsf(fx - pc) < G.cw * 0.45f) return c;
    return -1;
}

static void hypno_fight(Zombie *z)
{
    /* zombi hipnotizado: camina a la derecha y muerde zombis */
    for (int i = 0; i < MAXZ; i++) {
        Zombie *o = &Z[i];
        if (o == z || !z_hittable(o) || o->row != z->row || o->type == ZT_BOSS) continue;
        if (fabsf(o->x - z->x) < 14) {
            z->state = ZS_EAT; z_eat(z);
            if ((frame & 3) == 0) { quiet_dmg = 1; z_damage(o, 4, 0, 0); quiet_dmg = 0; }
            if (--z->chew <= 0) { z->chew = 50; static const int ch[] = { SFX_CHOMP, SFX_CHOMP2, SFX_CHOMPSOFT }; sfx_play(ch[rand() % 3]); }
            return;
        }
    }
    z->state = ZS_WALK; z_walk(z);
    z->x += z->speed;
    if (z->x > view_right() + 20) z->alive = 0;
}

/* Dr. Zombi: maquina de estados con los rangos de su animacion (archivo 16 del J2ME):
 * 0-8 entrada, 9-11 reposo, 12-27 mano (invoca), 52-60 pisoton, 71-80 baja la cabeza,
 * 91-98 cabeza abajo (vulnerable, escupe bola de fuego o hielo), 81-90 sube la cabeza, 114-131 muere */
static void boss_set(Zombie *z, int st)
{
    static const int rg[][3] = { {0,8,0}, {9,11,1}, {12,27,0}, {52,60,0}, {71,80,0}, {91,98,1}, {81,90,0}, {114,131,0} };
    z->angry = st; z->timer = 0;
    reanim_play(&z->anim, RE_BOSS, rg[st][0], rg[st][1], rg[st][2]);
}
static Proj *new_proj(int kind, int row, float x, float y);
static void update_boss(Zombie *z)
{
    z->timer++;
    switch (z->angry) {
    case BS_ENTER: if (reanim_done(&z->anim)) boss_set(z, BS_IDLE); break;
    case BS_IDLE:
        if (z->timer > 260) {
            static const int seq[] = { BS_SUMMON, BS_STOMP, BS_SUMMON, BS_HEADDOWN };
            boss_set(z, seq[(z->jumped++) % 4]);
        }
        break;
    case BS_SUMMON:
        if (z->timer == 70) {
            static const int pool[] = { ZT_NORMAL, ZT_CONE, ZT_BUCKET, ZT_POLE, ZT_LADDER, ZT_FOOTBALL, ZT_POGO, ZT_GARGANTUAR };
            int n = rnd(2, 3);
            for (int k = 0; k < n; k++) spawn_zombie(pool[rnd(0, z->hp < 20000 ? 7 : 5)], rnd(0, G.rows - 1), cell_x(rnd(7, 8)) + 10);
            sfx_play(SFX_HYDRAULIC);
        }
        if (reanim_done(&z->anim)) boss_set(z, BS_IDLE);
        break;
    case BS_STOMP:
        if (reanim_done(&z->anim)) {
            int c = rnd(6, 8);
            for (int r = 0; r < G.rows; r++) for (int cc = c - 1; cc <= c; cc++) if (cc >= 0) { kill_plant(r, cc, 1); kill_plant(r, cc, 0); }
            sfx_play(SFX_GARGANTUAR_THUMP); boss_set(z, BS_IDLE);
        }
        break;
    case BS_HEADDOWN:
        if (reanim_done(&z->anim)) {
            boss_set(z, BS_HEAD);
            int r = rnd(0, G.rows - 1);
            Proj *q = new_proj(rand() & 1 ? PJ_BOSSFIRE : PJ_BOSSICE, r, cell_x(8), cell_y(r) + G.rh * 0.55f);
            if (q) q->vx = -0.55f;
            sfx_play(SFX_BOSSBOULDERATTACK);
        }
        break;
    case BS_HEAD: if (z->timer > 550) boss_set(z, BS_HEADUP); break;
    case BS_HEADUP: if (reanim_done(&z->anim)) boss_set(z, BS_IDLE); break;
    }
}

static void update_zombies(void)
{
    for (int i = 0; i < MAXZ; i++) {
        Zombie *z = &Z[i];
        if (!z->alive) continue;
        if (z->state >= ZS_DYING) { if (--z->fade <= 0) z->alive = 0; continue; }
        if (z->type == ZT_BOSS) { update_boss(z); continue; }
        if (z->freeze > 0) { z->freeze--; z->anim.speed = 0; continue; }
        if (z->butter > 0) { z->butter--; z->anim.speed = 0; continue; }
        float slow = z->chill > 0 ? 0.5f : 1.0f;
        if (z->chill > 0) z->chill--;
        z->anim.speed = slow;
        if (z->hypno) { hypno_fight(z); continue; }
        const ZombieDef *d = zd(z);

        /* --- animaciones de una vez (romper el periodico, salir de la tierra, caer del globo, saltar) --- */
        if (z->state == ZS_SPECIAL) {
            if (reanim_done(&z->anim)) { z->state = ZS_WALK; z_walk(z); }
            continue;
        }
        if (z->state == ZS_JUMP) {
            if (z->type == ZT_POLE) {                 /* la animacion 13-23 lleva al zombi ~58 px a la izquierda */
                if (reanim_done(&z->anim)) {
                    z->x = z->jx0 - (z->timer ? 0 : 58);
                    z->jumped = 1; z->speed = 0.064f; z->state = ZS_WALK; z_walk(z); z->jsnd = 0;
                }
            } else {                                  /* saltarin / delfin: brinco por encima de la planta */
                float f = (++z->timer) / 60.0f;
                if (f > 1) f = 1;
                z->x = z->jx0 - G.cw * 1.3f * f;
                z->yoff = -sinf(f * 3.14159f) * 24;
                if (f >= 1) { z->yoff = 0; z->state = ZS_WALK; if (z->type == ZT_DOLPHIN) z->jumped = 1; z_walk(z); }
            }
            continue;
        }
        if (z->under) {                               /* minero: excava hasta la casa y sale (37-41) */
            z->x -= z->speed * slow * 1.6f;
            if (z->x < G.x0 + 6) {
                z->under = 0; z->dir = 1; z->speed = 0.064f; sfx_play(SFX_DIRT_RISE);
                part(PS_DIRT, z->x, cell_y(z->row) + G.rh - 6, 0, 0, 0, 60);
                z->state = ZS_SPECIAL; z_anim(z, 37, 41, 0);
            }
            continue;
        }
        /* entrar en la piscina: chapoteo y animacion de nadar */
        int water = z->swim && z->x < G.x0 + COLS * G.cw - 4;
        if (water != z->inwater) {
            z->inwater = water;
            if (water) { part(PS_SPLASH, z->x, cell_y(z->row) + G.rh * 0.75f, 0, 0, 0, 40); sfx_play(SFX_ZOMBIE_ENTERING_WATER); }
            if (z->state == ZS_EAT) z_eat(z); else z_walk(z);
        }
        if (z->type == ZT_JACK && z->x < bush_x() && ++z->timer > 700 + (i * 37) % 900) {
            /* caja sorpresa: explota en 3x3 */
            sfx_play(SFX_JACK_SURPRISE); sfx_play(SFX_EXPLOSION);
            int c = col_of(z->x);
            for (int r = z->row - 1; r <= z->row + 1; r++) for (int cc = c - 1; cc <= c + 1; cc++)
                if (r >= 0 && r < G.rows && cc >= 0 && cc < COLS) { kill_plant(r, cc, 1); kill_plant(r, cc, 0); }
            boom_fx(z->x, cell_y(z->row) + G.rh / 2);
            z->alive = 0; continue;
        }
        if (z->type == ZT_DANCER && !z->summoned && z->x < bush_x() - G.cw) {
            z->summoned = 1;
            int rr[2] = { z->row - 1, z->row + 1 };
            for (int k = 0; k < 2; k++) if (row_ok(rr[k]) && lane[rr[k]] != LN_WATER) spawn_zombie(ZT_BACKUP, rr[k], z->x);
            spawn_zombie(ZT_BACKUP, z->row, z->x + G.cw);
            if (z->x - G.cw > G.x0) spawn_zombie(ZT_BACKUP, z->row, z->x - G.cw);
            sfx_play(SFX_DANCER);
        }
        if (z->type == ZT_CATAPULT && z->ammo > 0 && z->x < cell_x(8) + G.cw * 0.6f) {
            /* para y lanza balones a la planta mas a la izquierda */
            z_anim(z, d->eat_s, d->eat_e, 1);
            if (++z->timer >= 300) {
                z->timer = 0;
                for (int c = 0; c < COLS; c++) if (top_plant(z->row, c)) {
                    for (int k = 0; k < MAXP; k++) if (!PJ[k].alive) {
                        Proj *q = &PJ[k]; memset(q, 0, sizeof(*q));
                        q->alive = 1; q->kind = PJ_BALL; q->row = z->row; q->x0 = z->x - 10; q->y0 = cell_y(z->row);
                        q->tx = cell_x(c) + G.cw / 2; q->ty = cell_y(z->row) + G.rh * 0.5f; q->dur = 90; q->plant_r = z->row; q->plant_c = c;
                        break;
                    }
                    z->ammo--; sfx_play(SFX_BASKETBALL); break;
                }
            }
            continue;
        }
        if (z->balloon) {                             /* globo: vuela por encima de todo (4-7) */
            z_walk(z);
            z->x -= z->speed * slow;
            if (z->x < G.x0 - 20 && state == ST_PLAY) { state = ST_LOST; state_timer = 0; music_stop(); sfx_play(SFX_LOSEMUSIC); }
            continue;
        }

        /* los zombis normales se defienden del hipnotizado que tienen delante */
        Zombie *foe = NULL;
        if (z->dir < 0) for (int k = 0; k < MAXZ; k++) {
            Zombie *o = &Z[k];
            if (o != z && o->alive && o->hypno && o->state < ZS_DYING && o->row == z->row && o->x < z->x + 2 && z->x - o->x < 16) { foe = o; break; }
        }
        int c = foe ? -1 : z_front_col(z);
        if (foe) {
            if (z->state != ZS_EAT) { z->state = ZS_EAT; z->chew = -1; z_eat(z); }
            if ((frame + i) % 4 == 0) { quiet_dmg = 1; z_damage(foe, 4, 0, 0); quiet_dmg = 0; }
        } else if (c >= 0) {
            Plant *p = top_plant(z->row, c);
            /* saltador / delfin / saltarin: pasan por encima (la nuez cascara-rabias los para) */
            int can_jump = (z->type == ZT_POLE || z->type == ZT_POGO || z->type == ZT_DOLPHIN) && !z->jumped;
            if (z->type == ZT_DOLPHIN && !z->inwater) can_jump = 0;
            if (p->type == PL_SQUASH) can_jump = 0;        /* la apisonaflor lo aplasta antes */
            if (can_jump) {
                z->jx0 = z->x; z->timer = p->type == PL_TALLNUT; z->state = ZS_JUMP;
                if (z->type == ZT_POLE) { z_anim(z, 13, 23, 0); z->anim.speed = 1.3f; z->jsnd = sfx_play(SFX_POLEVAULT) + 1; }
                else if (p->type == PL_TALLNUT) { z->state = ZS_WALK; z->jumped = 1; z_walk(z); sfx_play(SFX_BONK); continue; }
                else { z->timer = 0; sfx_play(z->type == ZT_POGO ? SFX_POGO_ZOMBIE : SFX_DOLPHIN_APPEARS); }
                continue;
            }
            if (z->type == ZT_LADDER && !z->jumped && (p->type == PL_WALLNUT || p->type == PL_TALLNUT)) {
                z->jumped = 1; p->ladder = 1; z->x -= G.cw * 1.2f; sfx_play(SFX_LADDER_ZOMBIE);
                z_anim(z, 33, 44, 1); continue;
            }
            if (p->ladder && z->type != ZT_GARGANTUAR) { z->x -= G.cw * 1.2f; continue; }
            if (z->type == ZT_GARGANTUAR) {           /* aplasta la planta de un golpe */
                z_anim(z, d->eat_s, d->eat_e, 0);
                if (++z->timer > 120) { z->timer = 0; kill_plant(z->row, c, 1); kill_plant(z->row, c, 0); sfx_play(SFX_GARGANTUAR_THUMP); z_walk(z); }
                continue;
            }
            if (z->type == ZT_CATAPULT) { kill_plant(z->row, c, 1); kill_plant(z->row, c, 0); continue; }
            if (p->type == PL_POTATOMINE && p->state == 2) continue;
            if (p->type == PL_HYPNOSHROOM && !p->sleeping) {   /* hipnotiza al que la muerde */
                p->alive = 0; z->hypno = 1; z->dir = 1; sfx_play(SFX_MINDCONTROLLED); continue;
            }
            if (z->state != ZS_EAT) { z->state = ZS_EAT; z->chew = -1; z_eat(z); }
            if ((frame + i) % 4 == 0) {
                int dmg = (int)(4 * slow) * (z->angry ? 2 : 1);
                p->hp -= dmg;
                if (p->hp <= 0) { p->alive = 0; sfx_play(SFX_GULP); }
            }
        } else {
            if (z->state != ZS_WALK) { z->state = ZS_WALK; z_walk(z); }
            z->x += z->dir * z->speed * slow;
            if (z->type == ZT_POGO && !z->jumped) z->yoff = -fabsf(sinf(frame * 0.12f)) * 10; else z->yoff = 0;
        }
        if (z->state == ZS_EAT) {                     /* mordiscos sincronizados con la animacion */
            int f = (int)z->anim.frame, mid = (z->anim.start + z->anim.end + 1) / 2;
            if (z->chew >= 0 && ((f == z->anim.start && z->chew != f) || (f == mid && z->chew != f))) {
                static const int ch[] = { SFX_CHOMP, SFX_CHOMP2, SFX_CHOMPSOFT }; sfx_play_vol(ch[rand() % 3], 220);
            }
            z->chew = f;
        }
        if (z->dir < 0 && z->x < G.x0 - 8 && M[z->row].state == 0) start_mower(z->row);
        if (z->dir < 0 && z->x < G.x0 - 26 && state == ST_PLAY) { state = ST_LOST; state_timer = 0; music_stop(); sfx_play(SFX_LOSEMUSIC); }
        if (z->dir > 0 && z->x > view_right() + 20) z->alive = 0;
        if (rand() % 2400 == 0) { static const int g[] = { SFX_GROAN, SFX_GROAN2, SFX_GROAN3, SFX_GROAN4 }; sfx_play_vol(g[rand() % 4], 140); }
    }
    for (int r = 0; r < G.rows; r++) {
        Mower *m = &M[r];
        if (m->state == 3) { m->sink += 0.6f; if (m->sink > 30) m->state = 2; continue; }
        if (m->state != 1) continue;
        m->x += 1.25f;
        if (lane[r] == LN_WATER && m->x > G.x0 - 4) {        /* el cortacesped no flota: cae a la piscina */
            m->state = 3; m->sink = 0;
            part(PS_SPLASH, m->x + 18, cell_y(r) + G.rh * 0.7f, 0, 0, 0, 40); sfx_play(SFX_ZOMBIE_ENTERING_WATER);
        }
        for (int i = 0; i < MAXZ; i++) if (Z[i].alive && Z[i].row == r && Z[i].state < ZS_DYING && !Z[i].balloon && Z[i].type != ZT_BOSS && fabsf(Z[i].x - (m->x + 16)) < 12) z_kill(&Z[i], 0);
        if (m->x > view_right() + 20) m->state = 2;
    }
}

/* ---------------- plantas ---------------- */
/* ---------------- plantas ---------------- */
static Zombie *z_ahead(int row, float x, float range, int air)
{
    Zombie *best = NULL;
    for (int i = 0; i < MAXZ; i++) {
        Zombie *z = &Z[i];
        if (!z_hittable(z) || (z->row != row && z->type != ZT_BOSS)) continue;
        if (z->balloon && !air) continue;
        if (z->x < x - 6 || z->x > x + range || z->x > bush_x() + 12) continue;
        if (!best || z->x < best->x) best = z;
    }
    return best;
}

static Proj *new_proj(int kind, int row, float x, float y)
{
    for (int i = 0; i < MAXP; i++) if (!PJ[i].alive) {
        Proj *q = &PJ[i]; memset(q, 0, sizeof(*q));
        q->alive = 1; q->kind = kind; q->row = row; q->x = q->x0 = x; q->y = q->y0 = y; q->vx = KPC(3.33f);
        q->maxx = view_right() + 10;
        static const int dm[] = { 20, 20, 40, 20, 20, 20, 40, 20, 40, 80, 75, 0, 20, 0, 0 };
        q->dmg = dm[kind];
        return q;
    }
    return NULL;
}

static void plant_origin(Plant *p, int r, int c, float *ox, float *oy)
{
    static float zero[4];
    float *bb = p->anim.def ? p->anim.def->bbox : zero;
    *ox = cell_x(c) + G.cw / 2 - (bb[0] + bb[2]) / 2;
    *oy = cy_c(r, c) + G.rh - 3 - bb[3];
    if (P[r][c][0].alive && p == &P[r][c][1] && P[r][c][0].anim.def) {
        float *pb = P[r][c][0].anim.def->bbox; float h = pb[3] - pb[1];
        *oy -= P[r][c][0].type == PL_LILYPAD ? h * 0.35f : h - 13;
    }
}

static void explode(float cx, int row, float rx, int rows, int dmg, int sfx, int charred)
{
    for (int i = 0; i < MAXZ; i++) {
        Zombie *z = &Z[i];
        if (!z->alive || z->state >= ZS_DYING || abs(z->row - row) > rows || z->hypno) continue;
        if (fabsf(z->x - cx) <= rx) { if (zhp(z) <= dmg) z_kill(z, charred); else z_damage(z, dmg, 0, 0); }
    }
    sfx_play(sfx);
}

static void plant_set_idle(Plant *p)
{
    const PlantDef *d = pdef(p->type);
    if (d->idle_s >= 0) reanim_play(&p->anim, d->re, d->idle_s, d->idle_e, 1);
    else reanim_play(&p->anim, d->re, -1, -1, 1);
    p->anim.hide_mask[0] = d->hide;
}

static void plant_act(Plant *p)
{
    const PlantDef *d = pdef(p->type);
    if (d->act_s >= 0) { reanim_play(&p->anim, d->re, d->act_s, d->act_e, 0); p->anim.hide_mask[0] = d->hide; }
}

static void update_plant(Plant *p, int r, int c)
{
    const PlantDef *d = pdef(p->type);
    float px = cell_x(c) + G.cw / 2;
    if (p->sleeping) return;
    switch (d->kind) {
    case PK_SUN: {
        if (p->type == PL_SUNFLOWER) {
            u32 m = (1u << 5) | (1u << 6) | (1u << 7);
            if (p->timer < 30 || p->glow > 70) m = (1u << 5) | (1u << 6);
            else if (p->timer < 60 || p->glow > 35) m = (1u << 5) | (1u << 7);
            else if (p->timer < 90 || p->glow > 0) m = (1u << 6) | (1u << 7);
            p->anim.hide_mask[0] = m;
            if (p->glow > 0) p->glow--;
        }
        if (p->type == PL_SUNSHROOM && p->state == 0 && ++p->aux > 12000) {     /* crece a los 2 min */
            p->state = 1; reanim_play(&p->anim, RE_SUNSHROOM, 9, 11, 1); sfx_play(SFX_PLANTGROW);
        }
        if (--p->timer <= 0) {
            p->timer = rnd(d->rate - 150, d->rate); p->glow = 100;
            int v = p->type == PL_SUNSHROOM && p->state == 0 ? 15 : 25;
            spawn_sun(px + rnd(-6, 6), cell_y(r) + 10, cell_y(r) + G.rh / 2 + 6, 1, v);
        }
        break;
    }
    case PK_SHOOTER: {
        if (p->timer > 0) p->timer--;
        if (p->type == PL_REPEATER && p->aux > 0 && --p->aux == 0) {
            float ox, oy, hx, hy; int img;
            plant_origin(p, r, c, &ox, &oy);
            if (reanim_track_info(&p->anim, 6, &hx, &hy, &img) && img >= 0) {
                Proj *q = new_proj(PJ_PEA, r, ox + hx + img_w(img) * 1.12f - 8, oy + hy + img_h(img) * 1.12f * 0.40f);
                if (q) q->x0 = px - 4;
                sfx_play_vol(rand() & 1 ? SFX_THROW : SFX_THROW2, 200);
            }
        }
        float range = 2000;
        int air = p->type == PL_CACTUS;
        if (p->type == PL_PUFFSHROOM || p->type == PL_SEASHROOM) range = G.cw * 3.2f;
        if (p->type == PL_SCAREDYSHROOM) {
            Zombie *near = z_ahead(r, px - G.cw * 1.5f, G.cw * 3.0f, 0);
            int scared = near && fabsf(near->x - px) < G.cw * 1.6f;
            if (scared != p->aux) { p->aux = scared; if (scared) reanim_play(&p->anim, RE_SCAREDYSHROOM, 11, 13, 1); else plant_set_idle(p); }
            if (scared) break;
        }
        int has = 0;
        if (p->type == PL_THREEPEATER) { for (int k = -1; k <= 1; k++) if (row_ok(r + k) && z_ahead(r + k, px, range, 0)) has = 1; }
        else has = z_ahead(r, px, range, air) != NULL;
        if (p->state == 0 && p->timer <= 0 && has) {
            p->timer = d->rate; p->state = 1; plant_act(p);
            /* frame del disparo medido en las animaciones del J2ME (la cabeza en su punto mas adelantado) */
            switch (p->type) {
            case PL_PEASHOOTER: case PL_SNOWPEA: case PL_REPEATER: p->shot_at = 9; break;
            case PL_THREEPEATER: p->shot_at = 5; break;
            case PL_PUFFSHROOM: case PL_SEASHROOM: case PL_CACTUS: case PL_SCAREDYSHROOM: p->shot_at = 7; break;
            default: p->shot_at = p->anim.start + (p->anim.end - p->anim.start) / 2;
            }
        }
        if (p->state == 1) {
            if (p->shot_at >= 0 && p->anim.frame >= p->shot_at) {
                float ox, oy, hx = 0, hy = 0; int img = -1;
                plant_origin(p, r, c, &ox, &oy);
                float mx = px + 8, my = cell_y(r) + G.rh * 0.42f;
                if (p->type == PL_PEASHOOTER || p->type == PL_SNOWPEA || p->type == PL_REPEATER) {
                    int head = p->type == PL_SNOWPEA ? 5 : p->type == PL_REPEATER ? 6 : 4;
                    if (reanim_track_info(&p->anim, head, &hx, &hy, &img) && img >= 0) {
                        mx = ox + hx + img_w(img) * 1.12f - 8; my = oy + hy + img_h(img) * 1.12f * 0.40f;   /* dentro de la boca (cabeza a escala 1.12) */
                    }
                }
                int kind = p->type == PL_SNOWPEA ? PJ_SNOW : p->type == PL_SEASHROOM ? PJ_SEA :
                           (p->type == PL_PUFFSHROOM || p->type == PL_SCAREDYSHROOM) ? PJ_PUFF : p->type == PL_CACTUS ? PJ_SPIKE : PJ_PEA;
                if (kind == PJ_PUFF) my = cell_y(r) + G.rh * 0.70f;          /* las setas son bajitas */
                if (p->type == PL_SCAREDYSHROOM) my = cell_y(r) + G.rh * 0.55f;
                if (p->type == PL_CACTUS && reanim_track_info(&p->anim, 11, &hx, &hy, &img)) { mx = ox + hx + 10; my = oy + hy + 5; }   /* sale de la boca */
                if (p->type == PL_THREEPEATER) {
                    for (int k = -1; k <= 1; k++) if (row_ok(r + k)) new_proj(PJ_PEA, r + k, mx, my + k * G.rh);
                } else {
                    Proj *q = new_proj(kind, r, mx, my);
                    if (q) q->x0 = px - 4;               /* tramo desde la planta: un zombi pegado a ella tambien recibe */
                    if (q && (p->type == PL_PUFFSHROOM || p->type == PL_SEASHROOM)) q->maxx = px + G.cw * 3.4f;
                    if (p->type == PL_REPEATER) p->aux = 22;   /* el segundo guisante sale de la boca un poco despues */
                }
                if (kind == PJ_PUFF || kind == PJ_SEA) sfx_play_vol(SFX_PUFF, 200); else sfx_play_vol(rand() & 1 ? SFX_THROW : SFX_THROW2, 200);
                p->shot_at = -1;
            }
            if (reanim_done(&p->anim)) { p->state = 0; plant_set_idle(p); }
        }
        break;
    }
    case PK_STAR: {
        if (p->timer > 0) p->timer--;
        int any = 0;
        for (int i = 0; i < MAXZ; i++) if (z_hittable(&Z[i]) && Z[i].x < bush_x()) { any = 1; break; }
        if (any && p->timer <= 0) {
            p->timer = d->rate; plant_act(p);
            float cy = cell_y(r) + G.rh * 0.4f, v = KPC(3.33f);
            static const float dirs[5][2] = { {0,-1}, {0,1}, {-1,0}, {0.866f,-0.5f}, {0.866f,0.5f} };
            for (int k = 0; k < 5; k++) { Proj *q = new_proj(PJ_STAR, r, px, cy); if (q) { q->vx = dirs[k][0] * v; q->vy = dirs[k][1] * v; } }
            sfx_play_vol(SFX_THROW, 180);
        }
        if (reanim_done(&p->anim)) plant_set_idle(p);
        break;
    }
    case PK_LOBBER: {
        /* el proyectil sale cuando la pista que lo sostiene desaparece (frame 6 en las tres catapultas) */
        if (p->timer > 0) p->timer--;
        Zombie *t = z_ahead(r, px, 2000, 0);
        if (p->state == 0 && t && p->timer <= 0) {
            p->timer = d->rate; p->state = 1; plant_act(p);
            p->aux = rand() % 4 == 0;                  /* lanzamaiz: mantequilla de vez en cuando */
        }
        if (p->state == 1 && p->anim.frame >= 6) {
            p->state = 2;
            if (t) {
                int kind = p->type == PL_CABBAGEPULT ? PJ_CABBAGE : p->type == PL_MELONPULT ? PJ_MELON : (p->aux ? PJ_BUTTER : PJ_KERNEL);
                float ox, oy; plant_origin(p, r, c, &ox, &oy);
                Proj *q = new_proj(kind, r, px - 4, oy + 2);
                if (q) { q->target = (int)(t - Z); q->tx = t->x; q->ty = cy_x(r, t->x) + G.rh * 0.35f; q->dur = 100; }
                sfx_play(p->type == PL_KERNELPULT ? SFX_KERNELPULT : SFX_THROW);
            }
        }
        if (p->state && reanim_done(&p->anim)) { p->state = 0; plant_set_idle(p); }
        break;
    }
    case PK_FUME: {
        /* J2ME (bk.i): en el frame 8 del gesto suelta 20 bolitas moradas (hoja 197) que recorren el alcance
         * y deja una nube (hoja 89) en cada zombi alcanzado. Traspasa puertas. */
        if (p->timer > 0) p->timer--;
        if (p->state == 0 && p->timer <= 0 && z_ahead(r, px, G.cw * 4.25f, 0)) { p->timer = d->rate; p->state = 1; plant_act(p); }
        if (p->state == 1 && p->anim.frame >= 8) {
            p->state = 2;
            float ox, oy; plant_origin(p, r, c, &ox, &oy);
            float mx = px + 8, my = cell_y(r) + G.rh * 0.55f;
            for (int k = 0; k < 20; k++) {
                float v = frnd(1.6f, 3.2f);
                part(PS_PUFF, mx + frnd(0, 6), my + frnd(-3, 3), v, frnd(-0.12f, 0.12f), 0, (int)(G.cw * 4.0f / v));
            }
            for (int i = 0; i < MAXZ; i++) {
                Zombie *z = &Z[i];
                if (z_hittable(z) && z->row == r && z->x > px - 6 && z->x < px + G.cw * 4.25f && !z->balloon && !z_submerged(z)) {
                    z_damage(z, 20, 0, 0);
                    part(PS_FUMEBIT, z->x + frnd(-4, 4), cell_y(r) + G.rh * 0.4f, 0, -0.1f, 0, 30);
                }
            }
            p->snd = sfx_play(SFX_FUME);
        }
        if (p->state && reanim_done(&p->anim)) { p->state = 0; plant_set_idle(p); }
        break;
    }
    case PK_INSTANT:
        if (reanim_done(&p->anim)) {
            if (p->type == PL_CHERRYBOMB) { explode(px, r, G.cw * 1.6f, 1, 1800, SFX_CHERRYBOMB, 1); boom_fx(px, cell_y(r) + G.rh / 2); }
            else if (p->type == PL_JALAPENO) {
                explode(px, r, 2000, 0, 1800, SFX_JALAPENO, 1);
                for (int k = 0; k < MAXP; k++) if (PJ[k].alive && PJ[k].kind == PJ_BOSSICE && PJ[k].row == r) PJ[k].alive = 0;   /* derrite la de hielo */
                for (int k = 0; k < COLS; k++) {   /* llamas (hoja 477) en toda la fila */
                    Part *f = part(PS_FIRE, cell_x(k) + G.cw / 2, cell_y(r) + G.rh * 0.45f, 0, 0, 0, rnd(90, 110));
                    if (f) { f->loop = 1; f->age = rnd(0, 20); f->ttl += f->age; }
                }
            } else if (p->type == PL_DOOMSHROOM) {
                explode(px, r, G.cw * 3.2f, 3, 1800, SFX_DOOMSHROOM, 1);
                for (int k = 0; k < 16; k++) {
                    float a = k * 0.3927f, v = frnd(0.3f, 0.9f);
                    part(k & 1 ? PS_DOOMCLOUD : PS_RED, px + cosf(a) * 12, cell_y(r) + G.rh / 2 + sinf(a) * 10, cosf(a) * v, sinf(a) * v * 0.6f - 0.15f, 0, rnd(80, 120));
                }
                { Part *t = part(PS_DOOM, px, cell_y(r) - 4, 0, -0.08f, 0, 140); if (t) t->sc = 1.4f; }
                crater[r][c] = 18000;
            } else if (p->type == PL_ICESHROOM) {
                for (int i = 0; i < MAXZ; i++) if (z_hittable(&Z[i]) && Z[i].type != ZT_BOSS) { Z[i].freeze = rnd(400, 600); Z[i].chill = 2000; z_damage(&Z[i], 20, 0, 0); }
                for (int k = 0; k < MAXP; k++) if (PJ[k].alive && PJ[k].kind == PJ_BOSSFIRE) PJ[k].alive = 0;   /* apaga la bola de fuego */
                sfx_play(SFX_FROZEN);
                for (int k = 0; k < 14; k++)          /* copos de la seta hielo por toda la pantalla */
                    part(k & 1 ? PS_ICE : PS_FLAKE, G.camx + frnd(20, SCREEN_W - 20), G.camy + frnd(40, SCREEN_H - 20), 0, frnd(0.1f, 0.3f), 0, rnd(70, 110));
            }
            p->alive = 0;
        }
        break;
    case PK_MINE:
        if (p->state == 0 && --p->timer <= 0) { p->state = 1; reanim_play(&p->anim, RE_POTATOMINE, 1, 4, 0); sfx_play(SFX_DIRT_RISE); }
        else if (p->state == 1 && reanim_done(&p->anim)) { p->state = 2; reanim_play(&p->anim, RE_POTATOMINE, 5, 7, 1); }
        else if (p->state == 3) { if (--p->timer <= 0) p->alive = 0; }
        else if (p->state == 2) {
            for (int i = 0; i < MAXZ; i++) {
                Zombie *z = &Z[i];
                if (z->alive && z->row == r && z->state < ZS_DYING && !z->balloon && fabsf(z->x - 6 - px) < G.cw * 0.55f) {
                    explode(px, r, G.cw * 0.75f, 0, 1800, SFX_POTATO_MINE, 0);   /* el J2ME no los chamusca */
                    spud_fx(px, cell_y(r) + G.rh * 0.6f);
                    p->state = 3; p->timer = 200;        /* queda el pure (frame 8) unos 2 s, como el J2ME (estado 11) */
                    reanim_play(&p->anim, RE_POTATOMINE, 8, 8, 1);
                    break;
                }
            }
        }
        break;
    case PK_WALL: {
        int st = p->hp * 3 > d->hp * 2 ? 0 : (p->hp * 3 > d->hp ? 1 : 2);
        if (st != p->state) {
            p->state = st;
            if (p->type == PL_WALLNUT) reanim_play(&p->anim, RE_WALLNUT, st == 1 ? 26 : 27, st == 1 ? 26 : 27, 1);
            else reanim_play(&p->anim, RE_TALLNUT, st == 1 ? 20 : 19, st == 1 ? 20 : 19, 1);
        }
        break;
    }
    case PK_CHOMPER:
        if (p->state == 0) {                          /* buscando */
            Zombie *t = z_ahead(r, px - 6, G.cw * 1.6f, 0);   /* llega al zombi que come la planta de delante */
            if (t && t->type != ZT_BOSS) { p->state = 1; p->aux = (int)(t - Z); p->shot_at = 10; reanim_play(&p->anim, RE_CHOMPER, 7, 12, 0); }
        } else if (p->state == 1 && p->shot_at >= 0 && p->anim.frame >= p->shot_at) {
            p->shot_at = -1; sfx_play(SFX_BIGCHOMP);       /* el mordisco suena cuando cierra la boca */
        } else if (p->state == 1 && reanim_done(&p->anim)) {
            Zombie *t = &Z[p->aux];
            if (z_hittable(t)) {
                if (t->type == ZT_GARGANTUAR) z_damage(t, 40, 0, 0);
                else { t->alive = 0; p->state = 2; p->timer = d->rate; reanim_play(&p->anim, RE_CHOMPER, 13, 16, 1); }
            }
            if (p->state == 1) { p->state = 0; plant_set_idle(p); }
        } else if (p->state == 2 && --p->timer <= 0) {
            p->state = 0; reanim_play(&p->anim, RE_CHOMPER, 17, 23, 0); sfx_play(SFX_GULP);
        } else if (p->state == 0 && reanim_done(&p->anim) && p->anim.start == 17) plant_set_idle(p);
        break;
    case PK_SQUASH:
        if (p->state == 0) {
            for (int i = 0; i < MAXZ; i++) {
                Zombie *z = &Z[i];
                if (z_hittable(z) && z->row == r && !z->balloon && fabsf(z->x - px) < G.cw * 1.1f) {
                    p->state = 1; p->aux = (int)(z->x); p->timer = 0; sfx_play(rand() & 1 ? SFX_SQUASH_HMM : SFX_SQUASH_HMM2);
                    reanim_play(&p->anim, RE_SQUASH, 5, 7, 1); break;            /* "hmm" mirando */
                }
            }
        } else if (p->state == 1 && ++p->timer > 45) {
            p->state = 2; p->timer = 0; reanim_play(&p->anim, RE_SQUASH, 8, 9, 0);   /* salto */
        } else if (p->state == 2) {
            p->timer++;
            float f = p->timer / 40.0f; if (f > 1) f = 1;
            p->dx = (p->aux - px) * f;
            p->dy = -sinf(f * 3.14159f) * G.rh * 1.2f - (f < 0.5f ? 0 : 0);
            if (p->timer >= 40) {
                for (int i = 0; i < MAXZ; i++) { Zombie *z = &Z[i]; if (z_hittable(z) && z->row == r && fabsf(z->x - p->aux) < G.cw * 0.8f) z_damage(z, 1800, 0, 0); }
                sfx_play(SFX_GARGANTUAR_THUMP); p->state = 3; p->timer = 0; p->dy = 0;
            }
        } else if (p->state == 3 && ++p->timer > 60) p->alive = 0;     /* aplastada un momento */
        break;
    case PK_KELP:
        if (p->state == 0) {
            for (int i = 0; i < MAXZ; i++) {
                Zombie *z = &Z[i];
                if (z_hittable(z) && z->row == r && !z->balloon && z->type != ZT_BOSS && fabsf(z->x - px) < G.cw * 0.75f) {
                    p->state = 1; p->aux = i; p->timer = 0; z->freeze = 10000; z->kelped = 1;
                    reanim_play(&p->anim, RE_TANGLEKELP, 5, 16, 0); sfx_play(SFX_ZOMBIE_ENTERING_WATER);
                    part(PS_SPLASH, z->x, cell_y(r) + G.rh * 0.75f, 0, 0, 0, 40);
                    break;
                }
            }
        } else {
            Zombie *z = &Z[p->aux];
            p->timer++;
            if (z->alive) { z->yoff = p->timer > 30 ? (p->timer - 30) * 0.9f : 0; z->freeze = 10000; }   /* lo agarra y lo hunde sin moverlo */
            if (p->timer > 90) { if (z->alive) { z->alive = 0; part(PS_SPLASH, z->x, cell_y(r) + G.rh * 0.75f, 0, 0, 0, 40); } p->alive = 0; sfx_play(SFX_ZOMBIESPLASH); }
        }
        break;
    case PK_SPIKE:
        if (--p->timer <= 0) {
            p->timer = 100;
            int hit = 0;
            for (int i = 0; i < MAXZ; i++) {
                Zombie *z = &Z[i];
                if (z_hittable(z) && z->row == r && !z->balloon && fabsf(z->x - px) < G.cw * 0.55f) {
                    if (z->type == ZT_CATAPULT) { z_kill(z, 0); p->hp -= 100; } else z_damage(z, 20, 0, 0);
                    hit = 1;
                }
            }
            if (hit) plant_act(p);
        }
        if (p->hp <= 0) p->alive = 0;
        if (reanim_done(&p->anim)) plant_set_idle(p);
        break;
    case PK_GRAVEBUSTER:
        if (p->timer == 0) p->snd = sfx_play(SFX_GRAVEBUSTERCHOMP);
        if (reanim_done(&p->anim) || ++p->timer > 500) { grave[r][c] = 0; p->alive = 0; sfx_stop(p->snd); }
        break;
    default: break;
    }
}

static void update_projectiles(void)
{
    for (int i = 0; i < MAXP; i++) {
        Proj *q = &PJ[i];
        if (!q->alive) continue;
        if (q->kind >= PJ_CABBAGE && q->kind <= PJ_BALL) {          /* bombeados */
            q->t += 1;
            float f = q->t / q->dur;
            if (q->kind != PJ_BALL && q->target >= 0 && z_hittable(&Z[q->target])) q->tx = Z[q->target].x;
            q->x = q->x0 + (q->tx - q->x0) * f;
            q->y = q->y0 + (q->ty - q->y0) * f - sinf(f * 3.14159f) * G.rh * 1.6f;
            if (f >= 1) {
                q->alive = 0;
                if (q->kind == PJ_BALL) {
                    Plant *p = top_plant(q->plant_r, q->plant_c);
                    if (p) { p->hp -= q->dmg; if (p->hp <= 0) p->alive = 0; }
                    continue;
                }
                Zombie *z = (q->target >= 0 && z_hittable(&Z[q->target])) ? &Z[q->target] : NULL;
                if (z) {
                    z_damage(z, q->dmg, 0, 0);
                    if (q->kind == PJ_BUTTER) { z->butter = 400; sfx_play(SFX_BUTTER); }
                    if (q->kind == PJ_MELON) {
                        sfx_play(rand() & 1 ? SFX_MELONIMPACT : SFX_MELONIMPACT2);
                        for (int k = 0; k < MAXZ; k++) if (&Z[k] != z && z_hittable(&Z[k]) && abs(Z[k].row - z->row) <= 1 && fabsf(Z[k].x - z->x) < G.cw * 1.2f) z_damage(&Z[k], 26, 0, 0);
                    }
                    if (q->kind == PJ_KERNEL) sfx_play(SFX_KERNELPULT2);
                    float hy = cell_y(z->row) + G.rh * 0.35f;
                    if (q->kind == PJ_CABBAGE) { splat(PS_PEA, z->x, hy); burst(PS_GREENBIT, z->x, hy, 4, 0.8f); }
                    else if (q->kind == PJ_MELON) { splat(PS_PEA, z->x, hy); burst(PS_GREENBIT, z->x, hy, 8, 1.1f); }
                    else if (q->kind == PJ_KERNEL) burst(PS_YELLOWBIT, z->x, hy, 3, 0.7f);
                    else if (q->kind == PJ_BUTTER) splat(PS_YELLOW, z->x, hy);
                }
            }
            continue;
        }
        if (q->kind == PJ_BOSSFIRE || q->kind == PJ_BOSSICE) {
            q->x += q->vx;
            int c = col_of(q->x);
            if (c >= 0 && c < COLS) { kill_plant(q->row, c, 1); kill_plant(q->row, c, 0); }
            if (q->x < G.x0 - 30) q->alive = 0;
            continue;
        }
        if (q->kind == PJ_BOWL) {                    /* nuez de bolos */
            q->x += KPC(1.6f);
            q->y += q->vy;
            int row = (int)((q->y - G.y0) / G.rh);
            if (row < first_row() || row > last_row()) { q->vy = -q->vy; q->y += q->vy * 2; row = (int)((q->y - G.y0) / G.rh); }
            q->row = row;
            q->t += 1;
            for (int k = 0; k < MAXZ; k++) {
                Zombie *z = &Z[k];
                if (z_hittable(z) && z->row == row && fabsf(z->x - q->x) < 12 && q->target != k) {
                    z_damage(z, 300, 1, 0); q->target = k; sfx_play(rand() & 1 ? SFX_BOWLINGIMPACT : SFX_BOWLINGIMPACT2);
                    q->vy = (row <= first_row() ? 1 : row >= last_row() ? -1 : (rand() & 1 ? 1 : -1)) * G.rh / 36.0f;
                    break;
                }
            }
            if (q->x > view_right() + 20) q->alive = 0;
            continue;
        }
        q->x += q->vx; q->y += q->vy;
        int first = q->t == 0; q->t = 1;
        if (q->kind == PJ_STAR) {
            q->row = (int)((q->y - G.y0) / G.rh);
            if (q->x < G.x0 - 20 || q->x > view_right() || q->y < G.camy || q->y > G.y0 + G.rows * G.rh) { q->alive = 0; continue; }
        }
        if (q->x > q->maxx) { q->alive = 0; continue; }
        /* plantorcha: el guisante pasa a ser de fuego */
        int c = col_of(q->x);
        if (c >= 0 && c < COLS && P[q->row][c][1].alive && P[q->row][c][1].type == PL_TORCHWOOD && fabsf(q->x - (cell_x(c) + G.cw / 2)) < 3) {
            if (q->kind == PJ_PEA) { q->kind = PJ_FIRE; q->dmg = 40; sfx_play_vol(SFX_FIREPEA, 160); }
            else if (q->kind == PJ_SNOW) { q->kind = PJ_PEA; q->dmg = 20; }
        }
        /* tejado: los proyectiles rectos de las 4 primeras columnas chocan con la pendiente */
        if (lane[q->row] == LN_ROOF && q->kind != PJ_STAR && q->x0 < cell_x(4) && q->x > cell_x(4) + 4) { q->alive = 0; continue; }
        for (int k = 0; k < MAXZ; k++) {
            Zombie *z = &Z[k];
            if (!z_hittable(z) || (z->row != q->row && z->type != ZT_BOSS)) continue;
            if (z->balloon && q->kind != PJ_SPIKE && q->kind != PJ_STAR) continue;
            if (z_submerged(z)) continue;
            float back = first && q->x0 < q->x ? q->x0 : q->x - q->vx;   /* tramo recorrido este tick */
            if (z->type == ZT_BOSS ? q->x >= cell_x(7) : (q->x >= z->x - 8 && back <= z->x + 10)) {
                if (z->balloon && q->kind == PJ_SPIKE) { z->balloon = 0; z->speed = 0.064f; sfx_play(SFX_BALLOON_POP); z_anim(z, zd(z)->walk_s, zd(z)->walk_e, 1); }
                else {
                    z_damage(z, q->dmg, 1, q->kind == PJ_SNOW);
                    static const signed char hitfx[] = { PS_PEA, PS_SNOW, PS_FLAME, PS_PUFF, PS_GREENBIT, PS_STARY, -1, -1, -1, -1, -1, -1, PS_PUFF, -1, -1 };
                    int hs = hitfx[q->kind];
                    if (hs == PS_GREENBIT) burst(hs, q->x, q->y, 3, 0.6f);
                    else if (hs >= 0) { Part *h = part(hs, q->x + 4, q->y, 0, 0, 0, hs == PS_FLAME ? 30 : 24); (void)h; }
                    if (q->kind == PJ_FIRE) { z->chill = 0; for (int o = 0; o < MAXZ; o++) if (&Z[o] != z && z_hittable(&Z[o]) && Z[o].row == z->row && fabsf(Z[o].x - z->x) < G.cw * 0.6f) z_damage(&Z[o], 13, 0, 0); sfx_play(SFX_IGNITE); }
                }
                q->alive = 0; break;
            }
        }
    }
}

/* ---------------- oleadas (codigo del J2ME, clase cj) ---------------- */
static int is_flag_wave(int w) { return (w % 10) == 9 || w == nwaves - 1; }
static int pick_row(int type)
{
    int rows[RMAX], n = 0;
    int water = zombie_defs[type].flags & ZF_SWIM;
    for (int r = 0; r < G.rows; r++) {
        if (!row_ok(r)) continue;
        int w = lane[r] == LN_WATER;
        if (water && !w) continue;
        if (!water && w && !(type == ZT_NORMAL || type == ZT_CONE || type == ZT_BUCKET || type == ZT_FLAG)) continue;
        rows[n++] = r;
    }
    return n ? rows[rnd(0, n - 1)] : first_row();
}
static void spawn_type(int t)
{
    int row = pick_row(t);
    spawn_zombie(t, row, view_right() + frnd(5, 35));
}
static void spawn_wave(void)
{
    int flag = is_flag_wave(wave);
    int pts = wave == 0 ? 1 : wave / 3 + 1;
    if (flag) {
        int n = pts < 8 ? pts : 8;
        for (int k = 0; k < n; k++) { spawn_type(ZT_NORMAL); pts -= 1; }
        pts = (int)((wave / 3 + 1) * 2.5f);
        spawn_type(ZT_FLAG);
    }
    if (lv == 49 && wave == 0) {
        Zombie *b = spawn_zombie(ZT_BOSS, first_row(), cell_x(7));
        if (b) { boss_set(b, BS_ENTER); b->jumped = 0; }
        pts = 0;
    }
    int guard = 0;
    while (pts > 0 && guard++ < 50) {
        int tot = 0, ok[ZT_COUNT];
        for (int t = 0; t < ZT_COUNT; t++) {
            ok[t] = z_minwave[t] <= wave + 1 && pts >= z_cost[t] && z_levels[t][lv] == '1' && z_weight[t] > 0 && t != ZT_BOSS;
            if (G.rows <= 5 && (zombie_defs[t].flags & ZF_SWIM)) ok[t] = 0;
            if (lane[0] == LN_ROOF && t == ZT_DIGGER) ok[t] = 0;
            if (ok[t]) tot += z_weight[t];
        }
        if (!tot) break;
        int pick = rnd(0, tot - 1), t;
        for (t = 0; t < ZT_COUNT; t++) { if (!ok[t]) continue; if (pick < z_weight[t]) break; pick -= z_weight[t]; }
        if (t >= ZT_COUNT) break;
        spawn_type(t); pts -= z_cost[t];
    }
    /* noche: en la ultima oleada salen zombis de las tumbas */
    if (wave == nwaves - 1) for (int r = 0; r < G.rows; r++) for (int c = 0; c < COLS; c++) if (grave[r][c]) {
        spawn_zombie(rand() & 1 ? ZT_NORMAL : ZT_CONE, r, cell_x(c) + G.cw / 2); sfx_play(SFX_GRAVESTONE_RUMBLE);
    }
    int hp = 0;
    for (int i = 0; i < MAXZ; i++) if (Z[i].alive && Z[i].state < ZS_DYING) hp += zhp(&Z[i]);
    wave_hp_trig = (int)(hp * frnd(0.5f, 0.65f));
    if (wave == 0) sfx_play(SFX_AWOOGA);
    if (flag && wave == nwaves - 1) { msg_kind = 2; msg_timer = 300; sfx_play(SFX_FINALWAVE); }
    wave++;
    wave_timer = 2500 + rnd(0, 599);
    if (wave < nwaves && is_flag_wave(wave)) wave_timer = 4500;
}

static void update_waves(void)
{
    if (wave >= nwaves) {
        int any = 0;
        Zombie *last = NULL;
        for (int i = 0; i < MAXZ; i++) if (Z[i].alive && !Z[i].hypno) { any = 1; last = &Z[i]; }
        (void)last;
        if (!any && state == ST_PLAY) {
            state = ST_WONWAIT; state_timer = 0; music_stop(); sfx_play(SFX_WINMUSIC);
            reward_type = level_reward[lv]; reward_alive = 1;
            reward_x = cell_x(5); reward_y = cell_y((first_row() + last_row()) / 2) + G.rh / 2;
        }
        return;
    }
    if (huge_timer > 0) { if (--huge_timer == 0) spawn_wave(); return; }
    if (wave > 0 && wave_timer > 200) {
        int hp = 0;
        for (int i = 0; i < MAXZ; i++) if (Z[i].alive && Z[i].state < ZS_DYING && !Z[i].hypno) hp += zhp(&Z[i]);
        if (hp <= wave_hp_trig) wave_timer = 200;
    }
    if (--wave_timer <= 0) {
        if (is_flag_wave(wave)) { huge_timer = 725; msg_kind = 1; msg_timer = 400; sfx_play(SFX_HUGEWAVE); sfx_play_vol(SFX_SIREN, 200); }
        else spawn_wave();
    }
}

/* ---------------- cinta transportadora (1-5 bolos, x-10) ---------------- */
static void update_belt(void)
{
    if (!conveyor) return;
    if (--belt_timer <= 0 && belt_n < 8) {
        belt_timer = lv == 4 ? 450 : 700;
        int t;
        if (lv == 4) t = PL_BOWLNUT;
        else {
            static const int set9[] = { PL_PEASHOOTER, PL_CHERRYBOMB, PL_WALLNUT, PL_POTATOMINE, PL_SNOWPEA, PL_CHOMPER, PL_REPEATER };
            static const int set19[] = { PL_PUFFSHROOM, PL_FUMESHROOM, PL_GRAVEBUSTER, PL_HYPNOSHROOM, PL_SCAREDYSHROOM, PL_ICESHROOM, PL_DOOMSHROOM };
            static const int set29[] = { PL_LILYPAD, PL_SQUASH, PL_THREEPEATER, PL_TANGLEKELP, PL_JALAPENO, PL_SPIKEWEED, PL_TORCHWOOD, PL_TALLNUT, PL_LILYPAD };
            static const int set39[] = { PL_LILYPAD, PL_SEASHROOM, PL_CACTUS, PL_STARFRUIT, PL_PUFFSHROOM, PL_JALAPENO, PL_TALLNUT, PL_LILYPAD };
            static const int set49[] = { PL_FLOWERPOT, PL_FLOWERPOT, PL_CABBAGEPULT, PL_KERNELPULT, PL_MELONPULT, PL_JALAPENO, PL_ICESHROOM, PL_FLOWERPOT };
            const int *s; int n;
            if (lv == 9) { s = set9; n = 7; } else if (lv == 19) { s = set19; n = 7; } else if (lv == 29) { s = set29; n = 9; }
            else if (lv == 39) { s = set39; n = 8; } else { s = set49; n = 8; }
            t = s[rnd(0, n - 1)];
        }
        belt[belt_n] = t; belt_x[belt_n] = SCREEN_H; belt_n++;   /* entran por abajo en la columna de la izquierda */
    }
    for (int i = 0; i < belt_n; i++) {
        float target = SLOT_Y(i);
        if (belt_x[i] > target) { belt_x[i] -= 0.8f; if (belt_x[i] < target) belt_x[i] = target; }
    }
}
static void belt_remove(int i)
{
    for (int k = i; k < belt_n - 1; k++) { belt[k] = belt[k + 1]; belt_x[k] = belt_x[k + 1]; }
    belt_n--;
    if (bank_sel >= belt_n) bank_sel = belt_n > 0 ? belt_n - 1 : 0;
}

/* ---------------- plantar ---------------- */
static int bank_count(void) { return conveyor ? belt_n : nseeds; }
static int bank_ready(int i)
{
    if (conveyor) return i < belt_n && belt_x[i] <= SLOT_Y(i) + 0.5f;
    return refresh[i] == 0 && sun >= plant_defs[seeds[i]].cost;
}

static int can_plant(int t, int r, int c)
{
    if (!row_ok(r) || c < 0 || c >= COLS || crater[r][c]) return 0;
    if (t == PL_BOWLNUT) return c <= 2 && !top_plant(r, c);
    const PlantDef *d = &plant_defs[t];
    if (t == PL_GRAVEBUSTER) return grave[r][c] && !P[r][c][1].alive;
    if (grave[r][c]) return 0;
    int water = lane[r] == LN_WATER, roof = lane[r] == LN_ROOF;
    if (d->kind == PK_POT) {
        if (t == PL_LILYPAD) return water && !P[r][c][0].alive;
        return !water && !P[r][c][0].alive && !P[r][c][1].alive;   /* maceta */
    }
    if (d->flags & PF_AQUATIC) return water && !P[r][c][0].alive && !P[r][c][1].alive;
    if (P[r][c][1].alive) return 0;
    if (water && !(P[r][c][0].alive && P[r][c][0].type == PL_LILYPAD)) return 0;
    if (roof && !(P[r][c][0].alive && P[r][c][0].type == PL_FLOWERPOT)) return 0;
    if (lane[r] == LN_GRASS && P[r][c][0].alive && P[r][c][0].type == PL_FLOWERPOT) return 1;
    return 1;
}

static void plant_at(int r, int c, int t)
{
    if (t == PL_BOWLNUT) {
        Proj *q = new_proj(PJ_BOWL, r, cell_x(c) + G.cw / 2, cell_y(r) + G.rh / 2);
        if (q) { q->target = -1; q->vy = 0; }
        sfx_play(SFX_BOWLING);
        return;
    }
    const PlantDef *d = &plant_defs[t];
    int layer = d->kind == PK_POT ? 0 : 1;
    Plant *p = &P[r][c][layer];
    memset(p, 0, sizeof(*p));
    p->alive = 1; p->type = t; p->hp = d->hp; p->shot_at = -1; p->snd = -1;
    p->sleeping = (d->flags & PF_MUSHROOM) && !G.night;
    if (t == PL_SUNFLOWER || t == PL_SUNSHROOM) p->timer = rnd(300, 1250);
    if (t == PL_POTATOMINE) { p->timer = d->rate; reanim_play(&p->anim, RE_POTATOMINE, 0, 0, 1); }
    else if (d->kind == PK_INSTANT) { reanim_play(&p->anim, d->re, d->act_s >= 0 ? d->idle_s : 0, d->act_e, 0); p->anim.speed = t == PL_CHERRYBOMB ? 3.0f : 2.0f; }
    else if (t == PL_GRAVEBUSTER) reanim_play(&p->anim, RE_GRAVEBUSTER, 0, 9, 0);
    else {
        plant_set_idle(p);
        p->anim.frame += (rand() % 100) / 100.0f * (p->anim.end - p->anim.start);
    }
    if (p->sleeping) {
        static const int sl[][3] = { { PL_PUFFSHROOM, 8, 11 }, { PL_FUMESHROOM, 12, 15 }, { PL_SCAREDYSHROOM, 17, 20 },
            { PL_DOOMSHROOM, 13, 19 }, { PL_SUNSHROOM, 12, 14 }, { PL_HYPNOSHROOM, 4, 8 }, { PL_SEASHROOM, 8, 11 } };
        for (unsigned k = 0; k < sizeof(sl) / sizeof(sl[0]); k++) if (sl[k][0] == t) reanim_play(&p->anim, d->re, sl[k][1], sl[k][2], 1);
    }
    p->anim.hide_mask[0] = d->hide;
    sfx_play(lane[r] == LN_WATER ? SFX_PLANT_WATER : (rand() & 1 ? SFX_PLANT : SFX_PLANT2));
}

/* PvZBV coloca el cursor al elegir semilla: misma idea */
static void auto_place(int t)
{
    Zombie *lead = NULL;
    for (int i = 0; i < MAXZ; i++) { Zombie *z = &Z[i]; if (z_hittable(z) && z->x < bush_x() && z->type != ZT_BOSS && (!lead || z->x < lead->x)) lead = z; }
    if ((t == PL_CHERRYBOMB || t == PL_POTATOMINE || t == PL_SQUASH || t == PL_JALAPENO || t == PL_CHOMPER) && lead) {
        int c = col_of(lead->x) - 1; if (c < 0) c = 0; if (c > COLS - 1) c = COLS - 1;
        for (; c >= 0; c--) if (can_plant(t, lead->row, c)) { cur_r = lead->row; cur_c = c; return; }
    }
    if (t != PL_SUNFLOWER && t != PL_SUNSHROOM && lead)
        for (int c = 0; c < COLS; c++) if (can_plant(t, lead->row, c)) { cur_r = lead->row; cur_c = c; return; }
    for (int c = 0; c < COLS; c++) for (int r = first_row(); r <= last_row(); r++) if (can_plant(t, r, c)) { cur_r = r; cur_c = c; return; }
}

/* ---------------- arranque ---------------- */
static void setup_area(void)
{
    int a = level_area(lv);
    memset(&G, 0, sizeof(G));
    G.x0 = 171; G.cw = 30.2f; G.rows = 5; G.y0 = 92; G.rh = 36.8f; G.sky_sun = 1;
    for (int r = 0; r < RMAX; r++) lane[r] = LN_GRASS;
    if (a == AR_DAY) {
        G.bg = lv <= 2 ? IMG_BG_DIRT : IMG_BG_DAY; G.music = "data/music/day_grasswalk.mp3";
        if (lv == 0) for (int r = 0; r < 5; r++) lane[r] = r == 2 ? LN_GRASS : LN_DIRT;
        if (lv == 1 || lv == 2) { lane[0] = lane[4] = LN_DIRT; }
    } else if (a == AR_NIGHT) { G.bg = IMG_BG_NIGHT; G.night = 1; G.sky_sun = 0; G.music = "data/music/night_moongrains.mp3"; }
    else if (a == AR_POOL || a == AR_FOG) {
        G.bg = a == AR_POOL ? IMG_BG_POOL : IMG_BG_FOG; G.rows = 6; G.y0 = 85; G.rh = 33.4f;
        lane[2] = lane[3] = LN_WATER;
        if (a == AR_FOG) { G.night = 1; G.sky_sun = 0; G.music = "data/music/fog_rigormormist.mp3"; } else G.music = "data/music/pool_waterygraves.mp3";
    } else {
        G.bg = IMG_BG_ROOF; G.y0 = 70; G.rh = 36.6f; G.x0 = 168; G.cw = 29; G.roof = 1; G.music = "data/music/roof_grazetheroof.mp3";
        for (int r = 0; r < 5; r++) lane[r] = LN_ROOF;
    }
    if (lv == 4) G.music = "data/music/minigame_loonboon.mp3";
    else if (lv == 49) G.music = "data/music/boss_brainiacmaniac.mp3";
    else if (level_is_conveyor(lv)) G.music = "data/music/conveyer.mp3";
    G.ws = 1.0f;                                   /* sin zoom (como el juego original) */
    G.camy = G.y0 + G.rows * G.rh - SCREEN_H + 6;  /* que se vea entero el jardin */
    if (G.camy > G.y0 - HUD_H) G.camy = G.y0 - HUD_H;
    if (G.camy < 0) G.camy = 0;
    G.camx = 0;   /* misma vista que el J2ME: casa a la izquierda, acera a la derecha */
    if (G.roof) G.camy = 18;   /* la pendiente baja la ultima fila de la columna 0 hasta y=286 */
}

/* orden de plantas del J2ME (ids 0..30 de su tabla; asi aparecen en "elige tus plantas") */
static const signed char j2me_order[PL_COUNT] = {
    PL_SEASHROOM, PL_TANGLEKELP, PL_SUNFLOWER, PL_PEASHOOTER, PL_WALLNUT, PL_REPEATER, PL_SNOWPEA, PL_CHERRYBOMB,
    PL_POTATOMINE, PL_CHOMPER, PL_THREEPEATER, PL_SQUASH, PL_JALAPENO, PL_SPIKEWEED, PL_TORCHWOOD, PL_TALLNUT,
    PL_CABBAGEPULT, PL_KERNELPULT, PL_MELONPULT, PL_STARFRUIT, PL_CACTUS, PL_PUFFSHROOM, PL_SUNSHROOM, PL_FUMESHROOM,
    PL_GRAVEBUSTER, PL_SCAREDYSHROOM, PL_ICESHROOM, PL_HYPNOSHROOM, PL_DOOMSHROOM, PL_LILYPAD, PL_FLOWERPOT };

/* reposo de cada zombi (primer rango de su animacion) para los de muestra */
static const signed char z_idle[ZT_COUNT][2] = {
    {0,7},{0,7},{0,7},{0,7},{0,5},{0,7},{0,3},{0,10},{4,10},{0,8},{0,4},{0,5},{0,6},{0,6},{0,7},{0,4},{0,3},{0,7},{0,5},{0,7},{0,8},{0,8} };

static void setup_preview(void)
{
    /* posiciones (pies) de los zombis de muestra medidas en el J2ME (sombras 550 en la calle) */
    static const short pos[9][2] = { {546,146},{507,146},{528,152},{508,208},{546,208},{582,239},{526,245},{580,270},{523,276} };
    int types[ZT_COUNT], nt = 0;
    for (int t = 0; t < ZT_COUNT; t++) {
        if (t == ZT_BOSS || t == ZT_BACKUP || t == ZT_IMP || z_levels[t][lv] != '1') continue;
        if (zombie_defs[t].flags & ZF_SWIM) continue;
        types[nt++] = t;
    }
    npz = lv == 49 ? 0 : (lv < 3 ? 4 + lv : 9);
    float dy = G.rows == 6 ? -6 : (lane[0] == LN_ROOF ? -14 : 0);
    for (int i = 0; i < npz; i++) {
        Zombie *z = &PZ[i]; memset(z, 0, sizeof(*z));
        int t = i < nt ? types[nt - 1 - i] : ZT_NORMAL;
        if (i >= nt && nt > 1 && (i & 1)) t = types[rnd(0, nt - 1)];
        const ZombieDef *d = &zombie_defs[t];
        z->alive = 1; z->type = t; z->dir = -1; z->x = pos[i][0] + frnd(-3, 3); z->yoff = pos[i][1] + 6 + dy;
        z->hp = d->hp; z->helm = z->maxhelm = d->helm; z->shield = z->maxshield = d->shield;
        reanim_play(&z->anim, d->re, z_idle[t][0], z_idle[t][1], 1);
        z->anim.frame += frnd(0, z_idle[t][1] - z_idle[t][0]);
        z->anim.speed = frnd(0.6f, 0.9f);
        z_parts(z);
    }
}

static void init_refresh(void)
{
    for (int i = 0; i < nseeds; i++) { int rt = plant_defs[seeds[i]].refresh; refresh[i] = rt >= 5000 ? 3500 : (rt >= 3000 ? 2000 : 0); }
}

void board_start(int level, const int *av, int nav, int nslots, int has_shovel)
{
    lv = level; setup_area();
    memset(P, 0, sizeof(P)); memset(Z, 0, sizeof(Z)); memset(PJ, 0, sizeof(PJ)); memset(S, 0, sizeof(S));
    memset(PT, 0, sizeof(PT)); memset(grave, 0, sizeof(grave)); memset(crater, 0, sizeof(crater));
    reanim_unload_all(); img_unload_all(); sfx_unload_all();
    img_load(G.bg); font_load(FONT_SMALL); font_load(FONT_BIG); font_load(FONT_NUM); font_load(FONT_MED);
    conveyor = level_is_conveyor(lv); belt_n = 0; belt_timer = 200;
    slots = nslots; has_shovel_flag = has_shovel;
    /* semillas: todas si caben; si no, se eligen durante la intro (como el J2ME) */
    ch_n = 0; ch_cur = 0; ch_scroll = 0;
    for (int k = 0; k < PL_COUNT; k++) for (int i = 0; i < nav; i++) if (av[i] == j2me_order[k]) ch_list[ch_n++] = av[i];
    choosing = !conveyor && ch_n > slots;
    nseeds = 0;
    if (!conveyor && !choosing) for (int i = 0; i < ch_n; i++) seeds[nseeds++] = ch_list[i];
    for (int i = 0; i < nseeds; i++) reanim_get(plant_defs[seeds[i]].re);
    if (choosing) for (int i = 0; i < ch_n; i++) img_load(plant_defs[ch_list[i]].packet);
    if (lv == 4) reanim_get(RE_WALLNUT);
    reanim_get(RE_ZOMBIE); reanim_get(RE_ZOMBIE_CHARRED);
    for (int t = 0; t < ZT_COUNT; t++) if (z_levels[t][lv] == '1') reanim_get(zombie_defs[t].re);
    shovel_ok = has_shovel;
    for (int r = 0; r < G.rows; r++) { M[r].state = row_ok(r) ? 0 : 2; M[r].x = G.x0 - 56; }
    /* tumbas (cs.a del J2ME) */
    if (G.bg == IMG_BG_NIGHT) {
        static const signed char gtab[][6] = { { 0,0,0,1,1,2 }, { 0,0,1,1,2,3 }, { 0,1,2,2,3,3 }, { 1,2,2,2,3,3 } };
        int set = (lv <= 12) ? 0 : (lv == 13 || lv == 15 || lv == 17) ? 1 : (lv == 16 || lv == 18) ? 2 : lv >= 19 ? 3 : 1;
        for (int c = 3; c < 9; c++) for (int k = 0; k < gtab[set][c - 3]; k++) {
            int r = rnd(0, 4), tries = 0; while (grave[r][c] && tries++ < 10) r = rnd(0, 4);
            grave[r][c] = rnd(1, 3);
        }
    }
    if (lane[0] == LN_ROOF) {
        reanim_get(RE_FLOWERPOT);
        for (int r = 0; r < G.rows; r++) for (int c = 0; c < 4; c++) plant_at(r, c, PL_FLOWERPOT);   /* 4 columnas, como el J2ME */
        sfx_stop_all();
    }
    sun = conveyor ? 0 : (lv == 0 ? 150 : 50);
    suns_fallen = 0; sky_timer = 425;
    init_refresh();
    mode = MODE_LAWN; held = -1; bank_sel = 0;
    cur_r = (first_row() + last_row()) / 2; cur_c = 0;
    nwaves = level_waves[lv]; wave = 0; wave_timer = 1800; huge_timer = 0; msg_timer = 0;
    state = ST_INTRO; state_timer = 0; paused = 0; result = BR_PLAYING; reward_alive = 0; reward_type = -1;
    intro_phase = IP_WAIT; intro_t = 0; hud_off = -60; G.camx = 0;
    setup_preview();
    static const int pre[] = { SFX_PLANT, SFX_PLANT2, SFX_POINTS, SFX_SEEDLIFT, SFX_BUZZER, SFX_SHOVEL, SFX_PAUSE, SFX_READYSETPLANT,
        SFX_TAP, SFX_SPLAT, SFX_SPLAT2, SFX_SPLAT3, SFX_CHOMP, SFX_CHOMP2, SFX_CHOMPSOFT, SFX_GULP, SFX_THROW, SFX_THROW2,
        SFX_GROAN, SFX_GROAN2, SFX_GROAN3, SFX_GROAN4, SFX_LAWNMOWER, SFX_AWOOGA, SFX_HUGEWAVE, SFX_SIREN, SFX_FINALWAVE,
        SFX_PLASTICHIT, SFX_SHIELDHIT, SFX_SHIELDHIT2, SFX_CHERRYBOMB, SFX_POTATO_MINE, SFX_FROZEN, SFX_LIMBS_POP,
        SFX_WINMUSIC, SFX_LOSEMUSIC };
    for (unsigned i = 0; i < sizeof(pre) / sizeof(pre[0]); i++) sfx_preload(pre[i]);
    music_stop();
    if (choosing) music_play("data/music/choose_seeds.mp3", 1);
}

/* ---------------- intro ---------------- */
static const float pan_out[] = { 0,1,2,3,4,6,8,10,13,17,22,28,35,43,53,65,80,91,99,105,109,112,114,116,118,120,122,124,126,128,130 };
static const float pan_back[] = { 130,129,128,127,126,124,122,120,117,113,108,102,95,87,77,65,50,38,29,22,16,11,7,4,2,0 };
static float table_at(const float *t, int n, float f)
{
    if (f <= 0) return t[0];
    if (f >= n - 1) return t[n - 1];
    int i = (int)f; float k = f - i;
    return t[i] + (t[i + 1] - t[i]) * k;
}
static int mow_frames(void) { return 7 + 3 * (last_row() - first_row()); }
/* posicion x del cortacesped de la fila r durante su entrada (pasos +5,+4,+4,+4,+3,+3,+2 del J2ME) */
static float mower_entry_x(int r, float f)
{
    static const float cum[] = { 0, 5, 9, 13, 17, 20, 23, 25 };
    float k = f - 3 * (last_row() - r);
    return G.x0 - 56 + table_at(cum, 8, k);
}
static int ch_is_chosen(int t) { for (int i = 0; i < nseeds; i++) if (seeds[i] == t) return 1; return 0; }
static int ch_need(void) { return ch_n < slots ? ch_n : slots; }
#define CH_COLS 4
#define CH_ROWS 5
static int ch_side, ch_sidx;      /* cursor en la columna de las elegidas (para quitarlas) */
static void chooser_input(void)
{
    int rows = (ch_n + CH_COLS - 1) / CH_COLS;
    if (ch_side) {                                 /* columna de la izquierda: arriba/abajo, X quita */
        if (nseeds == 0) { ch_side = 0; return; }
        if (ch_sidx >= nseeds) ch_sidx = nseeds - 1;
        if (btn_repeat(PSP_CTRL_UP) && ch_sidx > 0) { ch_sidx--; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_DOWN) && ch_sidx + 1 < nseeds) { ch_sidx++; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_RIGHT)) { ch_side = 0; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CROSS)) {
            for (int i = ch_sidx; i < nseeds - 1; i++) seeds[i] = seeds[i + 1];
            nseeds--; sfx_play(SFX_TAP);
            if (ch_sidx >= nseeds) ch_sidx = nseeds - 1;
            if (nseeds == 0) ch_side = 0;
        }
    } else {
        if (btn_repeat(PSP_CTRL_RIGHT) && ch_cur + 1 < ch_n && ch_cur % CH_COLS < CH_COLS - 1) { ch_cur++; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_LEFT)) {
            if (ch_cur % CH_COLS > 0) { ch_cur--; sfx_play(SFX_TAP); }
            else if (nseeds > 0) { ch_side = 1; ch_sidx = nseeds - 1; sfx_play(SFX_TAP); }
        }
        if (btn_repeat(PSP_CTRL_DOWN) && ch_cur / CH_COLS + 1 < rows) { ch_cur += CH_COLS; if (ch_cur >= ch_n) ch_cur = ch_n - 1; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_UP) && ch_cur >= CH_COLS) { ch_cur -= CH_COLS; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CROSS)) {
            int t = ch_list[ch_cur], found = -1;
            for (int i = 0; i < nseeds; i++) if (seeds[i] == t) found = i;
            if (found >= 0) { for (int i = found; i < nseeds - 1; i++) seeds[i] = seeds[i + 1]; nseeds--; sfx_play(SFX_TAP); }
            else if (nseeds < slots) { seeds[nseeds++] = t; sfx_play(SFX_SEEDLIFT); }
            else sfx_play(SFX_BUZZER);
        }
    }
    if (ch_cur / CH_COLS < ch_scroll) ch_scroll = ch_cur / CH_COLS;
    if (ch_cur / CH_COLS > ch_scroll + CH_ROWS - 1) ch_scroll = ch_cur / CH_COLS - (CH_ROWS - 1);
    if (btn_pressed(PSP_CTRL_TRIANGLE)) {
        if (nseeds >= ch_need()) { sfx_play(SFX_BUTTONCLICK); intro_phase = IP_CHOOSE_OUT; intro_t = 0; ch_side = 0; }
        else sfx_play(SFX_BUZZER);
    }
}
static void update_intro(void)
{
    intro_t += 100.0f / 60.0f;
    float f = intro_t / JF;
    for (int i = 0; i < npz; i++) reanim_update(&PZ[i].anim, 1.0f / 60.0f);
    switch (intro_phase) {
    case IP_WAIT:
        if (intro_t >= 90) { intro_phase = IP_OUT; intro_t = 0; }
        break;
    case IP_OUT:
        G.camx = table_at(pan_out, 31, f);
        if (f >= 30) { intro_phase = choosing ? IP_CHOOSE : IP_HOLD; intro_t = 0; }
        break;
    case IP_HOLD:
        if (f >= 16) { intro_phase = IP_BACK; intro_t = 0; }
        break;
    case IP_CHOOSE:
#ifdef AUTOTEST
        if (intro_t > 400) { nseeds = 0; for (int i = ch_n - 1; i >= 0 && nseeds < slots; i--) seeds[nseeds++] = ch_list[i]; intro_phase = IP_CHOOSE_OUT; intro_t = 0; }
        break;
#endif
        chooser_input();
        break;
    case IP_CHOOSE_OUT:
        if (f >= 3) {
            intro_phase = IP_BACK; intro_t = 0;
            for (int i = 0; i < nseeds; i++) reanim_get(plant_defs[seeds[i]].re);
            init_refresh(); music_stop();
        }
        break;
    case IP_BACK:
        G.camx = table_at(pan_back, 26, f);
        if (f >= 25) { intro_phase = IP_MOW; intro_t = 0; G.camx = 0; }
        break;
    case IP_MOW: {
        static const float hud_tab[] = { -60,-48,-38,-30,-23,-17,-12,-8,-5,-2,0,1,0 };
        hud_off = table_at(hud_tab, 13, f);
        for (int r = 0; r < G.rows; r++) if (M[r].state == 0) M[r].x = mower_entry_x(r, f);
        if (f >= mow_frames() && f >= 12) { intro_phase = IP_READY; intro_t = 0; hud_off = 0; sfx_play(SFX_READYSETPLANT); }
        break;
    }
    case IP_READY:
        if (intro_t >= 160) { state = ST_PLAY; state_timer = 0; music_play(G.music, 1); }
        break;
    }
}

int board_reward(void) { return reward_type; }

/* ---------------- control ---------------- */
static void board_input(void)
{
    /* pausa del J2ME: lapida con REANUDAR / REINICIAR / MENU / SONIDO / MUSICA (tambien al elegir plantas) */
    if (btn_pressed(PSP_CTRL_START) && !paused && state != ST_LOST) {
        paused = 1; pause_sel = 0; pause_confirm = 0;
        sfx_stop_all(); sfx_play(SFX_PAUSE); music_set_volume(96);
        return;
    }
    if (paused) {
        if (pause_confirm) {
            if (btn_pressed(PSP_CTRL_CROSS)) { paused = 0; music_set_volume(256); result = pause_confirm == 1 ? BR_RESTART : BR_QUIT; }
            if (btn_pressed(PSP_CTRL_CIRCLE)) { pause_confirm = 0; sfx_play(SFX_TAP); }
            return;
        }
        if (btn_pressed(PSP_CTRL_UP)) { pause_sel = (pause_sel + 4) % 5; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_DOWN)) { pause_sel = (pause_sel + 1) % 5; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CROSS)) {
            sfx_play(SFX_BUTTONCLICK);
            if (pause_sel == 0) { paused = 0; music_set_volume(256); }
            else if (pause_sel == 1) pause_confirm = 1;
            else if (pause_sel == 2) pause_confirm = 2;
            else if (pause_sel == 3) { g_opt_sound = !g_opt_sound; ui_apply_options(); }
            else { g_opt_music = !g_opt_music; ui_apply_options(); }
        }
        if (btn_pressed(PSP_CTRL_CIRCLE) || btn_pressed(PSP_CTRL_START)) { paused = 0; music_set_volume(256); }
        return;
    }
    if (state != ST_PLAY && state != ST_WONWAIT) return;
    int up = btn_repeat(PSP_CTRL_UP) || (stick_y() < -0.6f && frame % 9 == 0);
    int dn = btn_repeat(PSP_CTRL_DOWN) || (stick_y() > 0.6f && frame % 9 == 0);
    int lf = btn_repeat(PSP_CTRL_LEFT) || (stick_x() < -0.6f && frame % 9 == 0);
    int rt = btn_repeat(PSP_CTRL_RIGHT) || (stick_x() > 0.6f && frame % 9 == 0);
    int fire = btn_pressed(PSP_CTRL_CROSS);
    int nb = bank_count();

    if ((btn_pressed(PSP_CTRL_LTRIGGER) || btn_pressed(PSP_CTRL_RTRIGGER)) && nb > 0) {
        int d = btn_pressed(PSP_CTRL_LTRIGGER) ? nb - 1 : 1;
        bank_sel = (bank_sel + d) % nb;
        if (mode != MODE_BANK && bank_ready(bank_sel)) { held = bank_sel; mode = MODE_LAWN; sfx_play(SFX_SEEDLIFT); }
        else sfx_play(SFX_TAP);
    }
    if (btn_pressed(PSP_CTRL_TRIANGLE) && shovel_ok) { mode = mode == MODE_SHOVEL ? MODE_LAWN : MODE_SHOVEL; held = -1; sfx_play(SFX_SHOVEL); }
    if (btn_pressed(PSP_CTRL_CIRCLE) && (mode != MODE_LAWN || held >= 0)) { mode = MODE_LAWN; held = -1; sfx_play(SFX_TAP); }

    if (mode == MODE_BANK) {                  /* caja de semillas en columna (PvZBV): arriba/abajo, derecha vuelve */
        if (nb == 0) { mode = MODE_LAWN; return; }
        if (up) { bank_sel = (bank_sel + nb - 1) % nb; sfx_play(SFX_TAP); }
        if (dn) { bank_sel = (bank_sel + 1) % nb; sfx_play(SFX_TAP); }
        if (rt) { mode = MODE_LAWN; sfx_play(SFX_TAP); }
        if (fire) {
            if (bank_ready(bank_sel)) { held = bank_sel; mode = MODE_LAWN; sfx_play(SFX_SEEDLIFT); auto_place(seed_type(held)); }
            else sfx_play(SFX_BUZZER);
        }
        return;
    }
    if (up) { if (cur_r <= first_row()) cur_r = last_row(); else { do cur_r--; while (!row_ok(cur_r)); } }
    if (dn) { if (cur_r >= last_row()) cur_r = first_row(); else { do cur_r++; while (!row_ok(cur_r)); } }
    if (lf && cur_c == 0 && mode == MODE_LAWN && state == ST_PLAY && nb > 0) { mode = MODE_BANK; sfx_play(SFX_TAP); return; }
    if (lf && cur_c > 0) cur_c--;
    if (rt && cur_c < COLS - 1) cur_c++;
    if (!fire || state != ST_PLAY) return;
    if (mode == MODE_SHOVEL) {
        if (P[cur_r][cur_c][1].alive) P[cur_r][cur_c][1].alive = 0; else P[cur_r][cur_c][0].alive = 0;
        sfx_play(SFX_PLANT2); mode = MODE_LAWN; return;
    }
    if (held >= 0) {
        int t = seed_type(held);
        if (!bank_ready(held) || !can_plant(t, cur_r, cur_c)) { sfx_play(SFX_BUZZER); return; }
        if (conveyor) belt_remove(held);
        else { sun -= plant_defs[t].cost; refresh[held] = plant_defs[t].refresh; }
        plant_at(cur_r, cur_c, t);
        held = -1;
        return;
    }
    Plant *p = top_plant(cur_r, cur_c);
    mode = MODE_BANK;
    if (p && !conveyor) for (int i = 0; i < nseeds; i++) if (seeds[i] == p->type) bank_sel = i;
    sfx_play(SFX_TAP);
}

/* ---------------- bucle ---------------- */
int board_update(void)
{
    frame++;
    board_input();
    if (result != BR_PLAYING) return result;
    if (paused) return BR_PLAYING;
    state_timer++;
#if defined(AUTOTEST) && defined(AT_T2)
    if (state == ST_PLAY && state_timer == 170) {
        plant_at(2, 5, PL_TANGLEKELP); spawn_zombie(ZT_NORMAL, 2, cell_x(7));
        plant_at(1, 4, PL_SQUASH); spawn_zombie(ZT_CONE, 1, cell_x(6));
        plant_at(4, 3, PL_HYPNOSHROOM); P[4][3][1].sleeping = 0; spawn_zombie(ZT_NORMAL, 4, cell_x(4) + 20); spawn_zombie(ZT_NORMAL, 4, cell_x(8));
        plant_at(0, 1, PL_FLOWERPOT); plant_at(0, 1, PL_PEASHOOTER); plant_at(5, 1, PL_SEASHROOM);
        plant_at(3, 0, PL_LILYPAD); plant_at(3, 0, PL_CACTUS); plant_at(5, 0, PL_KERNELPULT); plant_at(0, 0, PL_CABBAGEPULT);
        spawn_zombie(ZT_NORMAL, 0, cell_x(6)); spawn_zombie(ZT_NORMAL, 5, cell_x(6)); spawn_zombie(ZT_NORMAL, 3, cell_x(6));
    }
    if (state == ST_PLAY && state_timer > 170) { for (int i = 0; i < MAXZ; i++) if (Z[i].alive && !Z[i].hypno && Z[i].row != 4) Z[i].speed = 0.12f; }
#endif
#if defined(AUTOTEST) && defined(AT_FX)
    /* prueba de efectos: patatapum armada, petacereza, guisantes, coltapulta, humoseta, jalapeno */
    if (state == ST_PLAY && state_timer == 20) {
        static const int pl[] = { PL_POTATOMINE, PL_CHERRYBOMB, PL_PEASHOOTER, PL_CABBAGEPULT, PL_FUMESHROOM, PL_MELONPULT, PL_SNOWPEA, PL_KERNELPULT };
        for (unsigned k = 0; k < sizeof(pl) / sizeof(pl[0]); k++) reanim_get(plant_defs[pl[k]].re);
        plant_at(0, 5, PL_POTATOMINE); P[0][5][1].timer = 1;
        plant_at(1, 0, PL_PEASHOOTER); plant_at(1, 1, PL_SNOWPEA);
        plant_at(2, 0, PL_CABBAGEPULT); plant_at(2, 1, PL_MELONPULT); plant_at(3, 0, PL_KERNELPULT);
        plant_at(4, 2, PL_FUMESHROOM); P[4][2][1].sleeping = 0;
        for (int r = 0; r < 5; r++) for (int k = 0; k < 2; k++) { Zombie *z = spawn_zombie(k ? ZT_CONE : ZT_BUCKET, r, cell_x(7 + k) + 5); if (z) z->speed *= 2; }
    }
    if (state == ST_PLAY && state_timer == 420) plant_at(3, 6, PL_CHERRYBOMB);
#endif
#if defined(AUTOTEST) && defined(AT_HUD)
    if (state == ST_PLAY && state_timer == 60) { sun = 500; held = 1; cur_r = 2; cur_c = 3; }
    if (state == ST_PLAY && state_timer == 200) { held = -1; mode = MODE_BANK; bank_sel = 2; }
    if (state == ST_PLAY && state_timer == 320) { mode = MODE_LAWN; held = 0; cur_r = 1; cur_c = 2; plant_at(1, 2, PL_PEASHOOTER); }
#endif
#if defined(AUTOTEST) && defined(AT_PAUSE)
    if (state == ST_PLAY && state_timer == 30) { paused = 1; pause_sel = 1; }
    if (state == ST_PLAY && state_timer == 31 && frame % 200 == 0) pause_confirm = 1;
#endif
#if defined(AUTOTEST) && defined(AT_Z2)
    /* prueba de zombis: pertiga, saltarin, nadadores, buzo, lector, minero, globo, hipnotizado */
    if (state == ST_PLAY && state_timer == 20) {
        static const int pl[] = { PL_PEASHOOTER, PL_WALLNUT, PL_TALLNUT, PL_LILYPAD, PL_HYPNOSHROOM, PL_SNOWPEA };
        for (unsigned k = 0; k < sizeof(pl) / sizeof(pl[0]); k++) reanim_get(plant_defs[pl[k]].re);
        plant_at(0, 0, PL_PEASHOOTER); plant_at(0, 4, PL_WALLNUT);
        plant_at(1, 0, PL_PEASHOOTER); plant_at(1, 4, PL_WALLNUT);
        plant_at(2, 0, PL_LILYPAD); plant_at(2, 0, PL_PEASHOOTER);
        plant_at(3, 1, PL_LILYPAD); plant_at(3, 1, PL_SNOWPEA);
        plant_at(4, 0, PL_PEASHOOTER); plant_at(4, 5, PL_HYPNOSHROOM); P[4][5][1].sleeping = 0;
        plant_at(5, 0, PL_PEASHOOTER); plant_at(5, 1, PL_PEASHOOTER);
        static const int zt[][2] = { {ZT_POLE,0}, {ZT_POGO,1}, {ZT_DUCKY,2}, {ZT_SNORKEL,3}, {ZT_NEWSPAPER,4}, {ZT_CONE,4}, {ZT_BUCKET,5}, {ZT_DIGGER,5}, {ZT_BALLOON,0}, {ZT_CONE,2} };
        for (unsigned k = 0; k < sizeof(zt) / sizeof(zt[0]); k++) { Zombie *z = spawn_zombie(zt[k][0], zt[k][1], cell_x(8) + 10 + (k > 4 ? 40 : 0)); if (z) z->speed *= 2.5f; }
        sfx_stop_all();
    }
    if (state == ST_PLAY) wave_timer = 9999;
#endif
#if defined(AUTOTEST) && defined(AT_PZOO)
    /* todas las plantas a la vez para revisar sus animaciones */
    if (state == ST_PLAY && state_timer == 20) {
        int k = 0;
        for (int t = 0; t < PL_COUNT; t++) {
            if (t == PL_LILYPAD || t == PL_FLOWERPOT || t == PL_GRAVEBUSTER) continue;
            reanim_get(plant_defs[t].re);
            int r = k / 7, c = 1 + k % 7; k++;
            if (r >= G.rows) break;
            if (lane[r] == LN_WATER && !(plant_defs[t].flags & PF_AQUATIC)) { reanim_get(RE_LILYPAD); plant_at(r, c, PL_LILYPAD); }
            if (lane[r] == LN_ROOF && !P[r][c][0].alive) { reanim_get(RE_FLOWERPOT); plant_at(r, c, PL_FLOWERPOT); }
            plant_at(r, c, t); P[r][c][1].sleeping = 0;
            if (plant_defs[t].kind == PK_INSTANT) { P[r][c][1].anim.loop = 1; P[r][c][1].anim.speed = 0.5f; P[r][c][1].type = PL_FLOWERPOT; }
        }
        sfx_stop_all();
    }
    if (state == ST_PLAY && state_timer == 200) { for (int r = 0; r < G.rows; r++) spawn_zombie(ZT_NORMAL, r, cell_x(8) + 10); }
#endif
#if defined(AUTOTEST) && defined(AT_ZOO)
    if (state == ST_PLAY && state_timer == 170) {
        for (int t = 0; t < ZT_COUNT; t++) if (t != ZT_BOSS && t != ZT_DUCKY && t != ZT_SNORKEL && t != ZT_DOLPHIN) {
            Zombie *z = spawn_zombie(t, t % 5, cell_x(1 + (t / 5) * 2) + 10);
            if (z) { z->speed = 0; z->under = 0; z->ammo = 0; }
        }
    }
    if (state == ST_PLAY) return BR_PLAYING;
#endif
#if defined(AUTOTEST) && !defined(AT_FX) && !defined(AT_PZOO) && !defined(AT_Z2) && !defined(AT_HUD)
    if (state == ST_PLAY && frame % 40 == 0) {          /* juega solo: planta de todo */
        sun = 9000;
        int nb = bank_count();
        for (int tries = 0; tries < 30 && nb > 0; tries++) {
            int i = rnd(0, nb - 1), t = seed_type(i);
            if (!bank_ready(i)) continue;
            int r = rnd(first_row(), last_row()), c = rnd(0, 6);
            if (lane[r] == LN_WATER && !P[r][c][0].alive) { if (!conveyor) plant_at(r, c, PL_LILYPAD); continue; }
            if (lane[r] == LN_ROOF && !P[r][c][0].alive) { if (!conveyor) plant_at(r, c, PL_FLOWERPOT); continue; }
            if (can_plant(t, r, c)) { if (conveyor) belt_remove(i); else refresh[i] = 0; plant_at(r, c, t); break; }
        }
        cur_r = rnd(first_row(), last_row()); cur_c = rnd(0, 8);
    }
    if (frame % 600 == 0) { int nz = 0; for (int i = 0; i < MAXZ; i++) nz += Z[i].alive; printf("lv%d f%d st%d wave%d/%d z%d sun%d\n", lv, frame, state, wave, nwaves, nz, sun); }
    if (state == ST_WONWAIT && reward_alive && state_timer > 60) { reward_alive = 0; result = BR_WON; }
#endif
    if (state == ST_INTRO) { update_intro(); return result; }
    if (state == ST_LOST) {
        if (state_timer > 150 && btn_pressed(PSP_CTRL_CROSS)) return BR_LOST;
        return BR_PLAYING;
    }
    if (state == ST_WONWAIT && (reward_type == -1 || reward_type == -2) && state_timer > 200) return BR_WON;
    if (state == ST_WONWAIT && reward_alive && ((state_timer > 40 && btn_pressed(PSP_CTRL_CROSS)) || state_timer > 500)) {
        reward_alive = 0; sfx_play(SFX_SEEDLIFT); return BR_WON;
    }
    float dt = 1.0f / 60.0f;
    for (int r = 0; r < G.rows; r++) for (int c = 0; c < COLS; c++) for (int l = 0; l < 2; l++) if (P[r][c][l].alive) reanim_update(&P[r][c][l].anim, dt);
    for (int i = 0; i < MAXZ; i++) if (Z[i].alive) reanim_update(&Z[i].anim, dt);
    auto_collect();
    tick_acc += 100.0f / 60.0f;
    while (tick_acc >= 1.0f) {
        tick_acc -= 1.0f;
        for (int i = 0; i < nseeds; i++) if (refresh[i] > 0) refresh[i]--;
        update_belt();
        update_suns();
        for (int r = 0; r < G.rows; r++) for (int c = 0; c < COLS; c++) {
            if (crater[r][c] > 0) crater[r][c]--;
            for (int l = 0; l < 2; l++) if (P[r][c][l].alive) update_plant(&P[r][c][l], r, c);
        }
        update_projectiles();
        update_zombies();
        if (state == ST_PLAY) update_waves();
        update_parts();
        if (reward_alive && reward_type >= 0 && rand() % 6 == 0)      /* destellos del sobre ganado */
            part(rand() & 1 ? PS_STARW : PS_STARY, reward_x + frnd(-28, 28), reward_y + frnd(-20, 20), 0, -0.15f, 0, 40);
        if (msg_timer > 0) msg_timer--;
    }
    return BR_PLAYING;
}

/* ---------------- dibujo ---------------- */
static void draw_anim_at(ReAnim *a, float cx, float bottom, u32 col, int flip)
{
    if (!a->def) return;
    float *bb = a->def->bbox;
    float ox, oy;
    /* pies de cada animacion de zombi (medidos en sus frames de andar): la pertiga los tiene 20 px a la
     * derecha del centro de su caja, por eso parecia que comia desde lejos */
    static const float zfeet[16] = { 0, 70.6f, 26.1f, 23.4f, 52.4f, 53.3f, 35.2f, 24.9f, 25.4f, 34.7f, 30.2f, 29.0f, 30.4f, 34.6f, -1, 32.6f };
    int id = reanim_id(a->def);
    if (id == RE_ZOMBIE) { ox = cx - ZAX; oy = bottom - ZAY; }
    else if (id > 0 && id < 16 && zfeet[id] > 0) { ox = cx - zfeet[id]; oy = bottom - bb[3]; }
    else { ox = cx - (bb[0] + bb[2]) / 2; oy = bottom - bb[3]; }
    if (flip) reanim_draw_flip(a, sxw(2 * cx - ox), syw(oy), G.ws, 1, col);   /* mirando a la derecha */
    else reanim_draw(a, sxw(ox), syw(oy), G.ws, col);
}

/* altura a la que se sienta una planta sobre el nenufar o la maceta */
static float pot_lift(int r, int c)
{
    Plant *b = &P[r][c][0];
    if (!b->alive || !b->anim.def) return 0;
    float *bb = b->anim.def->bbox;
    float h = bb[3] - bb[1];
    return b->type == PL_LILYPAD ? -h * 0.35f : -(h - 13);  /* la planta asienta en la tierra de la maceta (fila 13 de la imagen 101) */
}

static void draw_progress(void)
{
    /* barra de progreso de oleadas con banderas y cabeza de zombi (piezas 543/282 del J2ME) */
    char buf[32];
    float bx = SCREEN_W - 112, by = SCREEN_H - 14, bw = 100;
    float prog = nwaves ? (float)wave / nwaves : 0;
    gfx_rect(bx - 2, by - 2, bw + 4, 10, 0xC0202020);
    gfx_rect(bx, by, bw, 6, 0xFF203040);
    gfx_rect(bx + bw * (1 - prog), by, bw * prog, 6, 0xFF20C040);
    for (int w = 0; w < nwaves; w++) if (is_flag_wave(w)) {
        float fx = bx + bw * (1 - (float)(w + 1) / nwaves);
        gfx_draw(IMG_PFLAG, fx - 2, by - 12 + (w < wave ? -3 : 0), WHITE, 0);
    }
    gfx_draw(IMG_ZHEAD, bx + bw * (1 - prog) - 8, by - 6, WHITE, 0);
    snprintf(buf, sizeof(buf), "%s %d-%d", TXT[74], lv / 10 + 1, lv % 10 + 1);
    text_draw(FONT_SMALL, bx - 4 - text_width(FONT_SMALL, buf), SCREEN_H - 20, buf, 0xFFFFFFFF);
}

static void draw_corners(float x, float y, float w, float h, int img)
{
    gfx_draw(img, x - 3, y - 3, WHITE, 0);
    gfx_draw(img, x + w - 10, y - 3, WHITE, GFX_FLIPX);
    gfx_draw(img, x - 3, y + h - 9, WHITE, GFX_FLIPY);
    gfx_draw(img, x + w - 10, y + h - 9, WHITE, GFX_FLIPX | GFX_FLIPY);
}
/* desliza el cursor hacia el recuadro (x,y,w,h) y lo dibuja con las esquinas del PvZBV */
static void cursor_to(float x, float y, float w, float h, int img)
{
    if (curx < 0) { curx = x; cury = y; curw = w; curh = h; }
    curx += (x - curx) * 0.35f; cury += (y - cury) * 0.35f;
    curw += (w - curw) * 0.35f; curh += (h - curh) * 0.35f;
    draw_corners(curx, cury, curw, curh, img);
}

/* caja de semillas del J2ME/PvZBV: columna a la izquierda con el marco 41x31 y los sobres 38x28 */
static void draw_bank(void)
{
    char buf[32];
    if (state == ST_INTRO && intro_phase < IP_MOW) return;
    float yo = hud_off;
    if (!conveyor) {
        gfx_draw(IMG_SUNBAR, 46, 2 + yo, WHITE, 0);
        snprintf(buf, sizeof(buf), "%d", sun);
        text_draw_centered(FONT_SMALL, 89, 1 + yo, buf, 0xFF000000);
    }
    int nb = bank_count();
    for (int i = 0; i < nb; i++) {
        int t = seed_type(i);
        float x = 2 + (i == held ? 5 : 0), y = (conveyor ? belt_x[i] : SLOT_Y(i)) + yo;
        int pk = IMG_SMALLPACK + (t == PL_BOWLNUT ? PL_WALLNUT : t);
        int ok = bank_ready(i);
        gfx_draw(IMG_SEEDFRAME, x, y, WHITE, 0);
        gfx_draw(pk, x + 2, y + 2, ok ? WHITE : 0xFF707070, 0);
        if (!conveyor && refresh[i] > 0) gfx_rect(x + 2, y + 2, 38, 28 * (float)refresh[i] / plant_defs[t].refresh, 0x90000000);
    }
    if (shovel_ok && !conveyor) gfx_draw(IMG_SHOVEL_BTN, 114, 0 + yo, mode == MODE_SHOVEL ? 0xFF80FFFF : WHITE, 0);
    if (state != ST_INTRO) draw_progress();
}

/* "¡ELIGE TUS PLANTAS!" del J2ME (clase c): columna de la izquierda con las elegidas y panel con la
 * cuadricula de 3 columnas. Se dibuja en coordenadas del J2ME (480x320) con la escala vertical. */
#define IMG_CH_COLTOP 430
#define IMG_CH_COLTILE 481
#define IMG_CH_CORNER 386
#define IMG_CH_TOP 214
#define IMG_CH_SIDE 157
#define IMG_CH_BOTTOM 203
#define IMG_CH_BG 375
#define IMG_CH_UP 377
#define IMG_CH_DOWN 308
#define IMG_OK 99
static void draw_chooser(void)
{
    /* coordenadas de la PSP a escala 1:1 (las piezas del J2ME en mosaico no dejan rayas) */
    float slide = 1;
    if (intro_phase == IP_CHOOSE) slide = intro_t / (2 * JF) > 1 ? 1 : intro_t / (2 * JF);
    else if (intro_phase == IP_CHOOSE_OUT) slide = 1 - intro_t / (3 * JF);
    if (slide <= 0) return;
    float px = 54 - (1 - slide) * 300, lx = -5 - (1 - slide) * 60;
    int H = SCREEN_H;
    /* columna de semillas elegidas (430 arriba y abajo, 481 en tiras) */
    for (int y = 9; y < H - 9; y += 5) gfx_draw(IMG_CH_COLTILE, lx, y, WHITE, 0);
    gfx_draw(IMG_CH_COLTOP, lx, 0, WHITE, 0);
    gfx_draw(IMG_CH_COLTOP, lx, H - 9, WHITE, GFX_FLIPY);
    float sp = slots > 6 ? 216.0f / slots : 36;
    for (int i = 0; i < slots; i++) {
        float y = 6 + i * sp;
        if (i < nseeds) gfx_draw(plant_defs[seeds[i]].packet, lx + 5, y, WHITE, 0);
        else gfx_rect(lx + 6, y + 1, 45, 31, 0x50000000);
        if (ch_side && i == ch_sidx && intro_phase == IP_CHOOSE) draw_corners(lx + 5, y, 47, 33, IMG_CUR_OK);
    }
    u32 okc = nseeds >= ch_need() ? ((frame / 10) & 1 ? WHITE : 0xFFB0FFFF) : 0xFF707070;
    gfx_draw(IMG_OK, lx + 10, H - 24, okc, 0);
    /* panel: esquinas 386, borde 214, laterales 157, abajo 203, fondo 375 */
    const int PW = 16 + CH_COLS * 54, PIW = CH_COLS * 54;
    float top = 12, bot = H - 22;
    gfx_clip((int)px + 16, (int)top + 30, PIW, (int)(bot - top - 30));
    for (float ty = top + 30; ty < bot; ty += 42) for (float tx = px + 16; tx < px + PW; tx += 42) gfx_draw(IMG_CH_BG, tx, ty, WHITE, 0);
    gfx_noclip();
    for (float x = px + 16; x < px + PW; x += 8) gfx_draw(IMG_CH_TOP, x, top, WHITE, 0);
    gfx_draw(IMG_CH_CORNER, px, top, WHITE, 0);
    gfx_draw(IMG_CH_CORNER, px + PW, top, WHITE, GFX_FLIPX);
    for (float y = top + 36; y < bot; y += 8) { gfx_draw(IMG_CH_SIDE, px, y, WHITE, 0); gfx_draw(IMG_CH_SIDE, px + PW, y, WHITE, GFX_FLIPX); }
    gfx_rect(px + 16, bot, PIW, 16, 0xFF0C2244);
    gfx_rect(px + 16, bot + 14, PIW, 2, 0xFF061226);
    gfx_draw(IMG_CH_BOTTOM, px, bot, WHITE, 0);
    gfx_draw(IMG_CH_BOTTOM, px + PW, bot, WHITE, GFX_FLIPX);
    /* titulo en marquesina */
    const char *title = TXT[75];
    int tw = text_width(FONT_MED, title);
    gfx_clip((int)px + 16, (int)top, PIW, 30);
    float tx = tw <= PIW ? px + 16 + (PIW - tw) / 2.0f : px + 16 - fmodf(frame * 0.6f, tw + 40) + 40;
    text_draw(FONT_MED, tx, top + 4, title, WHITE);
    if (tw > PIW) text_draw(FONT_MED, tx + tw + 40, top + 4, title, WHITE);
    gfx_noclip();
    /* cuadricula de 3 columnas, 5 filas visibles */
    float gy = top + 40;
    gfx_clip((int)px, (int)gy - 3, PW + 16, (int)(bot - gy + 2));
    for (int i = 0; i < ch_n; i++) {
        int row = i / CH_COLS - ch_scroll;
        if (row < 0 || row > CH_ROWS - 1) continue;
        float x = px + 19 + (i % CH_COLS) * 54, y = gy + row * 38;
        int t = ch_list[i];
        int bad = (plant_defs[t].flags & PF_AQUATIC) && G.rows < 6;
        gfx_draw(plant_defs[t].packet, x, y, ch_is_chosen(t) || bad ? 0xFF505050 : WHITE, 0);
        if (i == ch_cur && !ch_side && intro_phase == IP_CHOOSE) draw_corners(x, y, 47, 33, IMG_CUR_OK);
    }
    gfx_noclip();
    int rows = (ch_n + CH_COLS - 1) / CH_COLS;
    if (ch_scroll > 0) gfx_draw(IMG_CH_UP, px + PW / 2 - 4, gy - 8, WHITE, 0);
    if (ch_scroll + CH_ROWS < rows) gfx_draw(IMG_CH_DOWN, px + PW / 2 - 4, bot + 6, WHITE, 0);
    /* ayuda (en la calle, a la derecha) */
    if (intro_phase == IP_CHOOSE && ch_n) {
        char b[64]; int t = ch_side && ch_sidx < nseeds ? seeds[ch_sidx] : ch_list[ch_cur];
        text_draw_centered(FONT_SMALL, 392, 8, TXT[plant_txt_name[t]], WHITE);
        snprintf(b, sizeof(b), XS(XS_COST), plant_defs[t].cost);
        text_draw_centered(FONT_SMALL, 392, 26, b, 0xFF80FFFF);
        snprintf(b, sizeof(b), "%d/%d", nseeds, ch_need());
        text_draw_centered(FONT_SMALL, 392, 150, b, 0xFF00FFFF);
        text_draw_centered(FONT_SMALL, 392, 172, XS(ch_side ? XS_X_REMOVE : XS_X_PICK), 0xFFE0E0E0);
        text_draw_centered(FONT_SMALL, 392, 234, XS(XS_TRIANGLE), nseeds >= ch_need() ? 0xFF00FFFF : 0xFF909090);
        text_draw_centered(FONT_SMALL, 392, 252, XS(XS_PLAY), nseeds >= ch_need() ? 0xFF00FFFF : 0xFF909090);
        text_draw_centered(FONT_SMALL, 392, 194, XS(XS_START_PAUSE), 0xFFE0E0E0);
    }
}

static void draw_zombie(Zombie *z, float base)
{
    u32 col = WHITE;
    if (z->chill > 0 || z->freeze > 0) col = 0xFFFFB0B0;
    if (z->hypno) col = 0xFFFFA0FF;
    if (z->butter > 0) col = 0xFF80FFFF;
    if (z->state >= ZS_DYING && z->fade < 100) col = (col & 0x00FFFFFF) | ((u32)(z->fade * 255 / 100) << 24);
    if (z->type == ZT_BOSS) return;   /* se dibuja aparte, encima del seto */
    float yo = z->yoff + (z->balloon ? -G.rh * 0.6f : 0);
    if (z->inwater) yo += G.rh * 0.22f;     /* nadando: la animacion 40-49 ya esconde las piernas */
    if (z->kelped) gfx_clip(0, 0, SCREEN_W, (int)syw(base - G.rh * 0.25f));   /* hundiendose: corta en la superficie */
    else if (z->state < ZS_DYING && !z->under) gfx_draw_ex(IMG_SHADOW, sxw(z->x), syw(base), 28, 11, 0.55f * G.ws, 0.5f * G.ws, 0, 0x50FFFFFF, 0);
    draw_anim_at(&z->anim, z->x, base + 1 + yo, col, z->dir > 0);
    if (z->butter > 0 && z->state < ZS_DYING) {            /* mantequilla en la cabeza */
        float hx, hy; int img;
        if (z_track_pos(z, 19, &hx, &hy, &img)) draw_world_c(IMG_BUTTER, hx, hy - 4, 1, WHITE);
        else if (z->anim.def) draw_world_c(IMG_BUTTER, z->x + 2, base + 1 + yo - (z->anim.def->bbox[3] - z->anim.def->bbox[1]) * 0.85f, 1, WHITE);
    }
    if (z->kelped) gfx_noclip();
}

static void proj_img(Proj *q, int *img, float *sc)
{
    *sc = 1;
    switch (q->kind) {
    case PJ_PEA: *img = IMG_PEA; break;  case PJ_SNOW: *img = IMG_SNOWPEA; break; case PJ_SEA: *img = IMG_SEASPORE; *sc = 0.8f; break;
    case PJ_BOSSFIRE: *img = IMG_FIREBALL; *sc = 0.6f; break; case PJ_BOSSICE: *img = IMG_ICEBALL; *sc = 0.9f; break;
    case PJ_FIRE: *img = IMG_FIREPEA; break; case PJ_PUFF: *img = IMG_PUFF; break;
    case PJ_SPIKE: *img = IMG_SPIKE; break; case PJ_STAR: *img = IMG_STAR; break;
    case PJ_CABBAGE: *img = IMG_CABBAGE; break; case PJ_KERNEL: *img = IMG_KERNEL; break;
    case PJ_BUTTER: *img = IMG_BUTTER; break; case PJ_MELON: *img = IMG_MELON; break;
    case PJ_BALL: *img = IMG_BALL; break; default: *img = IMG_PEA;
    }
}

void board_draw(void)
{
    draw_world(G.bg, 0, 0, WHITE);
    if (lv == 0) draw_world(IMG_SOD_1ROW, G.x0 - 5, cell_y(2) - 2, WHITE);
    else if (lv == 1 || lv == 2) draw_world(IMG_SOD_3ROW, G.x0 - 4, cell_y(1) - 2, WHITE);
    if (lv == 4) gfx_rect(sxw(cell_x(3)) - 1, syw(G.y0), 2, G.rows * G.rh * G.ws, 0xC02020FF);   /* linea roja de bolos */
    for (int r = 0; r < G.rows; r++) {
        float base0 = cell_y(r) + G.rh - 3;
        if ((M[r].state < 2 || M[r].state == 3) && (state != ST_INTRO || intro_phase >= IP_MOW)) {
            /* como el J2ME: 35x35 con la parte de arriba 6 px por encima de la casilla */
            int img = lane[r] == LN_ROOF ? IMG_ROOFCLEANER : IMG_MOWER;
            float my = cy_x(r, M[r].x) + G.rh * 0.5f - (lane[r] == LN_ROOF ? 6 : 24);
            if (M[r].state == 3) {                 /* hundiendose: se recorta por la superficie del agua */
                gfx_clip(0, 0, SCREEN_W, (int)syw(cell_y(r) + G.rh * 0.75f));
                draw_world(img, M[r].x, my + M[r].sink, ((u32)(255 - M[r].sink * 6) << 24) | 0xFFFFFF);
                gfx_noclip();
            } else draw_world(img, M[r].x, my, WHITE);
        }
        for (int c = 0; c < COLS; c++) {
            float base = cy_c(r, c) + G.rh - 3;
            if (grave[r][c]) draw_world_c(IMG_GRAVE1 + grave[r][c] - 1, cell_x(c) + G.cw / 2, base - 14, 1, WHITE);
            if (crater[r][c]) draw_world_c(IMG_CRATER, cell_x(c) + G.cw / 2, base - 8, 0.8f, WHITE);
            for (int l = 0; l < 2; l++) {
                Plant *p = &P[r][c][l];
                if (!p->alive) continue;
                if (lane[r] != LN_WATER && l == 1 && !P[r][c][0].alive)
                    gfx_draw_ex(IMG_SHADOW, sxw(cell_x(c) + G.cw / 2), syw(base), 28, 11, 0.45f * G.ws, 0.45f * G.ws, 0, 0x50FFFFFF, 0);
                float yo = (l == 1 && P[r][c][0].alive) ? pot_lift(r, c) : 0;
                draw_anim_at(&p->anim, cell_x(c) + G.cw / 2 + p->dx, base + yo + p->dy, p->sleeping ? 0xFFC0C0C0 : WHITE, 0);
                if (p->ladder) draw_world_c(IMG_LADDER, cell_x(c) + G.cw / 2, base - 18, 0.8f, WHITE);
            }
        }
        for (int i = 0; i < MAXZ; i++) if (Z[i].alive && Z[i].row == r && Z[i].x < view_right() + 40) draw_zombie(&Z[i], cy_x(r, Z[i].x) + G.rh - 3);
        (void)base0;
        for (int i = 0; i < MAXP; i++) {
            Proj *q = &PJ[i];
            if (!q->alive || q->row != r) continue;
            if (q->kind == PJ_BOWL) {
                static ReAnim nut;
                if (!nut.def) reanim_play(&nut, RE_WALLNUT, 0, 0, 1);
                float *bb = nut.def ? nut.def->bbox : NULL;
                if (bb) {
                    float a = q->x * 0.15f, cx = (bb[0] + bb[2]) / 2, cy = (bb[1] + bb[3]) / 2;
                    float ca = cosf(a) * G.ws, sa = sinf(a) * G.ws;
                    /* nuez rodando: la animacion 28 frame 0 rotada alrededor de su centro */
                    ReFrame *F = &nut.def->frames[4 * nut.def->nframes];
                    float ox = F->x - cx, oy = F->y - cy;
                    gfx_draw_affine(F->img, sxw(q->x) + ox * ca - oy * sa, syw(q->y) + ox * sa + oy * ca, ca, sa, -sa, ca, WHITE);
                }
                continue;
            }
            int img; float sc; proj_img(q, &img, &sc);
            draw_world_c(img, q->x, q->y, sc, WHITE);
        }
    }
    if (state == ST_INTRO && intro_phase < IP_MOW) {      /* zombis de muestra en la calle, de atras hacia delante */
        int ord[12];
        for (int i = 0; i < npz; i++) ord[i] = i;
        for (int i = 1; i < npz; i++) for (int k = i; k > 0 && PZ[ord[k]].yoff < PZ[ord[k - 1]].yoff; k--) { int t = ord[k]; ord[k] = ord[k - 1]; ord[k - 1] = t; }
        for (int i = 0; i < npz; i++) {
            Zombie *z = &PZ[ord[i]];
            if (sxw(z->x) < -40) continue;
            gfx_draw_ex(IMG_SHADOW, sxw(z->x), syw(z->yoff), 28, 11, 0.5f * G.ws, 0.5f * G.ws, 0, 0x60FFFFFF, 0);
            draw_anim_at(&z->anim, z->x, z->yoff + 1, WHITE, 0);
        }
    }
    for (int i = 0; i < MAXZ; i++) if (Z[i].alive && Z[i].type == ZT_BOSS && Z[i].anim.def) {
        /* Dr. Zombi (Zombistein robot): el J2ME solo muestra piernas y brazo; origen a la derecha */
        u32 col = Z[i].state >= ZS_DYING ? ((u32)(Z[i].fade * 255 / 400) << 24) | 0xFFFFFF : WHITE;
        reanim_draw(&Z[i].anim, sxw(view_right() - 330), syw(G.y0 + 30), G.ws, col);
        float hpf = (float)Z[i].hp / zombie_defs[ZT_BOSS].hp;
        gfx_rect(SCREEN_W - 112, SCREEN_H - 26, 104, 8, 0xA0000000);
        gfx_rect(SCREEN_W - 110, SCREEN_H - 24, 100 * hpf, 4, 0xFF2020FF);
    }
    draw_parts();
    /* cursor del PvZBV: 4 esquinas que se deslizan a la casilla; rojas si ahi no se puede plantar */
    if (mode != MODE_BANK && state == ST_PLAY) {
        float cx = sxw(cell_x(cur_c)), cy = syw(cy_c(cur_r, cur_c)), cw = G.cw * G.ws, ch = G.rh * G.ws;
        int bad = held >= 0 && !can_plant(seed_type(held), cur_r, cur_c);
        if (held >= 0 && !bad) {                 /* la planta elegida se ve en la casilla, como en el PvZBV */
            static ReAnim ghost;
            int t = seed_type(held); const PlantDef *d = pdef(t);
            if (ghost.def != reanim_get(d->re)) reanim_play(&ghost, d->re, d->idle_s, d->idle_e, 1);
            ghost.hide_mask[0] = d->hide;
            draw_anim_at(&ghost, cell_x(cur_c) + G.cw / 2, cy_c(cur_r, cur_c) + G.rh - 3 + (P[cur_r][cur_c][0].alive ? pot_lift(cur_r, cur_c) : 0), 0xC0FFFFFF, 0);
        }
        cursor_to(cx, cy, cw, ch, bad ? IMG_CUR_BAD : IMG_CUR_OK);
        if (mode == MODE_SHOVEL) gfx_draw_ex(IMG_SHOVEL, cx + cw / 2, cy + ch / 2, 19, 22, 0.8f, 0.8f, 0, WHITE, 0);
    }
    for (int i = 0; i < MAXS; i++) {
        Sun *s = &S[i];
        if (!s->alive) continue;
        u32 a = s->ttl < 200 && !s->collecting && ((frame / 6) & 1) ? 0x80FFFFFF : WHITE;
        float sc = 0.55f * (s->value < 25 ? 0.7f : 1.0f);
        gfx_draw_ex(IMG_SUN, sxw(s->x), syw(s->y), 25.5f, 26.0f, sc * G.ws, sc * G.ws, frame * 0.02f, a, 0);
    }
    if (reward_alive) {
        int img = reward_type >= 0 ? plant_defs[reward_type].packet : IMG_NOTE;
        float bob = sinf(frame * 0.1f) * 3, gl = 1.25f + 0.1f * sinf(frame * 0.15f);
        gfx_draw_ex(img, sxw(reward_x), syw(reward_y) + bob, img_w(img) / 2.0f, img_h(img) / 2.0f, gl, gl, 0, 0x60FFFFFF, 0);
        gfx_draw_ex(img, sxw(reward_x), syw(reward_y) + bob, img_w(img) / 2.0f, img_h(img) / 2.0f, 1, 1, 0, WHITE, 0);
        if ((frame / 20) & 1) text_draw_centered(FONT_SMALL, SCREEN_W / 2, 200, XS(XS_PRESS_X), WHITE);
    }
    draw_bank();
    if (mode == MODE_BANK && bank_count() > 0 && state == ST_PLAY) {
        float by = (conveyor ? belt_x[bank_sel] : SLOT_Y(bank_sel)) + hud_off;
        cursor_to(2, by, 41, 31, IMG_CUR_OK);
        char buf[64]; int t = seed_type(bank_sel);
        if (t == PL_BOWLNUT) snprintf(buf, sizeof(buf), "%s", XS(XS_NUT));
        else if (conveyor) snprintf(buf, sizeof(buf), "%s", TXT[plant_txt_name[t]]);
        else snprintf(buf, sizeof(buf), "%s  (%d)", TXT[plant_txt_name[t]], plant_defs[t].cost);
        int tw = text_width(FONT_SMALL, buf);
        gfx_rect(48, by + 6, tw + 8, 20, 0xA0000000);
        text_draw(FONT_SMALL, 52, by + 7, buf, WHITE);
    }
    if (state == ST_INTRO && choosing && intro_phase >= IP_CHOOSE && intro_phase <= IP_CHOOSE_OUT) draw_chooser();
    if (state == ST_INTRO && intro_phase == IP_READY) {
        const char *t = intro_t < 55 ? TXT_READY : (intro_t < 110 ? TXT_SET : TXT_PLANT);
        text_draw_centered(FONT_BIG, SCREEN_W / 2, 120, t, 0xFF2020FF);
    }
    if (msg_timer > 0) {
        if (msg_kind == 1) text_draw_centered(FONT_SMALL, SCREEN_W / 2, 125, TXT_HUGE, 0xFF2020FF);
        else text_draw_centered(FONT_BIG, SCREEN_W / 2, 120, TXT_FINAL, 0xFF2020FF);
    }
    if (state == ST_LOST) {
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, 0x80000000);
        text_draw_centered(FONT_BIG, SCREEN_W / 2, 110, TXT_LOSE, 0xFF4040FF);
        if (state_timer > 150) text_draw_centered(FONT_SMALL, SCREEN_W / 2, 150, XS(XS_X_CONTINUE), WHITE);
    }
    if (paused) {
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, 0x80000000);
        ui_tomb_dialog(125, 44, 230, 196);
        if (pause_confirm) {
            text_draw_centered(FONT_BIG, SCREEN_W / 2, 68, TXT[pause_confirm == 1 ? 62 : 64], 0xFFE8E8E8);
            ui_text_wrap(FONT_SMALL, 152, 100, 176, 18, TXT[pause_confirm == 1 ? 63 : 65], 0xFF40FFFF, 1);
            text_draw_centered(FONT_BIG, SCREEN_W / 2, 196, XS(XS_YES_NO), 0xFFE8E8E8);
        } else {
            const char *it[5] = { TXT[59], TXT[49], TXT[21], TXT[g_opt_sound ? 41 : 42], XS(g_opt_music ? XS_MUSIC_ON : XS_MUSIC_OFF) };
            for (int i = 0; i < 5; i++)
                text_draw_centered(FONT_BIG, SCREEN_W / 2, 70 + i * 28, it[i], i == pause_sel ? ((frame / 10) & 1 ? 0xFF40FFFF : 0xFF00D0FF) : 0xFFE8E8E8);
        }
    }
}
