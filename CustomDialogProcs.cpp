#include "CustomDialogProcs.h"
#include "Construction Set Extender_Resource.h"
#include "Hooks\Hooks-AssetSelector.h"
#include "Achievements.h"
#include "EditorAPI/Core.h"
#include <algorithm>
#include <cmath>

namespace cse
{
	namespace uiManager
	{

		BOOL CALLBACK AssetSelectorDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			switch (uMsg)
			{
			case WM_GETMINMAXINFO:
			{
				MINMAXINFO* SizeInfo = (MINMAXINFO*)lParam;
				SizeInfo->ptMaxTrackSize.x = SizeInfo->ptMinTrackSize.x = 189;
				SizeInfo->ptMaxTrackSize.y = SizeInfo->ptMinTrackSize.y = 240;

				break;
			}
			case WM_COMMAND:
				switch (LOWORD(wParam))
				{
				case IDC_ASSETSELECTOR_FILEBROWSER:
					EndDialog(hWnd, hooks::e_FileBrowser);

					return TRUE;
				case IDC_ASSETSELECTOR_ARCHIVEBROWSER:
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_AssetSelection);
					EndDialog(hWnd, hooks::e_BSABrowser);

					return TRUE;
				case IDC_ASSETSELECTOR_PATHEDITOR:
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_AssetSelection);
					EndDialog(hWnd, hooks::e_EditPath);

					return TRUE;
				case IDC_ASSETSELECTOR_CLEARPATH:
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_AssetSelection);
					EndDialog(hWnd, hooks::e_ClearPath);

					return TRUE;
				case IDC_ASSETSELECTOR_ASSETEXTRACTOR:
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_AssetSelection);
					EndDialog(hWnd, hooks::e_ExtractPath);

					return TRUE;
				case IDC_ASSETSELECTOR_OPENASSET:
					EndDialog(hWnd, hooks::e_OpenPath);

					return TRUE;
				case IDC_CSE_CANCEL:
					EndDialog(hWnd, hooks::e_Close);

					return TRUE;
				}

				break;
			}

			return FALSE;
		}

		BOOL CALLBACK TextEditDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			switch (uMsg)
			{
			case WM_COMMAND:
				{
					InitDialogMessageParamT<UInt32>* InitParam = (InitDialogMessageParamT<UInt32>*)GetWindowLongPtr(hWnd, GWL_USERDATA);

					switch (LOWORD(wParam))
					{
					case IDC_CSE_OK:
						GetDlgItemText(hWnd, IDC_TEXTEDIT_TEXT, InitParam->Buffer, sizeof(InitParam->Buffer));
						EndDialog(hWnd, 1);

						return TRUE;
					case IDC_CSE_CANCEL:
						EndDialog(hWnd, NULL);

						return TRUE;
					}
				}

				break;
			case WM_INITDIALOG:
				SetWindowLongPtr(hWnd, GWL_USERDATA, (LONG_PTR)lParam);
				SetDlgItemText(hWnd, IDC_TEXTEDIT_TEXT, ((InitDialogMessageParamT<UInt32>*)lParam)->Buffer);

				break;
			}

			return FALSE;
		}

		BOOL CALLBACK TESFileSaveDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			switch (uMsg)
			{
			case WM_COMMAND:
				switch (LOWORD(wParam))
				{
				case IDC_TESFILESAVE_SAVEESP:
					EndDialog(hWnd, 0);

					return TRUE;
				case IDC_TESFILESAVE_SAVEESM:
					achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_SaveAsESM);
					EndDialog(hWnd, 1);

					return TRUE;
				}

				break;
			}

			return FALSE;
		}

		BOOL CALLBACK TESComboBoxDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			HWND ComboBox = GetDlgItem(hWnd, IDC_TESCOMBOBOX_FORMLIST);

			switch (uMsg)
			{
			case WM_COMMAND:
				switch (LOWORD(wParam))
				{
				case IDC_CSE_OK:
					{
						TESForm* SelectedForm = (TESForm*)TESComboBox::GetSelectedItemData(ComboBox);
						EndDialog(hWnd, (INT_PTR)SelectedForm);

						return TRUE;
					}
				case IDC_CSE_CANCEL:
					EndDialog(hWnd, 0);

					return TRUE;
				}
				break;
			case WM_INITDIALOG:
				TESComboBox::PopulateWithForms(ComboBox, lParam, true, false);
				TESComboBox::SetSelectedItemByIndex(ComboBox, 0);

				break;
			}

			return FALSE;
		}

		LRESULT CALLBACK CreateGlobalScriptDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			switch (uMsg)
			{
			case WM_COMMAND:
				switch (LOWORD(wParam))
				{
				case IDC_CSE_OK:
					{
						char QuestID[0x200] = { 0 };
						char ScriptID[0x200] = { 0 };
						char Delay[8] = { 0 };
						char Buffer[0x200] = { 0 };

						GetDlgItemText(hWnd, IDC_GLOBALSCRIPT_QUESTID, QuestID, sizeof(QuestID));
						GetDlgItemText(hWnd, IDC_GLOBALSCRIPT_SCRIPTID, ScriptID, sizeof(ScriptID));
						GetDlgItemText(hWnd, IDC_GLOBALSCRIPT_DELAY, Delay, sizeof(Delay));

						TESForm* Form = nullptr;
						TESQuest* Quest = nullptr;
						Script* QuestScript = nullptr;

						Form = TESForm::LookupByEditorID(QuestID);

						if (Form)
						{
							if (Form->formType == TESForm::kFormType_Quest)
							{
								if (BGSEEUI->MsgBoxW(hWnd,
													 MB_YESNO,
													 "Quest '%s' already exists. Do you want to replace its script with a newly created one?",
													 QuestID) != IDYES)
								{
									return TRUE;
								}
							}
							else
							{
								BGSEEUI->MsgBoxE(hWnd, 0, "EditorID '%s' is already in use.", QuestID);

								return TRUE;
							}

							Quest = CS_CAST(Form, TESForm, TESQuest);
						}
						else
						{
							Quest = CS_CAST(TESForm::CreateInstance(TESForm::kFormType_Quest), TESForm, TESQuest);
							Quest->SetFromActiveFile(true);
							Quest->SetEditorID(QuestID);

							_DATAHANDLER->quests.AddAt(Quest, eListEnd);
						}

						if (strlen(ScriptID) < 1)
							FORMAT_STR(ScriptID, "%sScript", QuestID);

						Form = TESForm::LookupByEditorID(ScriptID);

						if (Form)
						{
							BGSEEUI->MsgBoxE(hWnd, 0, "EditorID '%s' is already in use.", ScriptID);

							return TRUE;
						}
						else
						{
							QuestScript = CS_CAST(TESForm::CreateInstance(TESForm::kFormType_Script), TESForm, Script);
							QuestScript->info.type = Script::kScriptType_Quest;
							QuestScript->SetFromActiveFile(true);

							if (strlen(Delay))
								FORMAT_STR(Buffer, "scn %s\n\nfloat fQuestDelayTime\n\nBegin GameMode\n\tset fQuestDelayTime to %s\n\nend", ScriptID, Delay);
							else
								FORMAT_STR(Buffer, "scn %s\n\nBegin GameMode\n\nEnd", ScriptID);

							QuestScript->SetText(Buffer);
							QuestScript->SetEditorID(ScriptID);

							_DATAHANDLER->scripts.AddAt(QuestScript, eListEnd);
							_DATAHANDLER->SortScripts();
						}

						Quest->script = QuestScript;

						Quest->LinkForm();
						QuestScript->LinkForm();

						Quest->UpdateUsageInfo();
						QuestScript->UpdateUsageInfo();
						QuestScript->AddCrossReference(Quest);

						achievements::kPowerUser->UnlockTool(achievements::AchievementPowerUser::kTool_GlobalScript);
						TESDialog::ShowScriptEditorDialog(TESForm::LookupByEditorID(ScriptID));
						DestroyWindow(hWnd);

						return TRUE;
					}
				case IDC_CSE_CANCEL:
					DestroyWindow(hWnd);

					return TRUE;
				}

				break;
			}

			return FALSE;
		}

		BOOL CALLBACK BindScriptDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			HWND EditorIDBox = GetDlgItem(hWnd, IDC_BINDSCRIPT_NEWFORMEDITORID);
			HWND RefIDBox = GetDlgItem(hWnd, IDC_BINDSCRIPT_NEWREFEDITORID);
			HWND ExistFormList = GetDlgItem(hWnd, IDC_BINDSCRIPT_EXISTINGFORMLIST);
			HWND SelParentCellBtn = GetDlgItem(hWnd, IDC_BINDSCRIPT_SELECTPARENTCELL);

			switch (uMsg)
			{
			case WM_COMMAND:
				switch (LOWORD(wParam))
				{
				case IDC_BINDSCRIPT_SELECTPARENTCELL:
					{
						TESForm* Selection = (TESForm*)BGSEEUI->ModalDialog(BGSEEMAIN->GetExtenderHandle(),
																			MAKEINTRESOURCE(IDD_TESCOMBOBOX),
																			hWnd,
																			(DLGPROC)TESComboBoxDlgProc,
																			(LPARAM)TESForm::kFormType_Cell);

						if (Selection)
						{
							char Buffer[0x200] = { 0 };
							FORMAT_STR(Buffer, "%s (%08X)", Selection->editorID.c_str(), Selection->formID);

							SetWindowText(SelParentCellBtn, (LPCSTR)Buffer);
							SetWindowLongPtr(SelParentCellBtn, GWL_USERDATA, (LONG_PTR)Selection);
						}
						break;
					}
				case IDC_CSE_OK:
					{
						if (IsDlgButtonChecked(hWnd, IDC_BINDSCRIPT_BINDEXISTFORM))
						{
							TESForm* SelectedForm = (TESForm*)TESComboBox::GetSelectedItemData(ExistFormList);
							if (SelectedForm)
							{
								EndDialog(hWnd, (INT_PTR)SelectedForm);
								return TRUE;
							}
							else
								BGSEEUI->MsgBoxE(hWnd, 0, "Invalid existing form selected");
						}
						else
						{
							char BaseEditorID[0x200] = { 0 };
							char RefEditorID[0x200] = { 0 };
							char Buffer[0x200] = { 0 };

							Edit_GetText(EditorIDBox, BaseEditorID, 0x200);
							if (TESForm::LookupByEditorID(BaseEditorID))
								BGSEEUI->MsgBoxE(hWnd, 0, "EditorID '%s' is already in use.", BaseEditorID);
							else
							{
								if (IsDlgButtonChecked(hWnd, IDC_BINDSCRIPT_QUESTFORM))
								{
									bool StartGameEnabledFlag = IsDlgButtonChecked(hWnd, IDC_BINDSCRIPT_QUESTSTARTGAMEENABLED);
									bool RepeatedStagesFlag = IsDlgButtonChecked(hWnd, IDC_BINDSCRIPT_QUESTREPEATEDSTAGES);

									TESQuest* Quest = CS_CAST(TESForm::CreateInstance(TESForm::kFormType_Quest), TESForm, TESQuest);
									Quest->SetFromActiveFile(true);
									Quest->SetEditorID(BaseEditorID);
									Quest->SetStartGameEnabledFlag(StartGameEnabledFlag);
									Quest->SetAllowedRepeatedStagesFlag(RepeatedStagesFlag);
									_DATAHANDLER->quests.AddAt(Quest, eListEnd);

									TESDialog::ResetFormListControls();
									EndDialog(hWnd, (INT_PTR)Quest);

									return TRUE;
								}
								else
								{
									if (IsDlgButtonChecked(hWnd, IDC_BINDSCRIPT_OBJECTTOKEN))
									{
										bool QuestItem = IsDlgButtonChecked(hWnd, IDC_BINDSCRIPT_OBJECTTOKENQUESTITEM);

										TESObjectCLOT* Token = CS_CAST(TESForm::CreateInstance(TESForm::kFormType_Clothing), TESForm, TESObjectCLOT);
										Token->SetFromActiveFile(true);
										Token->SetEditorID(BaseEditorID);
										_DATAHANDLER->AddTESObject(Token);

										SME::MiscGunk::ToggleFlag(&Token->bipedModelFlags, TESBipedModelForm::kBipedModelFlags_NotPlayable, true);
										SME::MiscGunk::ToggleFlag(&Token->formFlags, TESForm::kFormFlags_QuestItem, QuestItem);

										TESDialog::ResetFormListControls();
										EndDialog(hWnd, (INT_PTR)Token);

										return TRUE;
									}
									else
									{
										Edit_GetText(RefIDBox, RefEditorID, 0x200);
										if (TESForm::LookupByEditorID(RefEditorID))
											BGSEEUI->MsgBoxE(hWnd, 0, "EditorID '%s' is already in use.", BaseEditorID);
										else
										{
											bool InitiallyDisabled = IsDlgButtonChecked(hWnd, IDC_BINDSCRIPT_OBJECTREFERENCEDISABLED);
											TESObjectCELL* ParentCell = CS_CAST(GetWindowLongPtr(SelParentCellBtn, GWL_USERDATA), TESForm, TESObjectCELL);

											if (!ParentCell || ParentCell->IsInterior() == 0)
												BGSEEUI->MsgBoxE(hWnd, 0, "Invalid/exterior cell selected as parent.");
											else
											{
												TESObjectACTI* Activator = CS_CAST(TESForm::CreateInstance(TESForm::kFormType_Activator),
																				   TESForm, TESObjectACTI);
												Activator->SetFromActiveFile(true);
												Activator->SetEditorID(BaseEditorID);
												_DATAHANDLER->AddTESObject(Activator);

												static Vector3 ZeroVector(0.0, 0.0, 0.0);

												TESObjectREFR* Ref = _DATAHANDLER->PlaceObjectRef(Activator,
																								  &ZeroVector,
																								  &ZeroVector,
																								  CS_CAST(ParentCell, TESForm, TESObjectCELL),
																								  nullptr,
																								  nullptr);

												SME::MiscGunk::ToggleFlag(&Ref->formFlags, TESForm::kFormFlags_Disabled, InitiallyDisabled);
												SME::MiscGunk::ToggleFlag(&Ref->formFlags, TESForm::kFormFlags_QuestItem, true);

												Ref->SetEditorID(RefEditorID);

												TESDialog::ResetFormListControls();
												EndDialog(hWnd, (INT_PTR)Activator);

												return TRUE;
											}
										}
									}
								}
							}
						}

						return FALSE;
					}
				case IDC_CSE_CANCEL:
					EndDialog(hWnd, 0);

					return TRUE;
				}
				break;
			case WM_INITDIALOG:
				// show icon in taskbar
				auto WindowStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
				SetWindowLongPtr(hWnd, GWL_EXSTYLE, WindowStyle | WS_EX_APPWINDOW);

				Edit_SetText(EditorIDBox, "Base Form EditorID");
				Edit_SetText(RefIDBox, "Ref EditorID");

				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Activator, true, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Apparatus, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Armor, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Book, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Clothing, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Container, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Door, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Ingredient, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Light, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Misc, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Furniture, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Weapon, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Ammo, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_NPC, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Creature, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_SoulGem, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Key, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_AlchemyItem, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_SigilStone, false, false);
				TESComboBox::PopulateWithForms(ExistFormList, TESForm::kFormType_Quest, false, false);

				TESComboBox::SetSelectedItemByIndex(ExistFormList, 0);

				CheckDlgButton(hWnd, IDC_BINDSCRIPT_BINDEXISTFORM, BST_CHECKED);
				CheckDlgButton(hWnd, IDC_BINDSCRIPT_QUESTFORM, BST_CHECKED);
				CheckDlgButton(hWnd, IDC_BINDSCRIPT_OBJECTTOKEN, BST_CHECKED);

				SetWindowLongPtr(SelParentCellBtn, GWL_USERDATA, (LONG_PTR)0);

				break;
			}

			return FALSE;
		}

		BOOL CALLBACK EditResultScriptDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			switch (uMsg)
			{
			case WM_COMMAND:
				switch (LOWORD(wParam))
				{
				case IDC_CSE_CANCEL:
					EndDialog(hWnd, NULL);

					return TRUE;
				case IDC_EDITRESULTSCRIPT_COMPILE:
				case IDC_EDITRESULTSCRIPT_SAVE:
					char Buffer[0x1000] = { 0 };
					HWND Parent = (HWND)GetWindowLongPtr(hWnd, GWL_USERDATA);

					GetDlgItemText(hWnd, IDC_EDITRESULTSCRIPT_SCRIPTTEXT, Buffer, sizeof(Buffer));
					SetDlgItemText(Parent, kDialogEditor_ResultScriptTextBox, (LPSTR)Buffer);

					if (LOWORD(wParam) == IDC_EDITRESULTSCRIPT_COMPILE)
						EndDialog(hWnd, 1);
					else
						EndDialog(hWnd, NULL);

					return TRUE;
				}

				break;
			case WM_INITDIALOG:
				SetWindowLongPtr(hWnd, GWL_USERDATA, (LONG_PTR)lParam);

				HWND ResultScriptEditBox = GetDlgItem((HWND)lParam, kDialogEditor_ResultScriptTextBox);
				char Buffer[0x1000] = { 0 };

				GetWindowText(ResultScriptEditBox, Buffer, sizeof(Buffer));
				SetDlgItemText(hWnd, IDC_EDITRESULTSCRIPT_SCRIPTTEXT, (LPSTR)Buffer);

				break;
			}

			return FALSE;
		}


		struct MarkerPlacementState
		{
			int NextMarkerIndex = 1;
			int SelectedCellX = 0;
			int SelectedCellY = 0;
			bool ShowMapOverlap = true;
			bool ShowRegions = true;
			float Zoom = 1.0f;
			float PanCellX = 0.0f;
			float PanCellY = 0.0f;
			bool Dragging = false;
			POINT DragLastScreenPos = { 0, 0 };
		};

		static const char* MarkerPlacement_GetWorldspaceName(TESWorldSpace* Worldspace)
		{
			if (Worldspace == nullptr)
				return "<Worldspace>";

			const char* Name = Worldspace->name.c_str();
			if (Name == nullptr || strlen(Name) == 0)
				Name = Worldspace->GetEditorID();

			if (Name == nullptr || strlen(Name) == 0)
				Name = "<Unnamed Worldspace>";

			return Name;
		}

		static const char* MarkerPlacement_GetMarkerType(HWND hWnd)
		{
			if (IsDlgButtonChecked(hWnd, IDC_MARKERPLACEMENT_MARKERTYPE_CITY) == BST_CHECKED)
				return "City";
			if (IsDlgButtonChecked(hWnd, IDC_MARKERPLACEMENT_MARKERTYPE_TOWN) == BST_CHECKED)
				return "Town";
			if (IsDlgButtonChecked(hWnd, IDC_MARKERPLACEMENT_MARKERTYPE_SETTLEMENT) == BST_CHECKED)
				return "Settlement";
			if (IsDlgButtonChecked(hWnd, IDC_MARKERPLACEMENT_MARKERTYPE_CAVE) == BST_CHECKED)
				return "Cave";
			if (IsDlgButtonChecked(hWnd, IDC_MARKERPLACEMENT_MARKERTYPE_FORT) == BST_CHECKED)
				return "Fort";

			return nullptr;
		}

		static TESWorldSpace* MarkerPlacement_GetSelectedWorldspace(HWND hWnd)
		{
			HWND WorldspaceCombo = GetDlgItem(hWnd, IDC_MARKERPLACEMENT_WORLDSPACES);
			int Selection = (int)SendMessage(WorldspaceCombo, CB_GETCURSEL, 0, 0);
			if (Selection < 0)
				return nullptr;

			return (TESWorldSpace*)SendMessage(WorldspaceCombo, CB_GETITEMDATA, Selection, 0);
		}

		static void MarkerPlacement_UpdateCellCaption(HWND hWnd, MarkerPlacementState* State);

		struct MarkerPlacementWorldspaceBounds
		{
			float Left = -8.0f;
			float Right = 8.0f;
			float Bottom = -8.0f;
			float Top = 8.0f;
		};

		static MarkerPlacementWorldspaceBounds MarkerPlacement_GetWorldspaceBounds(TESWorldSpace* Worldspace)
		{
			MarkerPlacementWorldspaceBounds Result;
			if (Worldspace == nullptr)
				return Result;

			float LeftCell = (float)Worldspace->northWestCoordX;
			float TopCell = (float)Worldspace->northWestCoordY;
			float RightCell = (float)Worldspace->southEastCoordX;
			float BottomCell = (float)Worldspace->southEastCoordY;
			if (RightCell < LeftCell)
				std::swap(RightCell, LeftCell);
			if (TopCell < BottomCell)
				std::swap(TopCell, BottomCell);

			if ((RightCell - LeftCell) < 1.0f || (TopCell - BottomCell) < 1.0f)
			{
				const float HalfX = (std::max)(1.0f, Worldspace->usableDimensionsX * 0.5f);
				const float HalfY = (std::max)(1.0f, Worldspace->usableDimensionsY * 0.5f);
				LeftCell = -HalfX;
				RightCell = HalfX;
				BottomCell = -HalfY;
				TopCell = HalfY;
			}

			Result.Left = LeftCell;
			Result.Right = RightCell;
			Result.Bottom = BottomCell;
			Result.Top = TopCell;
			return Result;
		}

		static void MarkerPlacement_ResetViewForWorldspace(HWND hWnd, MarkerPlacementState* State, TESWorldSpace* Worldspace)
		{
			if (State == nullptr)
				return;

			auto Bounds = MarkerPlacement_GetWorldspaceBounds(Worldspace);
			State->PanCellX = (Bounds.Left + Bounds.Right) * 0.5f;
			State->PanCellY = (Bounds.Bottom + Bounds.Top) * 0.5f;
			const float SpanX = (std::max)(1.0f, Bounds.Right - Bounds.Left);
			const float SpanY = (std::max)(1.0f, Bounds.Top - Bounds.Bottom);
			const float Span = (std::max)(SpanX, SpanY) + 2.0f;
			State->Zoom = (std::max)(0.25f, (std::min)(8.0f, 16.0f / Span));

			MarkerPlacement_UpdateCellCaption(hWnd, State);
		}

		static bool MarkerPlacement_TryGetGridRect(HWND hWnd, RECT& OutRect)
		{
			HWND Grid = GetDlgItem(hWnd, IDC_MARKERPLACEMENT_CELLGRID);
			if (Grid == nullptr || GetWindowRect(Grid, &OutRect) == FALSE)
				return false;

			const int Width = OutRect.right - OutRect.left;
			const int Height = OutRect.bottom - OutRect.top;
			return Width > 4 && Height > 4;
		}

		static bool MarkerPlacement_ScreenToCell(HWND hWnd, const MarkerPlacementState* State, POINT CursorPos, float& OutCellX, float& OutCellY)
		{
			SME_ASSERT(State);
			RECT GridRect = { 0 };
			if (!MarkerPlacement_TryGetGridRect(hWnd, GridRect))
				return false;

			const int Width = GridRect.right - GridRect.left;
			const int Height = GridRect.bottom - GridRect.top;
			const int Side = (std::min)(Width, Height);
			const int OriginX = GridRect.left + (Width - Side) / 2;
			const int OriginY = GridRect.top + (Height - Side) / 2;

			const int LocalX = CursorPos.x - OriginX;
			const int LocalY = CursorPos.y - OriginY;
			if (LocalX < 0 || LocalY < 0 || LocalX >= Side || LocalY >= Side)
				return false;

			const float HalfCellsVisible = 8.0f / State->Zoom;
			const float CellsPerPixel = (HalfCellsVisible * 2.0f) / Side;

			OutCellX = (LocalX * CellsPerPixel) - HalfCellsVisible + State->PanCellX;
			OutCellY = ((Side - LocalY) * CellsPerPixel) - HalfCellsVisible + State->PanCellY;
			return true;
		}

		static bool MarkerPlacement_SelectCellFromScreenPoint(HWND hWnd, MarkerPlacementState* State, POINT CursorPos)
		{
			SME_ASSERT(State);

			float CellX = 0, CellY = 0;
			if (!MarkerPlacement_ScreenToCell(hWnd, State, CursorPos, CellX, CellY))
				return false;

			State->SelectedCellX = (int)floor(CellX);
			State->SelectedCellY = (int)floor(CellY);
			MarkerPlacement_UpdateCellCaption(hWnd, State);
			return true;
		}

		static void MarkerPlacement_DrawGrid(HWND hWnd, const DRAWITEMSTRUCT* DrawInfo, const MarkerPlacementState* State)
		{
			SME_ASSERT(DrawInfo);

			const RECT& Rect = DrawInfo->rcItem;
			const int Width = Rect.right - Rect.left;
			const int Height = Rect.bottom - Rect.top;
			if (Width <= 0 || Height <= 0)
				return;

			HDC DC = DrawInfo->hDC;
			int SavedDC = SaveDC(DC);
			SetBkMode(DC, TRANSPARENT);

			const int Side = (std::min)(Width, Height);
			const int OriginX = Rect.left + (Width - Side) / 2;
			const int OriginY = Rect.top + (Height - Side) / 2;
			RECT SquareRect = { OriginX, OriginY, OriginX + Side, OriginY + Side };

			HBRUSH BackBrush = CreateSolidBrush(RGB(24, 28, 34));
			FillRect(DC, &Rect, BackBrush);
			DeleteObject(BackBrush);

			HBRUSH PaneBrush = CreateSolidBrush(RGB(30, 35, 42));
			FillRect(DC, &SquareRect, PaneBrush);
			DeleteObject(PaneBrush);
			IntersectClipRect(DC, SquareRect.left, SquareRect.top, SquareRect.right, SquareRect.bottom);

			const float Zoom = State ? State->Zoom : 1.0f;
			const float PanX = State ? State->PanCellX : 0.0f;
			const float PanY = State ? State->PanCellY : 0.0f;
			const float HalfCellsVisible = 8.0f / Zoom;
			const float PixelsPerCell = Side / (HalfCellsVisible * 2.0f);

			if (State == nullptr || State->ShowMapOverlap)
			{
				auto Bounds = MarkerPlacement_GetWorldspaceBounds(MarkerPlacement_GetSelectedWorldspace(hWnd));
				RECT OverlayRect = {
					(int)(OriginX + (Bounds.Left - (PanX - HalfCellsVisible)) * PixelsPerCell),
					(int)(OriginY + (PanY + HalfCellsVisible - Bounds.Top) * PixelsPerCell),
					(int)(OriginX + (Bounds.Right - (PanX - HalfCellsVisible)) * PixelsPerCell),
					(int)(OriginY + (PanY + HalfCellsVisible - Bounds.Bottom) * PixelsPerCell)
				};

				if (OverlayRect.left > OverlayRect.right)
					std::swap(OverlayRect.left, OverlayRect.right);
				if (OverlayRect.top > OverlayRect.bottom)
					std::swap(OverlayRect.top, OverlayRect.bottom);

				HBRUSH OverlayBrush = CreateSolidBrush(RGB(48, 68, 88));
				FillRect(DC, &OverlayRect, OverlayBrush);
				DeleteObject(OverlayBrush);

				HBRUSH OverlayBorder = CreateSolidBrush(RGB(86, 130, 168));
				FrameRect(DC, &OverlayRect, OverlayBorder);
				DeleteObject(OverlayBorder);
			}

			HPEN GridPen = CreatePen(PS_SOLID, 1, RGB(82, 106, 128));
			HPEN AxisPen = CreatePen(PS_SOLID, 1, RGB(194, 158, 74));
			HGDIOBJ OldPen = SelectObject(DC, GridPen);

			const int GridLineCount = (int)(HalfCellsVisible * 2.0f) + 2;
			for (int i = -GridLineCount; i <= GridLineCount; i++)
			{
				const float CellX = floor(PanX) + i;
				const float CellY = floor(PanY) + i;
				const int X = (int)(OriginX + ((CellX - (PanX - HalfCellsVisible)) * PixelsPerCell));
				const int Y = (int)(OriginY + ((PanY + HalfCellsVisible - CellY) * PixelsPerCell));

				if ((int)CellX == 0 || (int)CellY == 0)
					SelectObject(DC, AxisPen);
				else
					SelectObject(DC, GridPen);

				MoveToEx(DC, X, SquareRect.top, nullptr);
				LineTo(DC, X, SquareRect.bottom);
				MoveToEx(DC, SquareRect.left, Y, nullptr);
				LineTo(DC, SquareRect.right, Y);
			}

			if (State && State->ShowRegions)
			{
				HPEN RegionPen = CreatePen(PS_SOLID, 1, RGB(152, 106, 178));
				SelectObject(DC, RegionPen);
				const int RegionStartX = ((int)floor(PanX - HalfCellsVisible) / 8) * 8;
				const int RegionEndX = ((int)ceil(PanX + HalfCellsVisible) / 8) * 8;
				const int RegionStartY = ((int)floor(PanY - HalfCellsVisible) / 8) * 8;
				const int RegionEndY = ((int)ceil(PanY + HalfCellsVisible) / 8) * 8;
				for (int x = RegionStartX; x <= RegionEndX; x += 8)
				{
					int Px = (int)(OriginX + ((x - (PanX - HalfCellsVisible)) * PixelsPerCell));
					MoveToEx(DC, Px, SquareRect.top, nullptr);
					LineTo(DC, Px, SquareRect.bottom);
				}
				for (int y = RegionStartY; y <= RegionEndY; y += 8)
				{
					int Py = (int)(OriginY + ((PanY + HalfCellsVisible - y) * PixelsPerCell));
					MoveToEx(DC, SquareRect.left, Py, nullptr);
					LineTo(DC, SquareRect.right, Py);
				}
				SelectObject(DC, GridPen);
				DeleteObject(RegionPen);
			}

			if (State)
			{
				RECT CellRect = {
					(int)(OriginX + ((State->SelectedCellX - (PanX - HalfCellsVisible)) * PixelsPerCell)),
					(int)(OriginY + ((PanY + HalfCellsVisible - (State->SelectedCellY + 1)) * PixelsPerCell)),
					(int)(OriginX + (((State->SelectedCellX + 1) - (PanX - HalfCellsVisible)) * PixelsPerCell)),
					(int)(OriginY + ((PanY + HalfCellsVisible - State->SelectedCellY) * PixelsPerCell))
				};

				HBRUSH SelectionBrush = CreateSolidBrush(RGB(225, 120, 36));
				FrameRect(DC, &CellRect, SelectionBrush);
				InflateRect(&CellRect, -1, -1);
				FrameRect(DC, &CellRect, SelectionBrush);
				DeleteObject(SelectionBrush);

				char OverlayText[0x120] = { 0 };
				FORMAT_STR(OverlayText,
					"Cell:(%d,%d) Zoom:%0.2fx  Drag:Pan  Wheel:Zoom  RMB:Place  Map:%s Regions:%s",
					State->SelectedCellX,
					State->SelectedCellY,
					State->Zoom,
					State->ShowMapOverlap ? "On" : "Off",
					State->ShowRegions ? "On" : "Off");

				SetTextColor(DC, RGB(235, 235, 235));
				RECT TextRect = Rect;
				InflateRect(&TextRect, -6, -6);
				DrawTextA(DC, OverlayText, -1, &TextRect, DT_LEFT | DT_TOP | DT_END_ELLIPSIS);
			}

			HBRUSH BorderBrush = CreateSolidBrush(RGB(120, 140, 160));
			FrameRect(DC, &SquareRect, BorderBrush);
			DeleteObject(BorderBrush);

			SelectObject(DC, OldPen);
			DeleteObject(GridPen);
			DeleteObject(AxisPen);
			RestoreDC(DC, SavedDC);
		}

		static void MarkerPlacement_UpdateCellCaption(HWND hWnd, MarkerPlacementState* State)
		{
			SME_ASSERT(State);
			HWND Grid = GetDlgItem(hWnd, IDC_MARKERPLACEMENT_CELLGRID);
			if (Grid)
				InvalidateRect(Grid, nullptr, FALSE);
		}

		static void MarkerPlacement_PopulateWorldspaces(HWND hWnd)
		{
			HWND WorldspaceCombo = GetDlgItem(hWnd, IDC_MARKERPLACEMENT_WORLDSPACES);
			SendMessage(WorldspaceCombo, CB_RESETCONTENT, 0, 0);

			int CurrentWorldspaceSelection = -1;
			int ItemIndex = 0;
			for (auto Itr = _DATAHANDLER->worldSpaces.Begin(); Itr.End() == false && Itr.Get(); ++Itr, ++ItemIndex)
			{
				TESWorldSpace* Worldspace = Itr.Get();
				int Added = (int)SendMessage(WorldspaceCombo, CB_ADDSTRING, 0, (LPARAM)MarkerPlacement_GetWorldspaceName(Worldspace));
				SendMessage(WorldspaceCombo, CB_SETITEMDATA, Added, (LPARAM)Worldspace);

				if (_TES->currentWorldSpace == Worldspace)
					CurrentWorldspaceSelection = ItemIndex;
			}

			if (SendMessage(WorldspaceCombo, CB_GETCOUNT, 0, 0) > 0)
				SendMessage(WorldspaceCombo, CB_SETCURSEL, (WPARAM)(std::max)(0, CurrentWorldspaceSelection), 0);
		}

		static void MarkerPlacement_AddPlacedMarker(HWND hWnd, MarkerPlacementState* State)
		{
			SME_ASSERT(State);

			TESWorldSpace* Worldspace = MarkerPlacement_GetSelectedWorldspace(hWnd);
			if (Worldspace == nullptr)
			{
				BGSEEUI->MsgBoxW(hWnd, 0, "Please select a worldspace first.");
				return;
			}

			const char* MarkerType = MarkerPlacement_GetMarkerType(hWnd);
			if (MarkerType == nullptr)
			{
				BGSEEUI->MsgBoxW(hWnd, 0, "Please select at least one map marker type.");
				return;
			}

			TESObject* MarkerBase = CS_CAST(TESForm::LookupByEditorID("MapMarker"), TESForm, TESObject);
			if (MarkerBase == nullptr)
			{
				BGSEEUI->MsgBoxE("Couldn't find the MapMarker base form.");
				return;
			}

			float MarkerX = (State->SelectedCellX * 4096.0f) + 2048.0f;
			float MarkerY = (State->SelectedCellY * 4096.0f) + 2048.0f;

			bool CreateCell = true;
			TESObjectCELL* ExteriorCell = _DATAHANDLER->GetExteriorCell(MarkerX, MarkerY, Worldspace, CreateCell);
			if (ExteriorCell == nullptr)
			{
				BGSEEUI->MsgBoxE("Couldn't resolve or create exterior cell (%d, %d).", State->SelectedCellX, State->SelectedCellY);
				return;
			}

			if (_TES->currentWorldSpace != Worldspace)
				_TES->SetCurrentWorldspace(Worldspace);

			Vector3 Position(MarkerX, MarkerY, 0.0f);
			Vector3 Rotation(0.0f, 0.0f, 0.0f);
			TESObjectREFR* PlacedRef = _DATAHANDLER->PlaceObjectRef(MarkerBase, &Position, &Rotation, ExteriorCell, Worldspace, nullptr);
			if (PlacedRef == nullptr)
			{
				BGSEEUI->MsgBoxE("Couldn't place map marker in cell (%d, %d).", State->SelectedCellX, State->SelectedCellY);
				return;
			}

			TESRenderWindow::Redraw();

			char Buffer[0x200] = { 0 };
			FORMAT_STR(Buffer, "Marker %03d | %s | %s | Cell (%d, %d) | Center (%0.1f, %0.1f)",
				State->NextMarkerIndex++,
				MarkerPlacement_GetWorldspaceName(Worldspace),
				MarkerType,
				State->SelectedCellX, State->SelectedCellY,
				MarkerX,
				MarkerY);

			SendDlgItemMessage(hWnd, IDC_MARKERPLACEMENT_MARKERLIST, LB_ADDSTRING, 0, (LPARAM)Buffer);
		}

		BOOL CALLBACK MarkerPlacementDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			auto* State = (MarkerPlacementState*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			switch (uMsg)
			{
			case WM_INITDIALOG:
				{
					auto* NewState = new MarkerPlacementState();
					SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)NewState);

					MarkerPlacement_PopulateWorldspaces(hWnd);
					CheckDlgButton(hWnd, IDC_MARKERPLACEMENT_MARKERTYPE_CITY, BST_CHECKED);
					CheckDlgButton(hWnd, IDC_MARKERPLACEMENT_SHOWMAPOVERLAP, BST_CHECKED);
					CheckDlgButton(hWnd, IDC_MARKERPLACEMENT_SHOWREGIONS, BST_CHECKED);
					NewState->ShowMapOverlap = true;
					NewState->ShowRegions = true;
					MarkerPlacement_ResetViewForWorldspace(hWnd, NewState, MarkerPlacement_GetSelectedWorldspace(hWnd));
					MarkerPlacement_UpdateCellCaption(hWnd, NewState);
				}
				return TRUE;
			case WM_NCDESTROY:
				if (State && State->Dragging)
				{
					State->Dragging = false;
					if (GetCapture() == hWnd)
						ReleaseCapture();
				}
				if (State)
					delete State;
				SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
				NotifyMarkerPlacementDialogDestroyed(hWnd);
				return FALSE;
			case WM_CONTEXTMENU:
				if (State && (HWND)wParam == GetDlgItem(hWnd, IDC_MARKERPLACEMENT_CELLGRID))
				{
					POINT Cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
					if (Cursor.x == -1 && Cursor.y == -1)
						GetCursorPos(&Cursor);

					if (MarkerPlacement_SelectCellFromScreenPoint(hWnd, State, Cursor))
						MarkerPlacement_AddPlacedMarker(hWnd, State);

					return TRUE;
				}
				break;
			case WM_LBUTTONDOWN:
				if (State)
				{
					POINT Cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
					ClientToScreen(hWnd, &Cursor);
					RECT GridRect = { 0 };
					if (MarkerPlacement_TryGetGridRect(hWnd, GridRect) && PtInRect(&GridRect, Cursor))
					{
						State->Dragging = true;
						State->DragLastScreenPos = Cursor;
						SetCapture(hWnd);
						MarkerPlacement_SelectCellFromScreenPoint(hWnd, State, Cursor);
						return TRUE;
					}
				}
				break;
			case WM_MOUSEMOVE:
				if (State)
				{
					POINT Cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
					ClientToScreen(hWnd, &Cursor);
					RECT GridRect = { 0 };
					if ((wParam & MK_LBUTTON) && MarkerPlacement_TryGetGridRect(hWnd, GridRect) && PtInRect(&GridRect, Cursor))
					{
						if (State->Dragging == false)
						{
							State->Dragging = true;
							State->DragLastScreenPos = Cursor;
							SetCapture(hWnd);
						}
					}
					if (State->Dragging && MarkerPlacement_TryGetGridRect(hWnd, GridRect))
					{
						const int Width = GridRect.right - GridRect.left;
						const int Height = GridRect.bottom - GridRect.top;
						const int Side = (std::min)(Width, Height);
						if (Side > 0)
						{
							const float HalfCellsVisible = 8.0f / State->Zoom;
							const float CellsPerPixel = (HalfCellsVisible * 2.0f) / Side;
							const int DeltaX = Cursor.x - State->DragLastScreenPos.x;
							const int DeltaY = Cursor.y - State->DragLastScreenPos.y;
							State->PanCellX -= DeltaX * CellsPerPixel;
							State->PanCellY += DeltaY * CellsPerPixel;
							State->DragLastScreenPos = Cursor;
							MarkerPlacement_UpdateCellCaption(hWnd, State);
							return TRUE;
						}
					}
				}
				break;
			case WM_LBUTTONUP:
				if (State && State->Dragging)
				{
					State->Dragging = false;
					if (GetCapture() == hWnd)
						ReleaseCapture();
					return TRUE;
				}
				break;
			case WM_MOUSEWHEEL:
				if (State)
				{
					POINT Cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
					RECT GridRect = { 0 };
					if (MarkerPlacement_TryGetGridRect(hWnd, GridRect) && PtInRect(&GridRect, Cursor))
					{
						float FocusCellX = 0.0f, FocusCellY = 0.0f;
						bool HasFocusCell = MarkerPlacement_ScreenToCell(hWnd, State, Cursor, FocusCellX, FocusCellY);

						const short Delta = GET_WHEEL_DELTA_WPARAM(wParam);
						if (Delta > 0)
							State->Zoom *= 1.1f;
						else if (Delta < 0)
							State->Zoom *= 0.9f;
						State->Zoom = (std::max)(0.25f, (std::min)(State->Zoom, 8.0f));

						if (HasFocusCell)
						{
							float NewFocusCellX = 0.0f, NewFocusCellY = 0.0f;
							if (MarkerPlacement_ScreenToCell(hWnd, State, Cursor, NewFocusCellX, NewFocusCellY))
							{
								State->PanCellX += FocusCellX - NewFocusCellX;
								State->PanCellY += FocusCellY - NewFocusCellY;
							}
						}

						MarkerPlacement_UpdateCellCaption(hWnd, State);
						return TRUE;
					}
				}
				break;
			case WM_COMMAND:
				if (HIWORD(wParam) == BN_CLICKED)
				{
					const int MarkerTypes[] = {
						IDC_MARKERPLACEMENT_MARKERTYPE_CITY,
						IDC_MARKERPLACEMENT_MARKERTYPE_TOWN,
						IDC_MARKERPLACEMENT_MARKERTYPE_SETTLEMENT,
						IDC_MARKERPLACEMENT_MARKERTYPE_CAVE,
						IDC_MARKERPLACEMENT_MARKERTYPE_FORT
					};

					for (auto TypeID : MarkerTypes)
					{
						if ((int)LOWORD(wParam) == TypeID)
						{
							for (auto OtherTypeID : MarkerTypes)
								CheckDlgButton(hWnd, OtherTypeID, OtherTypeID == TypeID ? BST_CHECKED : BST_UNCHECKED);
							return TRUE;
						}
					}
				}

				switch (LOWORD(wParam))
				{
				case IDC_MARKERPLACEMENT_CELLGRID:
					if (HIWORD(wParam) == STN_CLICKED && State)
					{
						POINT Cursor = { 0 };
						GetCursorPos(&Cursor);
						MarkerPlacement_SelectCellFromScreenPoint(hWnd, State, Cursor);
						return TRUE;
					}
					break;
				case IDC_MARKERPLACEMENT_WORLDSPACES:
					if (HIWORD(wParam) == CBN_SELCHANGE)
					{
						TESWorldSpace* SelectedWorldspace = MarkerPlacement_GetSelectedWorldspace(hWnd);
						if (SelectedWorldspace)
							_TES->SetCurrentWorldspace(SelectedWorldspace);
						MarkerPlacement_ResetViewForWorldspace(hWnd, State, SelectedWorldspace);
						return TRUE;
					}
					break;
				case IDC_MARKERPLACEMENT_SHOWMAPOVERLAP:
					if (State && HIWORD(wParam) == BN_CLICKED)
					{
						State->ShowMapOverlap = (IsDlgButtonChecked(hWnd, IDC_MARKERPLACEMENT_SHOWMAPOVERLAP) == BST_CHECKED);
						MarkerPlacement_UpdateCellCaption(hWnd, State);
						return TRUE;
					}
					break;
				case IDC_MARKERPLACEMENT_SHOWREGIONS:
					if (State && HIWORD(wParam) == BN_CLICKED)
					{
						State->ShowRegions = (IsDlgButtonChecked(hWnd, IDC_MARKERPLACEMENT_SHOWREGIONS) == BST_CHECKED);
						MarkerPlacement_UpdateCellCaption(hWnd, State);
						return TRUE;
					}
					break;
				case IDC_MARKERPLACEMENT_PLACEBTN:
					if (State)
						MarkerPlacement_AddPlacedMarker(hWnd, State);
					return TRUE;
				case IDC_MARKERPLACEMENT_CLOSEBTN:
					DestroyWindow(hWnd);
					return TRUE;
				}
				break;
			case WM_DRAWITEM:
				if (wParam == IDC_MARKERPLACEMENT_CELLGRID)
				{
					MarkerPlacement_DrawGrid(hWnd, (const DRAWITEMSTRUCT*)lParam, State);
					return TRUE;
				}
				break;
			}

			return FALSE;
		}

	}
}
