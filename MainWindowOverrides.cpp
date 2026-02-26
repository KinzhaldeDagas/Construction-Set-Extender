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

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <vector>

#include "[BGSEEBase]\ToolBox.h"
#include "[BGSEEBase]\Script\CodaVM.h"

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

		static TESNPC* GetSpeakerFromTopicInfo(TESTopicInfo* Info)
		{
			if (Info == nullptr)
				return nullptr;

			for (ConditionListT::Iterator Itr = Info->conditions.Begin(); Itr.End() == false && Itr.Get(); ++Itr)
			{
				TESConditionItem* Condition = Itr.Get();
				SME_ASSERT(Condition);

				const UInt16 FunctionIndex = Condition->functionIndex & 0x0FFF;
				if (FunctionIndex == 72 || FunctionIndex == 224)
				{
					TESNPC* Speaker = CS_CAST(Condition->param1.form, TESForm, TESNPC);
					if (Speaker)
						return Speaker;
				}
			}

			return nullptr;
		}


		static std::string GetDefaultRevoiceCSVFileName(const char* ActivePluginName)
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

			return std::string("reVoice_") + BaseName + ".csv";
		}

		static bool IsAbsoluteRevoicePath(const std::string& Path)
		{
			if (Path.size() >= 2 && Path[1] == ':')
				return true;
			if (Path.size() >= 2 && Path[0] == '\\' && Path[1] == '\\')
				return true;
			return false;
		}

		static std::string EscapeCSVField(const char* Input)
		{
			std::string Value = Input ? Input : "";
			for (auto& Ch : Value)
			{
				if (Ch == '\r' || Ch == '\n')
					Ch = ' ';
			}

			std::string Result;
			Result.reserve(Value.size() + 2);
			Result.push_back('"');
			for (const auto Ch : Value)
			{
				if (Ch == '"')
					Result += "\"\"";
				else
					Result.push_back(Ch);
			}
			Result.push_back('"');

			return Result;
		}

		static const char* GetNonEmptyToken(const char* Value, const char* Fallback)
		{
			if (Value && Value[0])
				return Value;

			return Fallback;
		}

		static bool WriteRevoiceCSVFile(const std::string& FilePath,
			const std::vector<std::string>& Rows,
			size_t StartIndex,
			size_t EndIndexExclusive)
		{
			std::ofstream Output(FilePath, std::ios::trunc);
			if (Output.good() == false)
				return false;

			Output << "FormID,VoiceID,SpeakerInfo,OutputPath,Dialogue\n";
			for (size_t i = StartIndex; i < EndIndexExclusive; i++)
				Output << Rows[i] << "\n";

			Output.flush();
			return Output.good();
		}

		static std::string BuildRevoicePartFilePath(const std::string& FilePath, UInt32 PartIndex)
		{
			std::string Base = FilePath;
			size_t LastDot = Base.find_last_of('.');
			if (LastDot != std::string::npos)
				Base.erase(LastDot);

			char Suffix[32] = { 0 };
			FORMAT_STR(Suffix, "_part%03u.csv", PartIndex);
			return Base + Suffix;
		}

		enum class RevoiceParentMasterExportMode
		{
			IncludeAll,
			ActivePluginOnly,
			IgnoreOblivion
		};

		static bool ShouldExportDialogueFromFile(TESFile* SourceFile,
			RevoiceParentMasterExportMode ExportMode,
			const std::vector<TESFile*>& AllowedParentMasters)
		{
			if (SourceFile == nullptr)
				return false;

			if (SourceFile == _DATAHANDLER->activeFile)
				return true;

			if (ExportMode == RevoiceParentMasterExportMode::ActivePluginOnly)
				return false;

			for (auto* MasterFile : AllowedParentMasters)
			{
				if (SourceFile == MasterFile)
					return true;
			}

			return false;
		}

		static std::string GetBasePluginName(const char* FileName)
		{
			std::string Name = FileName ? FileName : "Plugin";
			if (Name.empty())
				Name = "Plugin";

			size_t LastSlash = Name.find_last_of("\\/");
			if (LastSlash != std::string::npos)
				Name.erase(0, LastSlash + 1);

			size_t LastDot = Name.find_last_of('.');
			if (LastDot != std::string::npos)
				Name.erase(LastDot);
			return Name;
		}

		static std::string EscapeJSONString(const std::string& In)
		{
			std::string Out;
			Out.reserve(In.size() + 8);
			for (char Ch : In)
			{
				switch (Ch)
				{
				case '\\': Out += "\\\\"; break;
				case '"': Out += "\\\""; break;
				case '\n': Out += "\\n"; break;
				case '\r': Out += "\\r"; break;
				case '\t': Out += "\\t"; break;
				default: Out.push_back(Ch); break;
				}
			}
			return Out;
		}

		static std::string UnescapeJSONString(const std::string& In)
		{
			std::string Out;
			Out.reserve(In.size());
			for (size_t i = 0; i < In.size(); i++)
			{
				char Ch = In[i];
				if (Ch == '\\' && i + 1 < In.size())
				{
					char Next = In[++i];
					switch (Next)
					{
					case 'n': Out.push_back('\n'); break;
					case 'r': Out.push_back('\r'); break;
					case 't': Out.push_back('\t'); break;
					case '\\': Out.push_back('\\'); break;
					case '"': Out.push_back('"'); break;
					default: Out.push_back(Next); break;
					}
				}
				else
					Out.push_back(Ch);
			}
			return Out;
		}

		static bool ExtractJSONStringField(const std::string& Json, const char* Key, std::string& OutValue)
		{
			std::string Prefix = std::string("\"") + Key + "\":\"";
			size_t Start = Json.find(Prefix);
			if (Start == std::string::npos)
				return false;
			Start += Prefix.size();
			std::string Raw;
			for (size_t i = Start; i < Json.size(); i++)
			{
				char Ch = Json[i];
				if (Ch == '"' && (i == Start || Json[i - 1] != '\\'))
				{
					OutValue = UnescapeJSONString(Raw);
					return true;
				}
				Raw.push_back(Ch);
			}
			return false;
		}

		static UInt32 ComputeFNV1a(const std::string& Value)
		{
			UInt32 Hash = 2166136261u;
			for (unsigned char Ch : Value)
			{
				Hash ^= Ch;
				Hash *= 16777619u;
			}

			return Hash;
		}

		static std::string FormatHex32(UInt32 Value)
		{
			char Buffer[16] = { 0 };
			FORMAT_STR(Buffer, "%08X", Value);
			return Buffer;
		}

		template <typename tData>
		static void AddLinkedListFormsForPlugin(tList<tData>* List, TESFile* Plugin, std::set<UInt32>& SeenFormIDs, std::vector<TESForm*>& Out)
		{
			for (typename tList<tData>::Iterator Itr = List->Begin(); !Itr.End() && Itr.Get(); ++Itr)
			{
				TESForm* Form = Itr.Get();
				if (Form == nullptr)
					continue;

				TESFile* Parent = Form->GetOverrideFile(-1);
				if (Parent != Plugin)
					continue;

				if (SeenFormIDs.insert(Form->formID).second)
					Out.push_back(Form);
			}
		}

		static void AddObjectFormsForPlugin(TESFile* Plugin, std::set<UInt32>& SeenFormIDs, std::vector<TESForm*>& Out)
		{
			for (TESObject* Itr = _DATAHANDLER->objects->first; Itr; Itr = Itr->next)
			{
				TESForm* Form = Itr;
				if (Form == nullptr)
					continue;

				TESFile* Parent = Form->GetOverrideFile(-1);
				if (Parent != Plugin)
					continue;

				if (SeenFormIDs.insert(Form->formID).second)
					Out.push_back(Form);
			}
		}

		static void CollectFormsOwnedByPlugin(TESFile* Plugin, std::vector<TESForm*>& Out)
		{
			SME_ASSERT(Plugin);
			std::set<UInt32> SeenFormIDs;
			Out.clear();

			AddObjectFormsForPlugin(Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->packages, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->worldSpaces, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->climates, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->weathers, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->enchantmentItems, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->spellItems, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->hairs, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->eyes, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->races, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->landTextures, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->classes, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->factions, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->scripts, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->sounds, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->globals, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->topics, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->quests, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->birthsigns, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->combatStyles, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->loadScreens, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->waterForms, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->effectShaders, Plugin, SeenFormIDs, Out);
			AddLinkedListFormsForPlugin(&_DATAHANDLER->objectAnios, Plugin, SeenFormIDs, Out);
		}

		static bool ChooseLoadedPluginFromDialog(HWND hWnd, TESFile** OutPlugin)
		{
			SME_ASSERT(OutPlugin);
			*OutPlugin = nullptr;

			struct LoadedPluginSelectionContext
			{
				std::vector<TESFile*>	LoadedPlugins;
				TESFile*				SelectedPlugin = nullptr;
			};

			auto DialogProc = [](HWND Dialog, UINT Message, WPARAM wParam, LPARAM lParam) -> INT_PTR
			{
				enum
				{
					kCtrlId_Label = 1001,
					kCtrlId_Combo = 1002,
				};

				auto* Context = reinterpret_cast<LoadedPluginSelectionContext*>(GetWindowLongPtr(Dialog, DWLP_USER));
				switch (Message)
				{
				case WM_INITDIALOG:
					{
						Context = reinterpret_cast<LoadedPluginSelectionContext*>(lParam);
						SetWindowLongPtr(Dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(Context));

						CreateWindowExA(0, "STATIC", "List of loaded plugins",
							WS_CHILD | WS_VISIBLE,
							12, 12, 220, 16,
							Dialog, reinterpret_cast<HMENU>(kCtrlId_Label), *TESCSMain::Instance, nullptr);

						HWND Combo = CreateWindowExA(WS_EX_CLIENTEDGE,
							"COMBOBOX",
							nullptr,
							WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
							12, 32, 316, 160,
							Dialog,
							reinterpret_cast<HMENU>(kCtrlId_Combo),
							*TESCSMain::Instance,
							nullptr);

						for (auto* Plugin : Context->LoadedPlugins)
						{
							if (Plugin == nullptr)
								continue;

							LRESULT ItemIndex = SendMessageA(Combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Plugin->fileName));
							SendMessageA(Combo, CB_SETITEMDATA, ItemIndex, reinterpret_cast<LPARAM>(Plugin));
						}

						if (SendMessageA(Combo, CB_GETCOUNT, 0, 0) > 0)
							SendMessageA(Combo, CB_SETCURSEL, 0, 0);

						CreateWindowExA(0, "BUTTON", "OK",
							WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
							172, 72, 75, 24,
							Dialog, reinterpret_cast<HMENU>(IDOK), *TESCSMain::Instance, nullptr);

						CreateWindowExA(0, "BUTTON", "Cancel",
							WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
							253, 72, 75, 24,
							Dialog, reinterpret_cast<HMENU>(IDCANCEL), *TESCSMain::Instance, nullptr);

						SetFocus(Combo);
						return FALSE;
					}
				case WM_COMMAND:
					if (LOWORD(wParam) == IDOK)
					{
						HWND Combo = GetDlgItem(Dialog, kCtrlId_Combo);
						LRESULT Selection = SendMessageA(Combo, CB_GETCURSEL, 0, 0);
						if (Selection == CB_ERR)
							return TRUE;

						Context->SelectedPlugin = reinterpret_cast<TESFile*>(SendMessageA(Combo, CB_GETITEMDATA, Selection, 0));
						EndDialog(Dialog, IDOK);
						return TRUE;
					}
					else if (LOWORD(wParam) == IDCANCEL)
					{
						EndDialog(Dialog, IDCANCEL);
						return TRUE;
					}
					break;
				}

				return FALSE;
			};

			LoadedPluginSelectionContext Context;
			for (tList<TESFile>::Iterator Itr = _DATAHANDLER->fileList.Begin(); !Itr.End(); ++Itr)
			{
				TESFile* CurrentPlugin = Itr.Get();
				if (CurrentPlugin == nullptr)
					continue;

				if (_DATAHANDLER->IsPluginLoaded(CurrentPlugin))
					Context.LoadedPlugins.push_back(CurrentPlugin);
			}

			if (Context.LoadedPlugins.empty())
			{
				BGSEEUI->MsgBoxE("No loaded plugins are currently available.");
				return false;
			}

			DLGTEMPLATE Template = {};
			Template.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT;
			Template.dwExtendedStyle = 0;
			Template.cdit = 0;
			Template.x = 0;
			Template.y = 0;
			Template.cx = 340;
			Template.cy = 108;

			const INT_PTR Result = DialogBoxIndirectParamA(*TESCSMain::Instance, &Template, hWnd, DialogProc, reinterpret_cast<LPARAM>(&Context));
			if (Result != IDOK || Context.SelectedPlugin == nullptr)
				return false;

			*OutPlugin = Context.SelectedPlugin;
			return true;
		}

		static void ExportPluginToJSON(HWND hWnd)
		{
			TESFile* Plugin = nullptr;
			if (ChooseLoadedPluginFromDialog(hWnd, &Plugin) == false)
				return;

			std::string BaseName = GetBasePluginName(Plugin->fileName);
			std::string OutDir = "Data";

			std::vector<TESForm*> Forms;
			CollectFormsOwnedByPlugin(Plugin, Forms);
			TESCSMain::WriteToStatusBar(0, "JSON export in progress...");

			std::string ManifestPath = OutDir + "\\" + BaseName + "_manifest.json";
			std::ofstream ManifestOut(ManifestPath, std::ios::trunc);
			if (ManifestOut.good() == false)
			{
				BGSEEUI->MsgBoxE("Couldn't open output file for writing:\n%s", ManifestPath.c_str());
				return;
			}

			std::string FormsFileName = BaseName + "_forms.jsonl";
			std::string FormsPath = OutDir + "\\" + FormsFileName;
			std::ofstream Out(FormsPath, std::ios::trunc);
			if (Out.good() == false)
			{
				BGSEEUI->MsgBoxE("Couldn't open output file for writing:\n%s", FormsPath.c_str());
				return;
			}

			UInt32 Exported = 0;
			serialization::TESForm2Text Serializer;
			for (auto* Form : Forms)
			{
				if (Form == nullptr)
					continue;

				std::string FormToken;
				if (Serializer.Serialize(Form, FormToken) == false)
					continue;

				componentDLLInterface::FormData Data(Form);
				char FormIDHex[16] = { 0 };
				FORMAT_STR(FormIDHex, "%08X", Form->formID);

				const UInt32 TokenHash = ComputeFNV1a(FormToken);

				Out << "{\"schema\":\"cse-plugin-json-row-v2\","
					<< "\"plugin\":\"" << EscapeJSONString(Plugin->fileName) << "\","
					<< "\"formToken\":\"" << EscapeJSONString(FormToken) << "\","
					<< "\"formTokenHash\":\"" << FormatHex32(TokenHash) << "\","
					<< "\"editorID\":\"" << EscapeJSONString(Data.EditorID ? Data.EditorID : "") << "\","
					<< "\"parentPlugin\":\"" << EscapeJSONString(Data.ParentPluginName ? Data.ParentPluginName : "") << "\","
					<< "\"formType\":\"" << EscapeJSONString(Form->GetFormTypeIDLongName(Form->formType)) << "\","
					<< "\"formID\":\"" << FormIDHex << "\"}\n";
				Exported++;
				if ((Exported % 50) == 0)
				{
					char ProgressMessage[256] = { 0 };
					FORMAT_STR(ProgressMessage, "JSON export: %u / %u forms", Exported, static_cast<UInt32>(Forms.size()));
					TESCSMain::WriteToStatusBar(0, ProgressMessage);
				}
			}

			ManifestOut << "{\n"
				<< "  \"schema\": \"cse-plugin-json-v2\",\n"
				<< "  \"plugin\": \"" << EscapeJSONString(Plugin->fileName) << "\",\n"
				<< "  \"formCount\": " << Exported << ",\n"
				<< "  \"formsFile\": \"" << EscapeJSONString(FormsFileName) << "\"\n"
				<< "}\n";

			Out.flush();
			ManifestOut.flush();
			TESCSMain::WriteToStatusBar(0, "JSON export complete.");
			BGSEEUI->MsgBoxI("Plugin export complete.\nPlugin: %s\nExported forms: %u\nFolder: %s", Plugin->fileName, Exported, OutDir.c_str());
		}

		static void ImportJSONToPlugin(HWND hWnd)
		{
			TESFile* Plugin = nullptr;
			if (ChooseLoadedPluginFromDialog(hWnd, &Plugin) == false)
				return;

			std::string BaseName = GetBasePluginName(Plugin->fileName);
			std::string OutDir = "Data";
			std::string ManifestPath = OutDir + "\\" + BaseName + "_manifest.json";
			std::string FormsPath = OutDir + "\\" + BaseName + "_forms.jsonl";

			std::ifstream ManifestIn(ManifestPath);
			if (ManifestIn.good())
			{
				std::string Manifest((std::istreambuf_iterator<char>(ManifestIn)), std::istreambuf_iterator<char>());
				std::string ManifestPlugin;
				std::string ManifestFormsFile;
				if (ExtractJSONStringField(Manifest, "plugin", ManifestPlugin) && _stricmp(ManifestPlugin.c_str(), Plugin->fileName) != 0)
				{
					BGSEEUI->MsgBoxE("Manifest plugin mismatch.\nSelected: %s\nManifest: %s", Plugin->fileName, ManifestPlugin.c_str());
					return;
				}

				if (ExtractJSONStringField(Manifest, "formsFile", ManifestFormsFile) && ManifestFormsFile.empty() == false)
					FormsPath = OutDir + "\\" + ManifestFormsFile;
			}

			std::ifstream In(FormsPath);
			if (In.good() == false)
			{
				BGSEEUI->MsgBoxE("Couldn't open import file:\n%s", FormsPath.c_str());
				return;
			}

			TESFile* OldActive = _DATAHANDLER->activeFile;
			if (OldActive)
				OldActive->SetActive(false);
			_DATAHANDLER->activeFile = Plugin;
			Plugin->SetActive(true);

			UInt32 Imported = 0, Failed = 0, SkippedPluginMismatch = 0, SkippedMalformed = 0, SkippedHashMismatch = 0;
			std::string Line;
			serialization::TESForm2Text Serializer;
			TESCSMain::WriteToStatusBar(0, "JSON import in progress...");
			while (std::getline(In, Line))
			{
				if (Line.empty())
					continue;

				std::string PluginName;
				if (ExtractJSONStringField(Line, "plugin", PluginName) == false)
				{
					SkippedMalformed++;
					continue;
				}
				if (_stricmp(PluginName.c_str(), Plugin->fileName) != 0)
				{
					SkippedPluginMismatch++;
					continue;
				}

				std::string FormToken;
				if (ExtractJSONStringField(Line, "formToken", FormToken) == false)
				{
					SkippedMalformed++;
					continue;
				}

				std::string TokenHash;
				if (ExtractJSONStringField(Line, "formTokenHash", TokenHash) && TokenHash.empty() == false)
				{
					if (_stricmp(TokenHash.c_str(), FormatHex32(ComputeFNV1a(FormToken)).c_str()) != 0)
					{
						SkippedHashMismatch++;
						continue;
					}
				}

				TESForm* Form = nullptr;
				if (Serializer.Deserialize(FormToken, &Form) == false || Form == nullptr)
				{
					Failed++;
					continue;
				}

				TESFile* Parent = Form->GetOverrideFile(-1);
				if (Parent != Plugin)
				{
					SkippedPluginMismatch++;
					continue;
				}

				Form->SetFromActiveFile(true);
				Imported++;
				if ((Imported % 50) == 0)
				{
					char ProgressMessage[256] = { 0 };
					FORMAT_STR(ProgressMessage, "JSON import: %u forms applied", Imported);
					TESCSMain::WriteToStatusBar(0, ProgressMessage);
				}
			}

			Plugin->SetActive(false);
			_DATAHANDLER->activeFile = OldActive;
			if (OldActive)
				OldActive->SetActive(true);
			TESCSMain::WriteToStatusBar(0, "JSON import complete.");

			BGSEEUI->MsgBoxI("JSON import complete.\nPlugin: %s\nImported forms: %u\nResolve failures: %u\nSkipped malformed rows: %u\nSkipped plugin mismatches: %u\nSkipped hash mismatches: %u\nPath: %s",
				Plugin->fileName,
				Imported,
				Failed,
				SkippedMalformed,
				SkippedPluginMismatch,
				SkippedHashMismatch,
				FormsPath.c_str());
		}


		void ExportRevoiceCSVForActivePlugin(HWND hWnd)
		{
			if (_DATAHANDLER->activeFile == nullptr)
			{
				BGSEEUI->MsgBoxE("An active plugin must be set before using this tool.");
				return;
			}

			char SelectPath[MAX_PATH] = { 0 };
			std::string DefaultFileName = GetDefaultRevoiceCSVFileName(_DATAHANDLER->activeFile->fileName);
			strncpy_s(SelectPath, DefaultFileName.c_str(), _TRUNCATE);
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

			if (IsAbsoluteRevoicePath(FilePath) == false && _strnicmp(FilePath.c_str(), "Data\\", 5) != 0)
				FilePath = std::string("Data\\") + FilePath;

			if (FilePath.size() < 4 || _stricmp(FilePath.c_str() + FilePath.size() - 4, ".csv"))
				FilePath += ".csv";

			const bool SplitIntoParts = BGSEEUI->MsgBoxI(hWnd,
				MB_YESNO,
				"Split the export into multiple reVoice CSV files?\n\n(24 dialogue rows per file)") == IDYES;

			const int ParentMasterChoice = BGSEEUI->MsgBoxI(hWnd,
				MB_YESNOCANCEL,
				"Export with Parent Master Dialogue?\n\n"
				"Yes = Export Active Plugin + all Parent Masters (including Oblivion.esm)\n"
				"No = Export Active Plugin only\n"
				"Cancel = Ignore Oblivion.esm but export other Parent Masters");

			RevoiceParentMasterExportMode ExportMode = RevoiceParentMasterExportMode::ActivePluginOnly;
			switch (ParentMasterChoice)
			{
			case IDYES:
				ExportMode = RevoiceParentMasterExportMode::IncludeAll;
				break;
			case IDCANCEL:
				ExportMode = RevoiceParentMasterExportMode::IgnoreOblivion;
				break;
			case IDNO:
			default:
				ExportMode = RevoiceParentMasterExportMode::ActivePluginOnly;
				break;
			}

			std::vector<TESFile*> AllowedParentMasters;
			for (UInt32 i = 0; i < _DATAHANDLER->activeFile->masterCount; i++)
			{
				TESFile* MasterFile = _DATAHANDLER->activeFile->masterFiles[i];
				if (MasterFile == nullptr)
					continue;

				if (ExportMode == RevoiceParentMasterExportMode::IgnoreOblivion &&
					_stricmp(MasterFile->fileName, "Oblivion.esm") == 0)
				{
					continue;
				}

				AllowedParentMasters.push_back(MasterFile);
			}

			UInt32 Rows = 0;
			std::vector<std::string> CSVRows;
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

						TESFile* SourceFile = Info->GetOverrideFile(-1);
						if (ShouldExportDialogueFromFile(SourceFile, ExportMode, AllowedParentMasters) == false)
							continue;

						TESNPC* Speaker = GetSpeakerFromTopicInfo(Info);
						if (Speaker == nullptr || Speaker->race == nullptr)
							continue;

						const bool IsFemale = (Speaker->actorFlags & TESActorBaseData::kNPCFlag_Female) != 0;
						const char* SexToken = IsFemale ? "F" : "M";
						TESRace* SpeakerRace = Speaker->race;
						TESRace* VoiceRace = IsFemale ? SpeakerRace->femaleVoiceRace : SpeakerRace->maleVoiceRace;
						if (VoiceRace == nullptr)
							VoiceRace = SpeakerRace;

						const char* VoiceID = VoiceRace->GetEditorID() ? VoiceRace->GetEditorID() : "";
						if (VoiceID == nullptr || strlen(VoiceID) == 0)
						{
							VoiceID = SpeakerRace->GetEditorID() ? SpeakerRace->GetEditorID() : "";
						}
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
							const char* VoiceFolder = VoiceID;
							if (VoiceFolder == nullptr || strlen(VoiceFolder) == 0)
								VoiceFolder = RaceName;

							const char* QuestToken = GetNonEmptyToken(Quest->editorID.c_str(), "Quest");
							const char* TopicToken = GetNonEmptyToken(Topic->editorID.c_str(), "Topic");

							FORMAT_STR(OutPath, "Sound\\Voice\\%s\\%s\\%s\\%s_%s_%08X_%u.mp3",
								SourceFile->fileName,
								VoiceFolder,
								SexToken,
								QuestToken,
								TopicToken,
								Info->formID,
								Response->responseNumber);

							char FormID[9] = { 0 };
							FORMAT_STR(FormID, "%08X", Info->formID);

							std::string Row =
								EscapeCSVField(FormID)
								+ "," + EscapeCSVField(VoiceID)
								+ "," + EscapeCSVField(SpeakerInfo.c_str())
								+ "," + EscapeCSVField(OutPath)
								+ "," + EscapeCSVField(ResponseText);
							CSVRows.push_back(Row);
							Rows++;
						}
					}
				}
			}

			if (SplitIntoParts)
			{
				const size_t kPartSize = 24;
				const size_t PartCount = std::max<size_t>(1, (CSVRows.size() + kPartSize - 1) / kPartSize);
				for (size_t Part = 0; Part < PartCount; Part++)
				{
					const size_t StartIndex = Part * kPartSize;
					const size_t EndIndex = std::min(CSVRows.size(), StartIndex + kPartSize);
					std::string PartFilePath = BuildRevoicePartFilePath(FilePath, static_cast<UInt32>(Part + 1));
					if (WriteRevoiceCSVFile(PartFilePath, CSVRows, StartIndex, EndIndex) == false)
					{
						BGSEEUI->MsgBoxE("Couldn't open output file for writing:\n%s", PartFilePath.c_str());
						return;
					}
				}

				BGSEEUI->MsgBoxI("reVoice CSV export complete. Wrote %u dialogue rows across %u files.\nFirst file:\n%s",
					Rows,
					static_cast<UInt32>(PartCount),
					BuildRevoicePartFilePath(FilePath, 1).c_str());
			}
			else
			{
				if (WriteRevoiceCSVFile(FilePath, CSVRows, 0, CSVRows.size()) == false)
				{
					BGSEEUI->MsgBoxE("Couldn't open output file for writing:\n%s", FilePath.c_str());
					return;
				}

				BGSEEUI->MsgBoxI("reVoice CSV export complete. Wrote %u dialogue rows to:\n%s", Rows, FilePath.c_str());
			}
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
				case IDC_MAINMENU_EXPORT_PLUGINTOJSON:
					ExportPluginToJSON(hWnd);

					break;
				case IDC_MAINMENU_IMPORT_JSONTOPLUGIN:
					ImportJSONToPlugin(hWnd);

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
					SubclassParams->Out.MarkMessageAsHandled = true;
				}

				break;
			case WM_DESTROY:
				{
					KillTimer(hWnd, ID_PATHGRIDTOOLBARBUTTION_TIMERID);

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

							HWND TODSlider = GetDlgItem(hWnd, IDC_TOOLBAR_TODSLIDER);
							HWND TODEdit = GetDlgItem(hWnd, IDC_TOOLBAR_TODCURRENT);

							TESDialog::ClampDlgEditField(TODEdit, 0.0, 24.0);

							SendMessage(TODSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 23));
							SendMessage(TODSlider, TBM_SETLINESIZE, NULL, 1);
							SendMessage(TODSlider, TBM_SETPAGESIZE, NULL, 4);

							SendMessage(*TESCSMain::MainToolbarHandle, WM_MAINTOOLBAR_SETTOD, _TES->GetSkyTOD() * 4.0, NULL);
						}
					}
				}

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
					MainWindowToolbarData* xData = BGSEE_GETWINDOWXDATA(MainWindowToolbarData, SubclassParams->In.ExtraData);
					if (xData)
					{
						SubclassParams->In.ExtraData->Remove(MainWindowToolbarData::kTypeID);
						delete xData;
					}
				}

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
