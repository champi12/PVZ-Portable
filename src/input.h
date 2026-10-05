#ifndef INPUT_H
#define INPUT_H
#include <pspctrl.h>

void input_init(void);
void input_update(void);
int  btn_down(unsigned int b);     /* mantenido */
int  btn_pressed(unsigned int b);  /* recien pulsado */
int  btn_repeat(unsigned int b);   /* pulsado + autorepeticion (cruceta) */
/* stick analogico normalizado -1..1 con zona muerta */
float stick_x(void);
float stick_y(void);

#endif
