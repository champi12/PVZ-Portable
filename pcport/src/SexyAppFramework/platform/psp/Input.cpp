/*
 * PSP: cursor virtual con el stick o la cruceta. X = clic, O = clic derecho, START = ESC (menu),
 * CUADRADO = espacio (pausa), SELECT = zoom (la pantalla entera o a escala 0.6 siguiendo al cursor), R = rapido.
 * En un nivel, como en las versiones de consola: la cruceta salta de casilla en casilla, L/R eligen sobre,
 * TRIANGULO = pala y los soles y monedas se recogen al pasar el cursor por encima.
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
#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/Coin.h"
#include "Lawn/CursorObject.h"
#ifdef PSP_TEST_OPTIONS
#include "Lawn/Widget/NewOptionsDialog.h"
#include "widget/Slider.h"
#include "widget/Checkbox.h"
#endif

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
	/* nivel en juego: control de consola (ver arriba) */
	LawnApp *app = (LawnApp *)this;
	Board *bd = app->mBoard;
	bool inLevel = bd && app->mGameScene == GameScenes::SCENE_PLAYING && !bd->mPaused && GetDialogCount() == 0;
	static int sGX = 2, sGY = 2;
	static uint64_t sHoldT = 0;
	static Board *sLastBoard = nullptr;
	if (bd != sLastBoard) { sLastBoard = bd; sGX = 2; sGY = 2; }
#ifdef PSP_AUTOPAD
	{   /* pruebas: "segundo:mascara;..." pulsa esos botones 0,1 s */
		static const char *sNext = PSP_AUTOPAD; static int sDown = 0; static unsigned sMask = 0; static int sBase = -1;
		int ms = (int)(sceKernelGetSystemTimeWide() / 1000), f, n; unsigned m;
		if (sBase < 0 && inLevel) sBase = ms;   /* los segundos cuentan desde que se puede jugar */
		ms = sBase < 0 ? -1000000 : ms - sBase;
		if (*sNext && sscanf(sNext, "%d:%x%n", &f, &m, &n) == 2) {
			if (!sDown && ms >= f * 1000) { sDown = ms; sMask = m; FILE *lf = fopen("mem.log", "a"); if (lf) { fprintf(lf, "PAD %x\n", m); fclose(lf); } }
			else if (sDown && ms >= sDown + 100) { sDown = 0; sMask = 0; sNext += n; if (*sNext == ';') sNext++; }
		}
		b |= sMask; down = b & ~gPrev; up = gPrev & ~b;
	}
#endif
	unsigned int moveB = b;
	auto clickAt = [&](int cx, int cy, int btn) {
		int ox = (int)gCurX, oy = (int)gCurY;
		mWidgetManager->MouseMove(cx, cy); mWidgetManager->MouseDown(cx, cy, btn); mWidgetManager->MouseUp(cx, cy, btn);
		mWidgetManager->MouseMove(ox, oy);
	};
	if (inLevel) {
		unsigned int dirs = b & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_UP | PSP_CTRL_DOWN);
		uint64_t t = sceKernelGetSystemTimeWide();
		bool step = (down & dirs) != 0;
		if (dirs && !step && t - sHoldT > 300000) { step = true; sHoldT = t - 180000; }   /* repeticion: 0,3 s y luego cada 0,12 s */
		if (down & dirs) sHoldT = t;
		if (step) {
			int rows = bd->StageHas6Rows() ? 6 : 5;
			if (b & PSP_CTRL_LEFT) sGX--;
			if (b & PSP_CTRL_RIGHT) sGX++;
			if (b & PSP_CTRL_UP) sGY--;
			if (b & PSP_CTRL_DOWN) sGY++;
			sGX = std::clamp(sGX, 0, 8); sGY = std::clamp(sGY, 0, rows - 1);
			gCurX = bd->mX + bd->GridToPixelX(sGX, sGY) + 40;
			gCurY = bd->mY + bd->GridToPixelY(sGX, sGY) + 45;
			mLastUserInputTick = mLastTimerTime;
			mWidgetManager->MouseMove((int)gCurX, (int)gCurY);
		}
		moveB &= ~dirs;   /* la cruceta no mueve el cursor libre */
		SeedBank *bank = bd->mSeedBank.get();
		if (bank && bank->mNumPackets > 0 && (down & (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER))) {
			static int sSeed = -1;
			int n = bank->mNumPackets;
			sSeed = (down & PSP_CTRL_RTRIGGER) ? (sSeed + 1) % n : (sSeed - 1 + n) % n;
			SeedPacket &pk = bank->mSeedPackets[sSeed];
			if (bd->mCursorObject->mCursorType != CursorType::CURSOR_TYPE_NORMAL)
				clickAt((int)gCurX, (int)gCurY, -1);   /* suelta lo que llevaba */
			clickAt(bd->mX + bank->mX + pk.mX + pk.mOffsetX + pk.mWidth / 2, bd->mY + bank->mY + pk.mY + pk.mHeight / 2, 1);
		}
		if (down & PSP_CTRL_TRIANGLE) {
			if (bd->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_SHOVEL) clickAt((int)gCurX, (int)gCurY, -1);
			else {
				Rect r = bd->GetShovelButtonRect();
				clickAt(bd->mX + r.mX + r.mWidth / 2, bd->mY + r.mY + r.mHeight / 2, 1);
			}
		}
		/* soles y monedas: se recogen al pasar por encima */
		int lx = (int)gCurX - bd->mX, ly = (int)gCurY - bd->mY;
		for (Coin *c : bd->mCoins) {
			if (c->mType != CoinType::COIN_SUN && c->mType != CoinType::COIN_SMALLSUN && c->mType != CoinType::COIN_LARGESUN &&
				c->mType != CoinType::COIN_SILVER && c->mType != CoinType::COIN_GOLD && c->mType != CoinType::COIN_DIAMOND) continue;
			HitResult hr;
			if (c->MouseHitTest(lx, ly, &hr)) c->MouseDown(lx, ly, 1);
		}
	}

	/* velocidad por tiempo (no por llamada): 60 pasos por segundo */
	static uint64_t sLast = 0;
	uint64_t now = sceKernelGetSystemTimeWide();
	float k = sLast ? std::min((now - sLast) / 16667.0f, 4.0f) : 1.0f;
	sLast = now;
	float sp = ((!inLevel && (b & PSP_CTRL_RTRIGGER)) ? 14 : 8) * k, dp = 6 * k;
	float dx = ax * sp, dy = ay * sp;
	if (moveB & PSP_CTRL_LEFT) dx -= dp;
	if (moveB & PSP_CTRL_RIGHT) dx += dp;
	if (moveB & PSP_CTRL_UP) dy -= dp;
	if (moveB & PSP_CTRL_DOWN) dy += dp;
	if (dx != 0 || dy != 0)
	{
		gCurX = std::clamp(gCurX + dx, 0.0f, (float)mWidth - 1);
		gCurY = std::clamp(gCurY + dy, 0.0f, (float)mHeight - 1);
		mLastUserInputTick = mLastTimerTime;
		mWidgetManager->MouseMove((int)gCurX, (int)gCurY);
		if (inLevel) {   /* la cruceta sigue desde la casilla donde quedo el stick */
			sGX = std::clamp(bd->PixelToGridXKeepOnBoard((int)gCurX - bd->mX, (int)gCurY - bd->mY), 0, 8);
			sGY = std::clamp(bd->PixelToGridYKeepOnBoard((int)gCurX - bd->mX, (int)gCurY - bd->mY), 0, bd->StageHas6Rows() ? 5 : 4);
		}
	}
	gPspCursorX = (int)gCurX; gPspCursorY = (int)gCurY;

	/* la vista sigue al cursor: 600 logicos a 0.6 = 360, se ven 272 */
	if ((down & PSP_CTRL_SELECT) || (!inLevel && (down & PSP_CTRL_LTRIGGER))) PspSetZoom(!PspGetZoom());
	if (PspGetZoom())
	{
		float target = std::clamp(gCurY * 0.6f - 136.0f, 0.0f, 360.0f - 272.0f);
		PspSetCameraY(PspGetCameraY() + (target - PspGetCameraY()) * 0.25f);
	}

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
#ifdef PSP_TEST_OPTIONS
	{   /* pruebas: abre Opciones a los PSP_TEST_OPTIONS s y toca cada control */
		static int sStep = 0;
		int sec = (int)(sceKernelGetSystemTimeWide() / 1000000) - PSP_TEST_OPTIONS;
		if (sec >= sStep * 3 && sStep < 7) {
			FILE *lf = fopen("mem.log", "a"); if (lf) { fprintf(lf, "OPC paso %d\n", sStep); fclose(lf); }
			LawnApp *app = (LawnApp *)this;
			NewOptionsDialog *d = (NewOptionsDialog *)app->GetDialog(Dialogs::DIALOG_NEWOPTIONS);
			if (sStep == 0) app->DoNewOptions(true);
			else if (d && sStep == 1) d->SliderVal(4, 0.3);
			else if (d && sStep == 2) d->SliderVal(5, 0.4);
			else if (d && sStep == 3) { d->mFullscreenCheckbox->SetChecked(!d->mFullscreenCheckbox->IsChecked(), true); }
			else if (d && sStep == 4) { d->mHardwareAccelerationCheckbox->SetChecked(!d->mHardwareAccelerationCheckbox->IsChecked(), true); }
			else if (d && sStep == 5) app->KillNewOptionsDialog();
			sStep++;
			lf = fopen("mem.log", "a"); if (lf) { fprintf(lf, "OPC paso %d hecho\n", sStep - 1); fclose(lf); }
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
	if (down & PSP_CTRL_SQUARE) mWidgetManager->KeyDown(KEYCODE_SPACE);
	if (up & PSP_CTRL_SQUARE) mWidgetManager->KeyUp(KEYCODE_SPACE);
	gPrev = b;
	return false;
}
