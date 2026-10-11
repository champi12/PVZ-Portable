/*
 * PSP: control como en las versiones de consola. No hay cursor de raton: la cruceta (o el stick) salta entre las
 * cosas que se pueden pulsar de la pantalla (botones, sobres de plantas, plantas del almanaque, objetos de la
 * tienda...) y X pulsa la elegida. Aqui se reunen esas "metas" de la pantalla actual.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "Nav.h"
#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/Widget/GameButton.h"
#include "Lawn/Widget/SeedChooserScreen.h"
#include "Lawn/Widget/AlmanacDialog.h"
#include "Lawn/Widget/StoreScreen.h"
#include "Lawn/Widget/AwardScreen.h"
#include "Lawn/Widget/GameSelector.h"
#include "Lawn/Widget/ZombatarWidget.h"
#include "Lawn/Widget/AchievementsScreen.h"
#include "widget/WidgetManager.h"
#include "widget/ButtonWidget.h"
#include "widget/Checkbox.h"
#include "widget/Slider.h"
#include "widget/EditWidget.h"
#include "widget/ListWidget.h"
#include "widget/Dialog.h"
#include <cmath>

using namespace Sexy;

static bool OnScreen(const Rect& r)
{
	return r.mWidth > 2 && r.mHeight > 2 && r.mX + r.mWidth > 0 && r.mY + r.mHeight > 0 && r.mX < 800 && r.mY < 600;
}

static void Add(std::vector<NavTarget>& out, const Rect& r, Widget* w = nullptr)
{
	if (OnScreen(r))
		out.push_back({ r, w });
}

static void AddGameButton(std::vector<NavTarget>& out, GameButton* b)
{
	if (b == nullptr || b->mDisabled || b->mBtnNoDraw)
		return;
	Point p = b->mParentWidget ? b->mParentWidget->GetAbsPos() : Point(0, 0);
	Add(out, Rect(p.mX + b->mX, p.mY + b->mY, b->mWidth, b->mHeight));
}

static void AddWidgets(std::vector<NavTarget>& out, WidgetContainer* theContainer)
{
	for (Widget* w : theContainer->mWidgets)
	{
		if (!w->mVisible)
			continue;
		bool aClickable = !w->mDisabled && (dynamic_cast<ButtonWidget*>(w) || dynamic_cast<Checkbox*>(w) ||
			dynamic_cast<Slider*>(w) || dynamic_cast<EditWidget*>(w));
		if (aClickable)
		{
			Point p = w->GetAbsPos();
			Add(out, Rect(p.mX, p.mY, w->mWidth, w->mHeight), w);
		}
		else if (dynamic_cast<ZombatarWidget*>(w) || dynamic_cast<AchievementsWidget*>(w))
		{
			// pantallas que dibujan sus propios botones
			std::vector<Rect> aRects;
			if (auto* z = dynamic_cast<ZombatarWidget*>(w)) z->PspNavTargets(aRects);
			else ((AchievementsWidget*)w)->PspNavTargets(aRects);
			Point p = w->GetAbsPos();
			for (Rect& r : aRects)
				Add(out, Rect(p.mX + r.mX, p.mY + r.mY, r.mWidth, r.mHeight));
		}
		AddWidgets(out, w);
	}
}

static void AddSeedChooser(std::vector<NavTarget>& out, SeedChooserScreen* sc)
{
	Point p = sc->GetAbsPos();
	for (ChosenSeed& s : sc->mChosenSeeds)
	{
		if (s.mSeedType == SeedType::SEED_NONE)
			continue;
		if (s.mSeedState != ChosenSeedState::SEED_IN_CHOOSER && s.mSeedState != ChosenSeedState::SEED_IN_BANK)
			continue;
		Add(out, Rect(p.mX + s.mX, p.mY + s.mY, SEED_PACKET_WIDTH, SEED_PACKET_HEIGHT));
	}
	AddGameButton(out, sc->mStartButton.get());
	AddGameButton(out, sc->mRandomButton.get());
	AddGameButton(out, sc->mViewLawnButton.get());
	AddGameButton(out, sc->mStoreButton.get());
	AddGameButton(out, sc->mAlmanacButton.get());
	AddGameButton(out, sc->mMenuButton.get());
	AddGameButton(out, sc->mImitaterButton.get());
}

static void AddAlmanac(std::vector<NavTarget>& out, AlmanacDialog* d)
{
	Point p = d->GetAbsPos();
	if (d->mOpenPage == AlmanacPage::ALMANAC_PAGE_PLANTS)
	{
		for (int i = 0; i < NUM_ALMANAC_SEEDS; i++)
		{
			SeedType t = (SeedType)i;
			if (!d->mApp->HasSeedType(t))
				continue;
			int x, y;
			d->GetSeedPosition(t, x, y);
			Add(out, t == SeedType::SEED_IMITATER ? Rect(p.mX + x, p.mY + y, 34, 46) : Rect(p.mX + x, p.mY + y, SEED_PACKET_WIDTH, SEED_PACKET_HEIGHT));
		}
	}
	else if (d->mOpenPage == AlmanacPage::ALMANAC_PAGE_ZOMBIES)
	{
		for (int i = 0; i < NUM_ALMANAC_ZOMBIES; i++)
		{
			ZombieType t = AlmanacDialog::GetZombieType(i);
			if (t == ZombieType::ZOMBIE_INVALID || !d->ZombieIsShown(t))
				continue;
			int x, y;
			d->GetZombiePosition(t, x, y);
			Add(out, Rect(p.mX + x, p.mY + y, 76, 76));
		}
	}
	AddGameButton(out, d->mCloseButton.get());
	AddGameButton(out, d->mIndexButton.get());
	AddGameButton(out, d->mPlantButton.get());
	AddGameButton(out, d->mZombieButton.get());
}

static void AddStore(std::vector<NavTarget>& out, StoreScreen* st)
{
	Point p = st->GetAbsPos();
	for (int i = 0; i < MAX_PAGE_SPOTS; i++)
	{
		if (st->GetStoreItemType(i) == STORE_ITEM_INVALID)
			continue;
		int x, y;
		StoreScreen::GetStorePosition(i, x, y);
		Add(out, Rect(p.mX + x, p.mY + y, 50, 87));
	}
}

void PspNavCollect(LawnApp* theApp, std::vector<NavTarget>& theTargets)
{
	theTargets.clear();
	if (theApp->GetDialogCount() > 0 && !theApp->mDialogList.empty())
	{
		Dialog* aTop = theApp->mDialogList.back();
		if (auto* a = dynamic_cast<AlmanacDialog*>(aTop))
			AddAlmanac(theTargets, a);
		else if (auto* s = dynamic_cast<StoreScreen*>(aTop))
			AddStore(theTargets, s);
		AddWidgets(theTargets, aTop);
		return;
	}
	if (theApp->mSeedChooserScreen && theApp->mSeedChooserScreen->mVisible && theApp->mSeedChooserScreen->mParent != nullptr)
	{
		AddSeedChooser(theTargets, theApp->mSeedChooserScreen.get());
		return;
	}
	if (theApp->mAwardScreen && theApp->mAwardScreen->mParent != nullptr)
	{
		AddGameButton(theTargets, theApp->mAwardScreen->mStartButton.get());
		AddGameButton(theTargets, theApp->mAwardScreen->mMenuButton.get());
		AddGameButton(theTargets, theApp->mAwardScreen->mContinueButton.get());
		AddWidgets(theTargets, theApp->mAwardScreen.get());
		if (!theTargets.empty())
			return;
	}
	AddWidgets(theTargets, theApp->mWidgetManager.get());
}

int PspNavPick(const std::vector<NavTarget>& theTargets, float theX, float theY, int theDirX, int theDirY)
{
	int aBest = -1;
	float aBestScore = 1e9f;
	for (int i = 0; i < (int)theTargets.size(); i++)
	{
		const Rect& r = theTargets[i].mRect;
		float cx = r.mX + r.mWidth * 0.5f, cy = r.mY + r.mHeight * 0.5f;
		float dx = cx - theX, dy = cy - theY;
		float aScore;
		if (theDirX == 0 && theDirY == 0)
		{
			// la mas cercana al punto (0 si el punto esta dentro)
			float ex = std::max({ (float)r.mX - theX, 0.0f, theX - (float)(r.mX + r.mWidth) });
			float ey = std::max({ (float)r.mY - theY, 0.0f, theY - (float)(r.mY + r.mHeight) });
			aScore = ex * ex + ey * ey;
		}
		else
		{
			float aAlong = dx * theDirX + dy * theDirY;
			float aSide = fabsf(dx * theDirY - dy * theDirX);
			if (aAlong < 8.0f)
				continue;   // solo lo que esta hacia ese lado
			aScore = aAlong + aSide * 2.5f;
		}
		if (aScore < aBestScore)
		{
			aBestScore = aScore;
			aBest = i;
		}
	}
	return aBest;
}

void* PspNavScreen(LawnApp* theApp)
{
	if (theApp->GetDialogCount() > 0 && !theApp->mDialogList.empty())
		return theApp->mDialogList.back();
	if (theApp->mSeedChooserScreen && theApp->mSeedChooserScreen->mParent != nullptr)
		return theApp->mSeedChooserScreen.get();
	if (theApp->mAwardScreen && theApp->mAwardScreen->mParent != nullptr)
		return theApp->mAwardScreen.get();
	if (theApp->mGameSelector)
		return theApp->mGameSelector.get();
	return theApp->mTitleScreen.get();
}

int PspNavDefault(LawnApp* theApp, const std::vector<NavTarget>& theTargets, bool* theNotReady)
{
	*theNotReady = false;
	if (theTargets.empty())
		return -1;
	// menu principal: Aventura (mientras entra el menu aun no esta en pantalla: se vuelve a intentar)
	if (theApp->GetDialogCount() == 0 && theApp->mGameSelector && theApp->mGameSelector->mAdventureButton &&
		!theApp->mSeedChooserScreen && !theApp->mBoard)
	{
		for (int i = 0; i < (int)theTargets.size(); i++)
			if (theTargets[i].mWidget == theApp->mGameSelector->mAdventureButton)
				return i;
		*theNotReady = true;
	}
	// dialogo: el boton de abajo en el centro (Aceptar); si no, lo mas cercano al centro de la pantalla
	if (theApp->GetDialogCount() > 0 && !theApp->mDialogList.empty())
	{
		Dialog* d = theApp->mDialogList.back();
		Point p = d->GetAbsPos();
		return PspNavPick(theTargets, p.mX + d->mWidth * 0.5f, p.mY + d->mHeight, 0, 0);
	}
	return PspNavPick(theTargets, 400, 300, 0, 0);
}
