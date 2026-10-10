/*
 * PSP: cursor virtual con el stick o la cruceta. X = clic, O = clic derecho, START = ESC (menu),
 * SELECT = espacio (pausa). La vista (480x360 a escala 0.6) sube y baja siguiendo al cursor.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include <pspctrl.h>
#include <psputility.h>
#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include <psprtc.h>
#include <pspthreadman.h>
#include <algorithm>
#include <math.h>
#include "SexyAppBase.h"
#include "graphics/GLInterface.h"
#include "graphics/GLPlatform.h"
#include "widget/WidgetManager.h"

using namespace Sexy;

static float gCurX = 400, gCurY = 300;
static unsigned int gPrev = 0;
static int gAutoEnter = 0;   /* pruebas: Intro tras escribir el nombre */
int gPspCursorX = 400, gPspCursorY = 300;   /* gl_gu.cpp dibuja el cursor aqui */

void SexyAppBase::InitInput()
{
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	mMouseIn = true;
}

/* teclado en pantalla de la PSP (OSK) para escribir el nombre del jugador */
void PspGuFrameForOsk();   /* gl_gu.cpp: dibuja un fotograma vacio y llama a sceUtilityOskUpdate */
bool SexyAppBase::StartTextInput(std::string& theInput)
{
#ifdef PSP_AUTOPLAY
	theInput = "PSP"; gAutoEnter = 20; return true;
#endif
	static unsigned short desc[] = { 'N','o','m','b','r','e',0 };
	unsigned short in[64] = { 0 }, out[64] = { 0 };
	for (size_t i = 0; i < theInput.size() && i < 63; i++) in[i] = (unsigned char)theInput[i];
	SceUtilityOskData data; memset(&data, 0, sizeof(data));
	data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
	data.lines = 1; data.unk_24 = 1;
	data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
	data.desc = desc; data.intext = in; data.outtextlength = 64; data.outtextlimit = 12; data.outtext = out;
	SceUtilityOskParams params; memset(&params, 0, sizeof(params));
	params.base.size = sizeof(params);
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &params.base.language);
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &params.base.buttonSwap);
	params.base.graphicsThread = 17; params.base.accessThread = 19; params.base.fontThread = 18; params.base.soundThread = 16;
	params.datacount = 1; params.data = &data;
	if (sceUtilityOskInitStart(&params) < 0) return false;
	for (;;) {
		int st = sceUtilityOskGetStatus();
		if (st == PSP_UTILITY_DIALOG_NONE) break;
		if (st == PSP_UTILITY_DIALOG_VISIBLE) PspGuFrameForOsk();
		else if (st == PSP_UTILITY_DIALOG_QUIT) sceUtilityOskShutdownStart();
		else PspGuFrameForOsk();
	}
	if (data.result != PSP_UTILITY_OSK_RESULT_CHANGED && data.result != PSP_UTILITY_OSK_RESULT_UNCHANGED) return false;
	theInput.clear();
	for (int i = 0; out[i] && i < 63; i++) theInput += out[i] < 128 ? (char)out[i] : '?';
	gPrev = 0xFFFFFFFF;   /* que no cuente como pulsada la X que cerro el teclado */
	return true;
}
void SexyAppBase::StopTextInput() {}
void SexyAppBase::SetTextInputRect([[maybe_unused]] const Rect& theRect) {}

bool SexyAppBase::ProcessDeferredMessages([[maybe_unused]] bool singleMessage)
{
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);
	unsigned int b = pad.Buttons, down = b & ~gPrev, up = gPrev & ~b;

	float ax = (pad.Lx - 128) / 128.0f, ay = (pad.Ly - 128) / 128.0f;
	if (fabsf(ax) < 0.25f) ax = 0;
	if (fabsf(ay) < 0.25f) ay = 0;
	float sp = (b & PSP_CTRL_RTRIGGER) ? 14 : 8;
	float dx = ax * sp, dy = ay * sp;
	if (b & PSP_CTRL_LEFT) dx -= 6;
	if (b & PSP_CTRL_RIGHT) dx += 6;
	if (b & PSP_CTRL_UP) dy -= 6;
	if (b & PSP_CTRL_DOWN) dy += 6;
	if (dx != 0 || dy != 0)
	{
		gCurX = std::clamp(gCurX + dx, 0.0f, (float)mWidth - 1);
		gCurY = std::clamp(gCurY + dy, 0.0f, (float)mHeight - 1);
		mLastUserInputTick = mLastTimerTime;
		mWidgetManager->MouseMove((int)gCurX, (int)gCurY);
	}
	gPspCursorX = (int)gCurX; gPspCursorY = (int)gCurY;

	/* la vista sigue al cursor: 600 logicos a 0.6 = 360, se ven 272 */
	float target = std::clamp(gCurY * 0.6f - 136.0f, 0.0f, 360.0f - 272.0f);
	PspSetCameraY(PspGetCameraY() + (target - PspGetCameraY()) * 0.25f);

#ifdef PSP_AUTOPLAY
	if (gAutoEnter && --gAutoEnter == 0) { mWidgetManager->KeyDown(KEYCODE_RETURN); mWidgetManager->KeyChar('\r'); mWidgetManager->KeyUp(KEYCODE_RETURN); }
	{   /* pruebas: "segundo:x,y;..." -> lleva el cursor y hace clic en ese segundo desde el arranque */
		static const char *sNext = PSP_AUTOPLAY; static int sDown = 0;
		int ms = (int)(sceKernelGetSystemTimeWide() / 1000), f, cx, cy, n;
		static int sLog;
		if (sLog++ % 600 == 0) { FILE *lf = fopen("mem.log", "a"); if (lf) { fprintf(lf, "AUTO cur=%d,%d ms=%d next='%s' parse=%d\n", gPspCursorX, gPspCursorY, ms, sNext, sscanf(sNext, "%d:%d,%d%n", &f, &cx, &cy, &n)); fclose(lf); } }
		if (*sNext && sscanf(sNext, "%d:%d,%d%n", &f, &cx, &cy, &n) == 3) {
			if (!sDown && ms >= f * 1000) { gCurX = cx; gCurY = cy; mWidgetManager->MouseMove(cx, cy); mWidgetManager->MouseDown(cx, cy, 1); sDown = ms; }
			else if (sDown && ms >= sDown + 100) { mWidgetManager->MouseUp(cx, cy, 1); sDown = 0; sNext += n; if (*sNext == ';') sNext++; }
		}
	}
#endif
	int x = (int)gCurX, y = (int)gCurY;
	if (down & PSP_CTRL_CROSS) { mLastUserInputTick = mLastTimerTime; mWidgetManager->MouseDown(x, y, 1); }
	if (up & PSP_CTRL_CROSS) mWidgetManager->MouseUp(x, y, 1);
	if (down & PSP_CTRL_CIRCLE) { mLastUserInputTick = mLastTimerTime; mWidgetManager->MouseDown(x, y, -1); }
	if (up & PSP_CTRL_CIRCLE) mWidgetManager->MouseUp(x, y, -1);
	if (down & PSP_CTRL_START) mWidgetManager->KeyDown(KEYCODE_ESCAPE);
	if (up & PSP_CTRL_START) mWidgetManager->KeyUp(KEYCODE_ESCAPE);
	if (down & PSP_CTRL_SELECT) mWidgetManager->KeyDown(KEYCODE_SPACE);
	if (up & PSP_CTRL_SELECT) mWidgetManager->KeyUp(KEYCODE_SPACE);
	gPrev = b;
	return false;
}
