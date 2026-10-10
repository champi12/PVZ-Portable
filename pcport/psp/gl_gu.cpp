/*
 * gl_gu.cpp - Las ~40 funciones de OpenGL ES 2 que usa GLInterface.cpp, hechas con sceGu de la PSP.
 * El shader del juego es fijo (color * textura, mezcla normal o aditiva), asi que se emula sin shaders.
 * La pantalla logica de 800x600 se dibuja a escala 0.6 (480x360) y se ve una ventana de 272 de alto que
 * sigue al cursor (PspSetCameraY).
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspmoduleinfo.h>
#include <pspgu.h>
#include <psputility.h>
#include <malloc.h>
#include <pspsysmem.h>
#include <pspthreadman.h>
#include <pspintrman.h>
#include <string.h>
#include <stdio.h>
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
static float gScale = 0.6f, gCamY = 0;

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

void PspSetCameraY(float y) { gCamY = y; }
float PspGetCameraY() { return gCamY; }

static void FrameBegin()
{
	if (gFrameOpen) return;
	sceGuStart(GU_DIRECT, gList);
	gFrameOpen = true;
}

void PspGuInit()
{
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
static void DrawCursor()
{
	/* flecha blanca con borde negro en la posicion del cursor virtual */
	float x = gPspCursorX * gScale, y = gPspCursorY * gScale - gCamY;
	struct V { uint32_t c; float x, y, z; };
	V *v = (V *)sceGuGetMemory(6 * sizeof(V));
	v[0] = { 0xFF000000, x - 1, y - 2, 0 }; v[1] = { 0xFF000000, x - 1, y + 14, 0 }; v[2] = { 0xFF000000, x + 11, y + 11, 0 };
	v[3] = { 0xFFFFFFFF, x, y, 0 };         v[4] = { 0xFFFFFFFF, x, y + 11, 0 };     v[5] = { 0xFFFFFFFF, x + 8, y + 9, 0 };
	sceGuDisable(GU_TEXTURE_2D);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 6, 0, v);
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
	static int sFrames;
	gSwaps++;
	if (++sFrames % 120 == 0) PspMemReport("frame");
	FrameBegin();
	DrawCursor();
	sceGuFinish();
	sceGuSync(0, 0);
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
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
	gFrameOpen = false;
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
	int bytes = dw * dh * 2;
	uint16_t *d = (uint16_t *)memalign(16, bytes);
	if (!d) return;
	for (int i = 0; i < dw * dh; i++) {
		uint32_t p = src[i]; unsigned r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255, a = p >> 24;
		if (psm == GU_PSM_5650) d[i] = (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);
		else if (psm == GU_PSM_5551) d[i] = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | ((a >> 7) << 15);
		else d[i] = (r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8) | ((a >> 4) << 12);
	}
	sceKernelDcacheWritebackRange(d, bytes);
	/* se publica de una vez para que el hilo de dibujo nunca vea una textura a medias */
	void *old = t.data; int oldBytes = old ? t.bytes : 0;
	t.ow = w; t.oh = h; t.w = dw; t.h = dh; t.psm = psm; t.bytes = bytes; t.data = d;
	gTexBytes += bytes - oldBytes;
	free(old);
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
		sceGuFinish(); sceGuSync(0, 0); sceGuStart(GU_DIRECT, gList);
	}
	GuV *out = (GuV *)sceGuGetMemory(n * sizeof(GuV));
	Tex *t = gUniform[3] && gBoundTex > 0 && gBoundTex < MAX_TEX && gTex[gBoundTex].data ? &gTex[gBoundTex] : nullptr;
	float sx = gScale * 800.0f / gVp[2], sy = gScale * 600.0f / gVp[3];
	for (int i = 0; i < n; i++) {
		/* coordenadas logicas -> ndc con la matriz ortografica -> pixeles de la PSP */
		float nx = gMtx[0] * v[i].sx + gMtx[4] * v[i].sy + gMtx[12];
		float ny = gMtx[1] * v[i].sx + gMtx[5] * v[i].sy + gMtx[13];
		out[i].x = ((nx + 1) * 0.5f * gVp[2] + gVp[0]) * sx;
		out[i].y = ((1 - ny) * 0.5f * gVp[3] + gVp[1]) * sy - gCamY;
		out[i].z = 0;
		out[i].color = v[i].color;
		out[i].u = t ? v[i].tu * t->w : 0;
		out[i].v = t ? v[i].tv * t->h : 0;
	}
	if (t) {
		sceGuEnable(GU_TEXTURE_2D);
		sceGuTexMode(t->psm, 0, 0, 0);
		sceGuTexImage(0, t->w, t->h, t->w, t->data);
		sceGuTexFilter(t->filter, t->filter);
		sceGuTexWrap(t->wrap, t->wrap);
		sceGuTexFlush();
	} else sceGuDisable(GU_TEXTURE_2D);
	SetBlend(additive);
	sceGuDrawArray(prim, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, n, 0, out);
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
		uint16_t p = s[sy * t.w + sx]; uint32_t R, G, B, A;
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
static void PspOutOfMemory() { PspMemReport("SIN MEMORIA"); std::set_new_handler(nullptr); }
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
