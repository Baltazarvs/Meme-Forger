// Copyright (C) 2020 Baltazarus

#include "Application.h"
#include "resource.h"

Application::WClass Application::WClass::WCInstance;
Gdiplus::Graphics* Application::m_Gfx;

bool LV_InsertColumns(HWND w_lvHandle, std::vector<const wchar_t*> Columns);
bool LV_InsertItems(HWND w_lvHandle, int iItem, std::vector<const wchar_t*> Items);
bool IsXYOverMemeArea(int& x, int& y, RECT& memeAreaRect);
COLORREF GetColorFromDialog(HWND w_Handle, HINSTANCE w_Inst);
template <typename T> std::wstring ConvertToString(T val_to_str);
template <typename T> T ConvertToInt(const wchar_t* val_to_int);
template <typename T> std::wstring ConvertToHex(T val_to_hex);

// ============ Runtime Control Variables ===========
static int Runtime_MemeFormatWidth = 512;						// Format size for width
static int Runtime_MemeFormatHeight = 512;						// Format size for height
static std::size_t Runtime_CurrentImageSize = 0ull;				// Current image size in bytes.
static char Runtime_CurrentImagePath[MAX_PATH] = { };			// Current loaded meme image's filename.
static bool bRuntime_CursorIsOverMemeArea = false;				// Is cursor over meme area? If yes true :)
static bool bRuntime_EnableCoordinateSelection = false;			// User turned on coordinate selection mode.
static std::size_t Runtime_CurrentTextsAdded = 0ull;			// Count how many texts have been added to meme.
static COLORREF Runtime_customColors[16];						// Custom colors used for color dialog for meme text
static COLORREF Runtime_rgbCurrent = RGB(0xFF, 0xFF, 0xFF);		// Current meme text color RGB value.

static bool Runtime_ColorModeHexEnabled = false;				// If hexadecimal color mode is enabled in SETTINGS.

// ============ Control handle variables ============
static HWND w_TabControl = nullptr;
static HWND w_MemeArea = nullptr;
static HWND w_StatusBar = nullptr;
static HWND w_StringsTreeList = nullptr;

static HWND w_EditTextValue = nullptr;
static HWND w_ButtonAdd = nullptr;

static HWND w_GroupBoxPosition = nullptr;
static HWND w_StaticPosX = nullptr;
static HWND w_StaticPosY = nullptr;
static HWND w_EditPosX = nullptr;
static HWND w_EditPosY = nullptr;
static HWND w_ButtonToggleClickPositioning = nullptr;

static HWND w_GroupBoxStyle = nullptr;
static HWND w_EditR = nullptr;
static HWND w_EditG = nullptr;
static HWND w_EditB = nullptr;
static HWND w_ButtonSelectColor = nullptr;
static HWND w_StaticColorInspector = nullptr;
static HWND w_ButtonActions = nullptr;
// ==================================================

Application::WClass::WClass()
{
	WNDCLASSEXA wcex = { 0 };

	memset(&wcex, 0, sizeof(wcex));
	wcex.cbSize = sizeof(wcex);
	wcex.style = 0;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.lpfnWndProc = &Application::WndProcSetup;
	wcex.hInstance = Application::WClass::GetInstance();
	wcex.hCursor = LoadCursor(Application::WClass::GetInstance(), IDC_ARROW);
	wcex.hIcon = LoadIcon(Application::WClass::GetInstance(), IDI_APPLICATION);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcex.lpszClassName = Application::WClass::GetWClassName();
	wcex.lpszMenuName = MAKEINTRESOURCEA(IDR_MENUBAR);
	wcex.hIconSm = LoadIcon(this->GetInstance(), IDI_APPLICATION);

	if (!RegisterClassExA(&wcex))
		MessageBoxA(0, "Cannot Register Window Class!", "Error!", MB_OK | MB_ICONEXCLAMATION);
}

Application::WClass::~WClass()
{
	UnregisterClassA(this->GetWClassName(), this->GetInstance());
}

HINSTANCE Application::WClass::GetInstance() noexcept
{
	return WCInstance.w_Inst;
}

const char* Application::WClass::GetWClassName() noexcept
{
	return WCInstance.WClassName;
}

Application::Application(HWND w_Parent, const char* Caption, WTransform w_Transform)
 : w_Transform(w_Transform)
{
	w_Handle = CreateWindowExA(
		WS_EX_CLIENTEDGE,
		Application::WClass::GetWClassName(),
		Caption,
		WS_OVERLAPPEDWINDOW,
		w_Transform.X, w_Transform.Y, w_Transform.Width, w_Transform.Height,
		w_Parent, nullptr, Application::WClass::GetInstance(), this
	);

	ShowWindow(this->GetHandle(), SW_SHOWMAXIMIZED);
}

Application::Application(HWND w_Parent, const char* Caption, int X, int Y, int Width, int Height)
	: Application(w_Parent, Caption, { X, Y, Width, Height })
{ }

Application::~Application()
{
	DestroyWindow(this->GetHandle());
}

LRESULT __stdcall Application::WndProcSetup(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_NCCREATE)
	{
		const CREATESTRUCTA* const w_Create = reinterpret_cast<CREATESTRUCTA*>(lParam);
		Application* const w_App = reinterpret_cast<Application*>(w_Create->lpCreateParams);
		SetWindowLongPtrA(w_Handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(w_App));
		SetWindowLongPtrA(w_Handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Application::Thunk));
		return w_App->WndProc(w_Handle, Msg, wParam, lParam);
	}
	return DefWindowProcA(w_Handle, Msg, wParam, lParam);
}

LRESULT __stdcall Application::Thunk(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	Application* const w_App = reinterpret_cast<Application*>(GetWindowLongPtrA(w_Handle, GWLP_WNDPROC));
	return w_App->WndProc(w_Handle, Msg, wParam, lParam);
}

LRESULT __stdcall Application::WndProc(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HBRUSH defhbr = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
	static int coord_x = 0;
	static int coord_y = 0;

	switch (Msg)
	{
		case WM_CREATE:
		{
			InitUI(w_Handle, Application::WClass::GetInstance());
			SendMessageW(w_StatusBar, SB_SETTEXTW, 2u, reinterpret_cast<LPARAM>(L"No coord selected"));
			SetWindowSubclass(w_GroupBoxPosition, &Application::GroupBoxPosProc, 0u, 0u);
			SetWindowSubclass(w_TabControl, &Application::WndProc_TabControl, 0u, 0u);
			SetWindowSubclass(w_GroupBoxStyle, &Application::WndProc_GroupStyle, 0u, 0u);
			break;
		}
		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case ID_FILE_EXIT:
					DestroyWindow(w_Handle);
					break;
				case IDC_BUTTON_ADD:
				{
					PostQuitMessage(0);
					break;
				}
			}
			break;
		}
		case WM_SIZE:
		{
			SendMessageA(w_StatusBar, WM_SIZE, 0u, 0u);
			
			RECT wRect;
			GetClientRect(w_Handle, &wRect);


			RECT memeRect;
			MoveWindow(w_MemeArea, 0, 0, ::Runtime_MemeFormatWidth, ::Runtime_MemeFormatHeight, TRUE);
			GetClientRect(w_MemeArea, &memeRect);

			RECT sbRect;
			GetWindowRect(w_StatusBar, &sbRect);
			int status_bar_height = sbRect.bottom - sbRect.top;
			MoveWindow(w_TabControl, memeRect.right + 3, 0, wRect.right - memeRect.right - 3, wRect.bottom - status_bar_height, TRUE);
			
			int sb_first_part = wRect.right / 5;
			int sb_second_part = sb_first_part + (wRect.right / 10);
			int sb_third_part = sb_second_part + (wRect.right / 10);

			int sbparts[4] = { sb_first_part, sb_second_part, sb_third_part, -1 };
			SendMessage(w_StatusBar, SB_SETPARTS, (WPARAM)4u, reinterpret_cast<LPARAM>(sbparts));

			RECT tabRect;
			GetClientRect(w_TabControl, &tabRect);
			MoveWindow(w_StringsTreeList, 5, tabRect.bottom / 2 + 50, tabRect.right - 10, tabRect.bottom - (tabRect.bottom / 2) - 105, TRUE);
			
			RECT lvRect;
			GetClientRect(w_StringsTreeList, &lvRect);
			MoveWindow(w_ButtonActions, 5, tabRect.bottom / 2 + 350 + 2, lvRect.right - lvRect.left, 30, TRUE);

			MoveWindow(w_EditTextValue, 5, 35, tabRect.right - 110, 25, TRUE);
			
			RECT edRect;
			GetClientRect(w_EditTextValue, &edRect);
			MoveWindow(w_ButtonAdd, edRect.right + 10, 34, tabRect.right - edRect.right - 20, 27, TRUE);
			
			MoveWindow(w_GroupBoxPosition, 5, edRect.bottom + 50, tabRect.right / 2, 130, TRUE);
			
			RECT gposRect;
			GetClientRect(w_GroupBoxPosition, &gposRect);
			MoveWindow(w_StaticPosX, gposRect.right / 4, gposRect.bottom / 2 + 5 - 20, 20, 20, TRUE);
			MoveWindow(w_StaticPosY, gposRect.right / 2, gposRect.bottom / 2 + 5 - 20, 20, 20, TRUE);
			MoveWindow(w_EditPosX, (gposRect.right / 4) + 22, gposRect.bottom / 2 - 20, 50, 30, TRUE);
			MoveWindow(w_EditPosY, (gposRect.right / 2) + 22, gposRect.bottom / 2 - 20, 50, 30, TRUE);

			MoveWindow(
				w_ButtonToggleClickPositioning, 
				gposRect.left + 10,
				gposRect.bottom  - 40,
				gposRect.right - 20,
				30,
				TRUE
			);

			MoveWindow(
				w_GroupBoxStyle, 
				gposRect.right - gposRect.left + 15, 
				edRect.bottom + 50,
				tabRect.right - gposRect.right - 25,
				130,
				TRUE
			);

			RECT gpstyleRect;
			GetClientRect(w_GroupBoxStyle, &gpstyleRect);

			MoveWindow(
				w_EditR, 
				25, 
				gpstyleRect.bottom / 3 - 10, 
				50, 25, 
				TRUE
			);

			MoveWindow(
				w_EditG, 
				25 + 51, 
				gpstyleRect.bottom / 3 - 10, 
				50, 25, 
				TRUE
			);

			MoveWindow(
				w_EditB, 
				25 + 102, 
				gpstyleRect.bottom / 3 - 10, 
				50, 25, 
				TRUE
			);

			RECT blueRect;
			GetClientRect(w_EditB, &blueRect);

			MoveWindow(
				w_ButtonSelectColor, 
				24, 
				gpstyleRect.bottom / 3 + 26, 
				blueRect.right * 3 + 4, 
				35,
				TRUE
			);
			
			RECT btselRect;
			GetClientRect(w_ButtonSelectColor, &btselRect);

			MoveWindow(
				w_StaticColorInspector,
				(btselRect.right - btselRect.left) + 25,
				gpstyleRect.bottom / 3 - 10,
				(gpstyleRect.right - gpstyleRect.left) - (btselRect.right - btselRect.left) - 50, 
				blueRect.bottom,
				TRUE
			);

			RECT inspectRect;
			GetClientRect(w_StaticColorInspector, &inspectRect);

			MoveWindow(
				w_ButtonSelectColor, 
				24, 
				gpstyleRect.bottom / 3 + 26, 
				blueRect.right * 3 + 4 + ((inspectRect.right - inspectRect.left) + 4), 
				35,
				TRUE
			);
			return 0;
		}
		case WM_MOUSEMOVE:
		{
			coord_x = LOWORD(lParam);
			coord_y = HIWORD(lParam);

			RECT rendRect;
			GetClientRect(w_MemeArea, &rendRect);

			std::wostringstream oss;
			oss << '(' << coord_x << ',' << coord_y << ')';

			if(IsXYOverMemeArea(coord_x, coord_y, rendRect))
			{
				SendMessage(w_StatusBar, SB_SETTEXT, 2u, reinterpret_cast<LPARAM>(oss.str().c_str()));
				bRuntime_CursorIsOverMemeArea = true;

				if(::bRuntime_EnableCoordinateSelection)
				{
					static std::wostringstream oss;
					oss << coord_x;
					SendMessageW(w_EditPosX, WM_SETTEXT, 0u, reinterpret_cast<LPARAM>(oss.str().c_str()));
					oss.str(L"");
					oss << coord_y;
					SendMessageW(w_EditPosY, WM_SETTEXT, 0u, reinterpret_cast<LPARAM>(oss.str().c_str()));
					oss.str(L"");	
				}
			}
			else
			{
				if(bRuntime_CursorIsOverMemeArea)
				{
					SendMessageA(w_StatusBar, SB_SETTEXTA, 2u, reinterpret_cast<LPARAM>("No coord selected."));
					bRuntime_CursorIsOverMemeArea = false;
				}
			}
			break;
		}
		/*
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
            HDC hdc = BeginPaint(w_Handle, &ps);
			this->DrawMeme(w_MemeArea, hdc);
            FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));
            EndPaint(w_Handle, &ps);

			PAINTSTRUCT psa;
            hdc = BeginPaint(w_MemeArea, &psa);
			this->DrawMeme(w_MemeArea, hdc);
            EndPaint(w_Handle, &psa);
			break;
		}
		*/
		case WM_LBUTTONDOWN:
		{
			if(::bRuntime_EnableCoordinateSelection)
			{
				RECT memeRect;
				GetClientRect(w_MemeArea, &memeRect);
				if(::IsXYOverMemeArea(coord_x, coord_y, memeRect))
				{
					Button_SetCheck(w_ButtonToggleClickPositioning, BST_UNCHECKED);
					::bRuntime_EnableCoordinateSelection = false;
				}
				else
					MessageBoxA(w_Handle, "Click on meme image to select coordinates.", "Info", MB_OK);
			}
			break;
		}
		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetBkColor(hdc, RGB(0xFF, 0xFF, 0xFF));
			SetTextColor(hdc, RGB(0x00, 0x00, 0x00));
			return reinterpret_cast<INT_PTR>(defhbr);
		}
		case WM_CLOSE:
			DestroyWindow(w_Handle);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProcA(w_Handle, Msg, wParam, lParam);
	}
	return 0;
}

HWND Application::GetHandle() const noexcept
{
	return this->w_Handle;
}

WTransform Application::GetWindowTransform() const noexcept
{
	return this->w_Transform;
}

std::pair<int, int> Application::GetWindowScale() const noexcept
{
	return std::pair<int, int>(GetWindowTransform().Width, GetWindowTransform().Height);
}

std::pair<int, int> Application::GetWindowPosition() const noexcept
{
	return std::pair<int, int>(GetWindowTransform().X, GetWindowTransform().Y);
}

void Application::InitUI(HWND w_Handle, HINSTANCE w_Inst)
{
	DWORD defStyles = (WS_VISIBLE | WS_CHILD);

	w_MemeArea = CreateWindowA(
		WC_STATICA, nullptr,
		defStyles | WS_BORDER,
		0, 0, 512, 512,
		w_Handle, ID(IDC_MEME_AREA), w_Inst, nullptr
	);

	w_TabControl = CreateWindowW(
		WC_TABCONTROLW, nullptr,
		defStyles | TCS_TABS,
		0, 0, 0, 0,
		w_Handle, ID(IDC_TAB_CONTROL), w_Inst, nullptr
	);

	wchar_t tiebuff[255] = L"Text";
	TCITEMW tie;
	tie.mask = TCIF_TEXT;
	tie.pszText = tiebuff;
	TabCtrl_InsertItem(w_TabControl, 0, &tie);
	wcscpy(tiebuff, L"Image");
	TabCtrl_InsertItem(w_TabControl, 1, &tie);
	wcscpy(tiebuff, L"File");
	TabCtrl_InsertItem(w_TabControl, 2, &tie);

	Application::SetupStatusBar(w_Handle, w_Inst);

	DWORD lvexsty = (LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);
	w_StringsTreeList = CreateWindowExA(
		lvexsty, WC_LISTVIEWA, nullptr,
		defStyles | WS_BORDER | LVS_REPORT | LVS_EDITLABELS,
		15, 35, 300, 300,
		w_TabControl, ID(IDC_LIST_TEXT_TREE), w_Inst, nullptr
	);

	SendMessageA(w_StringsTreeList, LVM_SETEXTENDEDLISTVIEWSTYLE, lvexsty, lvexsty);

	std::vector<const wchar_t*> Columns { L"#", L"Text", L"Position", L"Action" };
	if(!LV_InsertColumns(w_StringsTreeList, Columns))
	{
		MessageBoxA(w_Handle, "Cannot initialize required control.", "Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(-1101);
	}
	SetWindowLongPtrA(w_StringsTreeList, GWLP_USERDATA, static_cast<LONG_PTR>(Columns.size()));

	w_EditTextValue = CreateWindowA(
		WC_EDITA, nullptr,
		defStyles | WS_BORDER | ES_CENTER,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_EDIT_MEME_TEXT), w_Inst, nullptr
	);

	SendMessage(w_EditTextValue, 0x1500 + 1, FALSE, reinterpret_cast<LPARAM>(L"Enter Text..."));

	HICON hAddIcon = (HICON)LoadImage(
		GetModuleHandle(nullptr), 
		MAKEINTRESOURCE(IDI_ICON_ADD),
		IMAGE_ICON,
		16, 16, LR_DEFAULTCOLOR
	);

	w_ButtonAdd = CreateWindowA(
		WC_BUTTONA, " Add",
		defStyles | BS_PUSHBUTTON,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_BUTTON_ADD), w_Inst, nullptr
	);
	
	SendMessage(
		w_ButtonAdd, BM_SETIMAGE, 
		static_cast<WPARAM>(IMAGE_ICON), 
		reinterpret_cast<LPARAM>(hAddIcon)
	);

	w_GroupBoxPosition = CreateWindowA(
		WC_BUTTONA, "Text Position",
		defStyles | BS_GROUPBOX,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_BUTTON_GROUP_POSITION), w_Inst, nullptr
	);

	w_StaticPosX = CreateWindowA(
		WC_STATICA, "X: ",
		defStyles,
		0, 0, 0, 0,
		w_GroupBoxPosition, nullptr, w_Inst, nullptr
	);

	w_StaticPosY = CreateWindowA(
		WC_STATICA, "Y: ",
		defStyles,
		0, 0, 0, 0,
		w_GroupBoxPosition, nullptr, w_Inst, nullptr
	);

	w_EditPosX = CreateWindowA(
		WC_EDITA, "0",
		defStyles | WS_BORDER | ES_NUMBER,
		0, 0, 0, 0,
		w_GroupBoxPosition, nullptr, w_Inst, nullptr
	);

	w_EditPosY = CreateWindowA(
		WC_EDITA, "0",
		defStyles | WS_BORDER | ES_NUMBER,
		0, 0, 0, 0,
		w_GroupBoxPosition, nullptr, w_Inst, nullptr
	);


	w_ButtonToggleClickPositioning = CreateWindowA(
		WC_BUTTONA, "Select position by click",
		defStyles | BS_AUTOCHECKBOX  | BS_PUSHLIKE,
		0, 0, 0, 0,
		w_GroupBoxPosition, ID(IDC_TOGGLE_BUTTON_SELECTION), w_Inst, nullptr
	);

	w_GroupBoxStyle = CreateWindowA(
		WC_BUTTONA, "Style",
		defStyles | BS_GROUPBOX,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_BUTTON_GROUP_STYLE), w_Inst, nullptr
	);

	w_EditR = CreateWindowA(
		WC_EDITA, "255",
		defStyles | WS_BORDER | ES_NUMBER | ES_READONLY,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_EDIT_R_COLOR), w_Inst, nullptr
	);

	w_EditG = CreateWindowA(
		WC_EDITA, "255",
		defStyles | WS_BORDER | ES_NUMBER | ES_READONLY,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_EDIT_G_COLOR), w_Inst, nullptr
	);

	w_EditB = CreateWindowA(
		WC_EDITA, "255",
		defStyles | WS_BORDER | ES_NUMBER | ES_READONLY,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_EDIT_B_COLOR), w_Inst, nullptr
	);
	
	w_ButtonSelectColor = CreateWindowA(
		WC_BUTTONA, "Select Color",
		defStyles | BS_PUSHBUTTON,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_BUTTON_SELECT_COLOR), w_Inst, nullptr
	);

	w_StaticColorInspector = CreateWindowA(
		WC_STATICA, "Color Review",
		defStyles | WS_BORDER,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_STATIC_COLOR_INSPECT), w_Inst, nullptr
	);

	w_ButtonActions = CreateWindowA(
		WC_BUTTONA, "Actions",
		defStyles | BS_PUSHBUTTON,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_BUTTON_ACTIONS), w_Inst, nullptr
	);

	const int controls_num = 13;
	HWND controlsList[controls_num] = { 
		w_MemeArea, w_TabControl, w_StatusBar, w_StringsTreeList, w_EditTextValue,
		w_StaticPosX, w_StaticPosY, w_EditPosX, w_EditPosY, w_EditR, w_EditG, w_EditB,
		w_StaticColorInspector
	};

	for(int i = 0; i < controls_num; ++i)
	{
		if(i >= 4)
		{
			HFONT hFont = CreateFontW(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, L"Tahoma");
			SendMessageW(controlsList[i], WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);
			continue;
		}
		SendMessageW(controlsList[i], WM_SETFONT, reinterpret_cast<WPARAM>((HFONT)GetStockObject(DEFAULT_GUI_FONT)), true);
	}
	return;
}

void Application::DrawMeme(HWND w_MemeHandle, HDC hdc)
{
	static Gdiplus::Graphics gfx(hdc);
	this->m_Gfx = &gfx;

	Gdiplus::Image img(L"45e.jpg");
	(*this->m_Gfx).DrawImage(&img, 0, 0, 512, 512); 
	return;
}

void Application::DrawMemeString(const wchar_t* lpwstrMemeString, Gdiplus::Graphics& rgfx)
{
	Gdiplus::SolidBrush sbr(Gdiplus::Color::Black);
	Gdiplus::FontFamily ffamily(L"Impact");
	Gdiplus::Font font(&ffamily, 24);
	Gdiplus::PointF ptf(0, 0);

	GDIPlusStringDesc dsc;
	dsc.length = -1;
	dsc.brush = &sbr;
	dsc.font = &font;
	dsc.ptf = &ptf;

	rgfx.DrawString(lpwstrMemeString, dsc.length, dsc.font, *dsc.ptf, dsc.brush);
	return;
}

void Application::ClearMemeContext(COLORREF rgb)
{
	RGBTRIPLE rgbt = { };
	rgbt.rgbtRed = GetRValue(rgb);
	rgbt.rgbtGreen = GetGValue(rgb);
	rgbt.rgbtBlue = GetBValue(rgb);
	(*this->m_Gfx).Clear(Gdiplus::Color(rgbt.rgbtRed, rgbt.rgbtGreen, rgbt.rgbtBlue));
	return;
}

void Application::SetupStatusBar(HWND w_Handle, HINSTANCE w_Inst)
{
	int* sbparts = new int[3];
	sbparts[0] = 300;
	sbparts[1] = 400;
	sbparts[2] = -1;

	w_StatusBar = CreateWindowW(
		STATUSCLASSNAMEW, nullptr,
		WS_VISIBLE | WS_CHILD | SBS_SIZEGRIP,
		0, 0, 0, 0,
		w_Handle, ID(IDC_STATUS_BAR), w_Inst, nullptr
	);

	SendMessage(w_StatusBar, SB_SETPARTS, (WPARAM)3u, reinterpret_cast<LPARAM>(sbparts));
	SendMessage(w_StatusBar, SB_SETTEXTW, (WPARAM)0u, reinterpret_cast<LPARAM>(L"No image opened."));
	SendMessage(w_StatusBar, SB_SETTEXTW, (WPARAM)1u, reinterpret_cast<LPARAM>(L"0 Bytes"));
	delete[] sbparts;
	return;
}

void Application::InitCommand(HWND w_Handle, WPARAM wParam, LPARAM lParam)
{
	switch(LOWORD(wParam))
	{
		case ID_FILE_EXIT:
			DestroyWindow(w_Handle);
			break;
	}
	return;
}

bool LV_InsertColumns(HWND w_lvHandle, std::vector<const wchar_t*> Columns)
{
	wchar_t buff[255];
	wcscpy(buff, Columns[0]);

	RECT wRect;
	GetClientRect(w_lvHandle, &wRect);

	LVCOLUMNW lvc = { 0 };
	
	memset(&lvc, 0, sizeof(lvc));
	lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
	lvc.iSubItem = 0;
	lvc.cx = wRect.right / 2;
	lvc.pszText = buff;

	for (std::size_t i = 0; i < Columns.size(); ++i)
	{
		wcscpy(buff, Columns[i]);
		lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
		lvc.iSubItem = i;
		if(i == 0)
			lvc.cx = 45;
		else
			lvc.cx = wRect.right / 2;
		lvc.pszText = buff;
		if (SendMessageW(w_lvHandle, LVM_INSERTCOLUMN, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&lvc)) == -1)
			return false;
	}
	return true;
}

bool LV_InsertItems(HWND w_lvHandle, int iItem, std::vector<const wchar_t*> Items)
{
	wchar_t buff[255];
	
	if ((Items.size() == GetWindowLongPtrA(w_lvHandle, GWLP_USERDATA)))
	{
		wcscpy(buff, Items[0]);
		LVITEMW lvi = { 0 };

		memset(&lvi, 0, sizeof(lvi));
		lvi.mask = LVIF_TEXT | LVIF_STATE;
		lvi.iItem = iItem;
		lvi.iSubItem = 0;
		lvi.pszText = buff;
		ListView_InsertItem(w_lvHandle, &lvi);

		for (std::size_t i = 0; i < Items.size(); ++i)
		{
			wcscpy(buff, (wchar_t*)Items[i]);
			lvi.mask = LVIF_TEXT;
			lvi.iItem = iItem;
			lvi.iSubItem = i;
			ListView_SetItem(w_StringsTreeList, &lvi);
		}
	}
	else
	{
		std::ostringstream oss;
		oss << "Cannot add " << Items.size() << " items in list with " << GetWindowLongPtrA(w_lvHandle, GWLP_USERDATA) << " columns!";
		MessageBoxA(GetParent(w_lvHandle), oss.str().c_str(), "Error!", MB_OK | MB_ICONERROR);
		return false;
	}
	return true;
}

LRESULT __stdcall Application::GroupBoxPosProc(
	HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData
)
{
	switch(Msg)
	{
		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDC_TOGGLE_BUTTON_SELECTION:
				{
					if(IsDlgButtonChecked(w_GroupBoxPosition, IDC_TOGGLE_BUTTON_SELECTION))
						bRuntime_EnableCoordinateSelection = true;
					else
						bRuntime_EnableCoordinateSelection = false;
					break;
				}
			}
			break;
		}
		case WM_CTLCOLORBTN:
			return reinterpret_cast<INT_PTR>(CreateSolidBrush(GetSysColor(COLOR_WINDOW)));
		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetBkColor(hdc, RGB(0xFF, 0xFF, 0xFF));
			SetTextColor(hdc, RGB(0x00, 0x00, 0x00));
			return reinterpret_cast<INT_PTR>(CreateSolidBrush(GetSysColor(COLOR_WINDOW)));
		}
		default:
			return DefSubclassProc(w_Handle, Msg, wParam, lParam);
	}
	return 0;
}

LRESULT __stdcall Application::WndProc_TabControl(
	HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData
)
{
	switch(Msg)
	{
		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDC_BUTTON_ADD:
				{
					std::wstring meme_text, str_pos_x, str_pos_y;
					std::wstring color_r, color_g, color_b;

					int len = SendMessage(w_EditTextValue, WM_GETTEXTLENGTH, 0u, 0u);
					if(len < 1)
					{
						MessageBoxA(
							GetParent(w_Handle),
							"Adding text is required.\nSpecify something.",
							"Text Required",
							MB_OK | MB_ICONINFORMATION
						);
						return 1;
					}

					wchar_t* buffer = new wchar_t[len + 1];
					GetWindowTextW(w_EditTextValue, buffer, len + 1);
					meme_text = buffer;
					delete[] buffer;

					len = GetWindowTextLengthW(w_EditPosX);
					buffer = new wchar_t[len + 1];
					GetWindowTextW(w_EditPosX, buffer, len + 1);
					str_pos_x = buffer;
					delete[] buffer;

					len = GetWindowTextLengthW(w_EditPosY);
					buffer = new wchar_t[len + 1];
					GetWindowTextW(w_EditPosY, buffer, len + 1);
					str_pos_y = buffer;
					delete[] buffer;

					HWND w_RGB[3] = { w_EditR, w_EditG, w_EditB };
					std::wstring* color_rgb[] = { &color_r, &color_g, &color_b };
					for (int i = 0; i < 3; ++i)
					{
						len = GetWindowTextLengthW(w_RGB[i]);
						buffer = new wchar_t[len + 1];
						GetWindowTextW(w_RGB[i], buffer, len + 1);
						*color_rgb[i] = buffer;
						delete[] buffer;
					}

					std::wstring str_pos = L"(" + str_pos_x + L"," + str_pos_y + L")";
					std::wstring str_color;

					if (::Runtime_ColorModeHexEnabled)
					{
						int rgb_r = ConvertToInt<int>(color_r.c_str());
						int rgb_g = ConvertToInt<int>(color_g.c_str());
						int rgb_b = ConvertToInt<int>(color_b.c_str());

						std::wstring color_r_hex = ConvertToHex<int>(rgb_r);
						std::wstring color_g_hex = ConvertToHex<int>(rgb_g);
						std::wstring color_b_hex = ConvertToHex<int>(rgb_b);

						str_color = L"#" + color_r_hex + color_g_hex + color_b_hex;
						std::transform(str_color.begin(), str_color.end(), str_color.begin(), ::toupper);
					}
					else
						str_color = L"RGB(" + color_r + L"," + color_g + L"," + color_b + L")";

					std::vector<const wchar_t*> Items;
					++::Runtime_CurrentTextsAdded;
					std::wstring index_to_str = ConvertToString<std::size_t>(::Runtime_CurrentTextsAdded).c_str();
					Items.push_back(index_to_str.c_str());
					Items.push_back(meme_text.c_str());
					Items.push_back(str_pos.c_str());
					Items.push_back(str_color.c_str());

					LV_InsertItems(w_StringsTreeList, 0, Items);

					SetWindowTextW(w_EditTextValue, nullptr);
					SetWindowTextW(w_EditPosX, L"0");
					SetWindowTextW(w_EditPosY, L"0");

					RECT lvRect;
					GetClientRect(w_StringsTreeList, &lvRect);
					break;
				}
			}	
			break;
		}
		case WM_NOTIFY:
		{
			if (((LPNMHDR)lParam)->code == LVN_COLUMNCLICK)
			{
				switch (((LPNMHDR)lParam)->idFrom)
				{
					case IDC_LIST_TEXT_TREE:
					{
						const char* explain_str = "# - text index\n"
												  "Text - value of text\n"
												  "Position - position of text on image\n"
												  "Action - modify current text or delete it";

						MessageBoxA(GetParent(w_Handle), explain_str, "Text Tree", MB_OK | MB_ICONINFORMATION);
						DialogBoxW(
							GetModuleHandleW(nullptr),
							MAKEINTRESOURCEW(IDD_ACTION),
							GetParent(w_Handle),
							reinterpret_cast<DLGPROC>(
								&Application::DlgProc_Actions
							)
						);
						break;
					}
				}
			}
			break;
		}
		default:
			return DefSubclassProc(w_Handle, Msg, wParam, lParam);
	}
	return 0;
}

LRESULT __stdcall Application::WndProc_GroupStyle(
	HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData
)
{
	static HBRUSH hbrcurrent = CreateSolidBrush(::Runtime_rgbCurrent);

	switch(Msg)
	{
		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDC_BUTTON_SELECT_COLOR:
				{
					COLORREF text_color = ::GetColorFromDialog(
						GetParent(w_Handle), 
						GetModuleHandle(nullptr)
					);

					BYTE red = GetRValue(text_color);
					BYTE green = GetGValue(text_color);
					BYTE blue = GetBValue(text_color);

					std::wostringstream woss;
					woss << red;
					SetWindowTextW(w_EditR, woss.str().c_str()); woss.str(L"");
					woss << green;
					SetWindowTextW(w_EditG, woss.str().c_str()); woss.str(L"");
					woss << blue;
					SetWindowTextW(w_EditB, woss.str().c_str()); woss.str(L"");
					break;
				}
			}
			break;
		}
		case WM_CTLCOLORBTN:
			return (LRESULT)CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));
		case WM_CTLCOLORSTATIC:
		{
			DWORD control_id = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
			if(control_id == IDC_STATIC_COLOR_INSPECT)
			{
				HDC hdc = reinterpret_cast<HDC>(wParam);
				SetTextColor(hdc, ::Runtime_rgbCurrent);
				SetBkColor(hdc, ::Runtime_rgbCurrent);
				return (LRESULT)CreateSolidBrush(::Runtime_rgbCurrent);
			}
			break;
		}
		default:
			return DefSubclassProc(w_Handle, Msg, wParam, lParam);
	}
	return 0;
}

LRESULT __stdcall Application::DlgProc_Actions(HWND w_Dlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	POINT cursorPos = { };
	switch(Msg)
	{
		case WM_INITDIALOG:
		{
			GetCursorPos(&cursorPos);
			MoveWindow(w_Dlg, cursorPos.x, cursorPos.y, 300, 150, TRUE);
			break;
		}
		case WM_LBUTTONDOWN:
		{
			SetCapture(w_Dlg);
			RECT dlgRect;
			GetWindowRect(w_Dlg, &dlgRect);

			POINT pt;
			GetCursorPos(&pt);
			
			bool coord_x_valid = (pt.x >= dlgRect.left && pt.x <= dlgRect.right);
			bool coord_y_valid = (pt.y >= dlgRect.top && pt.y <= dlgRect.bottom);
			bool bCheck = coord_x_valid && coord_y_valid;

			if(bCheck)
				break;
			else
				EndDialog(w_Dlg, 0);
			break;
		}
		case WM_LBUTTONUP:
			ReleaseCapture();
			break;
		case WM_CTLCOLORDLG:
		case WM_CTLCOLORBTN:
		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, RGB(0x00, 0x00, 0x00));
			SetBkColor(hdc, RGB(120, 174, 255));
			return (LRESULT)CreateSolidBrush(RGB(120, 174, 255));
		}
		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
				case IDC_BUTTON_DELETE:
				{
					break;
				}
				case IDC_BUTTON_MODIFY:
				{
					break;
				}
				case IDCANCEL:
					EndDialog(w_Dlg, IDCANCEL);
					break;
			}
			break;
		}
	}
	return 0;
}

bool IsXYOverMemeArea(int& x, int& y, RECT& memeAreaRect)
{
	bool coord_x_valid = (x >= memeAreaRect.left && x <= memeAreaRect.right);
	bool coord_y_valid = (y >= memeAreaRect.top && y <= memeAreaRect.bottom);
	return (coord_x_valid && coord_y_valid);
}

COLORREF GetColorFromDialog(HWND w_Handle, HINSTANCE w_Inst)
{
	CHOOSECOLOR lcc;

	memset(&lcc, 0, sizeof(CHOOSECOLOR));
	lcc.lStructSize = sizeof(CHOOSECOLOR);
	lcc.Flags = CC_FULLOPEN | CC_RGBINIT;
	lcc.hwndOwner = w_Handle;
	lcc.hInstance = nullptr;
	lcc.rgbResult = ::Runtime_rgbCurrent;
	lcc.lpCustColors = ::Runtime_customColors;

	if(ChooseColor(&lcc))
	{
		::Runtime_rgbCurrent = lcc.rgbResult;
		return lcc.rgbResult;
	}
	return ::Runtime_rgbCurrent;
}

void Application::RunMessageLoop()
{
	MSG Msg = { };
	while (GetMessageA(&Msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessageA(&Msg);
	}
	return;
}

template<typename T>
std::wstring ConvertToString(T val_to_str)
{
	static std::wstring res;
	std::wostringstream woss;
	woss << val_to_str;
	res = woss.str();
	return res;
}

template<typename T>
T ConvertToInt(const wchar_t* val_to_int)
{
	T int_res = 0;
	std::wstringstream wss;
	wss << val_to_int;
	wss >> int_res;
	return int_res;
}

template<typename T>
std::wstring ConvertToHex(T val_to_hex)
{
	std::wstring res;
	std::wstringstream wss;
	wss << std::hex << val_to_hex;
	res = wss.str();
	if (val_to_hex < 16)
	{
		std::wstring res_t(res);
		res = L"0";
		res.append(res_t);
	}
	return res;
}
