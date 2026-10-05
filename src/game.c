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

/* menu del J2ME (480x320): fondo 290, logo, lapida con la caja de nivel (527) y losas 270/271 */
#define IMG_TOMB_TOP 527
#define IMG_SLAB_A 271
#define IMG_SLAB_B 270
#define IMG_HAND 443
#define IMG_EXIT 525
#define IMG_BACK 361
#define IMG_NOTE_BG 139
#define FONT_MENU 1          /* fuente 566 de los botones del menu */
#define IMG_ZHEAD 282

enum { SC_BOOT, SC_TITLE, SC_OPTIONS, SC_ALMANAC, SC_ABOUT, SC_LEVELS, SC_DAVE, SC_BOARD, SC_REWARD };
enum { MI_ADVENTURE, MI_OPTIONS, MI_ALMANAC, MI_LEVELS, MI_ABOUT, MI_EXIT, MI_COUNT };
static int scene, frame, quit, timer, menu_sel, hand_t, confirm;
static int level, max_level, sel_level, opt_sound = 1, opt_music = 1;
static int avail[PL_COUNT], navail;
static int dave_line, dave_first, dave_last;
static int reward;
static ReAnim dave;
/* almanaque */
static int alm_page, alm_cur, alm_detail, alm_scroll, alm_list[PL_COUNT + ZT_COUNT], alm_n;
static ReAnim alm_anim;

/* ---------------- guardado ---------------- */
#define SAVE_PATH "data/save.dat"
static void save_game(void)
{
    SceUID f = sceIoOpen(SAVE_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (f < 0) return;
    int d[4] = { 0x5A565032, level, max_level, (opt_sound ? 1 : 0) | (opt_music ? 2 : 0) | 4 };
    sceIoWrite(f, d, sizeof(d)); sceIoClose(f);
}
static void load_game(void)
{
    level = 0; max_level = 0;
    SceUID f = sceIoOpen(SAVE_PATH, PSP_O_RDONLY, 0);
    if (f < 0) return;
    int d[4];
    if (sceIoRead(f, d, sizeof(d)) == sizeof(d) && d[0] == 0x5A565032) {
        level = d[1]; max_level = d[2];
        if (d[3] & 4) { opt_sound = d[3] & 1; opt_music = (d[3] >> 1) & 1; }
    }
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

static void apply_options(void) { sfx_set_volume(opt_sound ? 256 : 0); music_set_master(opt_music ? 256 : 0); }

static void start_board(void)
{
    scene = SC_BOARD;
    board_start(level, avail, navail, level_slots(level), level > 3);
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

/* la eleccion de plantas se hace en el tablero, con la camara en la calle (como el J2ME) */
static void prepare_level(void)
{
    compute_avail(level);
    go_dave_or_board();
}

static void enter_title(void)
{
    scene = SC_TITLE; hand_t = 0; confirm = 0;
    reanim_unload_all(); img_unload_all();
    font_load(FONT_MENU); font_load(FONT_SMALL);
    music_play("data/music/title_crazydave.mp3", 1);
}

static void enter_almanac(void)
{
    scene = SC_ALMANAC; alm_page = 0; alm_cur = 0; alm_detail = 0;
    reanim_unload_all(); img_unload_all();
}

/* entradas del almanaque: plantas desbloqueadas o zombis vistos (hasta el nivel alcanzado) */
static void almanac_build(void)
{
    alm_n = 0;
    if (alm_page == 1) {
        compute_avail(max_level);
        static const signed char order[PL_COUNT] = {
            PL_PEASHOOTER, PL_SUNFLOWER, PL_CHERRYBOMB, PL_WALLNUT, PL_POTATOMINE, PL_SNOWPEA, PL_CHOMPER, PL_REPEATER,
            PL_PUFFSHROOM, PL_SUNSHROOM, PL_FUMESHROOM, PL_GRAVEBUSTER, PL_HYPNOSHROOM, PL_SCAREDYSHROOM, PL_ICESHROOM,
            PL_DOOMSHROOM, PL_LILYPAD, PL_SQUASH, PL_THREEPEATER, PL_TANGLEKELP, PL_JALAPENO, PL_SPIKEWEED, PL_TORCHWOOD,
            PL_TALLNUT, PL_SEASHROOM, PL_CACTUS, PL_STARFRUIT, PL_CABBAGEPULT, PL_FLOWERPOT, PL_KERNELPULT, PL_MELONPULT };
        for (int k = 0; k < PL_COUNT; k++) for (int i = 0; i < navail; i++) if (avail[i] == order[k]) alm_list[alm_n++] = order[k];
    } else {
        static const signed char zorder[] = { ZT_NORMAL, ZT_FLAG, ZT_CONE, ZT_POLE, ZT_BUCKET, ZT_NEWSPAPER, ZT_DOOR, ZT_FOOTBALL,
            ZT_DANCER, ZT_BACKUP, ZT_DUCKY, ZT_SNORKEL, ZT_DOLPHIN, ZT_JACK, ZT_BALLOON, ZT_DIGGER, ZT_POGO, ZT_LADDER,
            ZT_CATAPULT, ZT_GARGANTUAR, ZT_IMP, ZT_BOSS };
        for (unsigned k = 0; k < sizeof(zorder); k++) {
            int t = zorder[k], seen = 0;
            for (int l = 0; l <= max_level; l++) if (z_levels[t][l] == '1') seen = 1;
            if (t == ZT_BACKUP) for (int l = 0; l <= max_level; l++) if (z_levels[ZT_DANCER][l] == '1') seen = 1;
            if (t == ZT_IMP) for (int l = 0; l <= max_level; l++) if (z_levels[ZT_GARGANTUAR][l] == '1') seen = 1;
            if (seen) alm_list[alm_n++] = t;
        }
    }
    alm_cur = 0; alm_scroll = 0;
}

static void almanac_open_detail(void)
{
    int t = alm_list[alm_cur];
    alm_detail = 1; alm_scroll = 0;
    reanim_unload_all();
    if (alm_page == 1) {
        const PlantDef *d = &plant_defs[t];
        reanim_play(&alm_anim, d->re, d->idle_s, d->idle_e, 1);
        alm_anim.hide_mask[0] = d->hide;
        if (t == PL_SUNFLOWER) alm_anim.hide_mask[0] = (1u << 5) | (1u << 6) | (1u << 7);
    } else {
        const ZombieDef *d = &zombie_defs[t];
        reanim_play(&alm_anim, d->re, d->walk_s, d->walk_e, 1);
        if (d->re == RE_ZOMBIE) {
            u32 m = (7u << 11) | (3u << 16) | (0x1FFu << 21);
            m &= ~d->show;
            if (t == ZT_CONE) m &= ~(1u << 21);
            if (t == ZT_BUCKET) m &= ~(1u << 24);
            if (t == ZT_DOOR) m &= ~(1u << 27);
            if (t == ZT_FLAG) m &= ~((1u << 11) | (1u << 12));
            alm_anim.hide_mask[0] = m;
        }
        if (t == ZT_BOSS) reanim_play(&alm_anim, RE_BOSS, 9, 11, 1);
    }
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
static int menu_up(void) { return btn_repeat(PSP_CTRL_UP); }
static int menu_down(void) { return btn_repeat(PSP_CTRL_DOWN); }

void game_update(void)
{
    frame++;
#ifdef AUTOTEST
#ifndef AT_LEVEL
#define AT_LEVEL 0
#endif
    if (frame == 2) { level = AT_LEVEL; }
    if (scene == SC_BOOT && frame > 30) { enter_title(); }
#ifdef AT_MENU
    if (scene == SC_TITLE && frame == 60) menu_sel = AT_MENU;
    if (scene == SC_TITLE && frame == 80) { if (AT_MENU == MI_OPTIONS) scene = SC_OPTIONS; else if (AT_MENU == MI_ALMANAC) { enter_almanac(); alm_page = 1; almanac_build(); } else if (AT_MENU == MI_ABOUT) scene = SC_ABOUT; }
    if (scene == SC_ALMANAC && frame == 140) { alm_cur = 2; almanac_open_detail(); }
    if (scene == SC_ALMANAC && frame == 220) { alm_detail = 0; alm_page = 2; almanac_build(); alm_cur = 2; }
    if (scene == SC_ALMANAC && frame == 260) almanac_open_detail();
    if (scene == SC_ALMANAC && alm_detail) reanim_update(&alm_anim, 1.0f / 60.0f);
    if (scene != SC_BOOT) return;
#else
    if (scene == SC_TITLE && frame > 40 && !hand_t) { hand_t = 1; }
#endif
    if (scene == SC_DAVE && frame % 20 == 0) { start_board(); }
#endif
    switch (scene) {
    case SC_BOOT:
        timer++;
        if (timer == 1) { img_load(IMG_EA); img_load(IMG_POPCAP); apply_options(); }
        if (timer == 190) {                  /* carga real de lo comun mientras avanza la barra */
            font_load(FONT_SMALL); font_load(FONT_BIG); font_load(FONT_NUM);
            img_load(IMG_TITLE_BG); img_load(IMG_LOGO_ES);
            sfx_preload(SFX_BUTTONCLICK); sfx_preload(SFX_TAP); sfx_preload(SFX_SEEDLIFT);
        }
        if (timer > 330 || (timer > 200 && btn_pressed(PSP_CTRL_CROSS))) enter_title();
        break;
    case SC_TITLE: {
        if (hand_t) {                       /* la mano de zombi sale de la tierra y empieza la aventura */
            if (hand_t == 1) sfx_play(SFX_EVILLAUGH);
            if (++hand_t > 110) { hand_t = 0; prepare_level(); }
            break;
        }
        if (confirm) {
            if (btn_pressed(PSP_CTRL_CROSS)) quit = 1;
            if (btn_pressed(PSP_CTRL_CIRCLE)) { confirm = 0; sfx_play(SFX_TAP); }
            break;
        }
        if (menu_up()) { menu_sel = (menu_sel + MI_COUNT - 1) % MI_COUNT; sfx_play(SFX_TAP); }
        if (menu_down()) { menu_sel = (menu_sel + 1) % MI_COUNT; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CIRCLE)) { menu_sel = MI_EXIT; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CROSS) || btn_pressed(PSP_CTRL_START)) {
            sfx_play(SFX_BUTTONCLICK);
            switch (menu_sel) {
            case MI_ADVENTURE: hand_t = 1; music_stop(); break;
            case MI_OPTIONS: scene = SC_OPTIONS; menu_sel = 0; break;
            case MI_ALMANAC: enter_almanac(); break;
            case MI_LEVELS: scene = SC_LEVELS; sel_level = level; break;
            case MI_ABOUT: scene = SC_ABOUT; break;
            default: confirm = 1; break;
            }
        }
        break;
    }
    case SC_OPTIONS: {
        if (confirm) {
            if (btn_pressed(PSP_CTRL_CROSS)) { level = max_level = 0; save_game(); confirm = 0; sfx_play(SFX_GRAVEBUTTON); }
            if (btn_pressed(PSP_CTRL_CIRCLE)) { confirm = 0; sfx_play(SFX_TAP); }
            break;
        }
        if (menu_up()) { menu_sel = (menu_sel + 3) % 4; sfx_play(SFX_TAP); }
        if (menu_down()) { menu_sel = (menu_sel + 1) % 4; sfx_play(SFX_TAP); }
        int back = btn_pressed(PSP_CTRL_CIRCLE);
        if (btn_pressed(PSP_CTRL_CROSS)) {
            sfx_play(SFX_BUTTONCLICK);
            if (menu_sel == 0) opt_sound = !opt_sound;
            else if (menu_sel == 1) opt_music = !opt_music;
            else if (menu_sel == 2) confirm = 1;
            else back = 1;
            apply_options(); save_game();
        }
        if (back) { scene = SC_TITLE; menu_sel = MI_OPTIONS; sfx_play(SFX_TAP); }
        break;
    }
    case SC_ALMANAC: {
        if (alm_detail) {
            reanim_update(&alm_anim, 1.0f / 60.0f);
            if (menu_down()) alm_scroll++;
            if (menu_up() && alm_scroll > 0) alm_scroll--;
            if (btn_repeat(PSP_CTRL_RIGHT) && alm_cur + 1 < alm_n) { alm_cur++; almanac_open_detail(); sfx_play(SFX_TAP); }
            if (btn_repeat(PSP_CTRL_LEFT) && alm_cur > 0) { alm_cur--; almanac_open_detail(); sfx_play(SFX_TAP); }
            if (btn_pressed(PSP_CTRL_CIRCLE) || btn_pressed(PSP_CTRL_CROSS)) { alm_detail = 0; reanim_unload_all(); sfx_play(SFX_TAP); }
            break;
        }
        if (alm_page == 0) {
            if (menu_up() || menu_down()) { alm_cur = !alm_cur; sfx_play(SFX_TAP); }
            if (btn_pressed(PSP_CTRL_CROSS)) { alm_page = alm_cur ? 2 : 1; almanac_build(); sfx_play(SFX_BUTTONCLICK); }
            if (btn_pressed(PSP_CTRL_CIRCLE)) { enter_title(); menu_sel = MI_ALMANAC; }
            break;
        }
        int cols = alm_page == 1 ? 7 : 6;
        if (btn_repeat(PSP_CTRL_RIGHT) && alm_cur + 1 < alm_n) { alm_cur++; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_LEFT) && alm_cur > 0) { alm_cur--; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_DOWN) && alm_cur + cols < alm_n) { alm_cur += cols; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_UP) && alm_cur - cols >= 0) { alm_cur -= cols; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CROSS) && alm_n) { almanac_open_detail(); sfx_play(SFX_BUTTONCLICK); }
        if (btn_pressed(PSP_CTRL_CIRCLE)) { alm_page = 0; alm_cur = 0; img_unload_all(); sfx_play(SFX_TAP); }
        break;
    }
    case SC_ABOUT:
        if (btn_pressed(PSP_CTRL_CIRCLE) || btn_pressed(PSP_CTRL_CROSS)) { scene = SC_TITLE; menu_sel = MI_ABOUT; sfx_play(SFX_TAP); }
        break;
    case SC_LEVELS:
        /* seleccion libre de los 50 niveles */
        if (btn_repeat(PSP_CTRL_RIGHT)) sel_level = (sel_level + 1) % 50;
        if (btn_repeat(PSP_CTRL_LEFT)) sel_level = (sel_level + 49) % 50;
        if (btn_repeat(PSP_CTRL_DOWN)) sel_level = (sel_level + 10) % 50;
        if (btn_repeat(PSP_CTRL_UP)) sel_level = (sel_level + 40) % 50;
        if (btn_pressed(PSP_CTRL_CIRCLE)) { scene = SC_TITLE; menu_sel = MI_LEVELS; }
        if (btn_pressed(PSP_CTRL_CROSS)) { level = sel_level; prepare_level(); }
        break;
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

/* ---- menu del J2ME (coordenadas 480x320, escala vertical 272/320) ---- */
static void draw_slab(int img, float x, float y, int flip, const char *txt, int sel)
{
    gfx_draw(img, x, y, sel ? WHITE : 0xFFC8C8C8, flip);
    u32 c = sel ? ((frame / 10) & 1 ? 0xFF40FFFF : 0xFF00E0FF) : 0xFFE8E8E8;
    text_draw_centered(FONT_MENU, x + img_w(img) / 2.0f, y + 9, txt, c);
}
static void draw_menu_bg(u32 col)
{
    gfx_draw(IMG_TITLE_BG, 0, 0, col, 0);
    gfx_draw(IMG_LOGO_ES, 0, 2, col, 0);
}
/* panel de papel con borde marron (fichas del almanaque, avisos) */
static void panel(float x, float y, float w, float h)
{
    gfx_rect(x, y, w, h, 0xFF10304A);
    gfx_rect(x + 3, y + 3, w - 6, h - 6, 0xFF2C5C88);
    gfx_rect(x + 6, y + 6, w - 12, h - 12, 0xFFB0E4F2);
}

static void draw_confirm(const char *title, const char *body)
{
    gfx_rect(0, 0, 480, 320, 0x90000000);
    panel(100, 80, 280, 160);
    text_draw_centered(FONT_SMALL, 240, 98, title, 0xFF0040A0);
    draw_text_wrapped(FONT_SMALL, 120, 126, 240, body, 0xFF202020);
    text_draw_centered(FONT_SMALL, 240, 212, "X: SÍ    O: NO", 0xFF004000);
}

static void draw_title(void)
{
    gfx_set_vscale(J2ME_VS);
    draw_menu_bg(WHITE);
    gfx_draw(IMG_TOMB_TOP, 246, 33, WHITE, 0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d-%d", level / 10 + 1, level % 10 + 1);
    text_draw_centered(FONT_SMALL, 335, 42, buf, 0xFF40FFFF);
    u32 c = menu_sel == MI_ADVENTURE ? ((frame / 10) & 1 ? 0xFF40FFFF : 0xFF00E0FF) : 0xFFE8E8E8;
    text_draw_centered(FONT_MENU, 335, 65, TXT_ES[33], c);
    draw_slab(IMG_SLAB_A, 241, 96, 0, TXT_ES[23], menu_sel == MI_OPTIONS);
    draw_slab(IMG_SLAB_B, 241, 137, 0, TXT_ES[82], menu_sel == MI_ALMANAC);
    draw_slab(IMG_SLAB_A, 241, 177, GFX_FLIPX, "ELEGIR NIVEL", menu_sel == MI_LEVELS);
    draw_slab(IMG_SLAB_B, 241, 217, GFX_FLIPX, TXT_ES[47], menu_sel == MI_ABOUT);
    gfx_draw(IMG_EXIT, 457, 297, menu_sel == MI_EXIT ? ((frame / 10) & 1 ? WHITE : 0xFF80FFFF) : 0xFFB0B0B0, 0);
    if (menu_sel == MI_EXIT) text_draw(FONT_SMALL, 452 - text_width(FONT_SMALL, TXT_ES[3]), 298, TXT_ES[3], 0xFF40FFFF);
    if (hand_t) {                          /* mano de zombi (443) saliendo de la tierra */
        float k = hand_t < 30 ? hand_t / 30.0f : 1;
        float shake = hand_t < 30 ? sinf(hand_t * 1.3f) * 2 : 0;
        gfx_clip(0, 0, 480, 300);
        gfx_draw(IMG_HAND, 150 + shake, 300 - 86 * k, WHITE, 0);
        gfx_noclip();
        if (hand_t > 70) gfx_rect(0, 0, 480, 320, ((u32)((hand_t - 70) * 255 / 40) << 24));
    }
    if (confirm) draw_confirm(TXT_ES[54], TXT_ES[55]);
    gfx_set_vscale(1);
}

static void draw_options(void)
{
    gfx_set_vscale(J2ME_VS);
    draw_menu_bg(WHITE);
    gfx_draw(IMG_TOMB_TOP, 246, 33, WHITE, 0);
    text_draw_centered(FONT_MENU, 335, 65, TXT_ES[23], 0xFFE8E8E8);
    draw_slab(IMG_SLAB_A, 241, 96, 0, TXT_ES[opt_sound ? 41 : 42], menu_sel == 0);
    draw_slab(IMG_SLAB_B, 241, 137, 0, opt_music ? "MÚSICA: SÍ" : "MÚSICA: NO", menu_sel == 1);
    draw_slab(IMG_SLAB_A, 241, 177, GFX_FLIPX, TXT_ES[39], menu_sel == 2);
    draw_slab(IMG_SLAB_B, 241, 217, GFX_FLIPX, TXT_ES[2], menu_sel == 3);
    gfx_draw(IMG_BACK, 457, 297, WHITE, 0);
    if (confirm) draw_confirm(TXT_ES[56], TXT_ES[57]);
    gfx_set_vscale(1);
}

static void draw_about(void)
{
    gfx_set_vscale(J2ME_VS);
    draw_menu_bg(0xFF808080);
    panel(80, 64, 320, 220);
    text_draw_centered(FONT_MENU, 240, 36, TXT_ES[47], 0xFFE8E8E8);
    draw_text_wrapped(FONT_SMALL, 98, 76, 290,
        "PLANTAS CONTRA ZOMBIS|VERSIÓN J2ME 4.6.0 DE POPCAP Y EA, PORTADA A PSP EN C NATIVO A 60 FPS.|"
        "|EL JUEGO, SUS GRÁFICOS, TEXTOS Y SONIDOS SON PROPIEDAD DE POPCAP GAMES Y ELECTRONIC ARTS.", 0xFF202020);
    gfx_draw(IMG_BACK, 457, 297, WHITE, 0);
    gfx_set_vscale(1);
}

static void draw_almanac(void)
{
    gfx_set_vscale(J2ME_VS);
    draw_menu_bg(0xFF707070);
    if (alm_page == 0) {
        text_draw_centered(FONT_MENU, 240, 40, TXT_ES[26], 0xFFE8E8E8);
        draw_text_wrapped(FONT_SMALL, 100, 80, 280, TXT_ES[83], WHITE);
        draw_slab(IMG_SLAB_A, 146, 150, 0, TXT_ES[27], alm_cur == 0);
        draw_slab(IMG_SLAB_B, 147, 200, 0, TXT_ES[28], alm_cur == 1);
    } else if (!alm_detail) {
        text_draw_centered(FONT_MENU, 240, 8, TXT_ES[alm_page == 1 ? 29 : 30], 0xFFE8E8E8);
        int cols = alm_page == 1 ? 7 : 6;
        for (int i = 0; i < alm_n; i++) {
            float x, y; int t = alm_list[i];
            if (alm_page == 1) {
                x = 32 + (i % cols) * 60; y = 50 + (i / cols) * 46;
                gfx_draw(plant_defs[t].packet, x, y, WHITE, 0);
            } else {
                x = 40 + (i % cols) * 68; y = 50 + (i / cols) * 62;
                gfx_rect(x, y, 56, 54, 0xC0303828);
                gfx_rect(x + 2, y + 2, 52, 50, 0xFF5A7048);
                char b[8]; snprintf(b, sizeof(b), "%d", i + 1);
                text_draw_centered(FONT_SMALL, x + 28, y + 2, b, 0x80FFFFFF);
                gfx_draw_ex(IMG_ZHEAD, x + 28, y + 30, 8, 8.5f, 2.2f, 2.2f, 0, t == ZT_BOSS ? 0xFF8080FF : WHITE, 0);
            }
            if (i == alm_cur) {
                float w = alm_page == 1 ? 47 : 56, h = alm_page == 1 ? 33 : 54;
                u32 c = (frame / 8) & 1 ? 0xFF00FFFF : 0xFF00C0FF;
                gfx_rect(x - 2, y - 2, w + 4, 2, c); gfx_rect(x - 2, y + h, w + 4, 2, c);
                gfx_rect(x - 2, y - 2, 2, h + 4, c); gfx_rect(x + w, y - 2, 2, h + 4, c);
            }
        }
        if (alm_n) {
            int t = alm_list[alm_cur];
            const char *nm = TXT_ES[alm_page == 1 ? plant_txt_name[t] : zombie_txt_name[t]];
            text_draw_centered(FONT_SMALL, 240, 290, nm, 0xFF40FFFF);
        }
        gfx_draw(IMG_BACK, 457, 297, WHITE, 0);
    } else {
        int t = alm_list[alm_cur];
        /* ficha: animacion a la izquierda, nombre y texto a la derecha */
        panel(20, 40, 184, 250);
        if (alm_anim.def) {
            float *bb = alm_anim.def->bbox;
            float sc = alm_page == 1 ? 2.2f : 1.5f;
            if (alm_page == 2 && t == ZT_BOSS) sc = 0.6f;
            if (alm_page == 2 && t == ZT_GARGANTUAR) sc = 1.0f;
            float cx = (bb[0] + bb[2]) / 2, by = bb[3];
            reanim_draw(&alm_anim, 112 - cx * sc, 236 - by * sc, sc, WHITE);
        }
        panel(214, 8, 256, 284);
        const char *nm = TXT_ES[alm_page == 1 ? plant_txt_name[t] : zombie_txt_name[t]];
        text_draw_centered(FONT_SMALL, 340, 22, nm, 0xFF0040A0);
        if (alm_page == 1) {
            char b[32]; snprintf(b, sizeof(b), "COSTE: %d", plant_defs[t].cost);
            text_draw_centered(FONT_SMALL, 112, 270, b, 0xFF203060);
        }
        gfx_clip(220, 44, 244, 242);
        draw_text_wrapped(FONT_SMALL, 224, 46 - alm_scroll * 20, 238, TXT_ES[alm_page == 1 ? plant_txt_desc[t] : zombie_txt_desc[t]], 0xFF202020);
        gfx_noclip();
        gfx_draw(IMG_BACK, 457, 297, WHITE, 0);
    }
    gfx_set_vscale(1);
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
    case SC_OPTIONS: draw_options(); break;
    case SC_ALMANAC: draw_almanac(); break;
    case SC_ABOUT: draw_about(); break;
    case SC_LEVELS: draw_levels(); break;
    case SC_DAVE: draw_dave(); break;
    case SC_BOARD: board_draw(); break;
    case SC_REWARD: draw_reward(); break;
    }
}
