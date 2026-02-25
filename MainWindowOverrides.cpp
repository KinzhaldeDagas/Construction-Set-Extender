#include "MainWindowOverrides.h"
#include "Construction Set Extender_Resource.h"
#include "[Common]\CLIWrapper.h"
#include "WorkspaceManager.h"
#include "Achievements.h"
#include "HallOfFame.h"
#include "OldCSInteropManager.h"
#include "GlobalClipboard.h"
#include "FormUndoStack.h"
#include "DialogImposterManager.h"
#include "ObjectPaletteManager.h"
#include "ObjectPrefabManager.h"
#include "Render Window\AuxiliaryViewport.h"
#include "Render Window\RenderWindowManager.h"
#include "CustomDialogProcs.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <fstream>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <unordered_set>

#include "[BGSEEBase]\ToolBox.h"
#include "[BGSEEBase]\Script\CodaVM.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace cse
{
	namespace uiManager
	{

		MainWindowMiscData::MainWindowMiscData() :
			bgsee::WindowExtraData(kTypeID)
		{
			ToolbarExtras = Subwindow::CreateInstance();
		}

		MainWindowMiscData::~MainWindowMiscData()
		{
			if (ToolbarExtras)
			{
				ToolbarExtras->TearDown();
				ToolbarExtras->DeleteInstance();
			}
		}

		MainWindowToolbarData::MainWindowToolbarData() :
			bgsee::WindowExtraData(kTypeID)
		{
			SettingTODSlider = false;
		}

		MainWindowToolbarData::~MainWindowToolbarData()
		{
			;//
		}

		static BOOL CALLBACK ApplyExplorerThemeToChildProc(HWND Child, LPARAM)
		{
			if (Child == nullptr || IsWindow(Child) == FALSE)
				return TRUE;

			char ClassName[64] = { 0 };
			GetClassNameA(Child, ClassName, sizeof(ClassName));

			if (_stricmp(ClassName, "ToolbarWindow32") == 0 ||
				_stricmp(ClassName, "ReBarWindow32") == 0 ||
				_stricmp(ClassName, "msctls_statusbar32") == 0 ||
				_stricmp(ClassName, "SysTabControl32") == 0 ||
				_stricmp(ClassName, "SysListView32") == 0 ||
				_stricmp(ClassName, "SysTreeView32") == 0 ||
				_stricmp(ClassName, "Button") == 0 ||
				_stricmp(ClassName, "Edit") == 0 ||
				_stricmp(ClassName, "ComboBox") == 0)
			{
				SetWindowTheme(Child, L"Explorer", nullptr);
			}

			return TRUE;
		}

		static void ApplyModernWindowChrome(HWND Window, bool Force = false)
		{
			if (Window == nullptr || IsWindow(Window) == FALSE)
				return;

			const char* kChromeDarkModeStateProp = "CSE_ModernChromeDarkModeState";
			const BOOL DarkTitleBar = BGSEEUI->GetColorThemer() && BGSEEUI->GetColorThemer()->IsEnabled();
			const INT_PTR DesiredState = DarkTitleBar ? 1 : 0;
			const INT_PTR CachedState = reinterpret_cast<INT_PTR>(GetPropA(Window, kChromeDarkModeStateProp));
			if (Force == false && CachedState == DesiredState)
				return;

			SetPropA(Window, kChromeDarkModeStateProp, reinterpret_cast<HANDLE>(DesiredState));

			typedef HRESULT(WINAPI* DwmSetWindowAttributeProcT)(HWND, DWORD, LPCVOID, DWORD);
			static DwmSetWindowAttributeProcT DwmSetWindowAttributeProc = []() -> DwmSetWindowAttributeProcT
			{
				HMODULE DwmapiModule = LoadLibraryA("dwmapi.dll");
				if (DwmapiModule == nullptr)
					return nullptr;
				return reinterpret_cast<DwmSetWindowAttributeProcT>(GetProcAddress(DwmapiModule, "DwmSetWindowAttribute"));
			}();

			if (DwmSetWindowAttributeProc)
				DwmSetWindowAttributeProc(Window, DWMWA_USE_IMMERSIVE_DARK_MODE, &DarkTitleBar, sizeof(DarkTitleBar));

			SetWindowTheme(Window, L"Explorer", nullptr);
			EnumChildWindows(Window, ApplyExplorerThemeToChildProc, 0);
			RedrawWindow(Window, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_NOERASE);
		}

		static void ApplyModernChromeToPrimaryWindows(bool Force = false)
		{
			ApplyModernWindowChrome(BGSEEUI->GetMainWindow(), Force);
			ApplyModernWindowChrome(TESRenderWindow::WindowHandle ? *TESRenderWindow::WindowHandle : nullptr, Force);
			ApplyModernWindowChrome(TESObjectWindow::WindowHandle ? *TESObjectWindow::WindowHandle : nullptr, Force);
			ApplyModernWindowChrome(TESCellViewWindow::WindowHandle ? *TESCellViewWindow::WindowHandle : nullptr, Force);
		}

		static ULONG_PTR GetGdiPlusToken()
		{
			static ULONG_PTR Token = 0;
			static bool Initialized = false;

			if (Initialized == false)
			{
				Gdiplus::GdiplusStartupInput StartupInput;
				if (Gdiplus::GdiplusStartup(&Token, &StartupInput, nullptr) != Gdiplus::Ok)
					Token = 0;
				Initialized = true;
			}

			return Token;
		}

		static std::wstring Utf8ToWide(const char* Input)
		{
			if (Input == nullptr || Input[0] == 0)
				return std::wstring();

			int CharCount = MultiByteToWideChar(CP_ACP, 0, Input, -1, nullptr, 0);
			if (CharCount <= 1)
				return std::wstring();

			std::wstring Result(CharCount - 1, L'\0');
			MultiByteToWideChar(CP_ACP, 0, Input, -1, &Result[0], CharCount);
			return Result;
		}

		static bool FileExists(const char* Path)
		{
			DWORD Attr = GetFileAttributesA(Path);
			return Attr != INVALID_FILE_ATTRIBUTES && ((Attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
		}

		static void LayoutToolbarExtras(HWND ToolbarWindow)
		{
			if (ToolbarWindow == nullptr || IsWindow(ToolbarWindow) == FALSE)
				return;

			HWND LaunchGameButton = GetDlgItem(ToolbarWindow, IDC_MAINMENU_LAUNCHGAME);
			HWND TODSlider = GetDlgItem(ToolbarWindow, IDC_TOOLBAR_TODSLIDER);
			HWND TODEdit = GetDlgItem(ToolbarWindow, IDC_TOOLBAR_TODCURRENT);
			if (LaunchGameButton == nullptr || TODSlider == nullptr || TODEdit == nullptr)
				return;

			HWND TODLabel = nullptr;
			HWND Child = nullptr;
			char ClassName[32] = { 0 };
			char Text[64] = { 0 };
			while ((Child = FindWindowEx(ToolbarWindow, Child, nullptr, nullptr)) != nullptr)
			{
				GetClassNameA(Child, ClassName, sizeof(ClassName));
				if (_stricmp(ClassName, "Static") != 0)
					continue;
				GetWindowTextA(Child, Text, sizeof(Text));
				if (_stricmp(Text, "Time of Day") == 0)
				{
					TODLabel = Child;
					break;
				}
			}
			if (TODLabel == nullptr)
				return;

			RECT ToolbarRect = { 0 };
			GetClientRect(ToolbarWindow, &ToolbarRect);
			RECT LaunchRect = { 0 }, LabelRect = { 0 }, SliderRect = { 0 }, EditRect = { 0 };
			GetWindowRect(LaunchGameButton, &LaunchRect);
			GetWindowRect(TODLabel, &LabelRect);
			GetWindowRect(TODSlider, &SliderRect);
			GetWindowRect(TODEdit, &EditRect);

			const int LaunchW = std::max(72, static_cast<int>(LaunchRect.right - LaunchRect.left));
			const int LaunchH = std::max(13, static_cast<int>(LaunchRect.bottom - LaunchRect.top));
			const int LabelW = std::max(52, static_cast<int>(LabelRect.right - LabelRect.left));
			const int LabelH = std::max(9, static_cast<int>(LabelRect.bottom - LabelRect.top));
			const int SliderW = std::max(90, static_cast<int>(SliderRect.right - SliderRect.left));
			const int SliderH = std::max(13, static_cast<int>(SliderRect.bottom - SliderRect.top));
			const int EditW = std::max(30, static_cast<int>(EditRect.right - EditRect.left));
			const int EditH = std::max(13, static_cast<int>(EditRect.bottom - EditRect.top));

			const int GapMajor = 10;
			const int GapMinor = 4;
			const int ClusterWidth = LaunchW + GapMajor + LabelW + GapMinor + SliderW + GapMinor + EditW;
			const int ClusterStartX = std::max(2, static_cast<int>(((ToolbarRect.right - ToolbarRect.left) - ClusterWidth) / 2));
			const int CenterY = (ToolbarRect.bottom - ToolbarRect.top) / 2;

			int X = ClusterStartX;
			MoveWindow(LaunchGameButton, X, std::max(0, CenterY - (LaunchH / 2)), LaunchW, LaunchH, TRUE);
			X += LaunchW + GapMajor;
			MoveWindow(TODLabel, X, std::max(0, CenterY - (LabelH / 2)), LabelW, LabelH, TRUE);
			X += LabelW + GapMinor;
			MoveWindow(TODSlider, X, std::max(0, CenterY - (SliderH / 2)), SliderW, SliderH, TRUE);
			X += SliderW + GapMinor;
			MoveWindow(TODEdit, X, std::max(0, CenterY - (EditH / 2)), EditW, EditH, TRUE);
		}

		static void ApplyExternalToolbarIcons(HWND ToolbarWindow, bool Force = false)
		{
			if (ToolbarWindow == nullptr || IsWindow(ToolbarWindow) == FALSE)
				return;
			if (GetGdiPlusToken() == 0)
				return;

			HIMAGELIST ImageList = reinterpret_cast<HIMAGELIST>(SendMessage(ToolbarWindow, TB_GETIMAGELIST, 0, 0));
			if (ImageList == nullptr)
				return;

			int IconW = 16, IconH = 16;
			ImageList_GetIconSize(ImageList, &IconW, &IconH);
			UINT Packed = (static_cast<UINT>(IconW) & 0xFFFF) | (static_cast<UINT>(IconH) << 16);
			const char* kCacheProp = "CSE_ToolbarExternalIconSizeCache";
			UINT Cached = static_cast<UINT>(reinterpret_cast<UINT_PTR>(GetPropA(ToolbarWindow, kCacheProp)));
			if (Force == false && Cached == Packed)
				return;

			char ModulePath[MAX_PATH] = { 0 };
			GetModuleFileNameA(BGSEEMAIN->GetExtenderHandle(), ModulePath, sizeof(ModulePath));
			char* LastSlash = strrchr(ModulePath, '\\');
			if (LastSlash == nullptr)
				return;
			*LastSlash = 0;

			int ButtonCount = static_cast<int>(SendMessage(ToolbarWindow, TB_BUTTONCOUNT, 0, 0));
			int VisualIndex = 0;
			for (int i = 0; i < ButtonCount; i++)
			{
				TBBUTTON Button = { 0 };
				if (SendMessage(ToolbarWindow, TB_GETBUTTON, i, reinterpret_cast<LPARAM>(&Button)) == FALSE)
					continue;
				if (Button.fsStyle & BTNS_SEP)
					continue;
				VisualIndex++;

				char IconPath[MAX_PATH] = { 0 };
				FORMAT_STR(IconPath, "%s\\cse_icons\\_%d.png", ModulePath, VisualIndex);
				if (FileExists(IconPath) == false || Button.iBitmap < 0)
					continue;

				std::wstring WidePath = Utf8ToWide(IconPath);
				if (WidePath.empty())
					continue;
				Gdiplus::Bitmap Bitmap(WidePath.c_str());
				if (Bitmap.GetLastStatus() != Gdiplus::Ok)
					continue;

				Gdiplus::Bitmap Scaled(IconW, IconH);
				Gdiplus::Graphics Graphics(&Scaled);
				Graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
				Graphics.DrawImage(&Bitmap, 0, 0, IconW, IconH);

				HBITMAP Hbm = nullptr;
				if (Scaled.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &Hbm) == Gdiplus::Ok && Hbm)
				{
					ImageList_Replace(ImageList, Button.iBitmap, Hbm, nullptr);
					DeleteObject(Hbm);
				}
			}

			SetPropA(ToolbarWindow, kCacheProp, reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(Packed)));
			InvalidateRect(ToolbarWindow, nullptr, FALSE);
		}

		static UINT GetWindowDPI(HWND hWnd)
		{
			typedef UINT(WINAPI* GetDpiForWindowProcT)(HWND);
			static GetDpiForWindowProcT GetDpiForWindowProc = reinterpret_cast<GetDpiForWindowProcT>(GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForWindow"));

			if (GetDpiForWindowProc && hWnd)
				return GetDpiForWindowProc(hWnd);

			return 96;
		}

		static void UpdateToolbarVisualsForDPIAndTheme(HWND ToolbarWindow, bool ForceRefresh = false)
		{
			if (ToolbarWindow == nullptr)
				return;

			const UINT Dpi = GetWindowDPI(ToolbarWindow);

			const char* kToolbarBaseGlyphWProp = "CSE_MainToolbarBaseGlyphW";
			const char* kToolbarBaseGlyphHProp = "CSE_MainToolbarBaseGlyphH";

			int BaseGlyphWidth = static_cast<int>(reinterpret_cast<INT_PTR>(GetPropA(ToolbarWindow, kToolbarBaseGlyphWProp)));
			int BaseGlyphHeight = static_cast<int>(reinterpret_cast<INT_PTR>(GetPropA(ToolbarWindow, kToolbarBaseGlyphHProp)));

			if (BaseGlyphWidth <= 0 || BaseGlyphHeight <= 0)
			{
				int ImageWidth = 16, ImageHeight = 16;
				HIMAGELIST ImageList = reinterpret_cast<HIMAGELIST>(SendMessage(ToolbarWindow, TB_GETIMAGELIST, 0, 0));
				if (ImageList)
					ImageList_GetIconSize(ImageList, &ImageWidth, &ImageHeight);

				// Normalize the currently reported size back to 96-DPI logical units once, then cache.
				BaseGlyphWidth = std::max(16, MulDiv(std::max(ImageWidth, 16), 96, std::max<UINT>(Dpi, 96)));
				BaseGlyphHeight = std::max(16, MulDiv(std::max(ImageHeight, 16), 96, std::max<UINT>(Dpi, 96)));

				SetPropA(ToolbarWindow, kToolbarBaseGlyphWProp, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(BaseGlyphWidth)));
				SetPropA(ToolbarWindow, kToolbarBaseGlyphHProp, reinterpret_cast<HANDLE>(static_cast<INT_PTR>(BaseGlyphHeight)));
			}

			const int ScaledImageWidth = std::max(16, MulDiv(BaseGlyphWidth, Dpi, 96));
			const int ScaledImageHeight = std::max(16, MulDiv(BaseGlyphHeight, Dpi, 96));
			const int HorizontalPadding = std::max(4, MulDiv(4, Dpi, 96));
			const int VerticalPadding = std::max(3, MulDiv(3, Dpi, 96));
			const int ButtonWidth = std::max(ScaledImageWidth + (HorizontalPadding * 2), MulDiv(24, Dpi, 96));
			const int ButtonHeight = std::max(ScaledImageHeight + (VerticalPadding * 2), MulDiv(24, Dpi, 96));

			const char* kToolbarVisualCacheProp = "CSE_MainToolbarVisualCache";
			const UINT PackedMetrics = (ScaledImageWidth & 0x3FF) |
				((ScaledImageHeight & 0x3FF) << 10) |
				((ButtonWidth & 0x3FF) << 20);
			const UINT PackedMetricsEx = (ButtonHeight & 0x3FF) | ((Dpi & 0x3FF) << 10);

			UINT PreviousMetrics = static_cast<UINT>(reinterpret_cast<UINT_PTR>(GetPropA(ToolbarWindow, kToolbarVisualCacheProp)));
			UINT PreviousMetricsEx = static_cast<UINT>(reinterpret_cast<UINT_PTR>(GetPropA(ToolbarWindow, "CSE_MainToolbarVisualCacheEx")));
			const bool MetricsChanged = (PreviousMetrics != PackedMetrics) || (PreviousMetricsEx != PackedMetricsEx);
			const bool RequiresRefresh = ForceRefresh || MetricsChanged;
			if (RequiresRefresh == false)
				return;

			SetPropA(ToolbarWindow, kToolbarVisualCacheProp, reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(PackedMetrics)));
			SetPropA(ToolbarWindow, "CSE_MainToolbarVisualCacheEx", reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(PackedMetricsEx)));

			SendMessage(ToolbarWindow, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DOUBLEBUFFER);
			SendMessage(ToolbarWindow, TB_SETBITMAPSIZE, 0, MAKELPARAM(ScaledImageWidth, ScaledImageHeight));
			SendMessage(ToolbarWindow, TB_SETPADDING, 0, MAKELPARAM(HorizontalPadding, VerticalPadding));
			SendMessage(ToolbarWindow, TB_SETBUTTONSIZE, 0, MAKELPARAM(ButtonWidth, ButtonHeight));
			LayoutToolbarExtras(ToolbarWindow);

#ifdef TB_SETMETRICS
			TBMETRICS Metrics = { 0 };
			Metrics.cbSize = sizeof(TBMETRICS);
			Metrics.dwMask = TBMF_PAD | TBMF_BARPAD | TBMF_BUTTONSPACING;
			Metrics.cxPad = HorizontalPadding;
			Metrics.cyPad = VerticalPadding;
			Metrics.cxBarPad = 0;
			Metrics.cyBarPad = 0;
			Metrics.cxButtonSpacing = 0;
			Metrics.cyButtonSpacing = 0;
			SendMessage(ToolbarWindow, TB_SETMETRICS, 0, reinterpret_cast<LPARAM>(&Metrics));
#endif

			const bool DarkModeEnabled = BGSEEUI->GetColorThemer() && BGSEEUI->GetColorThemer()->IsEnabled();
			const COLORREF ToolbarBackColor = DarkModeEnabled ? RGB(45, 45, 48) : CLR_DEFAULT;
			const COLORREF ToolbarTextColor = DarkModeEnabled ? RGB(220, 220, 220) : CLR_DEFAULT;
#ifdef TB_SETBKCOLOR
			SendMessage(ToolbarWindow, TB_SETBKCOLOR, 0, ToolbarBackColor);
#endif
#ifdef TB_SETTEXTCOLOR
			SendMessage(ToolbarWindow, TB_SETTEXTCOLOR, 0, ToolbarTextColor);
#endif

			auto ApplyImageListBkColor = [ToolbarWindow, ToolbarBackColor](UINT Message)
			{
				HIMAGELIST List = reinterpret_cast<HIMAGELIST>(SendMessage(ToolbarWindow, Message, 0, 0));
				if (List)
					ImageList_SetBkColor(List, ToolbarBackColor);
			};

			ApplyImageListBkColor(TB_GETIMAGELIST);
			ApplyImageListBkColor(TB_GETHOTIMAGELIST);
			ApplyImageListBkColor(TB_GETDISABLEDIMAGELIST);
			ApplyExternalToolbarIcons(ToolbarWindow, ForceRefresh);

			typedef HRESULT(WINAPI* SetWindowThemeProcT)(HWND, LPCWSTR, LPCWSTR);
			static SetWindowThemeProcT SetWindowThemeProc = []() -> SetWindowThemeProcT
			{
				HMODULE UXThemeModule = LoadLibraryA("uxtheme.dll");
				if (UXThemeModule == nullptr)
					return nullptr;

				return reinterpret_cast<SetWindowThemeProcT>(GetProcAddress(UXThemeModule, "SetWindowTheme"));
			}();

			if (SetWindowThemeProc)
				SetWindowThemeProc(ToolbarWindow, L"Explorer", nullptr);

			RedrawWindow(ToolbarWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
		}

		static bool SnapPrimaryWindowsIntoMainFrame(HWND MainWindow)
		{
			if (MainWindow == nullptr)
				return false;

			HWND RenderWindow = TESRenderWindow::WindowHandle ? *TESRenderWindow::WindowHandle : nullptr;
			HWND ObjectWindow = TESObjectWindow::WindowHandle ? *TESObjectWindow::WindowHandle : nullptr;
			HWND CellViewWindow = TESCellViewWindow::WindowHandle ? *TESCellViewWindow::WindowHandle : nullptr;

			if (RenderWindow == nullptr || ObjectWindow == nullptr || CellViewWindow == nullptr)
				return false;

			RECT WorkRect = { 0 };
			if (GetClientRect(MainWindow, &WorkRect) == FALSE)
				return false;

			auto ExcludeVerticalBandFromClientRect = [MainWindow, &WorkRect](HWND Child)
			{
				if (Child == nullptr || IsWindowVisible(Child) == FALSE)
					return;

				RECT ChildRect = { 0 };
				if (GetWindowRect(Child, &ChildRect) == FALSE)
					return;

				POINT ChildTopLeft = { ChildRect.left, ChildRect.top };
				POINT ChildBottomRight = { ChildRect.right, ChildRect.bottom };
				ScreenToClient(MainWindow, &ChildTopLeft);
				ScreenToClient(MainWindow, &ChildBottomRight);

				const int ChildTop = ChildTopLeft.y;
				const int ChildBottom = ChildBottomRight.y;
				if (ChildBottom <= WorkRect.top || ChildTop >= WorkRect.bottom)
					return;

				if (ChildTop <= WorkRect.top + 2)
					WorkRect.top = std::max<LONG>(WorkRect.top, static_cast<LONG>(ChildBottom + 2));
				else if (ChildBottom >= WorkRect.bottom - 2)
					WorkRect.bottom = std::min<LONG>(WorkRect.bottom, static_cast<LONG>(ChildTop - 2));
			};

			if (TESCSMain::MainToolbarHandle && *TESCSMain::MainToolbarHandle)
				ExcludeVerticalBandFromClientRect(*TESCSMain::MainToolbarHandle);

			HWND Child = nullptr;
			char ClassName[64] = { 0 };
			while ((Child = FindWindowEx(MainWindow, Child, nullptr, nullptr)) != nullptr)
			{
				GetClassNameA(Child, ClassName, sizeof(ClassName));
				if (_stricmp(ClassName, "msctls_statusbar32") == 0)
					ExcludeVerticalBandFromClientRect(Child);
			}

			POINT WorkTopLeft = { WorkRect.left, WorkRect.top };
			ClientToScreen(MainWindow, &WorkTopLeft);

			const int WorkWidth = WorkRect.right - WorkRect.left;
			const int WorkHeight = WorkRect.bottom - WorkRect.top;
			if (WorkWidth <= 0 || WorkHeight <= 0)
				return false;

			const int Gap = 6;
			const int MinLeftWidth = 320;
			const int MinRenderWidth = 420;
			int LeftWidth = std::max(MinLeftWidth, WorkWidth / 3);
			int RenderWidth = WorkWidth - LeftWidth - Gap;
			if (RenderWidth < MinRenderWidth)
			{
				RenderWidth = MinRenderWidth;
				LeftWidth = WorkWidth - RenderWidth - Gap;
			}

			if (LeftWidth < 220)
			{
				LeftWidth = std::max(120, WorkWidth / 2 - (Gap / 2));
				RenderWidth = std::max(120, WorkWidth - LeftWidth - Gap);
			}

			const int CellHeight = std::max(220, WorkHeight / 3);
			const int ObjectHeight = std::max(220, WorkHeight - CellHeight - Gap);
			const int FinalCellHeight = std::max(1, WorkHeight - ObjectHeight - Gap);

			SetWindowPos(ObjectWindow,
				nullptr,
				WorkTopLeft.x,
				WorkTopLeft.y,
				LeftWidth,
				ObjectHeight,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

			SetWindowPos(CellViewWindow,
				nullptr,
				WorkTopLeft.x,
				WorkTopLeft.y + ObjectHeight + Gap,
				LeftWidth,
				FinalCellHeight,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

			SetWindowPos(RenderWindow,
				nullptr,
				WorkTopLeft.x + LeftWidth + Gap,
				WorkTopLeft.y,
				RenderWidth,
				WorkHeight,
				SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

			ApplyModernChromeToPrimaryWindows(true);

			return true;
		}

		void BatchGenerateLipSyncFiles(HWND hWnd)
		{
			if (*TESQuest::WindowHandle != NULL || *TESQuest::FilteredDialogWindowHandle != NULL)
			{
				// will cause a CTD if the tool is executed when either of the above windows are open
				BGSEEUI->MsgBoxW("Please close any open Quest or Filtered Dialog windows before using this tool.");
				return;
			}

			bool SkipInactiveTopicInfos = false;
			bool OverwriteExisting = false;

			if (BGSEEUI->MsgBoxI(hWnd,
				MB_YESNO,
				"Only process active topic infos?") == IDYES)
			{
				SkipInactiveTopicInfos = true;
			}

			if (BGSEEUI->MsgBoxI(hWnd,
				MB_YESNO,
				"Overwrite existing LIP files?") == IDYES)
			{
				OverwriteExisting = true;
			}

			HWND IdleWindow = CreateDialogParam(BGSEEMAIN->GetExtenderHandle(), MAKEINTRESOURCE(IDD_IDLE), hWnd, nullptr, NULL);
			IFileStream ExistingFile;
			int BatchGenCounter = 0, FailedCounter = 0;
			bool HasError = false;

			struct LipGenInput
			{
				std::string Path;
				std::string ResponseText;

				LipGenInput(const char* Path, const char* Text)
					: Path(Path), ResponseText(Text) {}
			};
			std::vector<LipGenInput> CandidateInputs;

			for (tList<TESTopic>::Iterator ItrTopic = _DATAHANDLER->topics.Begin(); ItrTopic.End() == false && ItrTopic.Get(); ++ItrTopic)
			{
				TESTopic* Topic = ItrTopic.Get();
				SME_ASSERT(Topic);

				for (TESTopic::TopicDataListT::Iterator ItrTopicData = Topic->topicData.Begin();
					ItrTopicData.End() == false && ItrTopicData.Get();
					++ItrTopicData)
				{
					TESQuest* Quest = ItrTopicData->parentQuest;
					if (Quest == nullptr)
					{
						BGSEECONSOLE_MESSAGE("Topic %08X has an orphaned topic data; skipping", Topic->formID);
						continue;
					}

					for (int i = 0; i < ItrTopicData->questInfos.numObjs; i++)
					{
						TESTopicInfo* Info = ItrTopicData->questInfos.data[i];
						SME_ASSERT(Info);

						TESFile* OverrideFile = Info->GetOverrideFile(-1);

						if (OverrideFile)
						{
							if (SkipInactiveTopicInfos == false || (Info->formFlags & TESForm::kFormFlags_FromActiveFile))
							{
								for (tList<TESRace>::Iterator ItrRace = _DATAHANDLER->races.Begin();
									ItrRace.End() == false && ItrRace.Get();
									++ItrRace)
								{
									TESRace* Race = ItrRace.Get();
									SME_ASSERT(Race);

									for (TESTopicInfo::ResponseListT::Iterator ItrResponse = Info->responseList.Begin();
										ItrResponse.End() == false && ItrResponse.Get();
										++ItrResponse)
									{
										TESTopicInfo::ResponseData* Response = ItrResponse.Get();
										SME_ASSERT(Response);

										char VoiceFilePath[MAX_PATH] = { 0 };

										for (int j = 0; j < 2; j++)
										{
											const char* Sex = "M";
											if (j)
												Sex = "F";

											FORMAT_STR(VoiceFilePath, "Data\\Sound\\Voice\\%s\\%s\\%s\\%s_%s_%08X_%u",
												OverrideFile->fileName,
												Race->name.c_str(),
												Sex,
												Quest->editorID.c_str(),
												Topic->editorID.c_str(),
												(Info->formID & 0xFFFFFF),
												Response->responseNumber);

											CandidateInputs.emplace_back(VoiceFilePath, Response->responseText.c_str());														
										}
									}
								}
							}
						}
					}
				}
			}


			char Buffer[0x100];
			int Processed = 0;
			for (const auto& Input : CandidateInputs)
			{
				FORMAT_STR(Buffer, "Please Wait\nProcessing response %d/%d", Processed, CandidateInputs.size());
				Static_SetText(GetDlgItem(IdleWindow, -1), Buffer);

				std::string MP3Path(Input.Path); MP3Path += ".mp3";
				std::string WAVPath(Input.Path); WAVPath += ".wav";
				std::string LIPPath(Input.Path); LIPPath += ".lip";

				if (ExistingFile.Open(MP3Path.c_str()) ||
					ExistingFile.Open(WAVPath.c_str()))
				{
					if (OverwriteExisting || ExistingFile.Open(LIPPath.c_str()) == false)
					{
						if (CSIOM.GenerateLIPSyncFile(Input.Path.c_str(), Input.ResponseText.c_str()))
							BatchGenCounter++;
						else
						{
							HasError = true;
							FailedCounter++;
						}
					}
				}
				++Processed;
			}

			achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_GenerateLIP);
			DestroyWindow(IdleWindow);

			if (HasError)
				BGSEEUI->MsgBoxW("Batch generation completed with some errors!\n\nGenerated: %d files\nFailed: %d Files",
					BatchGenCounter, FailedCounter);
			else
				BGSEEUI->MsgBoxI("Batch generation completed successfully!\n\nGenerated: %d files.", BatchGenCounter);
		}


		static void CollectSpeakersFromTopicInfo(TESTopicInfo* Info, std::vector<TESNPC*>& OutSpeakers)
		{
			OutSpeakers.clear();
			if (Info == nullptr)
				return;

			std::unordered_set<TESNPC*> UniqueSpeakers;
			std::unordered_set<TESRace*> RaceFilters;
			bool HasExplicitNPCSpeaker = false;

			for (ConditionListT::Iterator Itr = Info->conditions.Begin(); Itr.End() == false && Itr.Get(); ++Itr)
			{
				TESConditionItem* Condition = Itr.Get();
				SME_ASSERT(Condition);

				const UInt16 FunctionIndex = Condition->functionIndex & 0x0FFF;
				if (FunctionIndex == 72)
				{
					TESNPC* Speaker = CS_CAST(Condition->param1.form, TESForm, TESNPC);
					if (Speaker)
					{
						UniqueSpeakers.insert(Speaker);
						HasExplicitNPCSpeaker = true;
					}
				}
				else if (FunctionIndex == 224)
				{
					TESRace* Race = CS_CAST(Condition->param1.form, TESForm, TESRace);
					if (Race)
						RaceFilters.insert(Race);
				}
			}

			// Only expand to race-matched NPCs when the line has explicit race filters and no
			// direct NPC speaker conditions, avoiding a global all-races fallback per line.
			if (HasExplicitNPCSpeaker == false && RaceFilters.empty() == false)
			{
				for (TESObject* Object = _DATAHANDLER->objects ? _DATAHANDLER->objects->first : nullptr;
					Object != nullptr;
					Object = Object->next)
				{
					TESNPC* NPC = CS_CAST(Object, TESForm, TESNPC);
					if (NPC == nullptr || NPC->race == nullptr)
						continue;

					if (RaceFilters.find(NPC->race) != RaceFilters.end())
						UniqueSpeakers.insert(NPC);
				}
			}

			OutSpeakers.reserve(UniqueSpeakers.size());
			for (auto Speaker : UniqueSpeakers)
				OutSpeakers.push_back(Speaker);
		}

		static std::string EscapeCSVField(const char* Input)
		{
			std::string Result = Input ? Input : "";
			std::string Escaped;
			Escaped.reserve(Result.length() + 2);

			Escaped.push_back('"');
			for (auto Ch : Result)
			{
				if (Ch == '"')
					Escaped += "\"\"";
				else
					Escaped.push_back(Ch);
			}
			Escaped.push_back('"');

			return Escaped;
		}

		static std::string GetDefaultRevoiceCSVName(const char* ActivePluginName)
		{
			std::string BaseName = ActivePluginName ? ActivePluginName : "ActivePlugin";
			if (BaseName.empty())
				BaseName = "ActivePlugin";

			size_t LastSlash = BaseName.find_last_of("\\/");
			if (LastSlash != std::string::npos)
				BaseName.erase(0, LastSlash + 1);

			size_t LastDot = BaseName.find_last_of('.');
			if (LastDot != std::string::npos)
				BaseName.erase(LastDot);

			if (BaseName.empty())
				BaseName = "ActivePlugin";

			return BaseName + "_reVoice.csv";
		}

		static bool IsAbsoluteRevoicePath(const std::string& Path)
		{
			if (Path.size() >= 2 && Path[1] == ':')
				return true;
			if (Path.size() >= 2 && Path[0] == '\\' && Path[1] == '\\')
				return true;
			return false;
		}

		static bool EnsureRevoiceParentDirectoryExists(const std::string& Path, std::string& OutError)
		{
			std::error_code Error;
			std::filesystem::path OutputPath(Path);
			std::filesystem::path ParentPath = OutputPath.parent_path();
			if (ParentPath.empty())
				return true;

			if (std::filesystem::exists(ParentPath, Error))
			{
				if (Error)
				{
					OutError = Error.message();
					return false;
				}

				if (std::filesystem::is_directory(ParentPath, Error) == false || Error)
				{
					OutError = Error ? Error.message() : "Parent path is not a directory";
					return false;
				}

				return true;
			}

			if (Error)
			{
				OutError = Error.message();
				return false;
			}

			if (std::filesystem::create_directories(ParentPath, Error) == false || Error)
			{
				OutError = Error ? Error.message() : "Couldn't create parent directory";
				return false;
			}

			return true;
		}

		void ExportRevoiceCSVForActivePlugin(HWND hWnd)
		{
			if (_DATAHANDLER->activeFile == nullptr)
			{
				BGSEEUI->MsgBoxE("An active plugin must be set before using this tool.");
				return;
			}

			char SelectPath[MAX_PATH] = { 0 };
			std::string DefaultCSVName = GetDefaultRevoiceCSVName(_DATAHANDLER->activeFile->fileName);
			strncpy_s(SelectPath, DefaultCSVName.c_str(), _TRUNCATE);

			if (TESDialog::ShowFileSelect(hWnd,
				"Data",
				"CSV Files\0*.csv\0\0",
				"Export reVoice CSV (Active Plugin)",
				"csv",
				nullptr,
				false,
				true,
				SelectPath,
				MAX_PATH) == false)
			{
				return;
			}

			std::string FilePath(SelectPath);
			if (FilePath.empty())
				return;

			for (auto& Ch : FilePath)
			{
				if (Ch == '/')
					Ch = '\\';
			}

			while (FilePath.empty() == false && (FilePath[0] == '\\' || FilePath[0] == '/'))
				FilePath.erase(0, 1);

			if (IsAbsoluteRevoicePath(FilePath) == false && _strnicmp(FilePath.c_str(), "Data\\", 5) != 0)
				FilePath = std::string("Data\\") + FilePath;

			if (FilePath.size() < 4 || _stricmp(FilePath.c_str() + FilePath.size() - 4, ".csv"))
				FilePath += ".csv";

			std::filesystem::path OutputPath(FilePath);
			std::error_code PathError;
			std::filesystem::path AbsoluteOutputPath = std::filesystem::absolute(OutputPath, PathError);
			if (PathError)
				AbsoluteOutputPath = OutputPath;

			const std::string OutputPathForIO = AbsoluteOutputPath.string();

			std::string DirectoryError;
			if (EnsureRevoiceParentDirectoryExists(OutputPathForIO, DirectoryError) == false)
			{
				BGSEEUI->MsgBoxE("Couldn't prepare output folder for reVoice CSV export:\n%s\n\nReason: %s", OutputPathForIO.c_str(), DirectoryError.c_str());
				return;
			}

			std::ofstream Output(OutputPathForIO, std::ios::trunc);
			if (Output.good() == false)
			{
				BGSEEUI->MsgBoxE("Couldn't open output file for writing:\n%s", OutputPathForIO.c_str());
				return;
			}

			if (!(Output << "FormID,VoiceID,SpeakerInfo,OutputPath,Dialogue\n"))
			{
				BGSEEUI->MsgBoxE("Couldn't write CSV header to output file:\n%s", OutputPathForIO.c_str());
				return;
			}

			UInt32 Rows = 0;
			for (tList<TESTopic>::Iterator ItrTopic = _DATAHANDLER->topics.Begin(); ItrTopic.End() == false && ItrTopic.Get(); ++ItrTopic)
			{
				TESTopic* Topic = ItrTopic.Get();
				SME_ASSERT(Topic);

				for (TESTopic::TopicDataListT::Iterator ItrTopicData = Topic->topicData.Begin();
					ItrTopicData.End() == false && ItrTopicData.Get();
					++ItrTopicData)
				{
					TESQuest* Quest = ItrTopicData->parentQuest;
					if (Quest == nullptr)
						continue;

					for (int i = 0; i < ItrTopicData->questInfos.numObjs; i++)
					{
						TESTopicInfo* Info = ItrTopicData->questInfos.data[i];
						if (Info == nullptr)
							continue;

						if ((Info->formFlags & TESForm::kFormFlags_FromActiveFile) == false)
							continue;

						std::vector<TESNPC*> Speakers;
						CollectSpeakersFromTopicInfo(Info, Speakers);
						if (Speakers.empty())
							continue;

						for (auto Speaker : Speakers)
						{
							if (Speaker == nullptr || Speaker->race == nullptr)
								continue;

							const bool IsFemale = (Speaker->actorFlags & TESActorBaseData::kNPCFlag_Female) != 0;
							const char* SexToken = IsFemale ? "F" : "M";
							TESRace* SpeakerRace = Speaker->race;
							TESRace* VoiceRace = IsFemale ? SpeakerRace->femaleVoiceRace : SpeakerRace->maleVoiceRace;
							if (VoiceRace == nullptr)
								VoiceRace = SpeakerRace;

							const char* VoiceID = VoiceRace->GetEditorID() ? VoiceRace->GetEditorID() : "";
							const char* RaceName = SpeakerRace->name.c_str();
							if (RaceName == nullptr || strlen(RaceName) == 0)
								RaceName = "Unknown";

							std::string SpeakerInfo = std::string(RaceName) + "\\" + SexToken;

							for (TESTopicInfo::ResponseListT::Iterator ItrResponse = Info->responseList.Begin();
								ItrResponse.End() == false && ItrResponse.Get();
								++ItrResponse)
							{
								TESTopicInfo::ResponseData* Response = ItrResponse.Get();
								if (Response == nullptr)
									continue;

								const char* ResponseText = Response->responseText.c_str();
								if (ResponseText == nullptr || strlen(ResponseText) == 0)
									continue;

								char OutPath[MAX_PATH] = { 0 };
								FORMAT_STR(OutPath, "Sound\\Voice\\%s\\%s\\%s\\%s_%s_%08X_%u.mp3",
									_DATAHANDLER->activeFile->fileName,
									(VoiceID && strlen(VoiceID)) ? VoiceID : RaceName,
									SexToken,
									Quest->editorID.c_str(),
									Topic->editorID.c_str(),
									Info->formID,
									Response->responseNumber);

								char FormID[16] = { 0 };
								FORMAT_STR(FormID, "%08X", Info->formID);

								if (!(Output
									<< EscapeCSVField(FormID)
									<< "," << EscapeCSVField(VoiceID)
									<< "," << EscapeCSVField(SpeakerInfo.c_str())
									<< "," << EscapeCSVField(OutPath)
									<< "," << EscapeCSVField(ResponseText)
									<< "\n"))
								{
									BGSEEUI->MsgBoxE("reVoice CSV export failed while writing output file:\n%s", OutputPathForIO.c_str());
									return;
								}
								Rows++;
							}
						}
					}
				}
			}

			Output.flush();
			if (Output.fail())
			{
				BGSEEUI->MsgBoxE("reVoice CSV export failed while finalizing output file:\n%s", OutputPathForIO.c_str());
				return;
			}

			Output.close();
			if (Output.fail())
			{
				BGSEEUI->MsgBoxE("reVoice CSV export failed while closing output file:\n%s", OutputPathForIO.c_str());
				return;
			}

			BGSEEUI->MsgBoxI("reVoice CSV export complete. Wrote %u dialogue rows to:\n%s", Rows, OutputPathForIO.c_str());
		}


		LRESULT CALLBACK MainWindowMenuInitSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
														bgsee::WindowSubclassProcCollection::SubclassProcExtraParams* SubclassParams)
		{
			LRESULT DlgProcResult = FALSE;

			switch (uMsg)
			{
			case WM_INITMENUPOPUP:
				{
					if (HIWORD(lParam) == FALSE)
					{
						HMENU Popup = (HMENU)wParam;

						for (int i = 0, j = GetMenuItemCount(Popup); i < j; i++)
						{
							MENUITEMINFO CurrentItem = { 0 };
							CurrentItem.cbSize = sizeof(MENUITEMINFO);
							CurrentItem.fMask = MIIM_ID | MIIM_STATE;

							if (GetMenuItemInfo(Popup, i, TRUE, &CurrentItem) == TRUE)
							{
								bool UpdateItem = true;
								bool CheckItem = false;
								bool DisableItem = false;

								switch (CurrentItem.wID)
								{
								case TESCSMain::kMainMenu_World_EditCellPathGrid:
									if (*TESRenderWindow::PathGridEditFlag)
										CheckItem = true;

									break;
								case IDC_MAINMENU_SAVEOPTIONS_SAVEESPMASTERS:
									if (settings::plugins::kSaveLoadedESPsAsMasters.GetData().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_SAVEOPTIONS_PREVENTCHANGESTOFILETIMESTAMPS:
									if (settings::plugins::kPreventTimeStampChanges.GetData().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_SAVEOPTIONS_CREATEBACKUPBEFORESAVING:
									if (settings::versionControl::kBackupOnSave.GetData().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_GLOBALUNDO:
									if (BGSEEUNDOSTACK->IsUndoStackEmpty())
										DisableItem = true;

									break;
								case IDC_MAINMENU_GLOBALREDO:
									if (BGSEEUNDOSTACK->IsRedoStackEmpty())
										DisableItem = true;

									break;
								case IDC_MAINMENU_CONSOLE:
									if (BGSEECONSOLE->IsVisible())
										CheckItem = true;

									break;
								case IDC_MAINMENU_AUXVIEWPORT:
									if (AUXVIEWPORT->IsVisible())
										CheckItem = true;

									break;
								case IDC_MAINMENU_HIDEUNMODIFIEDFORMS:
									if (FormEnumerationManager::Instance.GetVisibleUnmodifiedForms() == false)
										CheckItem = true;

									break;
								case IDC_MAINMENU_HIDEDELETEDFORMS:
									if (FormEnumerationManager::Instance.GetVisibleDeletedForms() == false)
										CheckItem = true;

									break;
								case IDC_MAINMENU_SORTACTIVEFORMSFIRST:
									if (settings::dialogs::kSortFormListsByActiveForm.GetData().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_COLORIZEACTIVEFORMS:
									if (settings::dialogs::kColorizeActiveForms.GetData().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_COLORIZEFORMOVERRIDES:
									if (settings::dialogs::kColorizeFormOverrides.GetData().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_CODABACKGROUNDER:
									if (CODAVM->GetBackgroundDaemon()->IsEnabled())
										CheckItem = true;

									break;
								case IDC_MAINMENU_INITIALLYDISABLEDREFERENCES:
									if (_RENDERWIN_XSTATE.ShowInitiallyDisabledRefs)
										CheckItem = true;

									break;
								case IDC_MAINMENU_CHILDREFERENCESOFTHEDISABLED:
									if (_RENDERWIN_XSTATE.ShowInitiallyDisabledRefChildren)
										CheckItem = true;

									break;
								case IDC_MAINMENU_MULTIPLEPREVIEWWINDOWS:
									if (PreviewWindowImposterManager::Instance.GetEnabled())
										CheckItem = true;

									break;
								case IDC_MAINMENU_PARENTCHILDINDICATORS:
									if (settings::renderer::kParentChildVisualIndicator().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORS:
									if (settings::renderer::kPathGridLinkedRefIndicator().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDEBOUNDINGBOX:
									if ((settings::renderer::kPathGridLinkedRefIndicatorFlags().u &
										 settings::renderer::kPathGridLinkedRefIndicatorFlag_HidePointBoundingBox))
									{
										CheckItem = true;
									}

									break;
								case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDELINKEDREFNODE:
									if ((settings::renderer::kPathGridLinkedRefIndicatorFlags().u &
										 settings::renderer::kPathGridLinkedRefIndicatorFlag_HideLinkedRefNode))
									{
										CheckItem = true;
									}

									break;
								case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDELINECONNECTOR:
									if ((settings::renderer::kPathGridLinkedRefIndicatorFlags().u &
										settings::renderer::kPathGridLinkedRefIndicatorFlag_HideLineConnector))
									{
										CheckItem = true;
									}

									break;
								case IDC_MAINMENU_CUSTOMCOLORTHEMEMODE:
									if (BGSEEUI->GetColorThemer()->IsEnabled())
										CheckItem = true;

									break;
								case IDC_MAINMENU_ONEDITORSTARTUP_LOADSTARTUPPLUGIN:
									if (settings::startup::kLoadPlugin().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_ONEDITORSTARTUP_SETSTARTUPWORKSPACE:
									if (settings::startup::kSetWorkspace().i)
										CheckItem = true;

									break;
								case IDC_MAINMENU_ONEDITORSTARTUP_OPENSTARTUPSCRIPT:
									if (settings::startup::kOpenScriptWindow().i)
										CheckItem = true;

									break;
								default:
									UpdateItem = false;
									break;
								}

								if (UpdateItem)
								{
									if (CheckItem)
									{
										CurrentItem.fState &= ~MFS_UNCHECKED;
										CurrentItem.fState |= MFS_CHECKED;
									}
									else
									{
										CurrentItem.fState &= ~MFS_CHECKED;
										CurrentItem.fState |= MFS_UNCHECKED;
									}

									if (DisableItem)
									{
										CurrentItem.fState &= ~MFS_ENABLED;
										CurrentItem.fState |= MFS_DISABLED;
									}
									else
									{
										CurrentItem.fState &= ~MFS_DISABLED;
										CurrentItem.fState |= MFS_ENABLED;
									}

									CurrentItem.fMask = MIIM_STATE;
									SetMenuItemInfo(Popup, i, TRUE, &CurrentItem);
								}
							}
						}
					}

					SubclassParams->Out.MarkMessageAsHandled = true;
				}

				break;
			}

			return DlgProcResult;
		}

		LRESULT CALLBACK MainWindowMenuSelectSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
														  bgsee::WindowSubclassProcCollection::SubclassProcExtraParams* SubclassParams)
		{
			LRESULT DlgProcResult = FALSE;

			switch (uMsg)
			{
			case WM_COMMAND:
				SubclassParams->Out.MarkMessageAsHandled = true;

				switch (LOWORD(wParam))
				{
				case TESCSMain::kMainMenu_View_PreviewWindow:
					if (PreviewWindowImposterManager::Instance.GetEnabled())
						BGSEEUI->MsgBoxI("Use the Object Window's context menu to preview objects when multiple preview windows are enabled.");
					else
						SubclassParams->Out.MarkMessageAsHandled = false;

					break;
				case TESCSMain::kMainMenu_Help_Contents:
				case TESCSMain::kMainMenu_Character_ExportDialogue:
					{
						if (achievements::kOldestTrickInTheBook->GetUnlocked() == false)
						{
							ShellExecute(nullptr, "open", "http://www.youtube.com/watch?v=oHg5SJYRHA0", nullptr, nullptr, SW_SHOWNORMAL);
							BGSEEACHIEVEMENTS->Unlock(achievements::kOldestTrickInTheBook);
						}
						else if (LOWORD(wParam) == TESCSMain::kMainMenu_Help_Contents)
							ShellExecute(nullptr, "open", "https://cs.uesp.net/wiki/Main_Page", nullptr, nullptr, SW_SHOWNORMAL);
						else
							SubclassParams->Out.MarkMessageAsHandled = false;
					}

					break;
				case IDC_MAINMENU_SAVEAS:
					{
						if (_DATAHANDLER->activeFile == nullptr)
						{
							BGSEEUI->MsgBoxE("An active plugin must be set before using this tool.");
							break;
						}

						*TESCSMain::AllowAutoSaveFlag = 0;
						char FileName[MAX_PATH] = { 0 };

						if (TESDialog::SelectTESFileCommonDialog(hWnd,
																 INISettingCollection::Instance->LookupByName("sLocalMasterPath:General")->value.s,
																 0,
																 FileName,
																 sizeof(FileName)))
						{
							TESFile* SaveAsBuffer = _DATAHANDLER->activeFile;

							SaveAsBuffer->SetActive(false);
							SaveAsBuffer->SetLoaded(false);

							_DATAHANDLER->activeFile = nullptr;

							if (SendMessage(hWnd, TESCSMain::kWindowMessage_Save, NULL, (LPARAM)FileName))
								TESCSMain::SetTitleModified(false);
							else
							{
								_DATAHANDLER->activeFile = SaveAsBuffer;

								SaveAsBuffer->SetActive(true);
								SaveAsBuffer->SetLoaded(true);
							}
							achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_SaveAs);
						}

						*TESCSMain::AllowAutoSaveFlag = 1;
					}

					break;
				case IDC_MAINMENU_CONSOLE:
					BGSEECONSOLE->ToggleVisibility();

					break;
				case IDC_MAINMENU_HIDEDELETEDFORMS:
					FormEnumerationManager::Instance.ToggleVisibilityDeletedForms();

					break;
				case IDC_MAINMENU_HIDEUNMODIFIEDFORMS:
					FormEnumerationManager::Instance.ToggleVisibilityUnmodifiedForms();

					break;
				case IDC_MAINMENU_CSEPREFERENCES:
					BGSEEMAIN->ShowPreferencesGUI();

					break;
				case IDC_MAINMENU_LAUNCHGAME:
					{
						std::string AppPath = BGSEEMAIN->GetAPPPath();
						AppPath += "\\";

						IFileStream SteamLoader;
						if (SteamLoader.Open((std::string(AppPath + "obse_steam_loader.dll")).c_str()) == false)
							AppPath += "obse_loader.exe";
						else
							AppPath += "Oblivion.exe";

						ShellExecute(nullptr, "open", (LPCSTR)AppPath.c_str(), nullptr, nullptr, SW_SHOW);
						BGSEEACHIEVEMENTS->Unlock(achievements::kLazyBum);
					}

					break;
				case IDC_MAINMENU_CREATEGLOBALSCRIPT:
					BGSEEUI->ModelessDialog(BGSEEMAIN->GetExtenderHandle(), MAKEINTRESOURCE(IDD_GLOBALSCRIPT), hWnd, (DLGPROC)CreateGlobalScriptDlgProc);

					break;
				case IDC_MAINMENU_TAGBROWSER:
					cliWrapper::interfaces::TAG->ShowTagBrowserDialog(nullptr);
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_TagBrowser);

					break;
				case IDC_MAINMENU_SETWORKSPACE:
					BGSEEWORKSPACE->SelectCurrentWorkspace();
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_Workspace);

					break;
				case IDC_MAINMENU_TOOLS:
					BGSEETOOLBOX->ShowToolListMenu(BGSEEMAIN->GetExtenderHandle(), hWnd);
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_Toolbox);

					break;
				case IDC_MAINMENU_CODAMANAGEGLOBALDATASTORE:
					CODAVM->ShowGlobalStoreEditDialog(BGSEEMAIN->GetExtenderHandle(), hWnd);

					break;
				case IDC_MAINMENU_CODABACKGROUNDER:
					if (CODAVM->GetBackgroundDaemon()->IsEnabled())
						CODAVM->GetBackgroundDaemon()->Suspend();
					else
						CODAVM->GetBackgroundDaemon()->Resume();

					break;
				case IDC_MAINMENU_SAVEOPTIONS_SAVEESPMASTERS:
					settings::plugins::kSaveLoadedESPsAsMasters.ToggleData();

					break;
				case IDC_MAINMENU_SAVEOPTIONS_PREVENTCHANGESTOFILETIMESTAMPS:
					settings::plugins::kPreventTimeStampChanges.ToggleData();

					break;
				case IDC_MAINMENU_AUXVIEWPORT:
					AUXVIEWPORT->ToggleVisibility();

					break;
				case IDC_MAINMENU_USEINFOLISTING:
					cliWrapper::interfaces::USE->ShowUseInfoListDialog(nullptr);
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_UseInfoListing);

					break;
				case IDC_MAINMENU_BATCHLIPGENERATOR:
					BatchGenerateLipSyncFiles(hWnd);
					
					break;
				case IDC_MAINMENU_EXPORT_REVOICECSV_ACTIVEPLUGIN:
					ExportRevoiceCSVForActivePlugin(hWnd);

					break;
				case IDC_MAINMENU_SAVEOPTIONS_CREATEBACKUPBEFORESAVING:
					settings::versionControl::kBackupOnSave.ToggleData();

					break;
				case IDC_MAINMENU_SORTACTIVEFORMSFIRST:
					settings::dialogs::kSortFormListsByActiveForm.ToggleData();

					break;
				case IDC_MAINMENU_COLORIZEACTIVEFORMS:
					settings::dialogs::kColorizeActiveForms.ToggleData();

					break;
				case IDC_MAINMENU_COLORIZEFORMOVERRIDES:
					settings::dialogs::kColorizeFormOverrides.ToggleData();

					break;
				case IDC_MAINMENU_GLOBALCLIPBOARDCONTENTS:
					BGSEECLIPBOARD->DisplayContents();

					break;
				case IDC_MAINMENU_PASTEFROMGLOBALCLIPBOARD:
					BGSEECLIPBOARD->Paste();
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_GlobalClipboard);

					break;
				case IDC_MAINMENU_GLOBALUNDO:
					BGSEEUNDOSTACK->PerformUndo();
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_GlobalUndo);

					break;
				case IDC_MAINMENU_GLOBALREDO:
					BGSEEUNDOSTACK->PerformRedo();
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_GlobalUndo);

					break;
				case IDC_MAINMENU_PURGELOADEDRESOURCES:
					{
						BGSEECONSOLE_MESSAGE("Purging resources...");
						BGSEECONSOLE->Indent();
						PROCESS_MEMORY_COUNTERS_EX MemCounter = { 0 };
						UInt32 RAMUsage = 0;

						GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&MemCounter, sizeof(MemCounter));
						RAMUsage = MemCounter.WorkingSetSize / (1024 * 1024);
						BGSEECONSOLE_MESSAGE("Current RAM Usage: %d MB", RAMUsage);
						_TES->PurgeLoadedResources();
						BGSEECONSOLE_MESSAGE("Resources purged!");

						GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&MemCounter, sizeof(MemCounter));
						RAMUsage = MemCounter.WorkingSetSize / (1024 * 1024);
						BGSEECONSOLE_MESSAGE("Current RAM Usage: %d MB", RAMUsage);
						BGSEECONSOLE->Outdent();
					}

					break;
				case IDC_MAINMENU_INITIALLYDISABLEDREFERENCES:
					_RENDERWIN_XSTATE.ShowInitiallyDisabledRefs = _RENDERWIN_XSTATE.ShowInitiallyDisabledRefs == false;
					TESRenderWindow::Redraw();

					break;
				case IDC_MAINMENU_CHILDREFERENCESOFTHEDISABLED:
					_RENDERWIN_XSTATE.ShowInitiallyDisabledRefChildren = _RENDERWIN_XSTATE.ShowInitiallyDisabledRefChildren == false;
					TESRenderWindow::Redraw();

					break;
				case IDC_MAINMENU_CODAOPENSCRIPTREPOSITORY:
					CODAVM->OpenScriptRepository();

					break;
				case IDC_MAINMENU_SPAWNEXTRAOBJECTWINDOW:
					ObjectWindowImposterManager::Instance.SpawnImposter();
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_MultipleObjectWindows);

					break;
				case IDC_MAINMENU_MULTIPLEPREVIEWWINDOWS:
					if (PreviewWindowImposterManager::Instance.GetEnabled())
					{
						PreviewWindowImposterManager::Instance.SetEnabled(false);
						settings::dialogs::kMultiplePreviewWindows.SetInt(0);
					}
					else
					{
						PreviewWindowImposterManager::Instance.SetEnabled(true);
						settings::dialogs::kMultiplePreviewWindows.SetInt(1);
					}

					break;
				case IDC_MAINMENU_OBJECTPALETTE:
					objectPalette::ObjectPaletteManager::Instance.Show();

					break;
				case IDC_MAINMENU_OBJECTPREFABS:
					objectPrefabs::ObjectPrefabManager::Instance.Show();

					break;
				case IDC_MAINMENU_PARENTCHILDINDICATORS:
					settings::renderer::kParentChildVisualIndicator.ToggleData();
					TESRenderWindow::Redraw();

					break;
				case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORS:
					settings::renderer::kPathGridLinkedRefIndicator.ToggleData();
					TESRenderWindow::Redraw(true);

					break;
				case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDEBOUNDINGBOX:
				case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDELINKEDREFNODE:
				case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDELINECONNECTOR:
					{
						UInt32 Flags = settings::renderer::kPathGridLinkedRefIndicatorFlags().u;
						UInt32 Comperand = 0;

						switch (LOWORD(wParam))
						{
						case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDEBOUNDINGBOX:
							Comperand = settings::renderer::kPathGridLinkedRefIndicatorFlag_HidePointBoundingBox;
							break;
						case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDELINKEDREFNODE:
							Comperand = settings::renderer::kPathGridLinkedRefIndicatorFlag_HideLinkedRefNode;
							break;
						case IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDELINECONNECTOR:
							Comperand = settings::renderer::kPathGridLinkedRefIndicatorFlag_HideLineConnector;
							break;
						}

						if ((Flags & Comperand))
							Flags &= ~Comperand;
						else
							Flags |= Comperand;

						settings::renderer::kPathGridLinkedRefIndicatorFlags.SetUInt(Flags);
						if (settings::renderer::kPathGridLinkedRefIndicator().i == 0)
							TESRenderWindow::Redraw(true);
					}

					break;
				case IDC_MAINMENU_CODARELOADBACKGROUNDSCRIPTS:
					CODAVM->GetBackgroundDaemon()->Rebuild();
					break;
				case IDC_MAINMENU_CODARESETSCRIPTCACHE:
					CODAVM->GetProgramCache()->Invalidate();
					break;
				case IDC_MAINMENU_SYNCSCRIPTSTODISK:
					cliWrapper::interfaces::SE->ShowDiskSyncDialog();
					break;
				case IDC_MAINMENU_CUSTOMCOLORTHEMEMODE:
					if (BGSEEUI->GetColorThemer()->IsEnabled())
						BGSEEUI->GetColorThemer()->Disable();
					else
						BGSEEUI->GetColorThemer()->Enable();

					if (TESCSMain::MainToolbarHandle && *TESCSMain::MainToolbarHandle)
						UpdateToolbarVisualsForDPIAndTheme(*TESCSMain::MainToolbarHandle, true);
					ApplyModernChromeToPrimaryWindows(true);

					break;
				case IDC_MAINMENU_ONEDITORSTARTUP_LOADSTARTUPPLUGIN:
					settings::startup::kLoadPlugin.ToggleData();
					break;
				case IDC_MAINMENU_ONEDITORSTARTUP_SETSTARTUPWORKSPACE:
					settings::startup::kSetWorkspace.ToggleData();
					break;
				case IDC_MAINMENU_ONEDITORSTARTUP_OPENSTARTUPSCRIPT:
					settings::startup::kOpenScriptWindow.ToggleData();
					break;
				case IDC_MAINMENU_CSEMANUAL:
				case IDC_MAINMENU_CODAMANUAL:
					{
						std::string ManualPath = BGSEEMAIN->GetAPPPath();
						ManualPath += "\\Data\\Docs\\Construction Set Extender\\";
						if (LOWORD(wParam) == IDC_MAINMENU_CODAMANUAL)
							ManualPath += "Coda Manual.pdf";
						else
							ManualPath += "Construction Set Extender Manual.pdf";

						IFileStream File;
						if (File.Open(ManualPath.c_str()) == false)
						{
							BGSEEUI->MsgBoxI(hWnd, NULL, "Could not find the manual file. Did you extract the bundled "
											 "documentation into the following directory?\n\n"
											 "Data\\Docs\\Construction Set Extender\\");
						}
						else
							ShellExecute(nullptr, "open", ManualPath.c_str(), nullptr, nullptr, SW_SHOW);
					}
					break;
				default:
					SubclassParams->Out.MarkMessageAsHandled = false;

					break;
				}

				break;
			}

			return DlgProcResult;
		}

#define ID_PATHGRIDTOOLBARBUTTION_TIMERID		0x99
#define ID_MAINWINDOW_SNAPLAYOUT_TIMERID		0x9A

		LRESULT CALLBACK MainWindowMiscSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
													bgsee::WindowSubclassProcCollection::SubclassProcExtraParams* SubclassParams)
		{
			LRESULT DlgProcResult = FALSE;

			switch (uMsg)
			{
			case TESCSMain::kWindowMessage_Unk413:
				// if multiple editor instances are disabled, the secondary instance will send this message to the primary
				// strangely, when the primary instance has an active/open preview control, the call crashes (presumably due to a corrupt lParam pointer)
				// as far as I can tell, the corresponding code path is never executed, so we can consume this message entirely

				SubclassParams->Out.MarkMessageAsHandled = true;
				break;
			case WM_MAINWINDOW_INIT_DIALOG:
				{
					SetTimer(hWnd, ID_PATHGRIDTOOLBARBUTTION_TIMERID, 500, nullptr);
					SetTimer(hWnd, ID_MAINWINDOW_SNAPLAYOUT_TIMERID, 250, nullptr);
					if (TESCSMain::MainToolbarHandle && *TESCSMain::MainToolbarHandle)
						LayoutToolbarExtras(*TESCSMain::MainToolbarHandle);
					ApplyModernChromeToPrimaryWindows(true);
					SubclassParams->Out.MarkMessageAsHandled = true;
				}

				break;
			case WM_DESTROY:
				{
					KillTimer(hWnd, ID_PATHGRIDTOOLBARBUTTION_TIMERID);
					KillTimer(hWnd, ID_MAINWINDOW_SNAPLAYOUT_TIMERID);
					RemovePropA(hWnd, "CSE_ModernChromeDarkModeState");
					RemovePropA(hWnd, "CSE_ToolbarExternalIconSizeCache");
					if (TESCSMain::MainToolbarHandle && *TESCSMain::MainToolbarHandle)
						RemovePropA(*TESCSMain::MainToolbarHandle, "CSE_ToolbarExternalIconSizeCache");

					MainWindowMiscData* xData = BGSEE_GETWINDOWXDATA(MainWindowMiscData, SubclassParams->In.ExtraData);
					if (xData)
					{
						SubclassParams->In.ExtraData->Remove(MainWindowMiscData::kTypeID);
						delete xData;
					}
				}

				break;
			case WM_MAINWINDOW_INIT_EXTRADATA:
				{
					MainWindowMiscData* xData = BGSEE_GETWINDOWXDATA(MainWindowMiscData, SubclassParams->In.ExtraData);
					if (xData == nullptr)
					{
						xData = new MainWindowMiscData();
						SubclassParams->In.ExtraData->Add(xData);

						xData->ToolbarExtras->hInstance = BGSEEMAIN->GetExtenderHandle();
						xData->ToolbarExtras->hDialog = *TESCSMain::MainToolbarHandle;
						xData->ToolbarExtras->hContainer = *TESCSMain::MainToolbarHandle;
						xData->ToolbarExtras->position.x = 485;
						xData->ToolbarExtras->position.y = 0;

						if (xData->ToolbarExtras->Build(IDD_TOOLBAREXTRAS) == false)
							BGSEECONSOLE_ERROR("Couldn't build main window toolbar subwindow!");
						else
						{
							SubclassParams->In.Subclasser->RegisterSubclassForWindow(*TESCSMain::MainToolbarHandle, MainWindowToolbarSubClassProc);
							SendMessage(*TESCSMain::MainToolbarHandle, WM_MAINTOOLBAR_INIT, NULL, NULL);
							UpdateToolbarVisualsForDPIAndTheme(*TESCSMain::MainToolbarHandle, true);

							HWND TODSlider = GetDlgItem(hWnd, IDC_TOOLBAR_TODSLIDER);
							HWND TODEdit = GetDlgItem(hWnd, IDC_TOOLBAR_TODCURRENT);

							TESDialog::ClampDlgEditField(TODEdit, 0.0, 24.0);

							SendMessage(TODSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 23));
							SendMessage(TODSlider, TBM_SETLINESIZE, NULL, 1);
							SendMessage(TODSlider, TBM_SETPAGESIZE, NULL, 4);

							SendMessage(*TESCSMain::MainToolbarHandle, WM_MAINTOOLBAR_SETTOD, _TES->GetSkyTOD() * 4.0, NULL);
							if (TESCSMain::MainToolbarHandle && *TESCSMain::MainToolbarHandle)
								LayoutToolbarExtras(*TESCSMain::MainToolbarHandle);
							ApplyModernChromeToPrimaryWindows(true);
						}
					}
				}

				break;
			case WM_THEMECHANGED:
			case WM_SETTINGCHANGE:
				ApplyModernChromeToPrimaryWindows(true);
				break;
			case WM_SIZE:
			case WM_WINDOWPOSCHANGED:
				ApplyModernWindowChrome(hWnd);
				if (TESCSMain::MainToolbarHandle && *TESCSMain::MainToolbarHandle)
					LayoutToolbarExtras(*TESCSMain::MainToolbarHandle);
				break;
			case WM_TIMER:
				DlgProcResult = TRUE;
				SubclassParams->Out.MarkMessageAsHandled = true;

				switch (wParam)
				{
				case TESCSMain::kTimer_Autosave:
					// autosave timer, needs to be handled here as the org wndproc doesn't compare the timerID
					if (*TESCSMain::AllowAutoSaveFlag != 0 && *TESCSMain::ExittingCSFlag == 0)
						TESCSMain::AutoSave();

					break;
				case ID_PATHGRIDTOOLBARBUTTION_TIMERID:
					{
						TBBUTTONINFO PathGridData = { 0 };
						PathGridData.cbSize = sizeof(TBBUTTONINFO);
						PathGridData.dwMask = TBIF_STATE;

						SendMessage(*TESCSMain::MainToolbarHandle, TB_GETBUTTONINFO, TESCSMain::kToolbar_PathGridEdit, (LPARAM)&PathGridData);
						if ((PathGridData.fsState & TBSTATE_CHECKED) == false && *TESRenderWindow::PathGridEditFlag)
						{
							PathGridData.fsState |= TBSTATE_CHECKED;
							SendMessage(*TESCSMain::MainToolbarHandle, TB_SETBUTTONINFO, TESCSMain::kToolbar_PathGridEdit, (LPARAM)&PathGridData);
						}

					}

					break;
				case ID_MAINWINDOW_SNAPLAYOUT_TIMERID:
					if (SnapPrimaryWindowsIntoMainFrame(hWnd))
						KillTimer(hWnd, ID_MAINWINDOW_SNAPLAYOUT_TIMERID);

					break;
				}

				break;
			}

			return DlgProcResult;
		}

		LRESULT CALLBACK MainWindowToolbarSubClassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
													   bgsee::WindowSubclassProcCollection::SubclassProcExtraParams* SubclassParams)
		{
			LRESULT DlgProcResult = FALSE;

			HWND TODSlider = GetDlgItem(hWnd, IDC_TOOLBAR_TODSLIDER);
			HWND TODEdit = GetDlgItem(hWnd, IDC_TOOLBAR_TODCURRENT);

			switch (uMsg)
			{
			case WM_MAINTOOLBAR_INIT:
				{
					MainWindowToolbarData* xData = BGSEE_GETWINDOWXDATA(MainWindowToolbarData, SubclassParams->In.ExtraData);
					if (xData == nullptr)
					{
						xData = new MainWindowToolbarData();
						SubclassParams->In.ExtraData->Add(xData);
					}
				}

				break;
			case WM_DESTROY:
				{
					RemovePropA(hWnd, "CSE_MainToolbarVisualCache");
					RemovePropA(hWnd, "CSE_MainToolbarVisualCacheEx");
					RemovePropA(hWnd, "CSE_MainToolbarBaseGlyphW");
					RemovePropA(hWnd, "CSE_MainToolbarBaseGlyphH");
					RemovePropA(hWnd, "CSE_ModernChromeDarkModeState");
					RemovePropA(hWnd, "CSE_ToolbarExternalIconSizeCache");

					MainWindowToolbarData* xData = BGSEE_GETWINDOWXDATA(MainWindowToolbarData, SubclassParams->In.ExtraData);
					if (xData)
					{
						SubclassParams->In.ExtraData->Remove(MainWindowToolbarData::kTypeID);
						delete xData;
					}
				}

				break;
			case WM_DPICHANGED:
			case WM_THEMECHANGED:
			case WM_SETTINGCHANGE:
				UpdateToolbarVisualsForDPIAndTheme(hWnd, true);
				ApplyModernChromeToPrimaryWindows(true);
				LayoutToolbarExtras(hWnd);
				break;
			case WM_COMMAND:
				{
					MainWindowToolbarData* xData = BGSEE_GETWINDOWXDATA(MainWindowToolbarData, SubclassParams->In.ExtraData);
					SME_ASSERT(xData);

					if (HIWORD(wParam) == EN_CHANGE &&
						LOWORD(wParam) == IDC_TOOLBAR_TODCURRENT &&
						xData->SettingTODSlider == false)
					{
						xData->SettingTODSlider = true;
						float TOD = TESDialog::GetDlgItemFloat(hWnd, IDC_TOOLBAR_TODCURRENT);
						SendMessage(hWnd, WM_MAINTOOLBAR_SETTOD, TOD * 4.0, NULL);
						xData->SettingTODSlider = false;
					}
				}

				break;
			case WM_HSCROLL:
				{
					bool BreakOut = true;

					switch (LOWORD(wParam))
					{
					case TB_BOTTOM:
					case TB_ENDTRACK:
					case TB_LINEDOWN:
					case TB_LINEUP:
					case TB_PAGEDOWN:
					case TB_PAGEUP:
					case TB_THUMBPOSITION:
					case TB_THUMBTRACK:
					case TB_TOP:
						if ((HWND)lParam == TODSlider)
							BreakOut = false;

						break;
					}

					if (BreakOut)
						break;
				}
			case WM_MAINTOOLBAR_SETTOD:
				{
					if (uMsg != WM_HSCROLL)
						SendDlgItemMessage(hWnd, IDC_TOOLBAR_TODSLIDER, TBM_SETPOS, TRUE, (LPARAM)wParam);

					int Position = SendMessage(TODSlider, TBM_GETPOS, NULL, NULL);
					float TOD = Position / 4.0;

					if (TOD > 24.0f)
						TOD = 24.0f;

					_TES->SetSkyTOD(TOD);

					MainWindowToolbarData* xData = BGSEE_GETWINDOWXDATA(MainWindowToolbarData, SubclassParams->In.ExtraData);
					SME_ASSERT(xData);

					if (xData->SettingTODSlider == false)
						TESDialog::SetDlgItemFloat(hWnd, IDC_TOOLBAR_TODCURRENT, TOD, 2);

					TESPreviewControl::UpdatePreviewWindows();
				}

				break;
			}

			return DlgProcResult;
		}

		void InitializeMainWindowOverrides()
		{
			BGSEEUI->GetSubclasser()->RegisterSubclassForWindow(BGSEEUI->GetMainWindow(), uiManager::MainWindowMenuInitSubclassProc);
			BGSEEUI->GetSubclasser()->RegisterSubclassForWindow(BGSEEUI->GetMainWindow(), uiManager::MainWindowMenuSelectSubclassProc);
			BGSEEUI->GetSubclasser()->RegisterSubclassForWindow(BGSEEUI->GetMainWindow(), uiManager::MainWindowMiscSubclassProc);

			SendMessage(BGSEEUI->GetMainWindow(), WM_MAINWINDOW_INIT_DIALOG, NULL, NULL);
		}

	}
}
