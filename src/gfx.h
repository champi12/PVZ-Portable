/* gfx.h - Render 2D con el GU de la PSP (sin SDL, para ahorrar RAM en PSP-1000) */
#ifndef GFX_H
#define GFX_H
#include <psptypes.h>

#define SCREEN_W 480
#define SCREEN_H 272

#define GFX_FLIPX 1
#define GFX_FLIPY 2

#define RGBA(r,g,b,a) ((u32)(((a)<<24)|((b)<<16)|((g)<<8)|(r)))
#define WHITE 0xFFFFFFFF

int  gfx_init(const char *pak_path);
void gfx_shutdown(void);
void gfx_begin(u32 clear_color);
void gfx_end(void);

/* Imagenes del paquete (ids = indices de imagen del J2ME, 0..604) */
int  img_load(int id);          /* carga en RAM; 0 = ok */
void img_unload(int id);
void img_unload_all(void);
int  img_w(int id);
int  img_h(int id);
u32  gfx_mem_used(void);        /* bytes de texturas cargadas */

/* Dibujo simple (1:1, sin filtro) */
void gfx_draw(int id, float x, float y, u32 color, int flags);
/* Dibuja una sub-region de la imagen */
void gfx_draw_region(int id, int sx, int sy, int sw, int sh, float x, float y, u32 color);
/* Dibujo con transformacion: (ax,ay) = pivote en px de la imagen */
void gfx_draw_ex(int id, float x, float y, float ax, float ay,
                 float sx, float sy, float rot, u32 color, int flags);
void gfx_rect(float x, float y, float w, float h, u32 color);
/* Dibujo con matriz afin (para animaciones Reanim): pixel (u,v) -> (ox + a*u + c*v, oy + b*u + d*v) */
void gfx_draw_affine(int id, float ox, float oy, float a, float b, float c, float d, u32 color);

/* Escala vertical global: 272/320 dibuja pantallas pensadas para 480x320 (coordenadas del J2ME) */
#define J2ME_VS (272.0f / 320.0f)
void gfx_set_vscale(float s);
float gfx_vscale(void);

void gfx_clip(int x, int y, int w, int h);   /* recorte (pantalla) */
void gfx_noclip(void);

/* Texto con las fuentes bitmap del juego (UTF-8, mayusculas) */
enum { FONT_SMALL = 8, FONT_MED = 6, FONT_BIG = 5, FONT_HUGE = 7, FONT_NUM = 0 };
int  font_load(int font);
int  text_width(int font, const char *utf8);
void text_draw(int font, float x, float y, const char *utf8, u32 color);
void text_draw_centered(int font, float cx, float y, const char *utf8, u32 color);

#endif
