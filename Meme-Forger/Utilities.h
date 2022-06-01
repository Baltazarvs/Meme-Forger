// Copyright (C) 2020 - 2022 Baltazarus

#pragma once
#define _WIN32_WINNT 0x501
#define _WIN32_IE 0x0300

#ifndef __cplusplus
	#error C++ is required for this application!
#endif


#include <windows.h>
#include <CommCtrl.h>
#include <commctrl.inl>
#include <vector>
#include <deque>
#include <map>
#include <sstream>
#include <fstream>
#include <windowsx.h>
#include <gdiplus.h>
#include <algorithm>

#ifdef _MSC_VER
	#pragma comment(lib, "GDIPlus.lib")
	#pragma comment(lib, "comctl32.lib")
	#pragma comment(linker,"\"/manifestdependency:type='win32' \
	name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
	processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

	#pragma warning(disable: 4996)
#endif

#define LP_CLASS_NAME		L"Baltazarus - Meme Forger"

#define I_ERROR_CANNOT_PROCESS_MESSAGE			-0x01
#define I_ERROR_CANNOT_REGISTER_WCLASS			-0x02
#define I_ERROR_CANNOT_CREATE_WINDOW			-0x03

#define ID(ctl_id)								reinterpret_cast<HMENU>(ctl_id)

typedef struct
{
	int X, Y, Width, Height;
} WTransform, *LPWTransform;