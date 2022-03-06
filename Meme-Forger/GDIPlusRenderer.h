// 2020 - 2022 Baltazarus

#pragma once
#ifndef RENDERER_H
#define RENDERER_H
#include "Utilities.h"
#include <gdiplus.h>

#define CLEAR_RED	RGB(0xFF, 0x00, 0x00)
#define CLEAR_GREEN RGB(0x00, 0xFF, 0x00)
#define CLEAR_BLUE	RGB(0x00, 0x00, 0xFF)
#define CLEAR_WHITE RGB(0xFF, 0xFF, 0xFF)
#define CLEAR_BLACK RGB(0x00, 0x00, 0x00)

class GDIPlusRenderer
{
private:
	Gdiplus::GdiplusStartupInput gp_i;
	ULONG_PTR gp_ulong;

public:
	GDIPlusRenderer()
	{
		Gdiplus::GdiplusStartup(&this->gp_ulong, &gp_i, nullptr);
	}

	~GDIPlusRenderer()
	{
		Gdiplus::GdiplusShutdown(this->gp_ulong);
	}
};

typedef struct GDIPlusStringDesc
{
	int length;
	Gdiplus::Font* font;
	Gdiplus::PointF* ptf;
	Gdiplus::Brush* brush;
} GDIPlusStringDesc;

#endif