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
#include "ui.h"

#define IMG_TITLE_BG 290
#define IMG_LOGO_ES 137
#define IMG_EA 475
#define IMG_POPCAP 439
#define IMG_L_URL 606
#define IMG_L_GRASS_A 607
#define IMG_L_GRASS_B 609    /* l4: tramo central de 40 px */
#define IMG_L_GRASS_C 608    /* l3: extremo derecho */
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

enum { SC_BOOT, SC_TITLE, SC_OPTIONS, SC_ALMANAC, SC_ABOUT, SC_LEVELS, SC_DAVE, SC_BOARD, SC_REWARD, SC_MINI };
enum { MI_ADVENTURE, MI_MINIGAMES, MI_OPTIONS, MI_ALMANAC, MI_LEVELS, MI_ABOUT, MI_EXIT, MI_COUNT };
static int mini_sel, mini_done, playing_mini;     /* minijuegos: seleccion, superados (bits), nivel 50.. en juego */
static int scene, frame, quit, timer, menu_sel, hand_t, confirm;
static int level, max_level, sel_level, lang_user;   /* lang_user: idioma elegido en opciones (si no, el de la consola) */
#define opt_sound g_opt_sound
#define opt_music g_opt_music
static int avail[PL_COUNT], navail;
static int dave_line, dave_first, dave_last;
static int reward;
static ReAnim dave;
/* almanaque */
static int alm_page, alm_cur, alm_detail, alm_scroll, alm_list[PL_COUNT + ZT_COUNT], alm_n;

/* ---------------- guardado ---------------- */
#define SAVE_PATH "data/save.dat"
static void save_game(void)
{
    SceUID f = sceIoOpen(SAVE_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (f < 0) return;
    int d[4] = { 0x5A565032, level, max_level, (opt_sound ? 1 : 0) | (opt_music ? 2 : 0) | 4 | (lang_user ? (g_lang + 1) << 8 : 0) | (mini_done << 16) };
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
        if ((d[3] >> 8) & 7) { lang_user = 1; ui_set_lang(((d[3] >> 8) & 7) - 1); }
        mini_done = (d[3] >> 16) & 31;
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

#define apply_options ui_apply_options

static void start_board(void)
{
    scene = SC_BOARD;
    if (playing_mini) {                           /* minijuegos: todas las plantas */
        navail = 0;
        for (int t = 0; t < PL_COUNT; t++) avail[navail++] = t;
        board_start(playing_mini, avail, navail, level_slots(playing_mini), 1);
        return;
    }
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
            PL_TALLNUT, PL_SEASHROOM, PL_PLANTERN, PL_CACTUS, PL_BLOVER, PL_STARFRUIT, PL_PUMPKIN, PL_CABBAGEPULT, PL_FLOWERPOT,
            PL_KERNELPULT, PL_GARLIC, PL_MELONPULT, PL_GATLING, PL_WINTERMELON, PL_CATTAIL, PL_COBCANNON };
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

static int alm_lines;     /* lineas del texto de la ficha (para no pasarse al bajar) */
static int alm_vis = 8;   /* lineas visibles de la ficha (lo fija el dibujo) */
#define ALM_VIS alm_vis
static const char *alm_text(void)
{
    if (alm_page == 3) return XS(XS_HELP);
    int t = alm_list[alm_cur];
    return (alm_page == 1 ? plant_desc(t) : TXT[zombie_txt_desc[t]]);
}
static void almanac_open_detail(void)
{
    alm_detail = 1; alm_scroll = 0;
    alm_lines = ui_text_wrap(FONT_MED, 0, 0, 392, 19, alm_text(), 0, 0);
}

void game_init(void)
{
    srand(sceKernelGetSystemTimeLow());
    ui_set_lang(ui_system_lang());
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
#ifndef AT_BOOT
    if (scene == SC_BOOT && frame > 30) { enter_title(); }
#endif
#ifdef AT_MENU
    /* recorrido de pantallas para las capturas de prueba */
    if (frame == 50) max_level = 49;
    if (scene == SC_TITLE && frame == 60) menu_sel = AT_MENU;
    if (frame == 80) { if (AT_MENU == MI_OPTIONS) scene = SC_OPTIONS; else if (AT_MENU == MI_ABOUT) scene = SC_ABOUT; else enter_almanac(); }
    if (frame == 120) { alm_page = 1; almanac_build(); alm_cur = 9; }
    if (frame == 160) almanac_open_detail();
    if (frame == 200) alm_scroll = alm_lines > ALM_VIS ? alm_lines - ALM_VIS : 0;
    if (frame == 240) { alm_detail = 0; alm_page = 2; almanac_build(); alm_cur = 5; }
    if (frame == 280) almanac_open_detail();
    if (frame == 320) { alm_detail = 0; alm_page = 3; almanac_open_detail(); }
    if (frame == 360) { scene = SC_REWARD; reward = PL_POTATOMINE; timer = 0; alm_page = 1; alm_n = 1; alm_list[0] = reward; alm_cur = 0; almanac_open_detail(); alm_detail = 0; }
    if (frame == 400) { level = 40; go_dave_or_board(); }
    if (frame == 440) { menu_sel = 0; scene = SC_TITLE; confirm = 1; }
    if (frame == 480) { confirm = 0; scene = SC_LEVELS; sel_level = 13; max_level = 17; }
    if (frame == 520) { scene = SC_OPTIONS; menu_sel = 0; ui_set_lang(LANG_DE); }
    if (frame == 560) { scene = SC_TITLE; ui_set_lang(LANG_FR); }
    if (frame == 600) { enter_almanac(); alm_page = 1; almanac_build(); alm_cur = 3; almanac_open_detail(); }
    if (scene == SC_DAVE) reanim_update(&dave, 1.0f / 60.0f);
    if (scene != SC_BOOT) return;
#else
#ifdef AT_MINI
    if (frame == 40) { scene = SC_MINI; mini_sel = AT_MINI; }
    if (frame == 120) { playing_mini = LV_MG_BOWL + AT_MINI; start_board(); }
#else
    if (scene == SC_TITLE && frame > 40 && !hand_t) { hand_t = 1; }
#endif
#endif
    if (scene == SC_DAVE && frame % 20 == 0) { start_board(); }
#endif
    switch (scene) {
    case SC_BOOT:
        timer++;
        if (timer == 1) { img_load(IMG_EA); img_load(IMG_POPCAP); apply_options(); }
        if (timer == 190) {                  /* carga real de lo comun mientras avanza la barra */
            font_load(FONT_SMALL); font_load(FONT_BIG); font_load(FONT_NUM);
            img_load(IMG_TITLE_BG); img_load(ui_logo());
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
            case MI_ADVENTURE: hand_t = 1; music_stop(); playing_mini = 0; break;
            case MI_MINIGAMES: scene = SC_MINI; img_load(1315); break;
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
        /* 0 idioma (en la ventanita de la lapida), 1 sonido, 2 musica, 3 borrar datos, 4 atras */
        if (menu_up()) { menu_sel = (menu_sel + 4) % 5; sfx_play(SFX_TAP); }
        if (menu_down()) { menu_sel = (menu_sel + 1) % 5; sfx_play(SFX_TAP); }
        int back = btn_pressed(PSP_CTRL_CIRCLE);
        if (menu_sel == 0 && (btn_pressed(PSP_CTRL_LEFT) || btn_pressed(PSP_CTRL_RIGHT))) {
            ui_set_lang((g_lang + (btn_pressed(PSP_CTRL_LEFT) ? LANG_COUNT - 1 : 1)) % LANG_COUNT);
            lang_user = 1; save_game(); sfx_play(SFX_TAP);
        }
        if (btn_pressed(PSP_CTRL_CROSS)) {
            sfx_play(SFX_BUTTONCLICK);
            if (menu_sel == 0) { ui_set_lang((g_lang + 1) % LANG_COUNT); lang_user = 1; }
            else if (menu_sel == 1) opt_sound = !opt_sound;
            else if (menu_sel == 2) opt_music = !opt_music;
            else if (menu_sel == 3) confirm = 1;
            else back = 1;
            apply_options(); save_game();
        }
        if (back) { scene = SC_TITLE; menu_sel = MI_OPTIONS; sfx_play(SFX_TAP); }
        break;
    }
    case SC_ALMANAC: {
        if (alm_detail) {
            int maxs = alm_lines > ALM_VIS ? alm_lines - ALM_VIS : 0;
            if (menu_down() && alm_scroll < maxs) { alm_scroll++; sfx_play(SFX_TAP); }
            if (menu_up() && alm_scroll > 0) { alm_scroll--; sfx_play(SFX_TAP); }
            if (alm_page != 3) {
                if (btn_repeat(PSP_CTRL_RIGHT) && alm_cur + 1 < alm_n) { alm_cur++; almanac_open_detail(); sfx_play(SFX_TAP); }
                if (btn_repeat(PSP_CTRL_LEFT) && alm_cur > 0) { alm_cur--; almanac_open_detail(); sfx_play(SFX_TAP); }
            }
            if (btn_pressed(PSP_CTRL_CIRCLE) || btn_pressed(PSP_CTRL_CROSS)) {
                alm_detail = 0; sfx_play(SFX_TAP);
                if (alm_page == 3) { alm_page = 0; alm_cur = 2; }
            }
            break;
        }
        if (alm_page == 0) {                   /* indice: plantas, zombis, ayuda */
            if (btn_repeat(PSP_CTRL_LEFT) && alm_cur > 0) { alm_cur--; sfx_play(SFX_TAP); }
            if (btn_repeat(PSP_CTRL_RIGHT) && alm_cur < 2) { alm_cur++; sfx_play(SFX_TAP); }
            if (btn_pressed(PSP_CTRL_CROSS)) {
                sfx_play(SFX_BUTTONCLICK);
                if (alm_cur == 2) { alm_page = 3; almanac_open_detail(); }
                else { alm_page = alm_cur ? 2 : 1; almanac_build(); }
            }
            if (btn_pressed(PSP_CTRL_CIRCLE)) { enter_title(); menu_sel = MI_ALMANAC; }
            break;
        }
        int cols = 7;
        if (btn_repeat(PSP_CTRL_RIGHT) && alm_cur + 1 < alm_n) { alm_cur++; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_LEFT) && alm_cur > 0) { alm_cur--; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_DOWN) && alm_cur + cols < alm_n) { alm_cur += cols; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_UP) && alm_cur - cols >= 0) { alm_cur -= cols; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CROSS) && alm_n) { almanac_open_detail(); sfx_play(SFX_BUTTONCLICK); }
        if (btn_pressed(PSP_CTRL_CIRCLE)) { alm_cur = alm_page - 1; alm_page = 0; sfx_play(SFX_TAP); }
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
    case SC_MINI:                          /* minijuegos: fila de 5 */
        if (btn_repeat(PSP_CTRL_RIGHT) && mini_sel < MG_COUNT - 1) { mini_sel++; sfx_play(SFX_TAP); }
        if (btn_repeat(PSP_CTRL_LEFT) && mini_sel > 0) { mini_sel--; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CIRCLE)) { scene = SC_TITLE; menu_sel = MI_MINIGAMES; sfx_play(SFX_TAP); }
        if (btn_pressed(PSP_CTRL_CROSS)) { sfx_play(SFX_BUTTONCLICK); playing_mini = LV_MG_BOWL + mini_sel; music_stop(); reanim_unload_all(); img_unload_all(); start_board(); }
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
        if (playing_mini && r != BR_PLAYING) {   /* minijuego: vuelve a su menu (o se repite) */
            if (r == BR_RESTART) { start_board(); break; }
            if (r == BR_WON) { mini_done |= 1 << (playing_mini - LV_MG_BOWL); save_game(); }
            if (r == BR_LOST) { start_board(); break; }
            enter_title(); scene = SC_MINI; playing_mini = 0;
            break;
        }
        if (r == BR_QUIT) enter_title();
        else if (r == BR_RESTART) start_board();
        else if (r == BR_LOST) prepare_level();
        else if (r == BR_WON) {
            reward = board_reward();
            if (level < 49) level++;
            if (level > max_level) max_level = level;
            save_game();
            if (reward >= 0) {
                scene = SC_REWARD; img_unload_all(); timer = 0;
                alm_page = 1; alm_n = 1; alm_list[0] = reward; alm_cur = 0; almanac_open_detail(); alm_detail = 0;
            }
            else prepare_level();
        }
        break;
    }
    case SC_REWARD: {                      /* ficha de la planta nueva, como el almanaque del J2ME */
        timer++;
        int maxs = alm_lines > ALM_VIS ? alm_lines - ALM_VIS : 0;
        if (menu_down() && alm_scroll < maxs) alm_scroll++;
        if (menu_up() && alm_scroll > 0) alm_scroll--;
        if (timer > 30 && btn_pressed(PSP_CTRL_CROSS)) { sfx_play(SFX_BUTTONCLICK); prepare_level(); }
        break;
    }
    }
}

/* ---------------- dibujo ---------------- */
/* pantalla de carga del J2ME (clase by): fondo negro, franja de cesped (l2 + l4... + l3) a lo ancho,
 * el cortacesped (l5) la recorre segun el progreso echando recortes (l6, 2 frames) y la web (l1) debajo */
static void draw_loading(float prog)
{
    /* a escala 1:1 (las posiciones del J2ME 480x320 pasadas a 272 de alto) para que no haya costuras */
    gfx_rect(0, 0, SCREEN_W, SCREEN_H, 0xFF000000);
    int wa = img_w(IMG_L_GRASS_A), wb = img_w(IMG_L_GRASS_B), wc = img_w(IMG_L_GRASS_C), wm = img_w(IMG_L_MOWER);
    int bar = 360 - 360 % wb + wa + wc;
    float x = 240 - bar / 2, y = (int)(163 * J2ME_VS);
    gfx_draw(IMG_L_GRASS_A, x, y, WHITE, 0);
    float tx = x + wa;
    for (int k = 0; k < (bar - wa - wc) / wb; k++, tx += wb) gfx_draw(IMG_L_GRASS_B, tx, y, WHITE, 0);
    gfx_draw(IMG_L_GRASS_C, tx, y, WHITE, 0);
    float mx = (int)(x + prog * (bar - wm));
    if (prog < 1) {
        int cw = img_w(IMG_L_CLIP) / 2, ch = img_h(IMG_L_CLIP), b = (frame / 6) & 1;
        float cx = mx - cw + 38;
        gfx_clip((int)cx, (int)(y - ch), cw, ch);
        gfx_draw(IMG_L_CLIP, cx - cw * b, y - ch, WHITE, 0);
        gfx_noclip();
    }
    gfx_draw(IMG_L_MOWER, mx, y - 18, WHITE, 0);
    gfx_draw(IMG_L_URL, 240 - img_w(IMG_L_URL) / 2, y + 15 + 42, WHITE, 0);
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
        float prog = (timer - 180) / 140.0f; if (prog > 1) prog = 1;
        draw_loading(prog);
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
    gfx_draw(ui_logo(), 0, 2, col, 0);
}
/* aviso con la lapida morada del J2ME (se dibuja a escala 1:1, fuera de la vista 480x320) */
static void draw_confirm(const char *title, const char *body)
{
    float vs = gfx_vscale();
    gfx_set_vscale(1);
    gfx_rect(0, 0, SCREEN_W, SCREEN_H, 0x90000000);
    ui_tomb_dialog(110, 50, 260, 180);
    text_draw_centered(FONT_BIG, SCREEN_W / 2, 74, title, 0xFFE8E8E8);
    ui_text_wrap(FONT_SMALL, 136, 106, 208, 18, body, 0xFF40FFFF, 1);
    text_draw_centered(FONT_BIG, SCREEN_W / 2, 184, XS(XS_YES_NO), 0xFFE8E8E8);
    gfx_set_vscale(vs);
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
    text_draw_centered(FONT_MENU, 335, 65, TXT[33], c);
    draw_slab(IMG_SLAB_B, 241, 92, 0, XS(XS_MINIGAMES), menu_sel == MI_MINIGAMES);
    draw_slab(IMG_SLAB_A, 241, 126, 0, TXT[23], menu_sel == MI_OPTIONS);
    draw_slab(IMG_SLAB_B, 241, 160, GFX_FLIPX, TXT[82], menu_sel == MI_ALMANAC);
    draw_slab(IMG_SLAB_A, 241, 194, GFX_FLIPX, XS(XS_PICK_LEVEL), menu_sel == MI_LEVELS);
    draw_slab(IMG_SLAB_B, 241, 228, 0, TXT[47], menu_sel == MI_ABOUT);
    gfx_draw(IMG_EXIT, 457, 297, menu_sel == MI_EXIT ? ((frame / 10) & 1 ? WHITE : 0xFF80FFFF) : 0xFFB0B0B0, 0);
    if (menu_sel == MI_EXIT) text_draw(FONT_SMALL, 452 - text_width(FONT_SMALL, TXT[3]), 298, TXT[3], 0xFF40FFFF);
    if (hand_t) {                          /* mano de zombi (443) saliendo de la tierra */
        float k = hand_t < 30 ? hand_t / 30.0f : 1;
        float shake = hand_t < 30 ? sinf(hand_t * 1.3f) * 2 : 0;
        gfx_clip(0, 0, 480, 300);
        gfx_draw(IMG_HAND, 150 + shake, 300 - 86 * k, WHITE, 0);
        gfx_noclip();
        if (hand_t > 70) gfx_rect(0, 0, 480, 320, ((u32)((hand_t - 70) * 255 / 40) << 24));
    }
    if (confirm) draw_confirm(TXT[54], TXT[55]);
    gfx_set_vscale(1);
}

static void draw_options(void)
{
    gfx_set_vscale(J2ME_VS);
    draw_menu_bg(WHITE);
    gfx_draw(IMG_TOMB_TOP, 246, 33, WHITE, 0);
    char lb[64];
    snprintf(lb, sizeof(lb), menu_sel == 0 ? "< %s >" : "%s", TXT[ui_lang_name(g_lang)]);
    text_draw_centered(FONT_SMALL, 335, 42, TXT[13], 0xFF40FFFF);
    text_draw_centered(FONT_MENU, 335, 65, lb, menu_sel == 0 ? ((frame / 10) & 1 ? 0xFF40FFFF : 0xFF00E0FF) : 0xFFE8E8E8);
    draw_slab(IMG_SLAB_A, 241, 96, 0, TXT[opt_sound ? 41 : 42], menu_sel == 1);
    draw_slab(IMG_SLAB_B, 241, 137, 0, XS(opt_music ? XS_MUSIC_ON : XS_MUSIC_OFF), menu_sel == 2);
    draw_slab(IMG_SLAB_A, 241, 177, GFX_FLIPX, TXT[39], menu_sel == 3);
    draw_slab(IMG_SLAB_B, 241, 217, GFX_FLIPX, TXT[2], menu_sel == 4);
    gfx_draw(IMG_BACK, 457, 297, WHITE, 0);
    if (confirm) draw_confirm(TXT[56], TXT[57]);
    gfx_set_vscale(1);
}

static void draw_about(void)
{
    ui_frame_bg();
    ui_title_bar(42, 15, 396, 423, 40, TXT[47], FONT_BIG);
    ui_panel(24, 60, 432, 190, 136, 37, 514);
    ui_text_wrap(FONT_MED, 44, 76, 392, 19,
        XS(XS_ABOUT1), 0xFF101010, 1);
    ui_text_wrap(FONT_MED, 44, 76 + 19 * (ui_text_wrap(FONT_MED, 0, 0, 392, 19, XS(XS_ABOUT1), 0, 0) + 1), 392, 19, XS(XS_ABOUT2), 0xFF101010, 1);
    gfx_draw(IMG_BACK, 455, 250, WHITE, 0);
}

/* retratos 45x45 del almanaque del J2ME, por tipo de zombi */
static const short zombie_portrait[ZT_COUNT] = {
    6, 334, 524, 131, 378, 245, 431, 569, 228, 174, 470, 445, 67, 476, 295, 100, 163, 69, 486, 486, 169, 417 };

/* ficha (almanaque o planta nueva): sobre o retrato con marco, barra de titulo y panel de texto con desplazamiento */
static void draw_card(int plant, int t, int scroll, int top_extra)
{
    float y0 = 12 + top_extra;
    if (plant) {
        gfx_draw(392, 24, y0, WHITE, 0);
        gfx_draw(plant_defs[t].packet, 31, y0 + 8, WHITE, 0);
        ui_title_bar(87, y0 + 8, 369, 595, 158, plant_name(t), FONT_BIG);
    } else {
        gfx_draw(zombie_portrait[t], 24, y0, WHITE, 0);
        ui_title_bar(71, y0 + 6, 385, 96, 7, TXT[zombie_txt_name[t]], FONT_BIG);
    }
    float py = y0 + 52, ph = SCREEN_H - 14 - py;
    if (plant) ui_panel(24, py, 432, ph, 136, 37, 514);
    else ui_panel(24, py, 432, ph, 311, 288, 120);
    int vis = (int)((ph - 24) / 19);
    alm_vis = vis;
    gfx_clip(30, (int)py + 11, 420, vis * 19);
    ui_text_wrap(FONT_MED, 44, py + 12 - scroll * 19, 392, 19, alm_text(), 0xFF101010, 1);
    gfx_noclip();
    if (alm_lines - scroll > vis) gfx_draw(522, 233, py + ph - 14, WHITE, GFX_FLIPY);
    if (scroll > 0) gfx_draw(522, 233, py + 2, WHITE, 0);
}

static void draw_almanac(void)
{
    ui_frame_bg();
    if (alm_page == 0) {
        ui_title_bar(42, 21, 396, 423, 40, TXT[26], FONT_BIG);
        static const short btn[3] = { 247, 125, 139 };
        for (int i = 0; i < 3; i++) {
            float x = 110 + i * 90, y = 112;
            gfx_draw(btn[i], x, y, WHITE, 0);
            if (i == 2) text_draw_centered(FONT_MED, x + 40, y + 30, TXT[31], 0xFF202020);
            if (i == alm_cur) ui_cursor(x, y, 80, 84, frame);
        }
        text_draw_centered(FONT_SMALL, SCREEN_W / 2, 210, TXT[alm_cur == 0 ? 27 : alm_cur == 1 ? 28 : 31], 0xFF103060);
    } else if (alm_page == 3) {
        ui_title_bar(42, 15, 396, 423, 40, TXT[31], FONT_BIG);
        ui_panel(24, 58, 432, 200, 136, 37, 514);
        alm_vis = 9;
        gfx_clip(30, 69, 420, 9 * 19);
        ui_text_wrap(FONT_MED, 44, 70 - alm_scroll * 19, 392, 19, alm_text(), 0xFF101010, 1);
        gfx_noclip();
    } else if (!alm_detail) {
        if (alm_page == 1) ui_title_bar(42, 15, 396, 595, 158, TXT[29], FONT_BIG);
        else ui_title_bar(42, 15, 396, 96, 7, TXT[30], FONT_BIG);
        for (int x = 15; x < 465; x += 23) gfx_draw(506, x, 49, WHITE, 0);
        for (int i = 0; i < alm_n; i++) {
            int t = alm_list[i];
            float x, y, w, h;
            if (alm_page == 1) { x = 61 + (i % 7) * 51; y = 60 + (i / 7) * 38; w = 47; h = 33; gfx_draw(plant_defs[t].packet, x, y, WHITE, 0); }
            else { x = 67 + (i % 7) * 50; y = 58 + (i / 7) * 47; w = 45; h = 45; gfx_draw(zombie_portrait[t], x, y, WHITE, 0); }
            if (i == alm_cur) ui_cursor(x, y, w, h, frame);
        }
        if (alm_n) {
            int t = alm_list[alm_cur];
            text_draw_centered(FONT_SMALL, SCREEN_W / 2, 244, (alm_page == 1 ? plant_name(t) : TXT[zombie_txt_name[t]]), 0xFF103060);
        }
    } else draw_card(alm_page == 1, alm_list[alm_cur], alm_scroll, 0);
    gfx_draw(IMG_BACK, 455, 250, WHITE, 0);
}

/* elegir nivel: marco del almanaque, una fila por zona con su fondo en miniatura y los 10 niveles en
 * marcos de sobre (614); el cursor son las esquinas del PvZBV */
static void draw_levels(void)
{
    static const short bgs[5] = { 147, 572, 8, 110, 45 };
    ui_frame_bg();
    ui_title_bar(42, 12, 396, 423, 40, XS(XS_PICK_LEVEL), FONT_BIG);
    for (int w = 0; w < 5; w++) {
        float y = 52 + w * 41;
        gfx_rect(22, y - 1, 62, 33, 0xFF10304A);
        gfx_draw_ex(bgs[w], 23, y, 0, 0, 60.0f / 610, 31.0f / 320, 0, WHITE, 0);
        for (int i = 0; i < 10; i++) {
            int l = w * 10 + i;
            float x = 90 + i * 38;
            int reached = l <= max_level;
            gfx_rect(x + 2, y + 2, 37, 27, reached ? 0xFF1A3A5A : 0xFF202830);
            gfx_draw(614, x, y, reached ? WHITE : 0xFF808080, 0);
            char b[8]; snprintf(b, sizeof(b), "%d-%d", w + 1, i + 1);
            u32 c = l == max_level ? 0xFF40FFFF : reached ? WHITE : 0xFF909090;
            text_draw_centered(FONT_SMALL, x + 21, y + 6, b, c);
            if (l == sel_level) {
                float cx = x, cy = y, cw = 41, ch = 31;
                gfx_draw(612, cx - 3, cy - 3, WHITE, 0); gfx_draw(612, cx + cw - 10, cy - 3, WHITE, GFX_FLIPX);
                gfx_draw(612, cx - 3, cy + ch - 9, WHITE, GFX_FLIPY); gfx_draw(612, cx + cw - 10, cy + ch - 9, WHITE, GFX_FLIPX | GFX_FLIPY);
            }
        }
    }
    text_draw_centered(FONT_SMALL, SCREEN_W / 2, 254, XS(XS_X_PLAY_O_BACK), 0xFF103060);
}

/* dialogo de Dave como el J2ME: el fondo del nivel, Dave abajo a la izquierda y un bocadillo blanco */
static void draw_dave(void)
{
    static const short bgs[5] = { 147, 572, 8, 110, 45 };
    int bg = level <= 2 ? 305 : bgs[level_area(level)];
    gfx_draw(bg, 0, -10, WHITE, 0);
    if (dave.def) reanim_draw(&dave, -30, 92, 1.75f, WHITE);
    float bx = 166, by = 62, bw = 176, bh = 112;
    gfx_rect(bx + 4, by, bw - 8, bh, 0xFF000000); gfx_rect(bx, by + 4, bw, bh - 8, 0xFF000000);
    gfx_rect(bx + 2, by + 2, bw - 4, bh - 4, 0xFF000000);
    gfx_rect(bx + 5, by + 2, bw - 10, bh - 4, WHITE); gfx_rect(bx + 2, by + 5, bw - 4, bh - 10, WHITE);
    gfx_rect(bx + 3, by + 3, bw - 6, bh - 6, WHITE);
    for (int k = 0; k < 12; k++) {                 /* pico del bocadillo hacia Dave */
        gfx_rect(bx + 14 - k * 0.8f - 1, by + bh - 3 + k, 12 - k + 2, 1, 0xFF000000);
        gfx_rect(bx + 14 - k * 0.8f, by + bh - 3 + k, 12 - k, 1, WHITE);
    }
    int n = ui_text_wrap(FONT_MED, 0, 0, bw - 20, 18, TXT[dave_line], 0, 0);
    float ty = by + (bh - n * 18) / 2;
    /* centrado linea a linea */
    char buf[300]; snprintf(buf, sizeof(buf), "%s", TXT[dave_line]);
    ui_wrap_center = 1;
    ui_text_wrap(FONT_MED, bx + bw / 2, ty, bw - 20, 18, buf, 0xFF000000, 1);
    ui_wrap_center = 0;
    if ((frame / 20) & 1) text_draw(FONT_SMALL, 360, 250, XS(XS_X_NEXT), 0xFFFFFFFF);
}

/* planta nueva: ficha como la del almanaque con el titulo "¡ENCONTRASTE UNA SEMILLA NUEVA!" */
static void draw_reward(void)
{
    ui_frame_bg();
    text_draw_centered(FONT_MED, SCREEN_W / 2, 8, TXT[51], 0xFF103060);
    draw_card(1, reward, alm_scroll, 22);
    /* destellos alrededor del sobre */
    for (int k = 0; k < 6; k++) {
        float a = frame * 0.05f + k * 1.047f, r = 30 + 4 * sinf(frame * 0.2f + k);
        float x = 55 + cosf(a) * r, y = 58 + sinf(a) * r * 0.8f;
        int f = (frame / 6 + k) % 5;
        gfx_draw_region(150, f * 9, 0, 9, 9, x - 4, y - 4, WHITE);
    }
    if (timer > 30 && (frame / 20) & 1) text_draw_centered(FONT_SMALL, SCREEN_W / 2, 252, XS(XS_X_CONTINUE), 0xFF103060);
}

/* minijuegos: el panel del Tencent (l15) con los iconos l49-l53, el marco l60 y el trofeo de superado (l62) */
static void draw_mini(void)
{
    gfx_draw(1295, 0, 1, WHITE, 0);                /* panel l15 a pantalla completa */
    text_draw_centered(FONT_BIG, SCREEN_W / 2, 12, XS(XS_MINIGAMES), 0xFF103060);
    float fw = img_w(1341), iw = img_w(1329), gap = 4, x0 = SCREEN_W / 2 - (MG_COUNT * fw + (MG_COUNT - 1) * gap) / 2;
    for (int i = 0; i < MG_COUNT; i++) {
        float fx = x0 + i * (fw + gap), fy = 66, x = fx + (fw - iw) / 2, y = fy + 6;
        gfx_draw(1341, fx, fy, i == mini_sel ? WHITE : 0xFFB0B0B0, 0);   /* marco l61 */
        gfx_draw(1329 + i, x, y, WHITE, 0);
        if (mini_done & (1 << i)) gfx_draw(1342, fx + fw - 30, fy - 8, WHITE, 0);   /* trofeo */
        ui_wrap_center = 1;
        ui_text_wrap(FONT_SMALL, fx + fw / 2, fy + img_h(1341) + 4, fw + 2, 15, XS(XS_MG_BOWL + i), i == mini_sel ? 0xFF0050E0 : 0xFF103060, 1);
        ui_wrap_center = 0;
        if (i == mini_sel) {
            gfx_draw(612, fx - 3, fy - 3, WHITE, 0); gfx_draw(612, fx + fw - 10, fy - 3, WHITE, GFX_FLIPX);
            gfx_draw(612, fx - 3, fy + img_h(1341) - 9, WHITE, GFX_FLIPY); gfx_draw(612, fx + fw - 10, fy + img_h(1341) - 9, WHITE, GFX_FLIPX | GFX_FLIPY);
        }
    }
    text_draw_centered(FONT_SMALL, SCREEN_W / 2, SCREEN_H - 30, XS(XS_X_PLAY_O_BACK), 0xFF103060);
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
    case SC_MINI: draw_mini(); break;
    }
}
