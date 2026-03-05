#include "CustomDialogProcs.h"
#include "Construction Set Extender_Resource.h"
#include "Hooks\Hooks-AssetSelector.h"
#include "Achievements.h"
#include "EditorAPI/Core.h"

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

		static void MarkerPlacement_UpdateCellCaption(HWND hWnd, MarkerPlacementState* State)
		{
			SME_ASSERT(State);

			char Buffer[0x100] = { 0 };
			FORMAT_STR(Buffer,
				"Selected Cell: (%d, %d)\r\nRight-click in this pane to place map marker at cell center.",
				State->SelectedCellX,
				State->SelectedCellY);
			SetDlgItemText(hWnd, IDC_MARKERPLACEMENT_CELLGRID, Buffer);
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
				SendMessage(WorldspaceCombo, CB_SETCURSEL, max(0, CurrentWorldspaceSelection), 0);
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

			char Buffer[0x200] = { 0 };
			FORMAT_STR(Buffer, "Marker %03d | %s | %s | Cell (%d, %d) | Center (%0.1f, %0.1f)",
				State->NextMarkerIndex++,
				MarkerPlacement_GetWorldspaceName(Worldspace),
				MarkerType,
				State->SelectedCellX, State->SelectedCellY,
				(State->SelectedCellX * 4096.0f) + 2048.0f,
				(State->SelectedCellY * 4096.0f) + 2048.0f);

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
					MarkerPlacement_UpdateCellCaption(hWnd, NewState);
				}
				return TRUE;
			case WM_NCDESTROY:
				if (State)
					delete State;
				SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
				return FALSE;
			case WM_CONTEXTMENU:
				if (State && (HWND)wParam == GetDlgItem(hWnd, IDC_MARKERPLACEMENT_CELLGRID))
				{
					POINT Cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
					if (Cursor.x == -1 && Cursor.y == -1)
						GetCursorPos(&Cursor);

					RECT GridRect = { 0 };
					HWND Grid = GetDlgItem(hWnd, IDC_MARKERPLACEMENT_CELLGRID);
					GetWindowRect(Grid, &GridRect);

					int Width = (GridRect.right - GridRect.left);
					int Height = (GridRect.bottom - GridRect.top);
					if (Width <= 0 || Height <= 0)
						return TRUE;

					int LocalX = Cursor.x - GridRect.left;
					int LocalY = Cursor.y - GridRect.top;
					if (LocalX < 0) LocalX = 0;
					if (LocalY < 0) LocalY = 0;
					if (LocalX >= Width) LocalX = Width - 1;
					if (LocalY >= Height) LocalY = Height - 1;

					const int GridCells = 16;
					State->SelectedCellX = (LocalX * GridCells) / Width - (GridCells / 2);
					State->SelectedCellY = ((Height - 1 - LocalY) * GridCells) / Height - (GridCells / 2);

					MarkerPlacement_UpdateCellCaption(hWnd, State);
					MarkerPlacement_AddPlacedMarker(hWnd, State);
					return TRUE;
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
				case IDC_MARKERPLACEMENT_WORLDSPACES:
					if (HIWORD(wParam) == CBN_SELCHANGE)
					{
						TESWorldSpace* SelectedWorldspace = MarkerPlacement_GetSelectedWorldspace(hWnd);
						if (SelectedWorldspace)
							_TES->SetCurrentWorldspace(SelectedWorldspace);
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
			}

			return FALSE;
		}

	}
}