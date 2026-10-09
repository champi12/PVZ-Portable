/* reanim.h - Animaciones por piezas (formato "re" del J2ME, equivalente al Reanim del PC) */
#ifndef REANIM_H
#define REANIM_H
#include <psptypes.h>

/* ids de archivo de animacion (indice en el archivo /re del J2ME) */
enum {
    RE_ZOMBIE = 0, RE_ZOMBIE_POLEVAULT = 1, RE_ZOMBIE_DANCER = 2, RE_ZOMBIE_BACKUP = 3,
    RE_ZOMBIE_FOOTBALL = 4, RE_ZOMBIE_JACKBOX = 5, RE_ZOMBIE_NEWSPAPER = 6, RE_ZOMBIE_GARGANTUAR = 7,
    RE_ZOMBIE_IMP = 8, RE_ZOMBIE_LADDER = 9, RE_ZOMBIE_CATAPULT = 10, RE_ZOMBIE_POGO = 11,
    RE_ZOMBIE_DIGGER = 12, RE_ZOMBIE_BALLOON = 13, RE_ZOMBIE_CHARRED = 14, RE_ZOMBIE_SNORKEL = 15,
    RE_BOSS = 16,
    RE_SEASHROOM = 20, RE_TANGLEKELP = 21, RE_SUNFLOWER = 22, RE_CHOMPER = 23, RE_POTATOMINE = 24, RE_SQUASH = 25, RE_SPIKEWEED = 26,
    RE_CABBAGEPULT = 27, RE_WALLNUT = 28, RE_TALLNUT = 29, RE_CHERRYBOMB = 30, RE_KERNELPULT = 31,
    RE_MELONPULT = 32, RE_THREEPEATER = 33, RE_TORCHWOOD = 34, RE_PEASHOOTER = 35, RE_STARFRUIT = 36,
    RE_JALAPENO = 37, RE_CACTUS = 38, RE_PUFFSHROOM = 39, RE_SUNSHROOM = 40, RE_FUMESHROOM = 41,
    RE_GRAVEBUSTER = 42, RE_SCAREDYSHROOM = 43, RE_ICESHROOM = 44, RE_HYPNOSHROOM = 45,
    RE_DOOMSHROOM = 46, RE_LILYPAD = 47, RE_CRAZYDAVE = 48, RE_FLOWERPOT = 49,
    RE_TC = 52,           /* animaciones de la version Tencent: RE_TC + n */
    RE_TC_DANCER = 54, RE_TC_DOLPHIN = 66, RE_TC_REDNUT = 67, RE_TC_PUMPKIN = 69, RE_TC_GARLIC = 70,
    RE_TC_BLOVER = 71, RE_TC_PLANTERN = 73, RE_TC_COBCANNON = 74, RE_TC_GATLING = 75, RE_TC_CATTAIL = 76,
    RE_TC_WINTERMELON = 78,
    RE_COUNT = 108
};

typedef struct {
    float x, y, sx, sy, kx, ky;
    short img;
    signed char vis;
    unsigned char pad;
} ReFrame;

typedef struct {
    int ntracks, nframes, fps;
    float bbox[4];          /* caja del primer frame visible (x0,y0,x1,y1) */
    ReFrame *frames;        /* [track * nframes + frame] */
    void *block;
    int bbox_frame;         /* +1 si la caja se recalculo con reanim_fix_bbox */
} ReDef;

int   reanim_init(const char *pak);
ReDef *reanim_get(int id);           /* carga bajo demanda (y sus imagenes) */
void  reanim_unload_all(void);
int   reanim_id(ReDef *d);
/* recalcula la caja con el frame dado (plantas cuyo primer frame es la semilla) */
void  reanim_fix_bbox(ReDef *d, int frame);          /* numero de archivo de una definicion cargada (-1 si NULL) */
/* rango [start,end] de la pista "de control" (sin imagen) numero n; -1 si no existe */
int   reanim_range(ReDef *d, int track, int *start, int *end);
/* rango de reposo: pista de control visible en el frame 0 */
void  reanim_idle_range(ReDef *d, int *start, int *end);

/* Instancia que se reproduce */
typedef struct {
    ReDef *def;
    float frame;          /* posicion actual (fraccional) */
    int start, end;       /* rango en bucle */
    float speed;          /* multiplicador de fps */
    int loop;
    u32 hide_mask[2];     /* pistas a ocultar (64 max) */
} ReAnim;

void reanim_play(ReAnim *a, int id, int start, int end, int loop);
void reanim_update(ReAnim *a, float dt_seconds);   /* devuelve al terminar si !loop */
int  reanim_done(ReAnim *a);
/* dibuja con el origen de la animacion en (x,y) y escala uniforme */
void reanim_draw(ReAnim *a, float x, float y, float scale, u32 color);
/* igual pero espejado en horizontal si flipx (x = origen ya espejado) */
void reanim_draw_flip(ReAnim *a, float x, float y, float scale, int flipx, u32 color);
/* posicion (x,y) y tamano de imagen de una pista en el frame actual, relativa al origen; 0 si no visible */
int  reanim_track_info(ReAnim *a, int track, float *x, float *y, int *img);
/* 1 = interpolar entre frames (como el PC); 0 = como el J2ME (por defecto) */
extern int reanim_interp;

#endif
