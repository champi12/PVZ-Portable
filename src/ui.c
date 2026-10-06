/* ui.c - Piezas de interfaz del J2ME (ver ui.h). Ids de imagen medidos con el registro de dibujo del J2ME. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gfx.h"
#include "audio.h"
#include "ui.h"

int g_opt_sound = 1, g_opt_music = 1, ui_wrap_center;
static void line_out(int font, float x, float y, const char *s, u32 col) { if (ui_wrap_center) text_draw_centered(font, x, y, s, col); else text_draw(font, x, y, s, col); }
void ui_apply_options(void) { sfx_set_volume(g_opt_sound ? 256 : 0); music_set_master(g_opt_music ? 256 : 0); }

/* lapida de dialogo (pausa del J2ME): 326 calavera, 359/354 esquinas de arriba, 410/408 de abajo,
 * 440 borde de arriba, 111 borde de abajo, 422/420 laterales, 122 fondo */
void ui_tomb_dialog(float x, float y, float w, float h)
{
    float x0 = x - 9, W = w + 19, yb = y + h - 35;
    gfx_clip((int)x0 + 27, (int)y + 17, (int)W - 53, (int)(yb - y - 10));
    for (float ty = y + 17; ty < yb + 10; ty += 39) for (float tx = x0 + 27; tx < x0 + W - 26; tx += 68) gfx_draw(122, tx, ty, WHITE, 0);
    gfx_noclip();
    for (float ty = y + 35; ty < yb; ty += 4) { gfx_draw(422, x0 + 9, ty, WHITE, 0); gfx_draw(420, x0 + W - 34, ty, WHITE, 0); }
    for (float tx = x0 + 67; tx < x0 + W - 68; tx += 6) { gfx_draw(440, tx, y, WHITE, 0); gfx_draw(111, tx, yb + 1, WHITE, 0); }
    gfx_draw(359, x0 + 9, y, WHITE, 0);
    gfx_draw(354, x0 + W - 68, y, WHITE, 0);
    gfx_draw(410, x0, yb, WHITE, 0);
    gfx_draw(408, x0 + W - 68, yb, WHITE, 0);
    gfx_draw(326, x0 + W / 2 - 33, y - 26, WHITE, 0);
}

/* marco del almanaque: 244 fondo, 367 esquinas, 374 bordes de arriba/abajo, 454 laterales */
void ui_frame_bg(void)
{
    for (int ty = 0; ty < SCREEN_H; ty += 42) for (int tx = 0; tx < SCREEN_W; tx += 42) gfx_draw(244, tx, ty, WHITE, 0);
    int yb = SCREEN_H - 42;
    for (int tx = 42; tx < SCREEN_W - 42; tx += 42) { gfx_draw(374, tx, 0, WHITE, 0); gfx_draw(374, tx, yb, WHITE, GFX_FLIPY); }
    for (int ty = 42; ty < yb; ty += 42) { gfx_draw(454, 0, ty, WHITE, 0); gfx_draw(454, SCREEN_W - 42, ty, WHITE, GFX_FLIPX); }
    gfx_draw(367, 0, 0, WHITE, 0);
    gfx_draw(367, SCREEN_W - 42, 0, WHITE, GFX_FLIPX);
    gfx_draw(367, 0, yb, WHITE, GFX_FLIPY);
    gfx_draw(367, SCREEN_W - 42, yb, WHITE, GFX_FLIPX | GFX_FLIPY);
}

void ui_title_bar(float x, float y, float w, int end, int mid, const char *txt, int font)
{
    int ew = img_w(end), mw = img_w(mid);
    gfx_clip((int)x + ew, (int)y, (int)w - 2 * ew, 40);
    int k = 0;
    for (float tx = x + ew; tx < x + w - ew; tx += mw, k++) gfx_draw(mid, tx, y, WHITE, (k & 1) ? GFX_FLIPX : 0);
    gfx_noclip();
    gfx_draw(end, x, y, WHITE, 0);
    gfx_draw(end, x + w - ew, y, WHITE, GFX_FLIPX);
    if (txt) text_draw_centered(font, x + w / 2, y + (img_h(end) - 23) / 2 + 2, txt, 0xFFE8E8E8);
}

void ui_panel(float x, float y, float w, float h, int corner, int edge, int body)
{
    gfx_clip((int)x + 8, (int)y + 8, (int)w - 16, (int)h - 16);
    for (float ty = y + 8; ty < y + h - 8; ty += 42) for (float tx = x + 8; tx < x + w - 8; tx += 42) gfx_draw(body, tx, ty, WHITE, 0);
    gfx_noclip();
    gfx_clip((int)x + 42, (int)y, (int)w - 84, (int)h);
    for (float tx = x + 42; tx < x + w - 42; tx += 42) { gfx_draw(edge, tx, y, WHITE, 0); gfx_draw(edge, tx, y + h - 42, WHITE, GFX_FLIPY); }
    gfx_noclip();
    gfx_clip((int)x, (int)y + 42, (int)w, (int)h - 84);
    for (float ty = y + 42; ty < y + h - 42; ty += 42) {     /* laterales: el borde girado 90 grados */
        gfx_draw_ex(edge, x + 21, ty + 21, 21, 21, 1, 1, -1.5707963f, WHITE, 0);
        gfx_draw_ex(edge, x + w - 21, ty + 21, 21, 21, 1, 1, 1.5707963f, WHITE, 0);
    }
    gfx_noclip();
    gfx_draw(corner, x, y, WHITE, 0);
    gfx_draw(corner, x + w - 42, y, WHITE, GFX_FLIPX);
    gfx_draw(corner, x, y + h - 42, WHITE, GFX_FLIPY);
    gfx_draw(corner, x + w - 42, y + h - 42, WHITE, GFX_FLIPX | GFX_FLIPY);
}

int ui_text_wrap(int font, float x, float y, float w, float lh, const char *s, u32 col, int draw)
{
    char line[460], word[128], test[460];
    int n = 0, lines = 0;
    const char *p = s;
    line[0] = 0;
    while (*p) {
        int k = 0;
        while (*p == ' ') p++;
        if (*p == '|') {                     /* salto de linea (|| = linea en blanco) */
            p++;
            if (draw) line_out(font, x, y + lines * lh, line, col);
            lines++; line[0] = 0; n = 0;
            continue;
        }
        while (*p && *p != ' ' && *p != '|' && k < 127) word[k++] = *p++;
        word[k] = 0;
        if (!k) continue;
        snprintf(test, sizeof(test), "%s%s%s", line, n ? " " : "", word);
        if (text_width(font, test) > w && n) {
            if (draw) line_out(font, x, y + lines * lh, line, col);
            lines++;
            snprintf(line, sizeof(line), "%s", word); n = 1;
        } else { snprintf(line, sizeof(line), "%s", test); n++; }
    }
    if (line[0]) { if (draw) line_out(font, x, y + lines * lh, line, col); lines++; }
    return lines;
}

void ui_cursor(float x, float y, float w, float h, int frame)
{
    u32 c = (frame / 8) & 1 ? 0xFF00FFFF : 0xFF00C0FF;
    gfx_rect(x - 2, y - 2, w + 4, 2, c); gfx_rect(x - 2, y + h, w + 4, 2, c);
    gfx_rect(x - 2, y - 2, 2, h + 4, c); gfx_rect(x + w, y - 2, 2, h + 4, c);
}
