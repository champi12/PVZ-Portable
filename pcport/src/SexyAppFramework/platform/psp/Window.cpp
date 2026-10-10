/*
 * PSP: pantalla con sceGu (sin SDL video). SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "SexyAppBase.h"
#include "graphics/GLInterface.h"
#include "graphics/GLImage.h"
#include "graphics/GLPlatform.h"
#include "widget/WidgetManager.h"

void PspGuInit();
using namespace Sexy;

void SexyAppBase::MakeWindow()
{
	static bool sInited = false;
	if (!sInited) { PspGuInit(); sInited = true; mWindow = (void*)1; }

	if (mGLInterface == nullptr)
	{
		mGLInterface = std::make_unique<GLInterface>(this);
		if (!InitGLInterface())
		{
			mGLInterface = nullptr;
			return;
		}
		mGLInterface->UpdateViewport();
		mWidgetManager->Resize(mScreenBounds, mGLInterface->mPresentationRect);
	}

	bool isActive = mActive;
	mActive = true;
	mPhysMinimized = false;
	if (isActive != mActive)
		RehupFocus();

	ReInitImages();
	mWidgetManager->mImage = mGLInterface->GetScreenImage();
	mWidgetManager->MarkAllDirty();
}
