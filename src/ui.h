/* ui.h - Piezas de interfaz del J2ME (lapida de dialogo, marco del almanaque, paneles, barras de titulo)
 * dibujadas a escala 1:1 en coordenadas de la PSP para que los mosaicos no dejen rayas. */
#ifndef UI_H
#define UI_H
#include <psptypes.h>

extern int g_opt_sound, g_opt_music;
extern int ui_wrap_center;     /* 1: ui_text_wrap centra cada linea en x */
void ui_apply_options(void);

/* idioma: las 6 tablas del J2ME (texts.h); por defecto el de la consola */
extern int g_lang;
void ui_set_lang(int lang);
int  ui_system_lang(void);
int  ui_lang_name(int lang);        /* indice en TXT del nombre del idioma */
int  ui_logo(void);                 /* logo del menu en el idioma actual */
/* textos propios del port (no estan en las tablas del J2ME) */
enum { XS_MUSIC_ON, XS_MUSIC_OFF, XS_PICK_LEVEL, XS_COST, XS_YES_NO, XS_X_PICK, XS_TRIANGLE, XS_PLAY,
       XS_START_PAUSE, XS_PRESS_X, XS_X_CONTINUE, XS_X_NEXT, XS_HELP, XS_ABOUT1, XS_ABOUT2, XS_AREA_DAY,
       XS_AREA_NIGHT, XS_AREA_POOL, XS_AREA_FOG, XS_AREA_ROOF, XS_NUT, XS_X_REMOVE, XS_X_PLAY_O_BACK, XS_COUNT };
const char *XS(int id);

/* lapida morada con calavera (pausa y avisos): x,y = esquina sup. izq. de la caja, w >= 140, h >= 80 */
void ui_tomb_dialog(float x, float y, float w, float h);
/* fondo naranja con marco de toda la pantalla (almanaque) */
void ui_frame_bg(void);
/* barra de titulo: extremos 'end' (42x32, el derecho espejado) y relleno 'mid' */
void ui_title_bar(float x, float y, float w, int end, int mid, const char *txt, int font);
/* panel con esquinas/bordes/fondo de 42x42 (los bordes laterales son el borde de arriba girado) */
void ui_panel(float x, float y, float w, float h, int corner, int edge, int body);
/* texto con saltos de linea ('|') y ajuste a 'w'; devuelve el numero de lineas (draw=0 solo mide) */
int  ui_text_wrap(int font, float x, float y, float w, float lh, const char *s, u32 col, int draw);
/* cursor de seleccion (marco amarillo parpadeante) */
void ui_cursor(float x, float y, float w, float h, int frame);
#endif
