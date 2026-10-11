/*
 * gl_gu.cpp - Las ~40 funciones de OpenGL ES 2 que usa GLInterface.cpp, hechas con sceGu de la PSP.
 * El shader del juego es fijo (color * textura, mezcla normal o aditiva), asi que se emula sin shaders.
 * La pantalla logica de 800x600 se ve entera (escala 272/600, con bandas a los lados) o, con zoom, a escala
 * 0.6 (480x360) con una ventana de 272 de alto que sigue al cursor (PspSetCameraY).
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspmoduleinfo.h>
#include <pspgu.h>
#include <psputility.h>
#include <psppower.h>
#include <malloc.h>
#include <pspsysmem.h>
#include <pspthreadman.h>
#include <pspintrman.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <algorithm>
#include <pspiofilemgr.h>
#include <math.h>
#include <vector>
#include "graphics/GLPlatform.h"

PSP_HEAP_SIZE_KB(-2048);     /* toda la memoria menos 2 MB para hilos */

#define BUF_W 512
#define SCR_W 480
#define SCR_H 272
static unsigned int __attribute__((aligned(64))) gList[512 * 1024];   /* 2 MB de lista de dibujo */
static bool gFrameOpen = false;
static volatile int gSwaps = 0, gUploads = 0, gDraws = 0;
volatile int gPspWhereLine = 0, gPspCalls = 0; volatile const char *gPspWhereFile = "";
/* modo de vista: 0 = 16:9 (800x600 estirado a 480x272), 1 = 4:3 con bandas, 2 = zoom 0.6 siguiendo al cursor */
static int gViewMode = 0;
static float gScaleX = 0.6f, gScaleY = 272.0f / 600.0f, gCamY = 0, gOffX = 0;

struct Tex { int ow = 0, oh = 0, w = 0, h = 0, psm = GU_PSM_8888, filter = GU_NEAREST, wrap = GU_CLAMP; void *data = nullptr; int bytes = 0; };
#define MAX_TEX 16384
static Tex gTex[MAX_TEX];          /* fijo: el hilo de carga crea texturas mientras se dibuja */
static volatile int gTexCount = 1;
static int gFreeIds[MAX_TEX], gFreeN = 0;
static long gTexBytes = 0;   /* memoria de texturas (para el informe) */
static thread_local GLuint gBoundTex = 0;   /* cada hilo la suya */
static int gUniform[8];                     /* 3 useTexture, 6 mixedBlend */
static float gMtx[16];
static int gVp[4] = { 0, 0, 800, 600 };
static GLenum gBlendS = GL_SRC_ALPHA, gBlendD = GL_ONE_MINUS_SRC_ALPHA;
static std::vector<unsigned char> gVbo;
static unsigned int gClear = 0xFF000000;

void PspSetCameraY(float y) { gCamY = gViewMode == 2 ? y : 0; }
void PspSetViewMode(int mode)
{
	gViewMode = mode % 3;
	gScaleX = gViewMode == 1 ? 272.0f / 600.0f : 0.6f;
	gScaleY = gViewMode == 2 ? 0.6f : 272.0f / 600.0f;
	gOffX = gViewMode == 1 ? (480 - 800 * gScaleX) / 2 : 0;
	if (gViewMode != 2) gCamY = 0;
}
int PspGetViewMode() { return gViewMode; }
float PspGetCameraY() { return gCamY; }

#ifdef PSP_MEMLOG
static uint64_t gTBegin, gTLastSwap, gAccDraw, gAccSync, gAccWait, gAccTotal;
#endif
/* estado de la GU ya enviado en esta lista (se olvida al empezar cada lista) */
static const void *gLastTexData = nullptr;
static int gLastPsm = -1, gLastFilter = -1, gLastWrap = -1, gLastTexOn = -1, gLastBlend = -1;
static void ForgetGuState() { gLastTexData = nullptr; gLastPsm = gLastFilter = gLastWrap = gLastTexOn = gLastBlend = -1; }
/* proyeccion ortografica: coordenadas en pixeles de la pantalla (0,0 arriba a la izquierda), z = 0 */
static void SetupMatrices()
{
	static ScePspFMatrix4 proj = {
		{ 2.0f / 480, 0, 0, 0 }, { 0, -2.0f / 272, 0, 0 }, { 0, 0, -1, 0 }, { -1, 1, 0, 1 } };
	static ScePspFMatrix4 ident = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
	sceGuSetMatrix(GU_PROJECTION, &proj);
	sceGuSetMatrix(GU_VIEW, &ident);
	sceGuSetMatrix(GU_MODEL, &ident);
	sceGuSetMatrix(GU_TEXTURE, &ident);
	sceGuTexMapMode(GU_TEXTURE_COORDS, 0, 0);
	sceGuTexScale(1.0f, 1.0f);
	sceGuTexOffset(0.0f, 0.0f);
	sceGuDepthRange(65535, 0);
	sceGuEnable(GU_CLIP_PLANES);
}
static unsigned int gSwapV = 0;   /* refresco en el que se pidio el ultimo cambio de buffer */
static void FrameBegin()
{
	if (gFrameOpen) return;
	/* no dibujar en el buffer que aun se ve: el cambio pedido se hace en el siguiente refresco */
	while (sceDisplayGetVcount() == gSwapV) sceDisplayWaitVblankStart();
#ifdef PSP_MEMLOG
	gTBegin = sceKernelGetSystemTimeWide();
#endif
	sceGuStart(GU_DIRECT, gList);
	ForgetGuState();
	SetupMatrices();
	gFrameOpen = true;
}

void PspGuInit()
{
	scePowerSetClockFrequency(333, 333, 166);   /* la PSP arranca a 222 MHz */
	sceGuInit();
	sceGuStart(GU_DIRECT, gList);
	sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
	sceGuDispBuffer(SCR_W, SCR_H, (void *)(BUF_W * SCR_H * 4), BUF_W);
	sceGuOffset(2048 - SCR_W / 2, 2048 - SCR_H / 2);
	sceGuViewport(2048, 2048, SCR_W, SCR_H);
	sceGuScissor(0, 0, SCR_W, SCR_H);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuFinish();
	sceGuSync(0, 0);
	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);
	gFrameOpen = false;
	void PspStartWatchdog(); PspStartWatchdog();
}

extern int gPspCursorX, gPspCursorY;
/* seleccion (control de consola): marco amarillo que late alrededor de lo elegido; sin ella, flecha */
static bool gFocusOn = false;
static int gFocus[4];
void PspSetFocus(bool on, int x, int y, int w, int h) { gFocusOn = on; gFocus[0] = x; gFocus[1] = y; gFocus[2] = w; gFocus[3] = h; }
static void DrawFocus()
{
	if (gFocus[2] <= 0 || gFocus[3] <= 0) return;
	float x0 = gFocus[0] * gScaleX + gOffX, y0 = gFocus[1] * gScaleY - gCamY;
	float x1 = (gFocus[0] + gFocus[2]) * gScaleX + gOffX, y1 = (gFocus[1] + gFocus[3]) * gScaleY - gCamY;
	float t = (sceKernelGetSystemTimeLow() % 1000000) / 1000000.0f;
	unsigned a = 160 + (unsigned)(95 * fabsf(1 - 2 * t));
	uint32_t c = (a << 24) | 0x0030E0FF;   /* amarillo */
	struct V { uint32_t c; float x, y, z; };
	V *v = (V *)sceGuGetMemory(8 * sizeof(V));
	const float k = 2;
	v[0] = { c, x0 - k, y0 - k, 0 }; v[1] = { c, x1 + k, y0, 0 };        /* arriba */
	v[2] = { c, x0 - k, y1, 0 };     v[3] = { c, x1 + k, y1 + k, 0 };    /* abajo */
	v[4] = { c, x0 - k, y0, 0 };     v[5] = { c, x0, y1, 0 };            /* izquierda */
	v[6] = { c, x1, y0, 0 };         v[7] = { c, x1 + k, y1, 0 };        /* derecha */
	sceGuDisable(GU_TEXTURE_2D);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 8, 0, v);
}
static void DrawCursor()
{
	if (gFocusOn) { DrawFocus(); return; }
	/* flecha blanca con borde negro en la posicion del cursor virtual */
	float x = gPspCursorX * gScaleX + gOffX, y = gPspCursorY * gScaleY - gCamY;
	struct V { uint32_t c; float x, y, z; };
	V *v = (V *)sceGuGetMemory(6 * sizeof(V));
	v[0] = { 0xFF000000, x - 1, y - 2, 0 }; v[1] = { 0xFF000000, x - 1, y + 14, 0 }; v[2] = { 0xFF000000, x + 11, y + 11, 0 };
	v[3] = { 0xFFFFFFFF, x, y, 0 };         v[4] = { 0xFFFFFFFF, x, y + 11, 0 };     v[5] = { 0xFFFFFFFF, x + 8, y + 9, 0 };
	sceGuDisable(GU_TEXTURE_2D);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 6, 0, v);
}

/* registro de rendimiento para probar en la consola (encendido; L + R + START lo apaga y enciende): cada segundo
 * escribe en rendimiento.txt (junto a SDL_Log.txt) los fps, las actualizaciones, el peor fotograma, la memoria y
 * la pantalla */
namespace Sexy { std::string GetAppDataFolder(); }
static bool gPspLogOn = true;   /* encendido desde el arranque; L + R + START lo apaga */
static const char *gPspScreen = "";
static float gWorstFrameMs = 0;
static void PspLogLine(const char *fmt, ...)
{
	if (!gPspLogOn) return;
	/* junto a SDL_Log.txt (la carpeta del juego); si ahi no se puede, en la carpeta de las partidas */
	static int sWhere = 0;   /* 0 = sin probar, 1 = carpeta del juego, 2 = carpeta de las partidas */
	FILE *f = nullptr;
	if (sWhere != 2) { f = fopen("rendimiento.txt", "a"); if (f) sWhere = 1; }
	if (!f) { std::string path = Sexy::GetAppDataFolder() + "rendimiento.txt"; f = fopen(path.c_str(), "a"); if (f) sWhere = 2; }
	if (!f) return;
	static bool sHeader = false;
	if (!sHeader) {
		sHeader = true;
		struct mallinfo mi = mallinfo();
		fprintf(f, "=== arranque (memoria usada %d KB, libre del sistema %d KB) ===\n"
			"segundo, FPS, L = actualizaciones del juego por segundo (lo normal 100), peor = fotograma mas lento en ms, memoria, pantalla\n",
			mi.uordblks / 1024, (int)(sceKernelTotalFreeMemSize() / 1024));
	}
	va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
	fputc('\n', f);
	fclose(f);
}
void PspLogToggle()
{
	if (gPspLogOn) { PspLogLine("--- registro apagado ---"); gPspLogOn = false; return; }
	gPspLogOn = true;
	PspLogLine("--- registro encendido (segundo, FPS, L = actualizaciones/s de 100, peor = fotograma mas lento en ms, memoria, pantalla) ---");
}
void PspLogScreen(const char *name)
{
	if (strcmp(name, gPspScreen) == 0) return;
	gPspScreen = name;
	PspLogLine("%6.1f s  >> %s", sceKernelGetSystemTimeWide() / 1e6, name);
}

/* contador de fps real (arriba a la izquierda): fotogramas dibujados y actualizaciones de la logica del
 * juego por segundo (el juego va a 100 por segundo; si baja, el juego va lento) */
volatile int gPspLogicUpdates = 0;
static void DrawFpsCounter()
{
	static uint64_t sT0 = 0; static int sFrames = 0, sFps = 0, sUps = 0, sU0 = 0;
	uint64_t now = sceKernelGetSystemTimeWide();
	sFrames++;
	if (!sT0) { sT0 = now; sU0 = gPspLogicUpdates; }
	if (now - sT0 >= 1000000) {
		sFps = (int)((sFrames * 1000000ull + (now - sT0) / 2) / (now - sT0));
		sUps = (int)(((gPspLogicUpdates - sU0) * 1000000ull + (now - sT0) / 2) / (now - sT0));
		sT0 = now; sFrames = 0; sU0 = gPspLogicUpdates;
		if (gPspLogOn) {
			struct mallinfo mi = mallinfo();
			PspLogLine("%6.1f s  FPS %2d  L %3d  peor %4.0f ms  memoria %5d KB (texturas %5ld KB)  %s",
				now / 1e6, sFps, sUps, gWorstFrameMs, mi.uordblks / 1024, gTexBytes / 1024, gPspScreen);
		}
		gWorstFrameMs = 0;
	}
	/* letras de 3x5 */
	static const unsigned short font[] = {
		0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF,   /* 0-9 */
		0x79E4, 0x7BE4, 0x79CF, 0x4927 };                                                    /* F P S L */
	char txt[24]; snprintf(txt, sizeof(txt), "FPS%3d L%3d", sFps, sUps);
	struct V { uint32_t c; float x, y, z; };
	int n = 0; for (const char *p = txt; *p; p++) n++;
	V *v = (V *)sceGuGetMemory((4 + n * 15 * 2) * sizeof(V));
	int k = 0;
	v[k++] = { 0xA0000000, 2, 2, 0 }; v[k++] = { 0xA0000000, 4.0f + n * 8, 16, 0 };
	for (int i = 0; i < n; i++) {
		char c = txt[i]; int g = c >= '0' && c <= '9' ? c - '0' : c == 'F' ? 10 : c == 'P' ? 11 : c == 'S' ? 12 : c == 'L' ? 13 : -1;
		if (g < 0) continue;
		for (int b = 0; b < 15; b++) if (font[g] & (0x4000 >> b)) {
			float x = 4 + i * 8 + (b % 3) * 2, y = 4 + (b / 3) * 2;
			uint32_t col = sFps >= 25 ? 0xFF40FF40 : sFps >= 15 ? 0xFF40FFFF : 0xFF4040FF;
			v[k++] = { col, x, y, 0 }; v[k++] = { col, x + 2, y + 2, 0 };
		}
	}
	if (gPspLogOn) { v[k++] = { 0xFF2020FF, 6.0f + n * 8, 4, 0 }; v[k++] = { 0xFF2020FF, 12.0f + n * 8, 14, 0 }; }   /* registrando */
	sceGuDisable(GU_TEXTURE_2D);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, k, 0, v);
}

void PspMemReport(const char *where)
{
#ifndef PSP_MEMLOG  /* sin -DPSP_MEMLOG solo se anota la falta de memoria */
	if (strcmp(where, "SIN MEMORIA") != 0) return;
#endif
	struct mallinfo mi = mallinfo();
	FILE *f = fopen("mem.log", "a");
	if (f) { fprintf(f, "MEM %s: heap usado %d KB (de %d), texturas %ld KB, libre del sistema %d KB\n", where, mi.uordblks / 1024, mi.arena / 1024, gTexBytes / 1024, (int)(sceKernelTotalFreeMemSize() / 1024)); fclose(f); }
}

void PspGuFrameForOsk()
{
	FrameBegin();
	sceGuClearColor(0xFF203020); sceGuClear(GU_COLOR_BUFFER_BIT);
	sceGuFinish(); sceGuSync(0, 0);
	sceUtilityOskUpdate(1);
	sceDisplayWaitVblankStart(); sceGuSwapBuffers();
	gFrameOpen = false; gSwaps++;
}

void PspStartWatchdog();
void PspSwap()
{
	{   /* fotograma mas lento de cada segundo (para el registro) */
		static uint64_t sLastSwap = 0;
		uint64_t t = sceKernelGetSystemTimeWide();
		if (sLastSwap) gWorstFrameMs = std::max(gWorstFrameMs, (t - sLastSwap) / 1000.0f);
		sLastSwap = t;
	}
	static int sFrames;
	gSwaps++;
	if (++sFrames % 120 == 0) PspMemReport("frame");
	FrameBegin();
	DrawCursor();
	DrawFpsCounter();
#ifdef PSP_MEMLOG
	uint64_t t0 = sceKernelGetSystemTimeWide();
#endif
	sceGuFinish();
	sceGuSync(0, 0);
#ifdef PSP_MEMLOG
	uint64_t t1 = sceKernelGetSystemTimeWide();
#endif
#ifdef PSP_SHOT_EVERY
	{   /* pruebas: guarda el fotograma cada PSP_SHOT_EVERY como shotNNNNNNN.raw (480x272 RGBA) */
		static int n;
		if (++n % PSP_SHOT_EVERY == 0) {
			char name[32]; snprintf(name, sizeof(name), "shot%07d.raw", n);
			SceUID f = sceIoOpen(name, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
			void *fb; int bw, pf; sceDisplayGetFrameBuf(&fb, &bw, &pf, 0);
			/* el que se acaba de dibujar es el de atras: el otro buffer */
			const uint32_t *p = (const uint32_t *)(0x44000000 | (((uintptr_t)fb & 0x1FFFFF) == 0 ? BUF_W * SCR_H * 4 : 0));
			if (f >= 0) { for (int y = 0; y < SCR_H; y++) sceIoWrite(f, p + y * BUF_W, SCR_W * 4); sceIoClose(f); }
		}
	}
#endif
	/* tope de 30 fps: se espera solo si el fotograma llego antes de 2 refrescos; el cambio de buffer se hace
	 * en el siguiente refresco (sin cortes) y mientras tanto la CPU sigue con la logica del juego */
	{
		static unsigned int sLastV = 0;
		while (sceDisplayGetVcount() - sLastV < 2) sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
		sLastV = gSwapV = sceDisplayGetVcount();
	}
	gFrameOpen = false;
#ifdef PSP_MEMLOG
	{
		uint64_t t2 = sceKernelGetSystemTimeWide();
		gAccDraw += t0 - gTBegin; gAccSync += t1 - t0; gAccWait += t2 - t1; gAccTotal += t2 - gTLastSwap; gTLastSwap = t2;
		if (sFrames % 120 == 0) {
			FILE *f = fopen("mem.log", "a");
			if (f) { fprintf(f, "TIEMPO fps=%.1f dibujo=%.1fms gpu=%.1fms espera=%.1fms resto=%.1fms\n", 120e6 / gAccTotal, gAccDraw / 120e3, gAccSync / 120e3, gAccWait / 120e3, (gAccTotal - gAccDraw - gAccSync - gAccWait) / 120e3); fclose(f); }
			gAccDraw = gAccSync = gAccWait = gAccTotal = 0;
		}
	}
#endif
}

/* ---- texturas ---- */
static void psp_glGenTextures(GLsizei n, GLuint *ids)
{
	int st = sceKernelCpuSuspendIntr();
	for (int i = 0; i < n; i++) ids[i] = gFreeN ? gFreeIds[--gFreeN] : (gTexCount < MAX_TEX ? gTexCount++ : 0);
	sceKernelCpuResumeIntr(st);
}
static void psp_glDeleteTextures(GLsizei n, const GLuint *ids)
{
	for (int i = 0; i < n; i++) if (ids[i] > 0 && ids[i] < MAX_TEX) {
		if (gTex[ids[i]].data) gTexBytes -= gTex[ids[i]].bytes;
		free(gTex[ids[i]].data); gTex[ids[i]] = Tex();
		int st = sceKernelCpuSuspendIntr(); gFreeIds[gFreeN++] = ids[i]; sceKernelCpuResumeIntr(st);
	}
}
static void psp_glBindTexture(GLenum, GLuint id) { gBoundTex = id; }
static void psp_glActiveTexture(GLenum) {}
static void psp_glTexParameteri(GLenum, GLenum pname, GLint v)
{
	Tex &t = gTex[gBoundTex];
	if (pname == GL_TEXTURE_WRAP_S) t.wrap = v == GL_REPEAT ? GU_REPEAT : GU_CLAMP;
	if (pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MAG_FILTER) t.filter = v == GL_LINEAR ? GU_LINEAR : GU_NEAREST;
}
static int TexStride(int w) { return (w + 7) & ~7; }
/* Las texturas se guardan a mitad de resolucion (las UV de GL van de 0 a 1, asi que el juego no lo nota) y en
 * 16 bits: 5650 si es opaca, 5551 si su alfa es todo o nada y 4444 si no. Un octavo de memoria que en RGBA. */
static void Upload(const void *pixels, int w, int h, GLenum format, GLenum type)
{
	Tex &t = gTex[gBoundTex];
	(void)format;
	gUploads++;
	/* a RGBA 8888 */
	std::vector<uint32_t> src(w * h);
	if (!pixels) memset(src.data(), 0, src.size() * 4);
	else if (type == GL_UNSIGNED_BYTE) memcpy(src.data(), pixels, src.size() * 4);
	else {
		const uint16_t *s = (const uint16_t *)pixels;
		for (int i = 0; i < w * h; i++) {
			uint16_t p = s[i]; uint32_t r, g, b, a;
			if (type == GL_UNSIGNED_SHORT_4_4_4_4) { r = (p >> 12) * 17; g = ((p >> 8) & 15) * 17; b = ((p >> 4) & 15) * 17; a = (p & 15) * 17; }
			else { r = (p >> 11) * 255 / 31; g = ((p >> 5) & 63) * 255 / 63; b = (p & 31) * 255 / 31; a = 255; }
			src[i] = r | (g << 8) | (b << 16) | (a << 24);
		}
	}
	/* mitad de resolucion (2x2 con alfa ponderado) en las grandes */
	int dw = w, dh = h;
	if (w >= 32 && h >= 32) {
		dw = w / 2; dh = h / 2;
		for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
			uint32_t q[4] = { src[(2 * y) * w + 2 * x], src[(2 * y) * w + 2 * x + 1], src[(2 * y + 1) * w + 2 * x], src[(2 * y + 1) * w + 2 * x + 1] };
			unsigned r = 0, g = 0, b = 0, a = 0;
			for (int k = 0; k < 4; k++) { unsigned qa = q[k] >> 24; a += qa; r += (q[k] & 255) * qa; g += ((q[k] >> 8) & 255) * qa; b += ((q[k] >> 16) & 255) * qa; }
			uint32_t o = 0;
			if (a) o = (r / a) | ((g / a) << 8) | ((b / a) << 16) | ((a / 4) << 24);
			src[y * dw + x] = o;
		}
	}
	int opaque = 1, binary = 1;
	for (int i = 0; i < dw * dh; i++) { unsigned a = src[i] >> 24; if (a != 255) opaque = 0; if (a != 0 && a != 255) binary = 0; }
	int psm = opaque ? GU_PSM_5650 : binary ? GU_PSM_5551 : GU_PSM_4444;
	/* la GU real exige filas de al menos 8 pixeles (16 bytes): las texturas mas estrechas se rellenan */
	int stride = TexStride(dw);
	int bytes = stride * dh * 2;
	uint16_t *d = (uint16_t *)memalign(16, bytes);
	if (!d) return;
	memset(d, 0, bytes);
	for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
		uint32_t p = src[y * dw + x]; unsigned r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255, a = p >> 24;
		uint16_t &o = d[y * stride + x];
		if (psm == GU_PSM_5650) o = (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);
		else if (psm == GU_PSM_5551) o = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | ((a >> 7) << 15);
		else o = (r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8) | ((a >> 4) << 12);
	}
	sceKernelDcacheWritebackRange(d, bytes);
	/* se publica de una vez para que el hilo de dibujo nunca vea una textura a medias */
	void *old = t.data; int oldBytes = old ? t.bytes : 0;
	t.ow = w; t.oh = h; t.w = dw; t.h = dh; t.psm = psm; t.bytes = bytes; t.data = d;
	gTexBytes += bytes - oldBytes;
	free(old);
	ForgetGuState();   /* la memoria nueva podria caer en la misma direccion que la vieja */
}
/* cache de texturas (GLInterface.cpp): leer y poner los datos ya convertidos de una textura */
bool PspTexGetRaw(GLuint id, int *ow, int *oh, int *w, int *h, int *psm, const void **data, int *bytes)
{
	if (id <= 0 || id >= MAX_TEX || !gTex[id].data) return false;
	const Tex &t = gTex[id];
	*ow = t.ow; *oh = t.oh; *w = t.w; *h = t.h; *psm = t.psm; *data = t.data; *bytes = t.bytes;
	return true;
}
void *PspTexAlloc(int bytes) { return memalign(16, bytes); }
void PspTexSetRaw(GLuint id, int ow, int oh, int w, int h, int psm, void *data, int bytes)
{
	if (id <= 0 || id >= MAX_TEX) { free(data); return; }
	Tex &t = gTex[id];
	sceKernelDcacheWritebackRange(data, bytes);
	void *old = t.data; int oldBytes = old ? t.bytes : 0;
	t.ow = ow; t.oh = oh; t.w = w; t.h = h; t.psm = psm; t.bytes = bytes; t.data = data;
	gTexBytes += bytes - oldBytes;
	free(old);
	ForgetGuState();   /* la memoria nueva podria caer en la misma direccion que la vieja */
}
static void psp_glTexImage2D(GLenum, GLint, GLint, GLsizei w, GLsizei h, GLint, GLenum format, GLenum type, const void *pixels) { Upload(pixels, w, h, format, type); }
static void psp_glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei w, GLsizei h, GLenum format, GLenum type, const void *pixels) { Upload(pixels, w, h, format, type); }

/* ---- "shaders" ---- */
static GLuint psp_glCreateShader(GLenum) { return 1; }
static void psp_glShaderSource(GLuint, GLsizei, const GLchar *const *, const GLint *) {}
static void psp_glCompileShader(GLuint) {}
static void psp_glGetShaderiv(GLuint, GLenum pname, GLint *p) { *p = pname == GL_COMPILE_STATUS ? 1 : 0; }
static void psp_glGetShaderInfoLog(GLuint, GLsizei, GLsizei *l, GLchar *s) { if (l) *l = 0; if (s) *s = 0; }
static void psp_glDeleteShader(GLuint) {}
static GLuint psp_glCreateProgram() { return 1; }
static void psp_glAttachShader(GLuint, GLuint) {}
static void psp_glBindAttribLocation(GLuint, GLuint, const GLchar *) {}
static void psp_glLinkProgram(GLuint) {}
static void psp_glUseProgram(GLuint) {}
static GLint psp_glGetUniformLocation(GLuint, const GLchar *n)
{
	static const char *names[] = { "", "u_viewProj", "u_texture", "u_useTexture", "u_uvBounds", "u_clampUvEnabled", "u_mixedBlend" };
	for (int i = 1; i < 7; i++) if (!strcmp(n, names[i])) return i;
	return -1;
}
static void psp_glUniform1i(GLint loc, GLint v) { if (loc >= 0 && loc < 8) gUniform[loc] = v; }
static void psp_glUniform4fv(GLint, GLsizei, const GLfloat *) {}
static void psp_glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat *m) { memcpy(gMtx, m, sizeof(gMtx)); }

/* ---- buffers y dibujo ---- */
struct GLV { float sx, sy, sz; uint32_t color; float tu, tv; };
struct GuV { float u, v; uint32_t color; float x, y, z; };
static void psp_glGenBuffers(GLsizei n, GLuint *ids) { for (int i = 0; i < n; i++) ids[i] = 1; }
static void psp_glBindBuffer(GLenum, GLuint) {}
static void psp_glBufferData(GLenum, GLsizeiptr size, const void *data, GLenum)
{
	if ((size_t)size > gVbo.size()) gVbo.resize(size);
	if (data) memcpy(gVbo.data(), data, size);
}
static void psp_glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *) {}
static void psp_glEnableVertexAttribArray(GLuint) {}
static void psp_glBlendFunc(GLenum s, GLenum d) { gBlendS = s; gBlendD = d; }
static void SetBlend(bool additive)
{
	if (additive || gBlendD == GL_ONE) sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
	else sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
}
static void DrawRun(int prim, const GLV *v, int n, bool additive)
{
	if (n <= 0) return;
	gDraws++;
	if (sceGuCheckList() > (int)sizeof(gList) - 256 * 1024 - n * (int)sizeof(GuV)) {   /* lista casi llena: se envia y se sigue */
		sceGuFinish(); sceGuSync(0, 0); sceGuStart(GU_DIRECT, gList); ForgetGuState(); SetupMatrices();
	}
	GuV *out = (GuV *)sceGuGetMemory(n * sizeof(GuV));
	Tex *t = gUniform[3] && gBoundTex > 0 && gBoundTex < MAX_TEX && gTex[gBoundTex].data ? &gTex[gBoundTex] : nullptr;
	float sx = gScaleX * 800.0f / gVp[2], sy = gScaleY * 600.0f / gVp[3];
	for (int i = 0; i < n; i++) {
		/* coordenadas logicas -> ndc con la matriz ortografica -> pixeles de la PSP */
		float nx = gMtx[0] * v[i].sx + gMtx[4] * v[i].sy + gMtx[12];
		float ny = gMtx[1] * v[i].sx + gMtx[5] * v[i].sy + gMtx[13];
		out[i].x = ((nx + 1) * 0.5f * gVp[2] + gVp[0]) * sx + gOffX;
		out[i].y = ((1 - ny) * 0.5f * gVp[3] + gVp[1]) * sy - gCamY;
		out[i].z = 0;
		out[i].color = v[i].color;
		out[i].u = t ? v[i].tu : 0;   /* modo 3D: coordenadas de textura normalizadas (0..1) */
		out[i].v = t ? v[i].tv : 0;
	}
	/* solo se manda a la GU lo que cambia: muchas piezas seguidas usan la misma textura y mezcla */
	if (t) {
		if (gLastTexOn != 1) { sceGuEnable(GU_TEXTURE_2D); gLastTexOn = 1; }
		if (gLastTexData != t->data || gLastPsm != t->psm) {
			sceGuTexMode(t->psm, 0, 0, 0);
			sceGuTexImage(0, t->w, t->h, TexStride(t->w), t->data);
			sceGuTexFlush();
			gLastTexData = t->data; gLastPsm = t->psm;
		}
		if (gLastFilter != t->filter) { sceGuTexFilter(t->filter, t->filter); gLastFilter = t->filter; }
		if (gLastWrap != t->wrap) { sceGuTexWrap(t->wrap, t->wrap); gLastWrap = t->wrap; }
	} else if (gLastTexOn != 0) { sceGuDisable(GU_TEXTURE_2D); gLastTexOn = 0; }
	int blend = (additive || gBlendD == GL_ONE) ? 1 : 0;
	if (gLastBlend != blend) { SetBlend(additive); gLastBlend = blend; }
	/* modo 3D con proyeccion ortografica en pixeles: la GU recorta lo que sale de la pantalla (en modo 2D las
	 * coordenadas negativas no valen en la PSP real: dan la vuelta y salen triangulos enormes) */
	sceGuDrawArray(prim, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, n, 0, out);
}
static void psp_glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	FrameBegin();
	const GLV *v = (const GLV *)gVbo.data() + first;
	int prim = mode == GL_TRIANGLES ? GU_TRIANGLES : mode == GL_TRIANGLE_STRIP ? GU_TRIANGLE_STRIP :
	           mode == GL_TRIANGLE_FAN ? GU_TRIANGLE_FAN : mode == GL_LINES ? GU_LINES : mode == GL_LINE_STRIP ? GU_LINE_STRIP : GU_POINTS;
	if (gUniform[6] && prim == GU_TRIANGLES) {      /* mezcla normal y aditiva en el mismo lote: por tramos */
		int s = 0;
		while (s < count) {
			bool add = v[s].sz != 0; int e = s;
			while (e < count && (v[e].sz != 0) == add) e += 3;
			if (e > count) e = count;
			DrawRun(prim, v + s, e - s, add);
			s = e;
		}
	} else DrawRun(prim, v, count, false);
}

/* ---- resto ---- */
static void psp_glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	gClear = ((unsigned)(a * 255) << 24) | ((unsigned)(b * 255) << 16) | ((unsigned)(g * 255) << 8) | (unsigned)(r * 255);
}
static void psp_glClear(GLbitfield) { FrameBegin(); sceGuClearColor(gClear); sceGuClear(GU_COLOR_BUFFER_BIT); }
static void psp_glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { gVp[0] = x; gVp[1] = y; gVp[2] = w; gVp[3] = h; }
static void psp_glEnable(GLenum) {}
static void psp_glDisable(GLenum) {}
static GLenum psp_glGetError() { return GL_NO_ERROR; }
static void psp_glGetIntegerv(GLenum p, GLint *v) { *v = p == GL_MAX_TEXTURE_SIZE ? 512 : 0; }
/* "Leer" una textura (RecoverBits): las imagenes ya no tienen sus pixeles en memoria normal, pero algunas partes
 * del juego dibujan por software (sobres, almanaque...). Se reconstruyen desde la textura reducida. */
static thread_local GLuint gFboTex = 0;
static void psp_glGenFramebuffers(GLsizei n, GLuint *ids) { for (int i = 0; i < n; i++) ids[i] = 1; }
static void psp_glBindFramebuffer(GLenum, GLuint fb) { if (!fb) gFboTex = 0; }
static void psp_glDeleteFramebuffers(GLsizei, const GLuint *) {}
static void psp_glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint tex, GLint) { gFboTex = tex; }
static GLenum psp_glCheckFramebufferStatus(GLenum) { return gFboTex > 0 && gFboTex < MAX_TEX && gTex[gFboTex].data ? GL_FRAMEBUFFER_COMPLETE : 0; }
static void psp_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum, GLenum, void *out)
{
	if (!(gFboTex > 0 && gFboTex < MAX_TEX)) return;
	const Tex &t = gTex[gFboTex];
	if (!t.data) return;
	const uint16_t *s = (const uint16_t *)t.data; uint32_t *o = (uint32_t *)out;
	for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) {
		int sx = (x + c) * t.w / t.ow, sy = (y + r) * t.h / t.oh;
		uint16_t p = s[sy * TexStride(t.w) + sx]; uint32_t R, G, B, A;
		if (t.psm == GU_PSM_5650) { R = (p & 31) * 255 / 31; G = ((p >> 5) & 63) * 255 / 63; B = (p >> 11) * 255 / 31; A = 255; }
		else if (t.psm == GU_PSM_5551) { R = (p & 31) * 255 / 31; G = ((p >> 5) & 31) * 255 / 31; B = ((p >> 10) & 31) * 255 / 31; A = (p >> 15) ? 255 : 0; }
		else { R = (p & 15) * 17; G = ((p >> 4) & 15) * 17; B = ((p >> 8) & 15) * 17; A = (p >> 12) * 17; }
		o[r * w + c] = R | (G << 8) | (B << 16) | (A << 24);
	}
}

void PspGLInit()
{
#define S(n) glad_##n = psp_##n
	S(glGenTextures); S(glDeleteTextures); S(glBindTexture); S(glActiveTexture); S(glTexParameteri);
	S(glTexImage2D); S(glTexSubImage2D); S(glCreateShader); S(glShaderSource); S(glCompileShader); S(glGetShaderiv);
	S(glGetShaderInfoLog); S(glDeleteShader); S(glCreateProgram); S(glAttachShader); S(glBindAttribLocation);
	S(glLinkProgram); S(glUseProgram); S(glGetUniformLocation); S(glUniform1i); S(glUniform4fv); S(glUniformMatrix4fv);
	S(glGenBuffers); S(glBindBuffer); S(glBufferData); S(glVertexAttribPointer); S(glEnableVertexAttribArray);
	S(glBlendFunc); S(glDrawArrays); S(glClearColor); S(glClear); S(glViewport); S(glEnable); S(glDisable);
	S(glGetError); S(glGetIntegerv); S(glGenFramebuffers); S(glBindFramebuffer); S(glDeleteFramebuffers);
	S(glFramebufferTexture2D); S(glCheckFramebufferStatus); S(glReadPixels);
#undef S
}

/* sin memoria: dejar constancia antes de abortar */
#include <new>
static void PspOutOfMemory() { PspLogLine("!!! SIN MEMORIA"); PspMemReport("SIN MEMORIA"); std::set_new_handler(nullptr); }
static struct PspNewHandler { PspNewHandler() { std::set_new_handler(PspOutOfMemory); } } sPspNewHandler;

/* vigilante: si no se dibuja nada en 8 s, anota el ultimo punto de control (PSPW) */
static int PspWatchdog(SceSize, void *)
{
	int last = -1, still = 0;
	for (;;) {
		sceKernelDelayThread(2000000);
		if (gSwaps == last) { if (++still == 4 || still % 15 == 0) { FILE *f = fopen("mem.log", "a"); if (f) { fprintf(f, "COLGADO en %s:%d subidas=%d dibujos=%d llamadas=%d\n", gPspWhereFile, gPspWhereLine, gUploads, gDraws, gPspCalls); fclose(f); } } }
		else { still = 0; last = gSwaps; }
	}
	return 0;
}
void PspStartWatchdog()
{
	SceUID th = sceKernelCreateThread("vigilante", PspWatchdog, 0x11, 0x4000, PSP_THREAD_ATTR_USER, nullptr);
	if (th >= 0) sceKernelStartThread(th, 0, nullptr);
}
