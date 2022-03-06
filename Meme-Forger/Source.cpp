// Copyright (C) 2020 Baltazarus

#include "Application.h"

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	Application App(nullptr, "Meme Forger v1.0", { 100, 100, 800, 600 });
	App.RunMessageLoop();
	return 0;
}