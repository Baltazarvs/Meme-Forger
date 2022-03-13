// Copyright (C) 2020 Baltazarus

#pragma once
#ifndef APP_CLASS
#define APP_CLASS
#include "Utilities.h"
#include "GDIPlusRenderer.h"

class Application
{
private:
	class WClass
	{
	private:
		WClass();
		~WClass();
		static WClass WCInstance;
		static constexpr const char* WClassName = LP_CLASS_NAME;
		HINSTANCE w_Inst;

	public:
		static HINSTANCE GetInstance() noexcept;
		static const char* GetWClassName() noexcept;
	};

public:
	Application(HWND w_Parent, const char* Caption, WTransform w_Transform);
	Application(HWND w_Parent, const char* Caption, int X, int Y, int Width, int Height);
	~Application();

	static void InitUI(HWND w_Handle, HINSTANCE w_Inst);
	static void InitCommand(HWND w_Handle, WPARAM wParam, LPARAM lParam);
	static void SetupStatusBar(HWND w_Handle, HINSTANCE w_Inst);

	static LRESULT __stdcall WndProcSetup(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam);
	static LRESULT __stdcall Thunk(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam);
	static LRESULT __stdcall WndProc(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam);

	static LRESULT __stdcall GroupBoxPosProc(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	static LRESULT __stdcall WndProc_TabControl(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	static LRESULT __stdcall WndProc_GroupStyle(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	
	static LRESULT __stdcall DlgProc_Actions(HWND w_Dlg, UINT Msg, WPARAM wParam, LPARAM lParam);
	static LRESULT __stdcall DlgProc_About(HWND w_Dlg, UINT Msg, WPARAM wParam, LPARAM lParam);

	void DrawMeme(HWND w_MemeHandle, HDC hdc);
	void DrawMemeString(const wchar_t* lpwstrMemeString, Gdiplus::Graphics& rgfx);
	void ClearMemeContext(COLORREF rgb);

	HWND GetHandle() const noexcept;

	WTransform GetWindowTransform() const noexcept;
	std::pair<int, int> GetWindowScale() const noexcept;
	std::pair<int, int> GetWindowPosition() const noexcept;
	
	void RunMessageLoop();

private:
	HWND w_Handle;
	WTransform w_Transform;
	GDIPlusRenderer gp_Renderer;
	static Gdiplus::Graphics* m_Gfx;
};

#endif