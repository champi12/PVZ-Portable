/* game.c - Flujo del juego: pantalla de carga, menu, seleccion de nivel, eleccion de semillas,
 * dialogos de Dave, tablero, recompensas y guardado. (Las pantallas de menu NO usan zoom.) */
#include <pspkernel.h>
#include <pspctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gfx.h"
#include "audio.h"
#include "input.h"
#include "game.h"
#include "reanim.h"
#include "defs.h"
#include "board.h"
#include "texts.h"

#define IMG_TITLE_BG 290
#define IMG_LOGO_ES 137
#define IMG_EA 475
#define IMG_POPCAP 439
#define IMG_L_URL 606
#define IMG_L_GRASS_A 607
#define IMG_L_GRASS_B 608
#define IMG_L_GRASS_C 609
#define IMG_L_MOWER 610
#define IMG_L_CLIP 611
#define IMG_CARD 392

enum { SC_BOOT, SC_TITLE, SC_LEVELS, SC_SEEDS, SC_DAVE, SC_BOARD, SC_REWARD };
static int scene, frame, quit, timer, menu_sel;
static int level, max_level, sel_level;
static int chosen[10], nchosen, avail[PL_COUNT], navail, seed_cur;
static int dave_line, dave_first, dave_last;
static int reward;
static ReAnim dave;

/* ---------------- guardado ---------------- */
#define SAVE_PATH "data/save.dat"
static void save_game(void)
{
    SceUID f = sceIoOpen(SAVE_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (f < 0) return;
    int d[4] = { 0x5A565032, level, max_level, 0 };
    sceIoWrite(f, d, sizeof(d)); sceIoClose(f);
}
static void load_game(void)
{
    level = 0; max_level = 0;
    SceUID f = sceIoOpen(SAVE_PATH, PSP_O_RDONLY, 0);
    if (f < 0) return;
    int d[4];
    if (sceIoRead(f, d, sizeof(d)) == sizeof(d) && d[0] == 0x5A565032) { level = d[1]; max_level = d[2]; }
    if (level < 0 || level > 49) level = 0;
    if (max_level < level) max_level = level;
    if (max_level > 49) max_level = 49;
    sceIoClose(f);
}

/* plantas disponibles al empezar el nivel lv: recompensas de los niveles anteriores */
static void compute_avail(int lv)
{
    int have[PL_COUNT] = { 0 };
    have[PL_PEASHOOTER] = 1;
    for (int i = 0; i < lv; i++) if (level_reward[i] >= 0) have[level_reward[i]] = 1;
    if (level_area(lv) == AR_POOL || level_area(lv) == AR_FOG) have[PL_LILYPAD] = 1;
    if (level_area(lv) == AR_ROOF) have[PL_FLOWERPOT] = 1;
    navail = 0;
    for (int t = 0; t < PL_COUNT; t++) if (have[t]) avail[navail++] = t;
}

static void start_board(void)
{
    scene = SC_BOARD;
    board_start(level, chosen, nchosen, level > 3);
}

static void go_dave_or_board(void)
{
    /* dialogos de Dave del J2ME (textos 98-129) */
    dave_first = -1;
    if (level == 4) { dave_first = 98; dave_last = 103; }
    else if (level == 10) { dave_first = 104; dave_last = 109; }
    else if (level == 20) { dave_first = 110; dave_last = 114; }
    else if (level == 40) { dave_first = 115; dave_last = 118; }
    else if (level == 49) { dave_first = 119; dave_last = 129; }
    if (dave_first >= 0) {
        scene = SC_DAVE; dave_line = dave_first;
        reanim_unload_all(); img_unload_all();
        reanim_play(&dave, RE_CRAZYDAVE, 1, 7, 1);
        sfx_preload(SFX_CRAZYDAVESHORT1); sfx_play(SFX_CRAZYDAVELONG1);
        music_play("data/music/title_crazydave.mp3", 1);
    } else start_board();
}

static void prepare_level(void)
{
    compute_avail(level);
    int slots = level_slots(level);
    nchosen = 0;
    if (level_is_conveyor(level)) { go_dave_or_board(); return; }
    if (navail <= slots) {
        for (int i = 0; i < navail; i++) chosen[nchosen++] = avail[i];
        go_dave_or_board();
    } else {
        scene = SC_SEEDS; seed_cur = 0;
        reanim_unload_all(); img_unload_all();
        music_play("data/music/choose_seeds.mp3", 1);
    }
}

static void enter_title(void)
{
    scene = SC_TITLE; menu_sel = 0;
    reanim_unload_all(); img_unload_all();
    music_play("data/music/title_crazydave.mp3", 1);
}

void game_init(void)
{
    srand(sceKernelGetSystemTimeLow());
    load_game();
    scene = SC_BOOT; timer = 0;
}
int game_quit_requested(void) { return quit; }
void game_shutdown(void) { music_stop(); }

/* ---------------- actualizacion ---------------- */
void game_update(void)
{
    frame++;
#ifdef AUTOTEST
#ifndef AT_LEVEL
#define AT_LEVEL 0
#endif
    if (frame == 2) { level = AT_LEVEL; }
    if (scene == SC_BOOT && frame > 30) { scene = SC_TITLE; }
    if (scene == SC_TITLE && frame > 40) { prepare_level(); }
    if (scene == SC_SEEDS) { nchosen = 0; for (int i = 0; i < navail && nchosen < level_slots(level); i++) chosen[nchosen++] = avail[navail - 1 - i]; go_dave_or_board(); }
    if (scene == SC_DAVE && frame % 20 == 0) { start_board(); }
#endif
    switch (scene) {
    case SC_BOOT:
        timer++;
        if (timer == 1) { img_load(IMG_EA); img_load(IMG_POPCAP); }
        if (timer == 190) {                  /* carga real de lo comun mientras avanza la barra */
            font_load(FONT_SMALL); font_load(FONT_BIG); font_load(FONT_NUM);
            img_load(IMG_TITLE_BG); img_load(IMG_LOGO_ES);
            sfx_preload(SFX_BUTTONCLICK); sfx_preload(SFX_TAP); sfx_preload(SFX_SEEDLIFT);
        }
        if (timer > 330 || (timer > 200 && btn_pressed(PSP_CTRL_CROSS))) enter_title();
        break;
    case SC_TITLE: {
        if (btn_pressed(PSP_CTRL_UP)) { menu_sel = (menu_sel + 2) % 3; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_DOWN)) { menu_sel = (menu_sel + 1) % 3; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CROSS) || btn_pressed(PSP_CTRL_START)) {
            sfx_play(SFX_BUTTONCLICK);
            if (menu_sel == 0) prepare_level();
            else if (menu_sel == 1) { scene = SC_LEVELS; sel_level = level; }
            else quit = 1;
        }
        break;
    }
    case SC_LEVELS:
        /* seleccion libre de los 50 niveles (para probar) */
        if (btn_repeat(PSP_CTRL_RIGHT)) sel_level = (sel_level + 1) % 50;
        if (btn_repeat(PSP_CTRL_LEFT)) sel_level = (sel_level + 49) % 50;
        if (btn_repeat(PSP_CTRL_DOWN)) sel_level = (sel_level + 10) % 50;
        if (btn_repeat(PSP_CTRL_UP)) sel_level = (sel_level + 40) % 50;
        if (btn_pressed(PSP_CTRL_CIRCLE)) enter_title();
        if (btn_pressed(PSP_CTRL_CROSS)) { level = sel_level; prepare_level(); }
        break;
    case SC_SEEDS: {
        int slots = level_slots(level);
        int cols = 8;
        if (btn_repeat(PSP_CTRL_RIGHT) && seed_cur < navail - 1) seed_cur++;
        if (btn_repeat(PSP_CTRL_LEFT) && seed_cur > 0) seed_cur--;
        if (btn_repeat(PSP_CTRL_DOWN) && seed_cur + cols < navail) seed_cur += cols;
        if (btn_repeat(PSP_CTRL_UP) && seed_cur - cols >= 0) seed_cur -= cols;
        if (btn_pressed(PSP_CTRL_CROSS)) {
            int t = avail[seed_cur], found = -1;
            for (int i = 0; i < nchosen; i++) if (chosen[i] == t) found = i;
            if (found >= 0) { for (int i = found; i < nchosen - 1; i++) chosen[i] = chosen[i + 1]; nchosen--; sfx_play(SFX_TAP); }
            else if (nchosen < slots) { chosen[nchosen++] = t; sfx_play(SFX_SEEDLIFT); }
            else sfx_play(SFX_BUZZER);
        }
        if (btn_pressed(PSP_CTRL_CIRCLE)) { if (nchosen > 0) { nchosen--; sfx_play(SFX_TAP); } else { enter_title(); break; } }
        if (btn_pressed(PSP_CTRL_START) && nchosen > 0) { sfx_play(SFX_BUTTONCLICK); go_dave_or_board(); }
        if (btn_pressed(PSP_CTRL_SELECT)) enter_title();
        break;
    }
    case SC_DAVE:
        reanim_update(&dave, 1.0f / 60.0f);
        if (btn_pressed(PSP_CTRL_CROSS)) {
            if (++dave_line > dave_last) { music_stop(); start_board(); }
            else sfx_play(rand() & 1 ? SFX_CRAZYDAVESHORT1 : SFX_CRAZYDAVELONG2);
        }
        if (btn_pressed(PSP_CTRL_START)) { music_stop(); start_board(); }
        break;
    case SC_BOARD: {
        int r = board_update();
        if (r == BR_QUIT) enter_title();
        else if (r == BR_RESTART) start_board();
        else if (r == BR_LOST) prepare_level();
        else if (r == BR_WON) {
            reward = board_reward();
            if (level < 49) level++;
            if (level > max_level) max_level = level;
            save_game();
            if (reward >= 0) { scene = SC_REWARD; img_unload_all(); }
            else prepare_level();
        }
        break;
    }
    case SC_REWARD:
        if (btn_pressed(PSP_CTRL_CROSS)) prepare_level();
        break;
    }
}

/* ---------------- dibujo ---------------- */
static void draw_text_wrapped(int font, float x, float y, float w, const char *s, u32 col)
{
    char line[300]; int n = 0; const char *p = s; float yy = y;
    char word[96];
    line[0] = 0;
    while (*p) {
        int k = 0;
        while (*p == ' ') p++;
        if (*p == '|') { p++; if (line[0]) { text_draw(font, x, yy, line, col); yy += 20; line[0] = 0; n = 0; } continue; }
        while (*p && *p != ' ' && *p != '|' && k < 95) word[k++] = *p++;
        word[k] = 0;
        if (!k) continue;
        char test[260];
        snprintf(test, sizeof(test), "%s%s%s", line, n ? " " : "", word);
        if (text_width(font, test) > w && n) { text_draw(font, x, yy, line, col); yy += 20; snprintf(line, sizeof(line), "%s", word); n = 1; }
        else { snprintf(line, sizeof(line), "%s", test); n++; }
    }
    if (line[0]) text_draw(font, x, yy, line, col);
}

static void draw_boot(void)
{
    if (timer < 90) {
        float a = timer < 20 ? timer / 20.0f : timer > 75 ? (90 - timer) / 15.0f : 1;
        gfx_draw(IMG_EA, SCREEN_W / 2 - img_w(IMG_EA) / 2, SCREEN_H / 2 - img_h(IMG_EA) / 2, ((u32)(a * 255) << 24) | 0xFFFFFF, 0);
    } else if (timer < 180) {
        int t = timer - 90;
        float a = t < 20 ? t / 20.0f : t > 75 ? (90 - t) / 15.0f : 1;
        gfx_draw(IMG_POPCAP, SCREEN_W / 2 - img_w(IMG_POPCAP) / 2, SCREEN_H / 2 - img_h(IMG_POPCAP) / 2, ((u32)(a * 255) << 24) | 0xFFFFFF, 0);
    } else {
        /* barra de carga del J2ME: cesped que se desenrolla y la podadora */
        gfx_draw_ex(IMG_LOGO_ES, SCREEN_W / 2, 70, img_w(IMG_LOGO_ES) / 2.0f, img_h(IMG_LOGO_ES) / 2.0f, 1.2f, 1.2f, 0, WHITE, 0);
        float prog = (timer - 180) / 140.0f; if (prog > 1) prog = 1;
        float bx = SCREEN_W / 2 - 150, by = 160, bw = 300;
        int seg = img_w(IMG_L_GRASS_B);
        gfx_draw(IMG_L_GRASS_A, bx, by, WHITE, 0);
        float x = bx + img_w(IMG_L_GRASS_A);
        float end = bx + bw * prog;
        while (x + seg < end) { gfx_draw(IMG_L_GRASS_B, x, by, WHITE, 0); x += seg; }
        if (prog >= 1) gfx_draw(IMG_L_GRASS_C, x, by, WHITE, 0);
        gfx_draw(IMG_L_MOWER, end - 10, by - 4, WHITE, 0);
        if (frame & 4) gfx_draw(IMG_L_CLIP, end - 40, by - 10, WHITE, 0);
        gfx_draw(IMG_L_URL, SCREEN_W / 2 - img_w(IMG_L_URL) / 2, by + 56, WHITE, 0);
        text_draw_centered(FONT_SMALL, SCREEN_W / 2, by + 74, prog < 1 ? "CARGANDO..." : "PULSA X", 0xFFFFFFFF);
    }
}

static void draw_title(void)
{
    gfx_draw(IMG_TITLE_BG, 0, -24, WHITE, 0);
    gfx_draw_ex(IMG_LOGO_ES, 330, 56, img_w(IMG_LOGO_ES) / 2.0f, img_h(IMG_LOGO_ES) / 2.0f, 1, 1, 0, WHITE, 0);
    char buf[48];
    snprintf(buf, sizeof(buf), "AVENTURA  %d-%d", level / 10 + 1, level % 10 + 1);
    const char *items[3] = { buf, "ELEGIR NIVEL", "SALIR" };
    for (int i = 0; i < 3; i++)
        text_draw_centered(FONT_SMALL, 330, 122 + i * 26, items[i], i == menu_sel ? ((frame / 10) & 1 ? 0xFF00FFFF : 0xFF40C0FF) : 0xFFFFFFFF);
    text_draw(FONT_SMALL, 6, 254, "PORT PSP - VERSION DE PRUEBA", 0xB0FFFFFF);
}

static void draw_levels(void)
{
    gfx_draw(IMG_TITLE_BG, 0, -24, 0xFF808080, 0);
    text_draw_centered(FONT_SMALL, SCREEN_W / 2, 10, "ELIGE NIVEL (X JUGAR, O VOLVER)", WHITE);
    static const char *zn[5] = { "DÍA", "NOCHE", "PISCINA", "PISCINA DE NOCHE", "TEJADO" };
    for (int w = 0; w < 5; w++) {
        text_draw(FONT_SMALL, 20, 40 + w * 44, zn[w], 0xFFC0FFC0);
        for (int i = 0; i < 10; i++) {
            int l = w * 10 + i;
            float x = 20 + i * 44, y = 58 + w * 44;
            u32 c = l == sel_level ? 0xFF00FFFF : (l <= max_level ? 0xC0FFFFFF : 0x80A0A0A0);
            gfx_rect(x, y, 40, 20, l == sel_level ? 0xA0004080 : 0x80000000);
            char b[8]; snprintf(b, sizeof(b), "%d-%d", w + 1, i + 1);
            text_draw_centered(FONT_SMALL, x + 20, y + 1, b, c);
        }
    }
}

static void draw_seeds(void)
{
    gfx_draw(IMG_TITLE_BG, 0, -24, 0xFF707070, 0);
    char b[64];
    snprintf(b, sizeof(b), "¡ELIGE TUS PLANTAS!  %d/%d", nchosen, level_slots(level));
    text_draw_centered(FONT_SMALL, SCREEN_W / 2, 4, b, 0xFF00FFFF);
    gfx_rect(0, 24, SCREEN_W, 40, 0x90000000);
    for (int i = 0; i < nchosen; i++) gfx_draw(plant_defs[chosen[i]].packet, 6 + i * 50, 28, WHITE, 0);
    for (int i = 0; i < navail; i++) {
        float x = 26 + (i % 8) * 54, y = 74 + (i / 8) * 40;
        int in = 0; for (int k = 0; k < nchosen; k++) if (chosen[k] == avail[i]) in = 1;
        gfx_draw(plant_defs[avail[i]].packet, x, y, in ? 0xFF606060 : WHITE, 0);
        if (i == seed_cur) {
            u32 c = (frame / 8) & 1 ? 0xFF00FFFF : 0xFF00C0FF;
            gfx_rect(x - 2, y - 2, 51, 2, c); gfx_rect(x - 2, y + 33, 51, 2, c);
            gfx_rect(x - 2, y - 2, 2, 37, c); gfx_rect(x + 47, y - 2, 2, 37, c);
        }
    }
    snprintf(b, sizeof(b), "%s  (%d)", plant_defs[avail[seed_cur]].name, plant_defs[avail[seed_cur]].cost);
    text_draw_centered(FONT_SMALL, SCREEN_W / 2, 232, b, WHITE);
    text_draw_centered(FONT_SMALL, SCREEN_W / 2, 252, "X ELEGIR  O QUITAR/VOLVER  START ¡A JUGAR!", 0xFFC0C0C0);
}

static void draw_dave(void)
{
    gfx_draw(IMG_TITLE_BG, 0, -24, 0xFF909090, 0);
    if (dave.def) reanim_draw(&dave, -20, 40, 2.6f, WHITE);
    gfx_rect(190, 150, 280, 100, 0xD0FFFFFF);
    gfx_rect(190, 150, 280, 2, 0xFF000000);
    draw_text_wrapped(FONT_SMALL, 198, 158, 262, TXT_ES[dave_line], 0xFF000000);
    text_draw(FONT_SMALL, 380, 252, "X: SEGUIR", 0xFFFFFFFF);
}

static void draw_reward(void)
{
    gfx_draw(IMG_TITLE_BG, 0, -24, 0xFF606060, 0);
    gfx_draw_ex(IMG_CARD, SCREEN_W / 2, 130, img_w(IMG_CARD) / 2.0f, img_h(IMG_CARD) / 2.0f, 5.5f, 3.6f, 0, WHITE, 0);
    text_draw_centered(FONT_SMALL, SCREEN_W / 2, 52, TXT_ES[51], 0xFF00FFFF);
    gfx_draw_ex(plant_defs[reward].packet, SCREEN_W / 2, 110, 23.5f, 16.5f, 1.6f, 1.6f, 0, WHITE, 0);
    text_draw_centered(FONT_SMALL, SCREEN_W / 2, 145, plant_defs[reward].name, WHITE);
    text_draw_centered(FONT_SMALL, SCREEN_W / 2, 200, "X: CONTINUAR", 0xFFC0C0C0);
}

void game_draw(void)
{
    switch (scene) {
    case SC_BOOT: draw_boot(); break;
    case SC_TITLE: draw_title(); break;
    case SC_LEVELS: draw_levels(); break;
    case SC_SEEDS: draw_seeds(); break;
    case SC_DAVE: draw_dave(); break;
    case SC_BOARD: board_draw(); break;
    case SC_REWARD: draw_reward(); break;
    }
}
