/* defs.h - Tablas de plantas, zombis y niveles (aventura completa del PvZ J2ME: 50 niveles).
 * Oleadas, costes y tipos de zombi por nivel: sacados del codigo J2ME (clase cj).
 * Vida, dano, recargas y velocidades: valores del PvZ de PC. */
#ifndef DEFS_H
#define DEFS_H
#include "reanim.h"

/* ---------------- plantas ---------------- */
enum {
    PL_PEASHOOTER, PL_SUNFLOWER, PL_CHERRYBOMB, PL_WALLNUT, PL_POTATOMINE, PL_SNOWPEA, PL_CHOMPER,
    PL_REPEATER, PL_PUFFSHROOM, PL_SUNSHROOM, PL_FUMESHROOM, PL_GRAVEBUSTER, PL_HYPNOSHROOM,
    PL_SCAREDYSHROOM, PL_ICESHROOM, PL_DOOMSHROOM, PL_LILYPAD, PL_SQUASH, PL_THREEPEATER,
    PL_TANGLEKELP, PL_JALAPENO, PL_SPIKEWEED, PL_TORCHWOOD, PL_TALLNUT, PL_SEASHROOM, PL_CACTUS,
    PL_STARFRUIT, PL_CABBAGEPULT, PL_FLOWERPOT, PL_KERNELPULT, PL_MELONPULT,
    /* plantas de la version Tencent */
    PL_GATLING, PL_WINTERMELON, PL_COBCANNON, PL_CATTAIL, PL_BLOVER, PL_PLANTERN, PL_GARLIC, PL_PUMPKIN,
    PL_GLOOM,             /* seta melancolica: piezas de la DS dibujadas por codigo */
    PL_SPLITPEA,          /* guisantralla: piezas del PC dibujadas por codigo */
    PL_COUNT,
    PL_J2ME_COUNT = PL_GATLING,
    PL_BOWLNUT = 100,     /* nuez de bolos (nivel 1-5) */
    PL_REDNUT = 101       /* nuez explosiva (minijuego de bolos) */
};

/* comportamiento */
enum { PK_SHOOTER, PK_SUN, PK_INSTANT, PK_MINE, PK_WALL, PK_CHOMPER, PK_LOBBER, PK_STAR, PK_FUME,
       PK_PASSIVE, PK_SQUASH, PK_KELP, PK_SPIKE, PK_GRAVEBUSTER, PK_HYPNO, PK_POT, PK_TORCH,
       PK_COB, PK_CATTAIL, PK_BLOVER, PK_LIGHT, PK_GARLIC, PK_PUMPKIN, PK_GLOOM, PK_SPLIT };
/* flags */
#define PF_MUSHROOM 1   /* duerme de dia */
#define PF_AQUATIC  2   /* solo en agua */
#define PF_NOEAT    4   /* los zombis no la comen (pinchohierba) */
#define PF_UPGRADE  8
#define PF_FLAT     16  /* no bloquea al saltador (pinchohierba, nenufar...) */

typedef struct {
    const char *name;
    int packet, cost, refresh, re, hp, kind, flags;
    unsigned hide;           /* pistas ocultas */
    int idle_s, idle_e;      /* rango de reposo (-1 = automatico) */
    int act_s, act_e;        /* rango de accion (disparo, mordisco, explosion...) */
    int rate;                /* cs entre disparos / produccion */
} PlantDef;

static const PlantDef plant_defs[PL_COUNT] = {
 /* nombre             sobre coste recarga anim             vida tipo         flags                     ocultar          reposo   accion  ritmo */
 { "LANZAGUISANTES",   429, 100,  750, RE_PEASHOOTER,     300, PK_SHOOTER, 0,                       (1u<<5)|(1u<<6), 6, 11,  0, 5,   150 },   /* 0-5 disparo (encoge y estira la cabeza) */
 { "GIRASOL",           14,  50,  750, RE_SUNFLOWER,      300, PK_SUN,     0,                       (1u<<5)|(1u<<6)|(1u<<7), -1,-1, -1,-1, 2500 },
 { "PETACEREZA",       144, 150, 5000, RE_CHERRYBOMB,     300, PK_INSTANT, 0,                       0,               0, 6,   0, 6,   0 },
 { "NUEZ",             432,  50, 3000, RE_WALLNUT,       4000, PK_WALL,    0,                       0,               0, 8,  -1,-1,   0 },
 { "PATATAPUM",        542,  25, 3000, RE_POTATOMINE,     300, PK_MINE,    PF_FLAT,                 0,               0, 0,   1, 4,   1500 },
 { "HIELAGUISANTES",   467, 175,  750, RE_PEASHOOTER,     300, PK_SHOOTER, 0,                       (1u<<4)|(1u<<6), 6, 11,  0, 5,   150 },
 { "PLANTA CARROÑÍVORA",304,150,  750, RE_CHOMPER,        300, PK_CHOMPER, 0,                       0,               0, 6,   7, 12,  4200 },
 { "REPETIDORA",       285, 200,  750, RE_PEASHOOTER,     300, PK_SHOOTER, 0,                       (1u<<4)|(1u<<5), 6, 11,  0, 5,   150 },
 { "SETA DESESPORADA", 196,   0,  750, RE_PUFFSHROOM,     300, PK_SHOOTER, PF_MUSHROOM,             0,               0, 4,   5, 7,   150 },
 { "SETA SOLAR",       187,  25,  750, RE_SUNSHROOM,      300, PK_SUN,     PF_MUSHROOM,             0,               0, 2,  -1,-1,  2500 },
 { "HUMOSETA",         483,  75,  750, RE_FUMESHROOM,     300, PK_FUME,    PF_MUSHROOM,             0,               0, 4,   5, 11,  150 },
 { "COMEPIEDRAS",      497,  75,  750, RE_GRAVEBUSTER,    300, PK_GRAVEBUSTER, 0,                   0,               0, 2,   3, 9,   0 },
 { "HIPNOSETA",        424,  75, 3000, RE_HYPNOSHROOM,    300, PK_HYPNO,   PF_MUSHROOM,             0,               0, 3,  -1,-1,   0 },
 { "SETA MIEDICA",      84,  25,  750, RE_SCAREDYSHROOM,  300, PK_SHOOTER, PF_MUSHROOM,             0,               0, 3,   4, 7,   150 },
 { "SETA CONGELADA",   330,  75, 5000, RE_ICESHROOM,      300, PK_INSTANT, PF_MUSHROOM,             0,               0, 4,   5, 8,   0 },
 { "PETASETA",          65, 125, 5000, RE_DOOMSHROOM,     300, PK_INSTANT, PF_MUSHROOM,             0,               0, 4,   5, 12,  0 },
 { "NENÚFAR",            3,  25,  750, RE_LILYPAD,        300, PK_POT,     PF_AQUATIC | PF_FLAT,    0,               0, 2,  -1,-1,   0 },
 { "APISONAFLOR",      389,  50, 3000, RE_SQUASH,         300, PK_SQUASH,  0,                       0,               0, 4,   8, 9,   0 },
 { "TRIPITIDORA",      547, 325,  750, RE_THREEPEATER,    300, PK_SHOOTER, 0,                       0,               0, 3,   4, 6,   150 },
 { "ZAMPALGA",         583,  25, 3000, RE_TANGLEKELP,     300, PK_KELP,    PF_AQUATIC,              0,               0, 4,   5, 16,  0 },
 { "JALAPEÑO",         229, 125, 5000, RE_JALAPENO,       300, PK_INSTANT, 0,                       0,               0, 1,   2, 4,   0 },
 { "PINCHOHIERBA",     195, 100,  750, RE_SPIKEWEED,      300, PK_SPIKE,   PF_NOEAT | PF_FLAT,      0,               0, 5,   6, 7,   100 },
 { "PLANTORCHA",       310, 175,  750, RE_TORCHWOOD,      300, PK_TORCH,   0,                       0,               0, 10, -1,-1,   0 },
 { "NUEZ CÁSCARA-RABIAS",241,125, 3000, RE_TALLNUT,       8000, PK_WALL,    0,                       0,              10, 18, -1,-1,   0 },
 { "MARSETA",          466,   0, 3000, RE_SEASHROOM,      300, PK_SHOOTER, PF_MUSHROOM | PF_AQUATIC,0,               8, 11, 12, 15,  150 },   /* 1-4 dormida (sin boca) */
 { "CACTUS",           562, 125,  750, RE_CACTUS,         300, PK_SHOOTER, 0,                       0,               0, 3,   4, 8,   150 },
 { "FRUSTRELLA",       491, 125,  750, RE_STARFRUIT,      300, PK_STAR,    0,                       0,               0, 3,   4, 7,   150 },
 { "COLTAPULTA",       427, 100,  750, RE_CABBAGEPULT,    300, PK_LOBBER,  0,                       0,               0, 3,   4, 8,   300 },
 { "MACETA",           362,  25,  750, RE_FLOWERPOT,      300, PK_POT,     PF_FLAT,                 0,               0, 2,  -1,-1,   0 },
 { "LANZAMAÍZ",        114, 100,  750, RE_KERNELPULT,     300, PK_LOBBER,  0,                       0,               0, 2,   3, 9,   300 },
 { "MELONPULTA",       219, 300,  750, RE_MELONPULT,      300, PK_LOBBER,  0,                       0,               0, 3,   4, 8,   300 },
 /* Tencent (sobres 1360+, animaciones RE_TC_*; costes del Tencent, no hace falta la planta base) */
 { "GUISANTRALLADORA",1360, 350, 5000, RE_TC_GATLING,     300, PK_SHOOTER, 0,                       0,               6, 14, 15, 27,  150 },
 { "MELONPULTA INVERNAL",1361,350,5000,RE_TC_WINTERMELON, 300, PK_LOBBER,  0,                       0,               0, 3,   4, 8,   300 },
 { "MAZORCAÑÓN",      1362, 500, 5000, RE_TC_COBCANNON,   300, PK_COB,     0,                       0,               5, 9,  17, 29,  3500 },
 { "ESPADAÑA",        1363, 225, 5000, RE_TC_CATTAIL,     300, PK_CATTAIL, PF_AQUATIC,              0,               5, 11, 12, 20,  150 },
 { "TRÉBOL",          1364, 100,  750, RE_TC_BLOVER,      300, PK_BLOVER,  0,                       0,               0, 16, 17, 31,  0 },
 { "PLANTERNA",       1365,  25, 3000, RE_TC_PLANTERN,    300, PK_LIGHT,   0,                       0,               5, 15, -1,-1,   0 },
 { "AJO",             1366,  50,  750, RE_TC_GARLIC,      400, PK_GARLIC,  0,                       0,               4, 13, -1,-1,   0 },
 { "CALABAZA",        1367, 125, 3000, RE_TC_PUMPKIN,    4000, PK_PUMPKIN, 0,                       0,               0, 8,  -1,-1,   0 },
 { "SETA MELANCÓLICA",1368, 150, 5000, -1,                300, PK_GLOOM,   PF_MUSHROOM,             0,              -1,-1, -1,-1,  200 },
 { "GUISANTRALLA",    1400, 125,  750, -1,                300, PK_SPLIT,   0,                       0,              -1,-1, -1,-1,  150 },
};
/* sobre pequeno 38x28 de la barra (PvZBV 620+t; Tencent 1380+) */
static inline int plant_smallpack(int t) { return t == PL_SPLITPEA ? 1440 : t >= PL_J2ME_COUNT ? 1380 + t - PL_J2ME_COUNT : 620 + t; }

/* textos del almanaque (indices en TXT_ES; las plantas del Tencent usan plant_name()/plant_desc() de ui.c) */
static const unsigned char plant_txt_name[PL_J2ME_COUNT] __attribute__((unused)) = {
    154,153,156,155,157,159,160,158,172,173,174,175,178,176,177,179,180,162,161,182,163,164,165,166,181,171,170,167,184,168,169 };
static const unsigned char plant_txt_desc[PL_J2ME_COUNT] __attribute__((unused)) = {
    185,186,188,187,189,191,192,190,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215 };

/* ---------------- zombis (orden del J2ME, clase cj) ---------------- */
enum {
    ZT_NORMAL, ZT_FLAG, ZT_CONE, ZT_DOOR, ZT_FOOTBALL, ZT_BUCKET, ZT_POLE, ZT_DANCER, ZT_JACK,
    ZT_NEWSPAPER, ZT_BACKUP, ZT_GARGANTUAR, ZT_LADDER, ZT_CATAPULT, ZT_POGO, ZT_DIGGER, ZT_BALLOON,
    ZT_DUCKY, ZT_SNORKEL, ZT_DOLPHIN, ZT_BOSS, ZT_IMP, ZT_COUNT
};
static const unsigned char zombie_txt_name[ZT_COUNT] __attribute__((unused)) = {
    130,131,132,135,136,134,133,138,140,141,139,142,143,144,145,146,147,148,149,150,152,151 };
static const unsigned char zombie_txt_desc[ZT_COUNT] __attribute__((unused)) = {
    217,218,219,223,224,220,221,225,230,222,226,236,234,235,233,232,231,227,228,229,238,237 };
/* tablas literales del J2ME (cj.a) */
__attribute__((unused)) static const unsigned char z_cost[ZT_COUNT + 1]     = {1,1,2,4,7,4,2,5,3,2,1,10,4,5,4,4,2,1,3,3,10,10,10};
static const unsigned short z_weight[ZT_COUNT + 1]  = {4000,0,4000,3500,2000,3000,2000,5000,1000,1000,0,1500,1000,1500,1000,1000,2000,4000,2000,2000,0,1500,1500};
static const unsigned char z_minwave[ZT_COUNT + 1]  = {1,1,1,5,5,1,5,1,10,1,1,15,10,10,10,10,10,1,10,10,0,15,15};
/* zombis permitidos por nivel (50 caracteres, '1' = si) */
static const char *z_levels[ZT_COUNT] __attribute__((unused)) = {
    /* tipos de zombi de cada nivel segun la lista del PvZ de PC (orden_zombis_pvz1.txt); 5-10 como el J2ME */
    "11111111111111111111111111111111111111111111111111",   /* normal */
    "01111111111111111111111111111111110111111111111110",   /* flag */
    "00111111111111111111111111101111011111111111111111",   /* cone */
    "00000000000011111011000000000000000000000000000001",   /* door */
    "00000000000000011011000000000000000000000000000000",   /* football */
    "00000001110000000000111110111100101010100101101111",   /* bucket */
    "00000111110000000000000010000000001000000000000001",   /* pole */
    "00000000000000000110000000000000000000000000000000",   /* dancer */
    "00000000000000000000000000000011010010110000000000",   /* jack */
    "00000000001111101010000010000000000000000000000000",   /* newspaper */
    "00000000000000000110000000000000000000000000000000",   /* backup */
    "00000000000000000000000000000000000000000000011111",   /* gargantuar */
    "00000000000000000000000000000000000000001111001011",   /* ladder */
    "00000000000000000000000000000000000000000011000110",   /* catapult */
    "00000000000000000000000000000000000110110000100001",   /* pogo */
    "00000000000000000000000000000000110010110000000000",   /* digger */
    "00000000000000000000000000000001100101110000000000",   /* balloon */
    "00000000000000000000111100000010100111110000000000",   /* ducky */
    "00000000000000000000011100001000100000110000000000",   /* snorkel */
    "00000000000000000000000000011100000000000000000000",   /* dolphin */
    "00000000000000000000000000000000000000000000000001",   /* boss */
    "00000000000000000000000000000000000000000000011110",   /* imp */
};
/* olas por nivel (cj) */
static const unsigned char level_waves[50] = {
    4,6,8,10,8,10,20,10,20,20, 10,20,10,20,10,10,20,10,20,20,
    10,20,20,30,20,20,30,20,30,30, 10,20,10,20,20,10,20,10,20,20,
    10,20,20,30,20,20,30,20,30,30 };

/* comportamiento de zombi */
#define ZF_SWIM     1     /* aparece en filas de agua con flotador */
#define ZF_JUMP     2     /* salta la primera planta (saltador) */
#define ZF_FLY      4     /* globo */
#define ZF_DIG      8     /* minero */
#define ZF_POGO     16
#define ZF_LADDER   32
#define ZF_CATAPULT 64
#define ZF_GARG     128
#define ZF_JACK     256
#define ZF_DANCER   512
#define ZF_PAPER    1024
#define ZF_SHIELD   2048  /* puerta */
#define ZF_SMALL    4096

typedef struct {
    int re;                 /* animacion */
    int hp, helm, shield;   /* vida del cuerpo / casco / escudo (PC) */
    float speed;            /* px del fondo por cs */
    int flags;
    int walk_s, walk_e, eat_s, eat_e, die_s, die_e;
    unsigned show;          /* pistas extra visibles (solo animacion 0) */
} ZombieDef;

/* animacion 0: pistas 11-13 bandera, 16-17 flotador, 21-23 cono, 24-26 cubo, 27-29 puerta.
 * Rangos medidos en los archivos re del J2ME: 0 normal (0-7 reposo, 8-19 andar, 20-29 comer, 30-39 morir,
 * 40-49 nadar, 50-55 hundirse), 1 pertiga (13-23 salto, 24-34 andar sin pertiga), 5 lector (26-33 pierde el
 * periodico), 5 caja sorpresa, 6 lector, 11 saltarin (39-41 con el palo), 12 minero (32-36 bajo tierra, 37-41 sale), 13 globo (4-7 vuela,
 * 8-14 cae), 15 buzo (15-19 se sumerge, 20-21 bajo el agua, 22-24 sale) */
static const ZombieDef zombie_defs[ZT_COUNT] = {
 /* anim                 vida casco escudo vel     flags                  andar   comer   morir   */
 { RE_ZOMBIE,            270, 0,    0,    0.064f, 0,                     8,19,  20,29,  30,39, 0 },              /* normal */
 { RE_ZOMBIE,            270, 0,    0,    0.084f, 0,                     8,19,  20,29,  30,39, (1u<<11)|(1u<<12) }, /* abanderado */
 { RE_ZOMBIE,            270, 370,  0,    0.064f, 0,                     8,19,  20,29,  30,39, 0 },              /* caracono */
 { RE_ZOMBIE,            270, 0,   1100,  0.064f, ZF_SHIELD,             8,19,  20,29,  30,39, 0 },              /* portero */
 { RE_ZOMBIE_FOOTBALL,   270, 1400, 0,    0.150f, 0,                     6,12,  13,20,  21,24, 0 },              /* deportista */
 { RE_ZOMBIE,            270, 1100, 0,    0.064f, 0,                     8,19,  20,29,  30,39, 0 },              /* caracubo */
 { RE_ZOMBIE_POLEVAULT,  500, 0,    0,    0.150f, ZF_JUMP,               4,12,  42,48,  35,41, 0 },              /* saltador */
 { RE_TC_DANCER,         500, 0,    0,    0.070f, ZF_DANCER,            14,22,  39,55,  56,73, 0 },              /* bailon (Tencent: 0-13 moonwalk, 23-33 brazos arriba: invoca) */
 { RE_ZOMBIE_JACKBOX,    500, 0,    0,    0.120f, ZF_JACK,               9,17,  18,25,  34,44, 0 },              /* cajita (26-33 sale el payaso) */
 { RE_ZOMBIE_NEWSPAPER,  270, 0,    150,  0.064f, ZF_PAPER,             11,21,  22,27,  28,36, 0 },              /* lector (0-3 sin periodico, 37-47 / 48-53 enfadado) */
 { RE_ZOMBIE_BACKUP,     270, 0,    0,    0.070f, 0,                    11,27,   5,10,  -1,-1, 0 },              /* extra */
 { RE_ZOMBIE_GARGANTUAR,3000, 0,    0,    0.050f, ZF_GARG,               6,17,  18,25,  33,48, 0 },              /* zombistein */
 { RE_ZOMBIE_LADDER,     500, 0,    0,    0.120f, ZF_LADDER,             7,17,  45,50,  24,32, 0 },              /* escalador */
 { RE_ZOMBIE_CATAPULT,   850, 0,    0,    0.080f, ZF_CATAPULT,           0,6,    7,14,  18,21, 0 },              /* zombipulta */
 { RE_ZOMBIE_POGO,       500, 0,    0,    0.120f, ZF_POGO,               8,18,  19,28,  29,38, 0 },              /* saltarin */
 { RE_ZOMBIE_DIGGER,     270, 100,  0,    0.150f, ZF_DIG,                5,13,  14,21,  22,31, 0 },              /* picado */
 { RE_ZOMBIE_BALLOON,    270, 0,    0,    0.064f, ZF_FLY,               15,20,  21,31,  32,38, 0 },              /* globo */
 { RE_ZOMBIE,            270, 0,    0,    0.064f, ZF_SWIM,               8,19,  20,29,  30,39, (1u<<16)|(1u<<17) }, /* playero */
 { RE_ZOMBIE_SNORKEL,    270, 0,    0,    0.064f, ZF_SWIM,               6,14,  25,30,  31,39, 0 },              /* buzo */
 { RE_TC_DOLPHIN,        500, 0,    0,    0.120f, ZF_SWIM | ZF_JUMP,    42,51,  52,62,  63,71, 0 },              /* delfin (Tencent: 4-13 en tierra, 36-41 salto, 52-62 sin delfin) */
 { RE_BOSS,            40000, 0,    0,    0.0f,   0,                     0,8,    0,8,    0,8,   0 },              /* Dr. Zombi */
 { RE_ZOMBIE_IMP,        270, 0,    0,    0.100f, ZF_SMALL,              9,16,   0,8,   17,23, 0 },              /* zombidito */
};

/* ---------------- niveles ---------------- */
/* recompensa al terminar cada nivel: planta, -2 pala, -1 nada (orden del PC adaptado al J2ME) */
static const signed char level_reward[50] = {
    PL_SUNFLOWER, PL_CHERRYBOMB, PL_WALLNUT, -2, PL_POTATOMINE, PL_SNOWPEA, PL_CHOMPER, PL_REPEATER, -1, PL_PUFFSHROOM,
    PL_SUNSHROOM, PL_FUMESHROOM, PL_GRAVEBUSTER, PL_HYPNOSHROOM, -1, PL_SCAREDYSHROOM, PL_ICESHROOM, PL_DOOMSHROOM, -1, PL_LILYPAD,
    PL_SQUASH, PL_THREEPEATER, PL_TANGLEKELP, PL_JALAPENO, -1, PL_SPIKEWEED, PL_TORCHWOOD, PL_TALLNUT, -1, PL_SEASHROOM,
    PL_PLANTERN, PL_CACTUS, PL_BLOVER, PL_SPLITPEA, -1, PL_STARFRUIT, PL_PUMPKIN, PL_CATTAIL, PL_GLOOM, PL_CABBAGEPULT,
    PL_FLOWERPOT, PL_KERNELPULT, PL_GATLING, PL_GARLIC, -1, PL_COBCANNON, -1, PL_MELONPULT, PL_WINTERMELON, -1 };
/* orden del PC (almanaque y "elige tus plantas") */
static const signed char pc_order[PL_COUNT] __attribute__((unused)) = {
    PL_PEASHOOTER, PL_SUNFLOWER, PL_CHERRYBOMB, PL_WALLNUT, PL_POTATOMINE, PL_SNOWPEA, PL_CHOMPER, PL_REPEATER,
    PL_PUFFSHROOM, PL_SUNSHROOM, PL_FUMESHROOM, PL_GRAVEBUSTER, PL_HYPNOSHROOM, PL_SCAREDYSHROOM, PL_ICESHROOM, PL_DOOMSHROOM,
    PL_LILYPAD, PL_SQUASH, PL_THREEPEATER, PL_TANGLEKELP, PL_JALAPENO, PL_SPIKEWEED, PL_TORCHWOOD, PL_TALLNUT,
    PL_SEASHROOM, PL_PLANTERN, PL_CACTUS, PL_BLOVER, PL_SPLITPEA, PL_STARFRUIT, PL_PUMPKIN, PL_CABBAGEPULT, PL_FLOWERPOT,
    PL_KERNELPULT, PL_GARLIC, PL_MELONPULT, PL_GATLING, PL_GLOOM, PL_CATTAIL, PL_WINTERMELON, PL_COBCANNON };

#endif
