// Copyright (C) 2020 - 2022 Baltazarus

#include "Application.h"
#include "resource.h"

Application::WClass Application::WClass::WCInstance;
Gdiplus::Graphics* Application::m_Gfx;

#define PATH_SETTINGS_DEFAULT_FONTS		"settings\\default_fonts.txt"

struct MemeText;

std::wstring OpenFileWithDialog(const wchar_t* Filters, HWND w_Handle, int criteria);
bool LV_InsertColumns(HWND w_lvHandle, std::vector<const wchar_t*> Columns);
bool LV_InsertItems(HWND w_lvHandle, int iItem, std::vector<const wchar_t*> Items);
bool IsXYOverMemeArea(int& x, int& y, RECT& memeAreaRect);
COLORREF GetColorFromDialog(HWND w_Handle, HINSTANCE w_Inst);
template <typename T> std::wstring ConvertToString(T val_to_str);
template <typename T> T ConvertToInt(const wchar_t* val_to_int);
template <typename T> std::wstring ConvertToHex(T val_to_hex);
void ToggleMenuBarVisibility(HWND);
void ManageMultipleSyncKeys(MSG&);
bool HDCToFile(const wchar_t* FilePath, HDC Context, RECT Area, uint16_t BitsPerPixel = 24);

// ============ Runtime Control Variables ===========
static bool Runtime_MemeOpened = false;
static wchar_t Runtime_CurrentMemePath[MAX_PATH];
static int Runtime_MemeFormatWidth = 512;						// Format size for width
static int Runtime_MemeFormatHeight = 512;						// Format size for height
static std::size_t Runtime_CurrentImageSize = 0ull;				// Current image size in bytes.
static char Runtime_CurrentImagePath[MAX_PATH] = { };			// Current loaded meme image's filename.
static bool bRuntime_CursorIsOverMemeArea = false;				// Is cursor over meme area? If yes true :)
static bool bRuntime_EnableCoordinateSelection = false;			// User turned on coordinate selection mode.
static std::size_t Runtime_CurrentTextsAdded = 0ull;			// Count how many texts have been added to meme.
static COLORREF Runtime_customColors[16];						// Custom colors used for color dialog for meme text
static COLORREF Runtime_rgbCurrent = RGB(0xFF, 0xFF, 0xFF);		// Current meme text color RGB value.
static int index_item = 0;										// Index of current added item to Tree List.
static bool Runtime_ColorModeHexEnabled = false;				// If hexadecimal color mode is enabled in SETTINGS.

static bool bRuntime_ShowStatusBar = true;
static bool bRuntime_ShowMenuBar = true;

static HMENU Runtime_hMenu = nullptr;

std::vector<MemeText> Runtime_MemeTexts;

static bool Runtime_MemeTextSelected = false;
static std::size_t Runtime_MemeTextSelectedIndex = 0;
LOGFONTW Runtime_LogFont = { 0 };

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

static HWND w_EditLoadedMeme = nullptr;
static HWND w_ButtonBrowse = nullptr;

static HWND w_GroupBoxFont = nullptr;
static HWND w_ComboFont = nullptr;
static HWND w_EditFontSize = nullptr;
static HWND w_AdvancedFontSelect = nullptr;

static HWND w_ExportMeme = nullptr;
// ==================================================

struct MemeText
{
	int index;
	Gdiplus::Rect text_rect;
	COLORREF text_color;
	LOGFONTW log_font;
	std::wstring text;
	bool bTransparent;
};

Application::WClass::WClass()
{
	WNDCLASSEXW wcex = { 0 };

	memset(&wcex, 0, sizeof(wcex));
	wcex.cbSize = sizeof(wcex);
	wcex.style = 0;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.lpfnWndProc = &Application::WndProcSetup;
	wcex.hInstance = Application::WClass::GetInstance();
	wcex.hCursor = LoadCursorW(Application::WClass::GetInstance(), IDC_ARROW);
	wcex.hIcon = LoadIconW(Application::WClass::GetInstance(), IDI_APPLICATION);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcex.lpszClassName = Application::WClass::GetWClassName();
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDR_MENUBAR);
	wcex.hIconSm = LoadIconW(this->GetInstance(), IDI_APPLICATION);

	if (!RegisterClassExW(&wcex))
		MessageBoxW(0, L"Cannot Register Window Class!", L"Error!", MB_OK | MB_ICONEXCLAMATION);
}

Application::WClass::~WClass()
{
	UnregisterClassW(this->GetWClassName(), this->GetInstance());
}

HINSTANCE Application::WClass::GetInstance() noexcept
{
	return WCInstance.w_Inst;
}

const wchar_t* Application::WClass::GetWClassName() noexcept
{
	return WCInstance.WClassName;
}

Application::Application(HWND w_Parent, const wchar_t* Caption, WTransform w_Transform)
 : w_Transform(w_Transform)
{
	w_Handle = CreateWindowExW(
		WS_EX_CLIENTEDGE,
		Application::WClass::GetWClassName(),
		Caption,
		WS_OVERLAPPEDWINDOW,
		w_Transform.X, w_Transform.Y, w_Transform.Width, w_Transform.Height,
		w_Parent, nullptr, Application::WClass::GetInstance(), this
	);

	ShowWindow(this->GetHandle(), SW_SHOWMAXIMIZED);
}

Application::Application(HWND w_Parent, const wchar_t* Caption, int X, int Y, int Width, int Height)
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
		SetWindowLongPtrW(w_Handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(w_App));
		SetWindowLongPtrW(w_Handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Application::Thunk));
		return w_App->WndProc(w_Handle, Msg, wParam, lParam);
	}
	return DefWindowProcW(w_Handle, Msg, wParam, lParam);
}

LRESULT __stdcall Application::Thunk(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	Application* const w_App = reinterpret_cast<Application*>(GetWindowLongPtrW(w_Handle, GWLP_WNDPROC));
	return w_App->WndProc(w_Handle, Msg, wParam, lParam);
}

LRESULT __stdcall Application::WndProc(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static HMENU hMenu = GetMenu(w_Handle);
	static int coord_x = 0;
	static int coord_y = 0;
	static HBRUSH defhbr = CreateSolidBrush(GetSysColor(COLOR_WINDOW));

	switch (Msg)
	{
		case WM_CREATE:
		{
			InitUI(w_Handle, Application::WClass::GetInstance());
			::Runtime_hMenu = GetMenu(w_Handle);

			SendMessageW(w_StatusBar, SB_SETTEXTW, 2u, reinterpret_cast<LPARAM>(L"No coord selected"));
			SetWindowSubclass(w_GroupBoxPosition, &Application::GroupBoxPosProc, 0u, 0u);
			SetWindowSubclass(w_TabControl, &Application::WndProc_TabControl, 0u, 0u);
			SetWindowSubclass(w_GroupBoxStyle, &Application::WndProc_GroupStyle, 0u, 0u);
			SetWindowSubclass(w_MemeArea, &Application::WndProc_MemeArea, 0u, 0u);
			SetWindowSubclass(w_GroupBoxFont, &Application::WndProc_GroupFont, 0u, 0u);
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
				case ID_HELP_ABOUT:
				{
					DialogBox(
						Application::WClass::GetInstance(),
						MAKEINTRESOURCE(IDD_ABOUT),
						w_Handle,
						reinterpret_cast<DLGPROC>(&Application::DlgProc_About)
					);
					break;
				}
				case ID_VIEW_STATUS_BAR:
				{
					if (bRuntime_ShowStatusBar)
					{
						ShowWindow(w_StatusBar, SW_HIDE);
						bRuntime_ShowStatusBar = false;
						CheckMenuItem(GetMenu(w_Handle), ID_VIEW_STATUS_BAR, (UINT)bRuntime_ShowStatusBar);
					}
					else
					{
						ShowWindow(w_StatusBar, SW_SHOWDEFAULT);
						bRuntime_ShowStatusBar = true;
						CheckMenuItem(GetMenu(w_Handle), ID_VIEW_STATUS_BAR, MF_CHECKED);
					}
					break;
				}
				case ID_VIEW_MENUBAR:
				{
					ToggleMenuBarVisibility(w_Handle);
					break;
				}

				case IDC_BUTTON_MEME_BROWSE:
				{
					std::wstring path = OpenFileWithDialog(
						L"JPG Image\0*.jpg\0"
						L"PNG Image\0*.png\0"
						L"Bitmap Image\0*.bmp\0",
						w_Handle,
						0
					);

					if (path.length() > 1)
						Runtime_MemeOpened = true;
					else
						Runtime_MemeOpened = false;

					wcscpy(Runtime_CurrentMemePath, path.c_str());
					SetWindowTextW(w_EditLoadedMeme, path.c_str());
					SendMessageW(w_StatusBar, SB_SETTEXTW, 0u, reinterpret_cast<LPARAM>(path.c_str()));

					RECT memeRect;
					GetClientRect(w_MemeArea, &memeRect);
					InvalidateRect(w_MemeArea, &memeRect, TRUE);
					UpdateWindow(w_MemeArea);
					break;
				}
				case IDC_BUTTON_EXPORT_MEME:
				{
					if (!Runtime_MemeOpened)
					{
						MessageBoxW(0, L"No image was imported.", L"Export", MB_ICONINFORMATION | MB_OK);
						break;
					}

					std::wstring path = OpenFileWithDialog(
						L"JPG Image\0*.jpg\0"
						L"PNG Image\0*.png\0"
						L"Bitmap Image\0*.bmp\0",
						w_Handle, 2
					);

					if (path.length() < 1)
						break;

					HDC hdc = GetDC(w_MemeArea);
					RECT mRect;
					GetClientRect(w_MemeArea, &mRect);
					HDCToFile(path.c_str(), hdc, mRect);
					ReleaseDC(w_MemeArea, hdc);
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
			MoveWindow(w_MemeArea, 5, 0, ::Runtime_MemeFormatWidth, ::Runtime_MemeFormatHeight, TRUE);
			GetClientRect(w_MemeArea, &memeRect);

			RECT sbRect;
			GetWindowRect(w_StatusBar, &sbRect);
			int status_bar_height = sbRect.bottom - sbRect.top;
			MoveWindow(w_TabControl, memeRect.right + 10, 0, wRect.right - memeRect.right - 3, wRect.bottom - status_bar_height, TRUE);
			
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


			MoveWindow(
				w_EditLoadedMeme, 
				memeRect.left + 5, memeRect.bottom - memeRect.top + 10, 
				memeRect.right - memeRect.left - 100, 30, TRUE
			);

			MoveWindow(
				w_ButtonBrowse, 
				memeRect.left + (memeRect.right - memeRect.left - 100) + 2 + 5, memeRect.bottom - memeRect.top + 10 - 1, 
				100, 32, TRUE
			);

			RECT styRect;
			GetClientRect(w_GroupBoxStyle, &styRect);

			int lvheight = lvRect.bottom - lvRect.top;

			MoveWindow(
				w_GroupBoxFont,
				gposRect.right - gposRect.left + 15,
				edRect.bottom + 50 + (styRect.bottom - styRect.top) + 15,
				tabRect.right - gposRect.right - 25,
				tabRect.bottom - lvheight - 300,
				TRUE
			);

			RECT brRect;
			GetClientRect(w_ButtonBrowse, &brRect);

			MoveWindow(
				w_ExportMeme,
				memeRect.left + 4,
				memeRect.bottom - memeRect.top + (brRect.bottom - brRect.top) + 10,
				(memeRect.right - memeRect.left - 100) + 3 + (brRect.right - brRect.left),
				30, TRUE
			);

			RECT fnRect;
			GetClientRect(w_GroupBoxFont, &fnRect);
			MoveWindow(
				w_ComboFont, 
				15, 25, 
				fnRect.right - fnRect.left - 100,
				30,
				TRUE
			);

			RECT cbfRect;
			GetClientRect(w_ComboFont, &cbfRect);

			MoveWindow(
				w_EditFontSize,
				cbfRect.right - cbfRect.left + 20,
				25,
				(fnRect.right - fnRect.left) - (cbfRect.right - cbfRect.left) - 27,
				21,
				TRUE
			);

			MoveWindow(
				w_AdvancedFontSelect,
				14, 50,
				fnRect.right - fnRect.left - 19,
				30,
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
					HCURSOR hCursor = reinterpret_cast<HCURSOR>(
						LoadImage(
							GetModuleHandle(nullptr), 
							MAKEINTRESOURCE(IDI_ICON_SELECT_POS),
							IMAGE_ICON,
							32, 32,
							LR_VGACOLOR
						)
					);
					SetCursor(hCursor);
					DestroyCursor(hCursor);

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
					SetCursor(LoadCursor(WClass::GetInstance(), IDC_ARROW));
					SendMessageA(w_StatusBar, SB_SETTEXTA, 2u, reinterpret_cast<LPARAM>("No coord selected."));
					bRuntime_CursorIsOverMemeArea = false;
				}
			}
			break;
		}
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
			HDC hdc = GetDC(reinterpret_cast<HWND>(lParam));
			SetBkColor(hdc, RGB(0xFF, 0xFF, 0xFF));
			SetTextColor(hdc, RGB(0x00, 0x00, 0x00));
			return reinterpret_cast<INT_PTR>(defhbr);
		}
		case WM_CLOSE:
			DestroyWindow(w_Handle);
			break;
		case WM_DESTROY:
			DeleteObject(defhbr);
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProcW(w_Handle, Msg, wParam, lParam);
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

	w_MemeArea = CreateWindowW(
		WC_STATICW, nullptr,
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
	w_StringsTreeList = CreateWindowExW(
		lvexsty, WC_LISTVIEWW, nullptr,
		defStyles | WS_BORDER | LVS_REPORT, // | LVS_EDITLABELS
		15, 35, 300, 300,
		w_TabControl, ID(IDC_LIST_TEXT_TREE), w_Inst, nullptr
	);

	SendMessageW(w_StringsTreeList, LVM_SETEXTENDEDLISTVIEWSTYLE, lvexsty, lvexsty);

	std::vector<const wchar_t*> Columns { L"#", L"Text", L"Position", L"Action" };
	if(!LV_InsertColumns(w_StringsTreeList, Columns))
	{
		MessageBoxW(w_Handle, L"Cannot initialize required control.", L"Error", MB_OK | MB_ICONERROR);
		PostQuitMessage(-1101);
	}
	SetWindowLongPtrW(w_StringsTreeList, GWLP_USERDATA, static_cast<LONG_PTR>(Columns.size()));

	w_EditTextValue = CreateWindowW(
		WC_EDITW, nullptr,
		defStyles | WS_BORDER | ES_CENTER,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_EDIT_MEME_TEXT), w_Inst, nullptr
	);

	SendMessageW(w_EditTextValue, 0x1500 + 1, FALSE, reinterpret_cast<LPARAM>(L"Enter Text..."));

	HICON hAddIcon = (HICON)LoadImageW(
		GetModuleHandle(nullptr), 
		MAKEINTRESOURCE(IDI_ICON_ADD),
		IMAGE_ICON,
		16, 16, LR_DEFAULTCOLOR
	);

	w_ButtonAdd = CreateWindowW(
		WC_BUTTONW, L" Add",
		defStyles | BS_PUSHBUTTON,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_BUTTON_ADD), w_Inst, nullptr
	);
	
	SendMessageW(
		w_ButtonAdd, BM_SETIMAGE, 
		static_cast<WPARAM>(IMAGE_ICON), 
		reinterpret_cast<LPARAM>(hAddIcon)
	);

	w_GroupBoxPosition = CreateWindowW(
		WC_BUTTONW, L"Text Position",
		defStyles | BS_GROUPBOX,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_BUTTON_GROUP_POSITION), w_Inst, nullptr
	);

	w_StaticPosX = CreateWindowW(
		WC_STATICW, L"X: ",
		defStyles,
		0, 0, 0, 0,
		w_GroupBoxPosition, nullptr, w_Inst, nullptr
	);

	w_StaticPosY = CreateWindowW(
		WC_STATICW, L"Y: ",
		defStyles,
		0, 0, 0, 0,
		w_GroupBoxPosition, nullptr, w_Inst, nullptr
	);

	w_EditPosX = CreateWindowW(
		WC_EDITW, L"0",
		defStyles | WS_BORDER | ES_NUMBER,
		0, 0, 0, 0,
		w_GroupBoxPosition, nullptr, w_Inst, nullptr
	);

	w_EditPosY = CreateWindowW(
		WC_EDITW, L"0",
		defStyles | WS_BORDER | ES_NUMBER,
		0, 0, 0, 0,
		w_GroupBoxPosition, nullptr, w_Inst, nullptr
	);


	w_ButtonToggleClickPositioning = CreateWindowW(
		WC_BUTTONW, L"Select position by click",
		defStyles | BS_AUTOCHECKBOX  | BS_PUSHLIKE,
		0, 0, 0, 0,
		w_GroupBoxPosition, ID(IDC_TOGGLE_BUTTON_SELECTION), w_Inst, nullptr
	);

	w_GroupBoxStyle = CreateWindowW(
		WC_BUTTONW, L"Style",
		defStyles | BS_GROUPBOX,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_BUTTON_GROUP_STYLE), w_Inst, nullptr
	);

	w_EditR = CreateWindowW(
		WC_EDITW, L"255",
		defStyles | WS_BORDER | ES_NUMBER | ES_READONLY,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_EDIT_R_COLOR), w_Inst, nullptr
	);

	w_EditG = CreateWindowW(
		WC_EDITW, L"255",
		defStyles | WS_BORDER | ES_NUMBER | ES_READONLY,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_EDIT_G_COLOR), w_Inst, nullptr
	);

	w_EditB = CreateWindowW(
		WC_EDITW, L"255",
		defStyles | WS_BORDER | ES_NUMBER | ES_READONLY,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_EDIT_B_COLOR), w_Inst, nullptr
	);
	
	w_ButtonSelectColor = CreateWindowW(
		WC_BUTTONW, L"Select Color",
		defStyles | BS_PUSHBUTTON,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_BUTTON_SELECT_COLOR), w_Inst, nullptr
	);

	w_StaticColorInspector = CreateWindowW(
		WC_STATICW, nullptr,
		defStyles | WS_BORDER,
		0, 0, 0, 0,
		w_GroupBoxStyle, ID(IDC_STATIC_COLOR_INSPECT), w_Inst, nullptr
	);

	w_ButtonActions = CreateWindowW(
		WC_BUTTONW, L"Actions",
		defStyles | BS_PUSHBUTTON,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_BUTTON_ACTIONS), w_Inst, nullptr
	);

	w_EditLoadedMeme = CreateWindowW(
		WC_EDITW, nullptr,
		WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
		0, 0, 0, 0,
		w_Handle, ID(IDC_EDIT_MEME_LOADED_FNAME), w_Inst, nullptr
	);

	w_ButtonBrowse = CreateWindowW(
		WC_BUTTONW, L"Browse",
		WS_VISIBLE | WS_CHILD,
		0, 0, 0, 0,
		w_Handle, ID(IDC_BUTTON_MEME_BROWSE), w_Inst, nullptr
	);

	w_GroupBoxFont = CreateWindowW(
		WC_BUTTONW, L"Font",
		defStyles | BS_GROUPBOX,
		0, 0, 0, 0,
		w_TabControl, ID(IDC_BUTTON_GROUP_FONT), w_Inst, nullptr
	);

	w_EditFontSize = CreateWindowW(
		WC_EDITW, nullptr,
		defStyles | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
		0, 0, 0, 0,
		w_GroupBoxFont, ID(IDC_BUTTON_EDIT_FONT_SIZE), w_Inst, nullptr
	);

	w_AdvancedFontSelect = CreateWindowW(
		WC_BUTTONW, L"Select",
		defStyles | BS_PUSHBUTTON,
		0, 0, 0, 0,
		w_GroupBoxFont, ID(IDC_BUTTON_ADVANCED_FONT), w_Inst, nullptr
	);

	w_ExportMeme = CreateWindowW(
		WC_BUTTONW, L"Export",
		defStyles | BS_PUSHBUTTON,
		0, 0, 0, 0,
		w_Handle, ID(IDC_BUTTON_EXPORT_MEME), w_Inst, nullptr
	);

	w_ComboFont = CreateWindowW(
		WC_COMBOBOXW, nullptr,
		defStyles | CBS_DROPDOWNLIST,
		0, 0, 0, 0,
		w_GroupBoxFont, ID(IDC_COMBO_SELECT_FONT), w_Inst, nullptr
	);

	w_AdvancedFontSelect = CreateWindowW(
		WC_BUTTONW, L"Choose Font...",
		defStyles | BS_PUSHBUTTON,
		0, 0, 0, 0,
		w_GroupBoxFont, ID(IDC_BUTTON_SELECT_FONT), w_Inst, nullptr
	);

	std::wifstream file;
	file.open(PATH_SETTINGS_DEFAULT_FONTS);
	if (file.is_open())
	{
		std::wstring line;
		while (std::getline(file, line))
			ComboBox_AddString(w_ComboFont, line.c_str());
		file.close();
	}

	HWND controlsList[] = { 
		w_MemeArea, w_TabControl, w_StatusBar, w_StringsTreeList, w_EditTextValue,
		w_StaticPosX, w_StaticPosY, w_EditPosX, w_EditPosY, w_EditR, w_EditG, w_EditB,
		w_StaticColorInspector, w_EditLoadedMeme, w_ButtonBrowse, w_GroupBoxFont,
		w_EditFontSize, w_AdvancedFontSelect, w_ExportMeme, w_ComboFont, w_EditFontSize
	};

	HFONT hFont = nullptr;
	for(int i = 0; i < (sizeof(controlsList) / sizeof(controlsList[0])); ++i)
	{
		hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		if (controlsList[i] == w_EditLoadedMeme)
		{
			DeleteObject(hFont);
			hFont = CreateFontW(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, L"Consolas");
			SendMessageW(controlsList[i], WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);
			continue;
		}
		if(i >= 4 && i < 18)
		{
			DeleteObject(hFont);
			hFont = CreateFontW(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, L"Tahoma");
			SendMessageW(controlsList[i], WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);
			if(i != 14)
				continue;
		}
		
		SendMessageW(controlsList[i], WM_SETFONT, reinterpret_cast<WPARAM>(hFont), true);
	}
	DeleteObject(hFont);

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

std::wstring OpenFileWithDialog(const wchar_t* Filters, HWND w_Handle, int criteria)
{
	OPENFILENAMEW ofn = { };
	wchar_t* _Path = new wchar_t[MAX_PATH];

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = w_Handle;
	ofn.lpstrFilter = Filters;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = _Path;
	ofn.lpstrFile[0] = '\0';
	if (criteria == 1) ofn.lpstrTitle = L"Open File";
	else ofn.lpstrTitle = L"Save File";
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = (OFN_EXPLORER | OFN_PATHMUSTEXIST);

	if (criteria == 1)
	{
		if (!GetOpenFileNameW(&ofn))
			return std::wstring(L"\0");
	}
	else
	{
		if (!GetSaveFileNameW(&ofn))
			return std::wstring(L"\0");
	}

	std::wstring __Path(_Path);
	delete[] _Path;
	return __Path;
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
			HDC hdc = GetDC(reinterpret_cast<HWND>(lParam));
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

					LV_InsertItems(w_StringsTreeList, index_item, Items);
					++index_item;

					SetWindowTextW(w_EditTextValue, nullptr);
					SetWindowTextW(w_EditPosX, L"0");
					SetWindowTextW(w_EditPosY, L"0");

					RECT lvRect;
					GetClientRect(w_StringsTreeList, &lvRect);

					Gdiplus::Rect current_rect;
					current_rect.X = std::stoi(str_pos_x);
					current_rect.Y = std::stoi(str_pos_y);
					current_rect.Width = 300;
					current_rect.Height = 200;

					len = GetWindowTextLengthW(w_ComboFont) + 1;
					buffer = new wchar_t[len + 1];
					GetWindowTextW(w_ComboFont, buffer, len);
					wcscpy(Runtime_LogFont.lfFaceName, buffer);
					delete[] buffer;

					MemeText memeTextObj;
					memeTextObj.index = index_item;
					memeTextObj.text = meme_text.c_str();
					memeTextObj.text_color = Runtime_rgbCurrent;
					memeTextObj.log_font = Runtime_LogFont;
					//memeTextObj.font = (HFONT)GetStockObject(DEFAULT_GUI_FONT); // TODO: This is test. Remove!
					memeTextObj.text_rect = current_rect;
					memeTextObj.bTransparent = true;
					Runtime_MemeTexts.push_back(memeTextObj);

					RECT memeRect;
					GetClientRect(w_MemeArea, &memeRect);
					InvalidateRect(w_MemeArea, &memeRect, TRUE);
					UpdateWindow(w_MemeArea);
					break;
				}
			}	
			break;
		}
		case WM_NOTIFY:
		{
			if (reinterpret_cast<LPNMHDR>(lParam)->code == LVN_KEYDOWN)
			{
				NMLVKEYDOWN* nmlvk;
				nmlvk = reinterpret_cast<NMLVKEYDOWN*>(lParam);
				if (nmlvk->wVKey == 0x41)
				{
					if (ListView_GetSelectedCount(w_StringsTreeList) < 1)
					{
						MessageBoxW(w_Handle, L"No text was selected.", L"No Selection", MB_ICONINFORMATION | MB_OK);
						return 1;
					}

					Runtime_MemeTextSelected = true;
					::Runtime_MemeTextSelectedIndex = static_cast<std::size_t>(ListView_GetNextItem(w_StringsTreeList, -1, LVNI_SELECTED));

					DialogBoxW(
						Application::WClass::GetInstance(),
						MAKEINTRESOURCEW(IDD_ACTION),
						w_Handle,
						reinterpret_cast<DLGPROC>(&Application::DlgProc_Actions)
					);
				}
			}

			if (reinterpret_cast<LPNMHDR>(lParam)->code == LVN_COLUMNCLICK)
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

					RECT inspectRect;
					GetClientRect(w_StaticColorInspector, &inspectRect);
					InvalidateRect(w_StaticColorInspector, &inspectRect, TRUE);
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
				HDC hdc = GetDC(reinterpret_cast<HWND>(lParam));
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

LRESULT __stdcall Application::WndProc_GroupFont(
	HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData
)
{
	switch (Msg)
	{
		case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDC_BUTTON_SELECT_FONT)
			{
				CHOOSEFONTW cfn = { 0 };
				cfn.lStructSize = sizeof(CHOOSEFONT);
				cfn.hInstance = Application::WClass::GetInstance();
				cfn.Flags = CF_NOSCRIPTSEL | CF_INITTOLOGFONTSTRUCT | CF_LIMITSIZE;
				cfn.lpLogFont = &Runtime_LogFont;
				cfn.hwndOwner = w_Handle;
				cfn.rgbColors = ::Runtime_rgbCurrent;
				cfn.hDC = nullptr;
				cfn.nSizeMin = 8;
				cfn.nSizeMax = 50;
				cfn.nFontType = REGULAR_FONTTYPE;

				ChooseFontW(&cfn);

				ComboBox_AddString(w_ComboFont, Runtime_LogFont.lfFaceName);
				ComboBox_SetCurSel(w_ComboFont, ComboBox_GetCount(w_ComboFont) - 1);
				SetWindowTextW(w_EditFontSize, std::to_wstring(cfn.iPointSize / 10).c_str());
			}
			break;
		}
		default:
			return DefSubclassProc(w_Handle, Msg, wParam, lParam);
	}
	return 0;
}

LRESULT __stdcall Application::WndProc_MemeArea(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (Msg)
	{

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(w_Handle, &ps);

			RECT wRect;
			GetClientRect(w_Handle, &wRect);

			Gdiplus::Rect wrct(0, 0, wRect.right - wRect.left, wRect.bottom - wRect.top);

			std::wostringstream woss;
			woss << Runtime_CurrentMemePath;

			if (woss.str().length() < 1)
			{
				EndPaint(w_Handle, &ps);
				return 1;
			}

			Gdiplus::Graphics gfx(hdc);
			Gdiplus::Image img(woss.str().c_str());
			gfx.DrawImage(&img, wrct);

			if (Runtime_MemeTexts.size() != 0)
			{
				for (std::size_t i = 0ull; i < Runtime_MemeTexts.size(); ++i)
				{
					Gdiplus::Rect rect_t = Runtime_MemeTexts[i].text_rect;
					RECT trect = { rect_t.GetLeft(), rect_t.GetTop(), rect_t.GetRight(), rect_t.GetBottom() };

					if(Runtime_MemeTexts[i].bTransparent)
						SetBkMode(hdc, TRANSPARENT);
					SetTextColor(hdc, Runtime_MemeTexts[i].text_color);

					HFONT hFont = CreateFontIndirectW(&Runtime_MemeTexts[i].log_font);

					SelectObject(hdc, hFont);
					DrawTextW(
						hdc, Runtime_MemeTexts[i].text.c_str(),
						Runtime_MemeTexts[i].text.length(),
						&trect, 0
					);
					DeleteObject(hFont);
				}
			}

			EndPaint(w_Handle, &ps);
			gfx.ReleaseHDC(hdc);
			break;
		}
		default:
			return DefSubclassProc(w_Handle, Msg, wParam, lParam);
	}
	return 0;
}

LRESULT __stdcall Application::WndProc_ColorReview(HWND w_Handle, UINT Msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	switch (Msg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(w_Handle, &ps);
			HBRUSH hbr = CreateSolidBrush((COLORREF)dwRefData);
			RECT wRect;
			GetClientRect(w_Handle, &wRect);
			FillRect(hdc, &wRect, hbr);
			EndPaint(w_Handle, &ps);
			DeleteObject(hbr);
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
					ListView_DeleteItem(w_StringsTreeList, ::Runtime_MemeTextSelectedIndex);
					::Runtime_MemeTexts.erase(::Runtime_MemeTexts.begin() + ::Runtime_MemeTextSelectedIndex);

					RECT memeRect;
					GetClientRect(w_MemeArea, &memeRect);
					InvalidateRect(w_MemeArea, &memeRect, TRUE);
					UpdateWindow(w_MemeArea);

					if (ListView_GetItemCount(w_StringsTreeList) == 0)
						--::Runtime_CurrentTextsAdded = 0;
					else
					{
						--::Runtime_CurrentTextsAdded;
						--::index_item = Runtime_MemeTexts.size();
					}

					std::wstring str_temp;
					for (std::size_t i = 0ull; i < Runtime_MemeTexts.size(); ++i)
					{
						str_temp = std::to_wstring(i + 1).c_str();
						ListView_SetItemText(w_StringsTreeList, i, 0, (wchar_t*)str_temp.c_str());
					}
				EndDialog(w_Dlg, 0);
					break;
				}
				case IDC_BUTTON_MODIFY:
				{
					EndDialog(w_Dlg, IDC_BUTTON_MODIFY);
					DialogBoxW(
						Application::WClass::GetInstance(),
						MAKEINTRESOURCEW(IDD_MODIFYTEXT),
						GetParent(w_Dlg),
						reinterpret_cast<DLGPROC>(&Application::DlgProc_Modify)
					);
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

LRESULT __stdcall Application::DlgProc_About(HWND w_Dlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HWND w_StaticBy = GetDlgItem(w_Dlg, IDC_STATIC_ABOUT_BY);
	HWND w_StaticDesc = GetDlgItem(w_Dlg, IDC_STATIC_ABOUT_DESC);

	switch (Msg)
	{
		case WM_INITDIALOG:
		{
			wchar_t* buffer = new wchar_t[255];
			LoadString(GetModuleHandle(nullptr), IDS_STRING_ABOUT_BY, buffer, 255);
			SetWindowText(w_StaticBy, buffer);
			LoadString(GetModuleHandle(nullptr), IDS_STRING_ABOUT_DESC, buffer, 255);
			SetWindowText(w_StaticDesc, buffer);
			delete[] buffer;
			break;
		}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
				EndDialog(w_Dlg, IDOK);
			break;
		case WM_CLOSE:
			EndDialog(w_Dlg, 0);
			break;
	}
	return 0;
}

LRESULT __stdcall Application::DlgProc_Modify(HWND w_Dlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HWND w_EditX = GetDlgItem(w_Dlg, IDC_EDIT_MODIFY_X);
	HWND w_EditY = GetDlgItem(w_Dlg, IDC_EDIT_MODIFY_Y);
	
	HWND w_EditR = GetDlgItem(w_Dlg, IDC_EDIT_MODIFY_R);
	HWND w_EditG = GetDlgItem(w_Dlg, IDC_EDIT_MODIFY_G);
	HWND w_EditB = GetDlgItem(w_Dlg, IDC_EDIT_MODIFY_B);

	HWND W_ButtonColorPick = GetDlgItem(w_Dlg, IDC_BUTTON_MODIFY_COLOR_PICK);
	HWND w_ButtonFontPick = GetDlgItem(w_Dlg, IDC_BUTTON_MODIFY_PICK_FONT);

	HWND w_EditText = GetDlgItem(w_Dlg, IDC_EDIT_MODIFY_TEXT);
	HWND w_StaticInspect = GetDlgItem(w_Dlg, IDC_STATIC_MODIFY_COLOR_INSPECT);

	HWND w_CheckTransparent = GetDlgItem(w_Dlg, IDC_CHECK_MODIFY_TRANSPARENT);
	HWND w_ComboModFonts = GetDlgItem(w_Dlg, IDC_COMBO_MODIFY_FONT_FAMILY);


	static COLORREF currRgb = ::Runtime_rgbCurrent;

	switch (Msg)
	{
		case WM_INITDIALOG:
		{
			if (::Runtime_MemeTextSelectedIndex >= ::Runtime_MemeTexts.size())
			{
				MessageBoxW(w_Dlg, L"Invalid Text Index", L"Error", MB_ICONERROR | MB_OK);
				EndDialog(w_Dlg, -1);
				return -1;
			}
			
			currRgb = ::Runtime_MemeTexts[::Runtime_MemeTextSelectedIndex].text_color;
			SetWindowSubclass(w_StaticInspect, &Application::WndProc_ColorReview, 0, (DWORD_PTR)currRgb);
			std::wstring selected_text_index_tostr = std::to_wstring(::Runtime_MemeTextSelectedIndex);

			SetWindowTextW(w_EditText, ::Runtime_MemeTexts[::Runtime_MemeTextSelectedIndex].text.c_str());
			
			SetWindowTextW(w_EditX, std::to_wstring(::Runtime_MemeTexts[::Runtime_MemeTextSelectedIndex].text_rect.X).c_str());
			SetWindowTextW(w_EditY, std::to_wstring(::Runtime_MemeTexts[::Runtime_MemeTextSelectedIndex].text_rect.Y).c_str());
			
			SetWindowTextW(w_EditR, std::to_wstring(GetRValue(::Runtime_MemeTexts[::Runtime_MemeTextSelectedIndex].text_color)).c_str());
			SetWindowTextW(w_EditG, std::to_wstring(GetGValue(::Runtime_MemeTexts[::Runtime_MemeTextSelectedIndex].text_color)).c_str());
			SetWindowTextW(w_EditB, std::to_wstring(GetBValue(::Runtime_MemeTexts[::Runtime_MemeTextSelectedIndex].text_color)).c_str());
			
			Button_SetCheck(w_CheckTransparent, (int)::Runtime_MemeTexts[::Runtime_MemeTextSelectedIndex].bTransparent);
			
			std::wifstream file;
			file.open(PATH_SETTINGS_DEFAULT_FONTS);
			if (file.is_open())
			{
				std::wstring line;
				while (std::getline(file, line))
					ComboBox_AddString(w_ComboModFonts, line.c_str());
				file.close();
			}
			break;
		}
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDC_BUTTON_MODIFY_COLOR_PICK:
				{
					COLORREF text_color = ::GetColorFromDialog(
						w_Dlg,
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

					RECT inspectRect;
					GetClientRect(w_StaticInspect, &inspectRect);
					InvalidateRect(w_StaticInspect, &inspectRect, TRUE);
					break;
				}
				case IDCANCEL:
					EndDialog(w_Dlg, 0);
					break;
			}
			break;
		}
		case WM_CLOSE:
			EndDialog(w_Dlg, 0);
			break;
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

void ToggleMenuBarVisibility(HWND w_Handle)
{
	if (::bRuntime_ShowMenuBar)
	{
		SetMenu(w_Handle, nullptr);
		::bRuntime_ShowMenuBar = false;
		CheckMenuItem(GetMenu(w_Handle), ID_VIEW_STATUS_BAR, MF_UNCHECKED);
	}
	else
	{
		SetMenu(w_Handle, ::Runtime_hMenu);
		::bRuntime_ShowMenuBar = true;
		CheckMenuItem(GetMenu(w_Handle), ID_VIEW_STATUS_BAR, MF_CHECKED);
		// Restore check state from static runtime variable.
		CheckMenuItem(GetMenu(w_Handle), ID_VIEW_STATUS_BAR, ::bRuntime_ShowStatusBar ? MF_CHECKED : MF_UNCHECKED);
	}
	return;
}

void ManageMultipleSyncKeys(MSG& Msg)
{
	switch (Msg.message)
	{
		case WM_KEYDOWN:
		{
			switch (Msg.wParam)
			{
				case 'M':
					if (GetAsyncKeyState(VK_CONTROL))
						::ToggleMenuBarVisibility(Msg.hwnd);
					break;
			}
			break;
		}
	}
	return;
}

bool HDCToFile(const wchar_t* FilePath, HDC Context, RECT Area, uint16_t BitsPerPixel)
{
	uint32_t Width = Area.right - Area.left;
	uint32_t Height = Area.bottom - Area.top;

	BITMAPINFO Info;
	BITMAPFILEHEADER Header;
	memset(&Info, 0, sizeof(Info));
	memset(&Header, 0, sizeof(Header));
	Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	Info.bmiHeader.biWidth = Width;
	Info.bmiHeader.biHeight = Height;
	Info.bmiHeader.biPlanes = 1;
	Info.bmiHeader.biBitCount = BitsPerPixel;
	Info.bmiHeader.biCompression = BI_RGB;
	Info.bmiHeader.biSizeImage = Width * Height * (BitsPerPixel > 24 ? 4 : 3);
	Header.bfType = 0x4D42;
	Header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);


	char* Pixels = NULL;
	HDC MemDC = CreateCompatibleDC(Context);
	HBITMAP Section = CreateDIBSection(Context, &Info, DIB_RGB_COLORS, (void**)&Pixels, 0, 0);
	DeleteObject(SelectObject(MemDC, Section));
	BitBlt(MemDC, 0, 0, Width, Height, Context, Area.left, Area.top, SRCCOPY);
	DeleteDC(MemDC);

	std::fstream hFile(FilePath, std::ios::out | std::ios::binary);
	if (hFile.is_open())
	{
		hFile.write((char*)&Header, sizeof(Header));
		hFile.write((char*)&Info.bmiHeader, sizeof(Info.bmiHeader));
		hFile.write(Pixels, (((BitsPerPixel * Width + 31) & ~31) / 8) * Height);
		hFile.close();
		DeleteObject(Section);
		return true;
	}

	DeleteObject(Section);
	return false;
}

void Application::RunMessageLoop()
{
	MSG Msg = { };
	while (GetMessageW(&Msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		::ManageMultipleSyncKeys(Msg);
		DispatchMessageW(&Msg);
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
