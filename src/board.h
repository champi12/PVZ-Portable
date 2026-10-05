#ifndef BOARD_H
#define BOARD_H
enum { BR_PLAYING, BR_WON, BR_LOST, BR_QUIT, BR_RESTART };
enum { AR_DAY, AR_NIGHT, AR_POOL, AR_FOG, AR_ROOF };

int  level_area(int level);
int  level_is_conveyor(int level);       /* cinta: no se eligen semillas */
int  level_slots(int level);
/* avail = plantas disponibles; si hay mas que huecos se eligen en la intro (como el J2ME) */
void board_start(int level, const int *avail, int navail, int slots, int has_shovel);
int  board_update(void);                 /* devuelve BR_* */
void board_draw(void);
int  board_reward(void);                 /* planta ganada (o -1/-2) */
#endif
