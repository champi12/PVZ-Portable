/* PSP: control por seleccion como en consola (ver Nav.cpp) */
#pragma once
#include "misc/Rect.h"
#include <vector>

namespace Sexy { class Widget; }
class LawnApp;

struct NavTarget
{
	Sexy::Rect		mRect;      // coordenadas logicas (800x600)
	Sexy::Widget*	mWidget;    // si es un widget (para los deslizadores)
};

// lo que se puede pulsar en la pantalla actual (vacio en un nivel: alli se usa la rejilla)
void PspNavCollect(LawnApp* theApp, std::vector<NavTarget>& theTargets);
// la mejor meta en esa direccion desde (x, y); con direccion 0,0 la mas cercana. -1 si no hay
int PspNavPick(const std::vector<NavTarget>& theTargets, float theX, float theY, int theDirX, int theDirY);
// identifica la pantalla actual (para volver a la seleccion inicial al cambiar) y la seleccion inicial
void* PspNavScreen(LawnApp* theApp);
int PspNavDefault(LawnApp* theApp, const std::vector<NavTarget>& theTargets);
