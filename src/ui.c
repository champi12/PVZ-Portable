/* ui.c - Piezas de interfaz del J2ME (ver ui.h). Ids de imagen medidos con el registro de dibujo del J2ME. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gfx.h"
#include "audio.h"
#include "ui.h"
#include "texts.h"
#include "defs.h"
#include <psputility.h>

int g_lang = LANG_ES;
const char *const *TXT = TXT_ES;
void ui_set_lang(int lang) { if (lang < 0 || lang >= LANG_COUNT) lang = LANG_EN; g_lang = lang; TXT = TXT_LANGS[lang]; }
int ui_system_lang(void)
{
    int v = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    if (sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &v) < 0) return LANG_EN;
    switch (v) {
    case PSP_SYSTEMPARAM_LANGUAGE_FRENCH: return LANG_FR;
    case PSP_SYSTEMPARAM_LANGUAGE_SPANISH: return LANG_ES;
    case PSP_SYSTEMPARAM_LANGUAGE_GERMAN: return LANG_DE;
    case PSP_SYSTEMPARAM_LANGUAGE_ITALIAN: return LANG_IT;
    case PSP_SYSTEMPARAM_LANGUAGE_PORTUGUESE: return LANG_PT;
    default: return LANG_EN;
    }
}
int ui_lang_name(int lang) { static const unsigned char n[LANG_COUNT] = { 14, 15, 17, 16, 19, 18 }; return n[lang]; }
int ui_logo(void) { static const short l[LANG_COUNT] = { 17, 651, 652, 653, 137, 137 }; return l[g_lang]; }

static const char *const xs_tab[XS_COUNT][LANG_COUNT] = {
    /*            EN                       FR                          DE                          IT                          PT                          ES */
    { "MUSIC: ON",              "MUSIQUE : OUI",              "MUSIK: AN",                  "MUSICA: SÌ",                 "MÚSICA: SIM",                "MÚSICA: SÍ" },
    { "MUSIC: OFF",             "MUSIQUE : NON",              "MUSIK: AUS",                 "MUSICA: NO",                 "MÚSICA: NÃO",                "MÚSICA: NO" },
    { "LEVEL SELECT",           "CHOIX DU NIVEAU",            "LEVEL WÄHLEN",               "SCEGLI LIVELLO",             "ESCOLHER NÍVEL",             "ELEGIR NIVEL" },
    { "COST: %d",               "COÛT : %d",                  "KOSTEN: %d",                 "COSTO: %d",                  "CUSTO: %d",                  "COSTE: %d" },
    { "X: YES   O: NO",         "X : OUI   O : NON",          "X: JA   O: NEIN",            "X: SÌ   O: NO",              "X: SIM   O: NÃO",            "X: SÍ   O: NO" },
    { "X: PICK",                "X : CHOISIR",                "X: WÄHLEN",                  "X: SCEGLI",                  "X: ESCOLHER",                "X: ELEGIR" },
    { "TRIANGLE:",              "TRIANGLE :",                 "DREIECK:",                   "TRIANGOLO:",                 "TRIÂNGULO:",                 "TRIÁNGULO:" },
    { "LET'S ROCK!",            "C'EST PARTI !",              "LOS GEHT'S!",                "SI COMINCIA!",               "VAMOS LÁ!",                  "¡A JUGAR!" },
    { "START: PAUSE",           "START : PAUSE",              "START: PAUSE",               "START: PAUSA",               "START: PAUSA",               "START: PAUSA" },
    { "PRESS X TO COLLECT",     "APPUIE SUR X",               "DRÜCKE X",                   "PREMI X",                    "APERTE X",                   "PULSA X PARA RECOGER" },
    { "X: CONTINUE",            "X : CONTINUER",              "X: WEITER",                  "X: CONTINUA",                "X: CONTINUAR",               "X: CONTINUAR" },
    { "X: NEXT",                "X : SUITE",                  "X: WEITER",                  "X: AVANTI",                  "X: SEGUIR",                  "X: SEGUIR" },
    { "MOVE THE CURSOR AROUND YOUR LAWN WITH THE D-PAD. PRESS LEFT ON THE FIRST COLUMN TO REACH YOUR SEED BOX.||X: PICK A SEED PACKET AND PLANT IT.|O: CANCEL.|TRIANGLE: SHOVEL.|L AND R: CHANGE SEED PACKET.|START: PAUSE.||SUN IS COLLECTED BY MOVING THE CURSOR NEAR IT.",
      "DÉPLACE LE CURSEUR SUR LA PELOUSE AVEC LA CROIX. GAUCHE SUR LA PREMIÈRE COLONNE : BOÎTE À GRAINES.||X : CHOISIR UN SACHET ET PLANTER.|O : ANNULER.|TRIANGLE : PELLE.|L ET R : CHANGER DE SACHET.|START : PAUSE.||LE SOLEIL SE RAMASSE EN APPROCHANT LE CURSEUR.",
      "BEWEGE DEN CURSOR MIT DEM STEUERKREUZ. LINKS IN DER ERSTEN SPALTE: SAMENBOX.||X: SAMENTÜTE WÄHLEN UND PFLANZEN.|O: ABBRECHEN.|DREIECK: SCHAUFEL.|L UND R: SAMENTÜTE WECHSELN.|START: PAUSE.||SONNEN SAMMELST DU, WENN DER CURSOR IN DER NÄHE IST.",
      "MUOVI IL CURSORE SUL PRATO CON LA CROCE. SINISTRA SULLA PRIMA COLONNA: SCATOLA DEI SEMI.||X: SCEGLI UN SEME E PIANTALO.|O: ANNULLA.|TRIANGOLO: PALA.|L E R: CAMBIA SEME.|START: PAUSA.||IL SOLE SI RACCOGLIE AVVICINANDO IL CURSORE.",
      "MOVA O CURSOR PELO GRAMADO COM O DIRECIONAL. ESQUERDA NA PRIMEIRA COLUNA: CAIXA DE SEMENTES.||X: ESCOLHER UM PACOTE E PLANTAR.|O: CANCELAR.|TRIÂNGULO: PÁ.|L E R: TROCAR DE PACOTE.|START: PAUSA.||O SOL É COLETADO AO APROXIMAR O CURSOR.",
      "MUEVE EL CURSOR POR EL CÉSPED CON LA CRUCETA. IZQUIERDA EN LA PRIMERA COLUMNA: CAJA DE SEMILLAS.||X: ELIGE UN SOBRE Y PLANTA.|O: CANCELA.|TRIÁNGULO: PALA.|L Y R: CAMBIAN DE SOBRE.|START: PAUSA.||LOS SOLES SE RECOGEN SOLOS AL PASAR EL CURSOR CERCA." },
    { "PLANTS VS. ZOMBIES|J2ME VERSION 4.6.0 BY POPCAP AND EA, PORTED TO PSP IN NATIVE C AT 60 FPS.",
      "PLANTES CONTRE ZOMBIES|VERSION J2ME 4.6.0 DE POPCAP ET EA, PORTÉE SUR PSP EN C NATIF À 60 IPS.",
      "PFLANZEN GEGEN ZOMBIES|J2ME-VERSION 4.6.0 VON POPCAP UND EA, FÜR DIE PSP IN NATIVEM C MIT 60 FPS.",
      "PIANTE CONTRO ZOMBI|VERSIONE J2ME 4.6.0 DI POPCAP ED EA, PORTATA SU PSP IN C NATIVO A 60 FPS.",
      "PLANTAS VS. ZUMBIS|VERSÃO J2ME 4.6.0 DA POPCAP E EA, PORTADA PARA PSP EM C NATIVO A 60 FPS.",
      "PLANTAS CONTRA ZOMBIS|VERSIÓN J2ME 4.6.0 DE POPCAP Y EA, PORTADA A PSP EN C NATIVO A 60 FPS." },
    { "THE GAME, ITS GRAPHICS, TEXTS AND SOUNDS BELONG TO POPCAP GAMES AND ELECTRONIC ARTS.",
      "LE JEU, SES GRAPHISMES, TEXTES ET SONS APPARTIENNENT À POPCAP GAMES ET ELECTRONIC ARTS.",
      "DAS SPIEL, SEINE GRAFIKEN, TEXTE UND SOUNDS GEHÖREN POPCAP GAMES UND ELECTRONIC ARTS.",
      "IL GIOCO, LA GRAFICA, I TESTI E I SUONI APPARTENGONO A POPCAP GAMES ED ELECTRONIC ARTS.",
      "O JOGO, SEUS GRÁFICOS, TEXTOS E SONS PERTENCEM À POPCAP GAMES E À ELECTRONIC ARTS.",
      "EL JUEGO, SUS GRÁFICOS, TEXTOS Y SONIDOS SON PROPIEDAD DE POPCAP GAMES Y ELECTRONIC ARTS." },
    { "DAY", "JOUR", "TAG", "GIORNO", "DIA", "DÍA" },
    { "NIGHT", "NUIT", "NACHT", "NOTTE", "NOITE", "NOCHE" },
    { "POOL", "PISCINE", "POOL", "PISCINA", "PISCINA", "PISCINA" },
    { "FOG", "BROUILLARD", "NEBEL", "NEBBIA", "NEBLINA", "NIEBLA" },
    { "ROOF", "TOIT", "DACH", "TETTO", "TELHADO", "TEJADO" },
    { "WALL-NUT", "NOIX", "WALLNUSS", "NOCE", "NOZ", "NUEZ" },
    { "X: REMOVE", "X : ENLEVER", "X: ENTFERNEN", "X: TOGLI", "X: TIRAR", "X: QUITAR" },
    { "X: PLAY   O: BACK", "X : JOUER   O : RETOUR", "X: SPIELEN   O: ZURÜCK", "X: GIOCA   O: INDIETRO", "X: JOGAR   O: VOLTAR", "X: JUGAR   O: VOLVER" },
    { "MINI-GAMES", "MINI-JEUX", "MINISPIELE", "MINIGIOCHI", "MINIJOGOS", "MINIJUEGOS" },
    { "WALL-NUT BOWLING", "BOWLING DE NOIX", "WALLNUSS-BOWLING", "BOWLING CON LE NOCI", "BOLICHE DE NOZES", "BOLOS CON NUECES" },
    { "PORTAL COMBAT", "COMBAT DE PORTAILS", "PORTAL-KAMPF", "BATTAGLIA DEI PORTALI", "COMBATE DE PORTAIS", "COMBATE DE PORTALES" },
    { "LAST STAND", "DERNIER COMBAT", "LETZTES GEFECHT", "ULTIMA RESISTENZA", "ÚLTIMA RESISTÊNCIA", "ÚLTIMA RESISTENCIA" },
    { "INVISI-GHOUL", "ZOMBIES INVISIBLES", "UNSICHTBARE ZOMBIES", "ZOMBI INVISIBILI", "ZUMBIS INVISÍVEIS", "ZOMBIS INVISIBLES" },
    { "ZOMBIE NIMBLE ZOMBIE QUICK", "ZOMBIES RAPIDES", "SCHNELLE ZOMBIES", "ZOMBI VELOCI", "ZUMBIS RÁPIDOS", "ZOMBIS VELOCES" },
    { "SELECT: START ONSLAUGHT", "SELECT : LANCER L'ASSAUT", "SELECT: ANGRIFF STARTEN", "SELECT: INIZIA L'ASSALTO", "SELECT: COMEÇAR O ATAQUE", "SELECT: ¡QUE EMPIECE EL ATAQUE!" },
    { "EXPLODE-O-NUT", "NOIX EXPLOSIVE", "EXPLOSIONS-NUSS", "NOCE ESPLOSIVA", "NOZ EXPLOSIVA", "NUEZ EXPLOSIVA" },
};
const char *XS(int id) { return id >= 0 && id < XS_COUNT ? xs_tab[id][g_lang] : ""; }

/* plantas de la version Tencent: nombre y ficha en los 6 idiomas (EN FR DE IT PT ES) */
static const char *const tc_names[PL_COUNT - PL_J2ME_COUNT][LANG_COUNT] = {
    { "GATLING PEA", "MITRAILLE-POIS", "GATLING-ERBSE", "MITRAGLIAPISELLI", "ERVILHA METRALHADORA", "GUISANTRALLADORA" },
    { "WINTER MELON", "MELON D'HIVER", "WINTERMELONE", "MELONE INVERNALE", "MELANCIA DE INVERNO", "MELONPULTA INVERNAL" },
    { "COB CANNON", "CANON À MAÏS", "KOLBEN-KANONE", "CANNONE A PANNOCCHIA", "CANHÃO DE MILHO", "MAZORCAÑÓN" },
    { "CATTAIL", "MASSETTE", "ROHRKOLBEN", "TIFA", "TABOA", "ESPADAÑA" },
    { "BLOVER", "TRÈFLE-VENTILO", "WINDKLEE", "QUADRIFOGLIO", "TREVENTO", "TRÉBOL" },
    { "PLANTERN", "LANTERNE", "LATERNENPFLANZE", "LANTERNA", "LANTERPLANTA", "PLANTERNA" },
    { "GARLIC", "AIL", "KNOBLAUCH", "AGLIO", "ALHO", "AJO" },
    { "PUMPKIN", "CITROUILLE", "KÜRBIS", "ZUCCA", "ABÓBORA", "CALABAZA" },
    { "GLOOM-SHROOM", "CHAMPI-SOMBRE", "TRÜBSAL-PILZ", "FUNGO TETRO", "COGUMELO SOMBRIO", "SETA MELANCÓLICA" },
    { "SPLIT PEA", "POIS CASSÉ", "SPALTERBSE", "PISELLO DIVISO", "ERVILHA PARTIDA", "GUISANTRALLA" },
};
static const char *const tc_descs[PL_COUNT - PL_J2ME_COUNT][LANG_COUNT] = {
    { "GATLING PEAS SHOOT FOUR PEAS AT A TIME.", "LE MITRAILLE-POIS TIRE QUATRE POIS À LA FOIS.", "DIE GATLING-ERBSE SCHIESST VIER ERBSEN AUF EINMAL.",
      "IL MITRAGLIAPISELLI SPARA QUATTRO PISELLI ALLA VOLTA.", "A ERVILHA METRALHADORA ATIRA QUATRO ERVILHAS DE UMA VEZ.", "LA GUISANTRALLADORA DISPARA CUATRO GUISANTES A LA VEZ." },
    { "WINTER MELONS DO HEAVY DAMAGE AND SLOW DOWN GROUPS OF ZOMBIES.", "LE MELON D'HIVER FAIT DE GROS DÉGÂTS ET RALENTIT LES GROUPES DE ZOMBIES.",
      "DIE WINTERMELONE MACHT GROSSEN SCHADEN UND VERLANGSAMT ZOMBIEGRUPPEN.", "IL MELONE INVERNALE FA MOLTI DANNI E RALLENTA I GRUPPI DI ZOMBI.",
      "A MELANCIA DE INVERNO CAUSA MUITO DANO E DEIXA OS GRUPOS DE ZUMBIS LENTOS.", "LA MELONPULTA INVERNAL HACE MUCHO DAÑO Y RALENTIZA A LOS GRUPOS DE ZOMBIS." },
    { "COB CANNON LAUNCHES DEADLY COBS ANYWHERE ON THE LAWN.|PUT THE CURSOR ON IT AND PRESS X, THEN PICK THE TARGET.",
      "LE CANON À MAÏS LANCE DES ÉPIS MORTELS N'IMPORTE OÙ SUR LA PELOUSE.|METS LE CURSEUR DESSUS, APPUIE SUR X ET CHOISIS LA CIBLE.",
      "DIE KOLBEN-KANONE SCHIESST TÖDLICHE KOLBEN ÜBERALL HIN.|CURSOR DARAUF, X DRÜCKEN UND DAS ZIEL WÄHLEN.",
      "IL CANNONE A PANNOCCHIA LANCIA PANNOCCHIE MORTALI OVUNQUE.|METTI IL CURSORE SOPRA, PREMI X E SCEGLI IL BERSAGLIO.",
      "O CANHÃO DE MILHO LANÇA ESPIGAS MORTAIS EM QUALQUER LUGAR.|PONHA O CURSOR NELE, APERTE X E ESCOLHA O ALVO.",
      "EL MAZORCAÑÓN LANZA MAZORCAS EXPLOSIVAS A CUALQUIER PARTE DEL JARDÍN.|PON EL CURSOR ENCIMA, PULSA X Y ELIGE EL BLANCO." },
    { "CATTAILS SHOOT SPIKES AT ANY ZOMBIE AND POP BALLOONS. ONLY IN WATER.", "LA MASSETTE TIRE DES PIQUES SUR N'IMPORTE QUEL ZOMBIE. SEULEMENT DANS L'EAU.",
      "DER ROHRKOLBEN SCHIESST STACHELN AUF JEDEN ZOMBIE. NUR IM WASSER.", "LA TIFA SPARA SPINE A QUALSIASI ZOMBI E FA SCOPPIARE I PALLONCINI. SOLO IN ACQUA.",
      "A TABOA ATIRA ESPINHOS EM QUALQUER ZUMBI E ESTOURA BALÕES. SÓ NA ÁGUA.", "LA ESPADAÑA DISPARA PINCHOS A CUALQUIER ZOMBI Y REVIENTA GLOBOS. SOLO EN EL AGUA." },
    { "BLOVER BLOWS AWAY ALL BALLOON ZOMBIES AND THE FOG.", "LE TRÈFLE-VENTILO SOUFFLE LES ZOMBIES À BALLON ET LE BROUILLARD.",
      "DER WINDKLEE PUSTET ALLE BALLONZOMBIES UND DEN NEBEL WEG.", "IL QUADRIFOGLIO SOFFIA VIA GLI ZOMBI COL PALLONCINO E LA NEBBIA.",
      "O TREVENTO SOPRA PARA LONGE OS ZUMBIS DE BALÃO E A NEBLINA.", "EL TRÉBOL SOPLA A TODOS LOS ZOMBIS CON GLOBO Y LA NIEBLA." },
    { "PLANTERNS LIGHT UP AN AREA, LETTING YOU SEE THROUGH THE FOG.", "LA LANTERNE ÉCLAIRE UNE ZONE À TRAVERS LE BROUILLARD.",
      "DIE LATERNENPFLANZE ERHELLT EINEN BEREICH IM NEBEL.", "LA LANTERNA ILLUMINA UNA ZONA E FA VEDERE ATTRAVERSO LA NEBBIA.",
      "A LANTERPLANTA ILUMINA UMA ÁREA E DEIXA VER ATRAVÉS DA NEBLINA.", "LA PLANTERNA ILUMINA UNA ZONA Y DEJA VER A TRAVÉS DE LA NIEBLA." },
    { "GARLIC DIVERTS ZOMBIES INTO OTHER LANES.", "L'AIL ENVOIE LES ZOMBIES DANS D'AUTRES RANGÉES.", "KNOBLAUCH SCHICKT ZOMBIES IN ANDERE REIHEN.",
      "L'AGLIO DEVIA GLI ZOMBI IN ALTRE FILE.", "O ALHO DESVIA OS ZUMBIS PARA OUTRAS FILEIRAS.", "EL AJO DESVÍA A LOS ZOMBIS A OTRAS FILAS." },
    { "PUMPKINS PROTECT PLANTS THAT ARE WITHIN THEIR SHELLS.", "LA CITROUILLE PROTÈGE LA PLANTE QUI EST DEDANS.", "DER KÜRBIS SCHÜTZT DIE PFLANZE IN SEINER SCHALE.",
      "LA ZUCCA PROTEGGE LA PIANTA CHE STA AL SUO INTERNO.", "A ABÓBORA PROTEGE A PLANTA QUE ESTÁ DENTRO DELA.", "LA CALABAZA PROTEGE A LA PLANTA QUE TIENE DENTRO." },
    { "GLOOM-SHROOMS RELEASE HEAVY FUMES ALL AROUND THEMSELVES. SLEEPS DURING THE DAY.",
      "LE CHAMPI-SOMBRE LIBÈRE DE LOURDES FUMÉES TOUT AUTOUR DE LUI. DORT LE JOUR.",
      "DER TRÜBSAL-PILZ STÖSST RINGSUM DICHTE DÄMPFE AUS. SCHLÄFT TAGSÜBER.",
      "IL FUNGO TETRO SPRIGIONA FUMI PESANTI TUTTO INTORNO. DORME DI GIORNO.",
      "O COGUMELO SOMBRIO SOLTA FUMAÇA PESADA AO SEU REDOR. DORME DE DIA.",
      "LA SETA MELANCÓLICA SUELTA HUMO DENSO A SU ALREDEDOR. DUERME DE DÍA." },
    { "SPLIT PEAS SHOOT FORWARD AND BACKWARD.|DAMAGE: NORMAL|RANGE: FORWARD AND BACKWARD|FIRING SPEED: 1X FORWARD, 2X BACKWARD",
      "LE POIS CASSÉ TIRE DEVANT ET DERRIÈRE.|DÉGÂTS : NORMAUX|PORTÉE : DEVANT ET DERRIÈRE|CADENCE : 1X DEVANT, 2X DERRIÈRE",
      "DIE SPALTERBSE SCHIESST NACH VORNE UND NACH HINTEN.|SCHADEN: NORMAL|REICHWEITE: VORNE UND HINTEN|FEUERRATE: 1X VORNE, 2X HINTEN",
      "IL PISELLO DIVISO SPARA IN AVANTI E ALL'INDIETRO.|DANNO: NORMALE|RAGGIO: AVANTI E INDIETRO|CADENZA: 1X AVANTI, 2X INDIETRO",
      "A ERVILHA PARTIDA ATIRA PARA A FRENTE E PARA TRÁS.|DANO: NORMAL|ALCANCE: FRENTE E TRÁS|CADÊNCIA: 1X FRENTE, 2X TRÁS",
      "LA GUISANTRALLA DISPARA HACIA DELANTE Y HACIA ATRÁS.|DAÑO: NORMAL|ALCANCE: DELANTE Y DETRÁS|VELOCIDAD DE DISPARO: 1X DELANTE, 2X DETRÁS" },
};
const char *plant_name(int t)
{
    if (t == PL_BOWLNUT || t == PL_REDNUT) t = PL_WALLNUT;
    if (t < 0 || t >= PL_COUNT) return "";
    return t < PL_J2ME_COUNT ? TXT[plant_txt_name[t]] : tc_names[t - PL_J2ME_COUNT][g_lang];
}
const char *plant_desc(int t)
{
    if (t < 0 || t >= PL_COUNT) return "";
    return t < PL_J2ME_COUNT ? TXT[plant_txt_desc[t]] : tc_descs[t - PL_J2ME_COUNT][g_lang];
}

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
    if (txt) {                                    /* si el nombre no cabe se encoge para quedar dentro */
        float room = w - 2 * ew + 16, tw = text_width(font, txt);
        if (tw > room) text_scale = room / tw;
        float th = 23 * text_scale;
        text_draw_centered(font, x + w / 2, y + (img_h(end) - th) / 2 + 2 * text_scale, txt, 0xFFE8E8E8);
        text_scale = 1;
    }
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
        gfx_draw_ex(edge, x + 21, ty + 21, 21, 21, 1, 1, -1.5707963f, WHITE, GFX_NEAREST);
        gfx_draw_ex(edge, x + w - 21, ty + 21, 21, 21, 1, 1, 1.5707963f, WHITE, GFX_NEAREST);
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
