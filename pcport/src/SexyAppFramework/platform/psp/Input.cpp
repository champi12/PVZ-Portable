/*
 * PSP: control como en las versiones de consola, sin cursor de raton. La cruceta o el stick mueven la seleccion
 * entre lo que se puede pulsar (Nav.cpp) y X lo pulsa; O vuelve (ESC), START = menu, CUADRADO = pausa,
 * SELECT = vista (16:9, 4:3 o zoom). En un nivel la seleccion va de casilla en casilla, L/R eligen sobre,
 * TRIANGULO = pala, O suelta la planta y los soles y monedas se recogen al pasar por encima.
 * Si una pantalla no tiene nada que elegir, queda un cursor libre como antes.
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
#include "widget/Slider.h"
#include "widget/SliderListener.h"
#include "Nav.h"
#include <vector>
#ifdef PSP_TEST_OPTIONS
#include "Lawn/Widget/NewOptionsDialog.h"
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
		int msAbs = ms;   /* tiempos de 1000 o mas: segundos desde el arranque (+1000) */
		ms = sBase < 0 ? -1000000 : ms - sBase;
		if (*sNext && sscanf(sNext, "%d:%x%n", &f, &m, &n) == 2) {
			if (f >= 1000) { f -= 1000; ms = msAbs; }
			if (!sDown && ms >= f * 1000) { sDown = ms; sMask = m; FILE *lf = fopen("mem.log", "a"); if (lf) { fprintf(lf, "PAD %x\n", m); fclose(lf); } }
			else if (sDown && ms >= sDown + 100) { sDown = 0; sMask = 0; sNext += n; if (*sNext == ';') sNext++; }
		}
		b |= sMask; down = b & ~gPrev; up = gPrev & ~b;
	}
#endif
	auto clickAt = [&](int cx, int cy, int btn) {
		int ox = (int)gCurX, oy = (int)gCurY;
		mWidgetManager->MouseMove(cx, cy); mWidgetManager->MouseDown(cx, cy, btn); mWidgetManager->MouseUp(cx, cy, btn);
		mWidgetManager->MouseMove(ox, oy);
	};
	uint64_t now = sceKernelGetSystemTimeWide();

	/* direcciones: cruceta o stick, con repeticion (0,3 s y luego cada 0,12 s) */
	unsigned int dirs = b & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT | PSP_CTRL_UP | PSP_CTRL_DOWN);
	if (ax < -0.6f) dirs |= PSP_CTRL_LEFT;
	if (ax > 0.6f) dirs |= PSP_CTRL_RIGHT;
	if (ay < -0.6f) dirs |= PSP_CTRL_UP;
	if (ay > 0.6f) dirs |= PSP_CTRL_DOWN;
	static unsigned int sPrevDirs = 0;
	unsigned int newDirs = dirs & ~sPrevDirs;
	bool step = newDirs != 0;
	if (newDirs) sHoldT = now;
	else if (dirs && now - sHoldT > 300000) { step = true; sHoldT = now - 180000; }
	sPrevDirs = dirs;
	int dirX = step ? ((dirs & PSP_CTRL_RIGHT) ? 1 : (dirs & PSP_CTRL_LEFT) ? -1 : 0) : 0;
	int dirY = step ? ((dirs & PSP_CTRL_DOWN) ? 1 : (dirs & PSP_CTRL_UP) ? -1 : 0) : 0;
	if (step) mLastUserInputTick = mLastTimerTime;

	static std::vector<NavTarget> sTargets;
	bool navMode = false;
	Rect focus(0, 0, 0, 0);
	if (inLevel) {
		/* nivel: la seleccion salta de casilla en casilla */
		int rows = bd->StageHas6Rows() ? 6 : 5;
		if (dirX || dirY) {
			sGX = std::clamp(sGX + dirX, 0, 8); sGY = std::clamp(sGY + dirY, 0, rows - 1);
		}
		int cx0 = bd->mX + bd->GridToPixelX(sGX, sGY), cy0 = bd->mY + bd->GridToPixelY(sGX, sGY);
		int ch = bd->StageHas6Rows() ? 85 : 100;
		focus = Rect(cx0, cy0, 80, ch);
		float nx = cx0 + 40, ny = cy0 + ch / 2;
		if (nx != gCurX || ny != gCurY) { gCurX = nx; gCurY = ny; mWidgetManager->MouseMove((int)gCurX, (int)gCurY); }
		navMode = true;
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
		/* X con las manos vacias recoge el premio del final del nivel, este donde este */
		if ((down & PSP_CTRL_CROSS) && bd->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL) {
			for (Coin *c : bd->mCoins) {
				if (!c->IsLevelAward() || c->mIsBeingCollected) continue;
				c->MouseDown(0, 0, 1);
				down &= ~PSP_CTRL_CROSS; up &= ~PSP_CTRL_CROSS;
				break;
			}
		}
	} else {
		/* menus y pantallas: la seleccion salta entre lo que se puede pulsar */
		PspNavCollect(app, sTargets);
		if (!sTargets.empty()) {
			navMode = true;
			static void *sScreen = nullptr;
			void *scr = PspNavScreen(app);
			bool notReady = false;
			int cur = scr != sScreen ? PspNavDefault(app, sTargets, &notReady) : PspNavPick(sTargets, gCurX, gCurY, 0, 0);
			if (cur < 0) cur = PspNavPick(sTargets, gCurX, gCurY, 0, 0);
			if (!notReady || dirX || dirY) sScreen = scr;
			Slider *sl = cur >= 0 ? dynamic_cast<Slider *>(sTargets[cur].mWidget) : nullptr;
			if (sl && dirX && sl->mListener) {
				double v = std::clamp<double>(sl->mVal + dirX * 0.1, 0.0, 1.0);
				sl->SetValue(v);
				sl->mListener->SliderVal(sl->mId, v);
			} else if (dirX || dirY) {
				const Rect &r = sTargets[cur].mRect;
				int nx = PspNavPick(sTargets, r.mX + r.mWidth * 0.5f, r.mY + r.mHeight * 0.5f, dirX, dirY);
				if (nx >= 0) cur = nx;
			}
			focus = sTargets[cur].mRect;
			float cx = focus.mX + focus.mWidth * 0.5f, cy = focus.mY + focus.mHeight * 0.5f;
			if (cx != gCurX || cy != gCurY) { gCurX = cx; gCurY = cy; mWidgetManager->MouseMove((int)gCurX, (int)gCurY); }
		}
	}
	if (!navMode) {
		/* pantalla sin nada que elegir (o especial): cursor libre con el stick o la cruceta */
		static uint64_t sLast = 0;
		float k = sLast ? std::min((now - sLast) / 16667.0f, 4.0f) : 1.0f;
		sLast = now;
		float sp = ((b & PSP_CTRL_RTRIGGER) ? 14 : 8) * k, dp = 6 * k;
		float dx = ax * sp, dy = ay * sp;
		if (b & PSP_CTRL_LEFT) dx -= dp;
		if (b & PSP_CTRL_RIGHT) dx += dp;
		if (b & PSP_CTRL_UP) dy -= dp;
		if (b & PSP_CTRL_DOWN) dy += dp;
		if (dx != 0 || dy != 0) {
			gCurX = std::clamp(gCurX + dx, 0.0f, (float)mWidth - 1);
			gCurY = std::clamp(gCurY + dy, 0.0f, (float)mHeight - 1);
			mLastUserInputTick = mLastTimerTime;
			mWidgetManager->MouseMove((int)gCurX, (int)gCurY);
		}
	}
	/* nombre de la pantalla para el registro de rendimiento */
	{
		const char *scr = GetDialogCount() > 0 ? "dialogo" : inLevel ? "nivel" :
			app->mSeedChooserScreen ? "elegir plantas" : bd ? "nivel (presentacion)" :
			app->mAwardScreen ? "premio" : app->mGameSelector ? "menu" : app->mTitleScreen ? "titulo / carga" : "otra";
		PspLogScreen(scr);
	}
	/* en las presentaciones de un nivel no hay nada que elegir ni cursor que mostrar */
	PspSetFocus(navMode || bd != nullptr, focus.mX, focus.mY, focus.mWidth, focus.mHeight);
	gPspCursorX = (int)gCurX; gPspCursorY = (int)gCurY;

	/* la vista sigue al cursor: 600 logicos a 0.6 = 360, se ven 272 */
	if (down & PSP_CTRL_SELECT) PspSetViewMode(PspGetViewMode() + 1);
	if (PspGetViewMode() == 2)
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
	/* O: en un nivel suelta la planta o la pala; fuera, volver (como ESC) */
	if (inLevel) {
		if (down & PSP_CTRL_CIRCLE) { mLastUserInputTick = mLastTimerTime; mWidgetManager->MouseDown(x, y, -1); }
		if (up & PSP_CTRL_CIRCLE) mWidgetManager->MouseUp(x, y, -1);
	} else {
		if (down & PSP_CTRL_CIRCLE) mWidgetManager->KeyDown(KEYCODE_ESCAPE);
		if (up & PSP_CTRL_CIRCLE) mWidgetManager->KeyUp(KEYCODE_ESCAPE);
	}
	if ((down & PSP_CTRL_START) && (b & PSP_CTRL_LTRIGGER) && (b & PSP_CTRL_RTRIGGER)) {
		PspLogToggle();   /* L + R + START: registro de rendimiento */
		down &= ~PSP_CTRL_START; up &= ~PSP_CTRL_START;
	}
	if (down & PSP_CTRL_START) mWidgetManager->KeyDown(KEYCODE_ESCAPE);
	if (up & PSP_CTRL_START) mWidgetManager->KeyUp(KEYCODE_ESCAPE);
	if (down & PSP_CTRL_SQUARE) mWidgetManager->KeyDown(KEYCODE_SPACE);
	if (up & PSP_CTRL_SQUARE) mWidgetManager->KeyUp(KEYCODE_SPACE);
	gPrev = b;
	return false;
}
