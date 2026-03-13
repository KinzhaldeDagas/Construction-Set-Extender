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

#include <set>
#include <cctype>
#include <unordered_map>

namespace
{
	std::string SanitizePathComponent(const std::string& Input)
	{
		std::string Out = Input;
		for (size_t i = 0; i < Out.size(); i++)
		{
			char& Ch = Out[i];
			if (Ch == '<' || Ch == '>' || Ch == ':' || Ch == '"' || Ch == '/' || Ch == '\\' || Ch == '|' || Ch == '?' || Ch == '*')
				Ch = '_';
		}

		if (Out.empty())
			Out = "Unknown";

		return Out;
	}

	std::string MakeFallbackEditorID(const char* Prefix, UInt32 FormID)
	{
		char Buffer[0x40] = { 0 };
		FORMAT_STR(Buffer, "%s_%08X", Prefix, FormID);
		return Buffer;
	}

	bool HasNonWhitespaceText(const std::string& Input)
	{
		for (size_t i = 0; i < Input.size(); i++)
		{
			if (isspace(static_cast<unsigned char>(Input[i])) == 0)
				return true;
		}
		return false;
	}
}

#include "[BGSEEBase]\ToolBox.h"
#include "[BGSEEBase]\Script\CodaVM.h"

namespace cse
{
	namespace uiManager
	{
		static HWND g_MarkerPlacementDialog = nullptr;

		void NotifyMarkerPlacementDialogDestroyed(HWND Dialog)
		{
			if (Dialog && g_MarkerPlacementDialog == Dialog)
				g_MarkerPlacementDialog = nullptr;
		}

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
			std::unordered_map<std::string, std::string> CandidateInputResponses;

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
						if (OverrideFile == nullptr)
							OverrideFile = Info->GetOverrideFile(0);

						if (OverrideFile)
						{
							if (Info->formFlags & TESForm::kFormFlags_Deleted)
								continue;

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
										std::string RaceID = Race->editorID.c_str();
										if (RaceID.empty())
											RaceID = MakeFallbackEditorID("Race", Race->formID);

										std::string QuestID = Quest->editorID.c_str();
										if (QuestID.empty())
											QuestID = MakeFallbackEditorID("Quest", Quest->formID);

										std::string TopicID = Topic->editorID.c_str();
										if (TopicID.empty())
											TopicID = MakeFallbackEditorID("Topic", Topic->formID);

										RaceID = SanitizePathComponent(RaceID);
										QuestID = SanitizePathComponent(QuestID);
										TopicID = SanitizePathComponent(TopicID);

										std::string ResponseText = Response->responseText.c_str();
										if (HasNonWhitespaceText(ResponseText) == false)
											continue;

										for (int j = 0; j < 2; j++)
										{
											const char* Sex = j ? "F" : "M";

											FORMAT_STR(VoiceFilePath, "Data\\Sound\\Voice\\%s\\%s\\%s\\%s_%s_%08X_%u",
												OverrideFile->fileName,
												RaceID.c_str(),
												Sex,
												QuestID.c_str(),
												TopicID.c_str(),
												(Info->formID & 0xFFFFFF),
												Response->responseNumber);

											auto Existing = CandidateInputResponses.find(VoiceFilePath);
											if (Existing == CandidateInputResponses.end())
											{
												CandidateInputResponses.emplace(VoiceFilePath, ResponseText);
												CandidateInputs.emplace_back(VoiceFilePath, ResponseText.c_str());
											}
											else if (Existing->second != ResponseText)
											{
												BGSEECONSOLE_MESSAGE("Voice path collision for %s; keeping first response text", VoiceFilePath);
											}
										}
									}
								}
							}
						}
						else
							BGSEECONSOLE_MESSAGE("Topic info %08X has no override/source file; skipping", Info->formID);
					}
				}
			}


			char Buffer[0x100];
			int Processed = 0;
			for (const auto& Input : CandidateInputs)
			{
				FORMAT_STR(Buffer, "Please Wait\nProcessing response %d/%d", Processed + 1, CandidateInputs.size());
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

		struct RevoiceSpeakerContext
		{
			TESNPC* Speaker = nullptr;
			TESRace* RaceHint = nullptr;
			bool HasSexHint = false;
			bool IsFemale = false;
		};

		static TESNPC* ResolveSpeakerNPCFromConditionParam(TESForm* CandidateForm)
		{
			if (CandidateForm == nullptr || CandidateForm->formID == 0)
				return nullptr;

			if (TESNPC* NPC = CS_CAST(CandidateForm, TESForm, TESNPC))
				return NPC;

			TESObjectREFR* Ref = CS_CAST(CandidateForm, TESForm, TESObjectREFR);
			if (Ref == nullptr || Ref->baseForm == nullptr || Ref->baseForm->formID == 0)
				return nullptr;

			return CS_CAST(Ref->baseForm, TESForm, TESNPC);
		}

		static RevoiceSpeakerContext
		GetSpeakerContextFromTopicInfo(TESTopicInfo* Info)
		{
			RevoiceSpeakerContext Result;
			if (Info == nullptr)
				return Result;

			constexpr UInt16 kFunction_GetIsRace = 69;
			constexpr UInt16 kFunction_GetIsSex = 70;
			constexpr UInt16 kFunction_GetIsID = 72;
			constexpr UInt16 kFunction_CSEGetIsID = 224;

			for (ConditionListT::Iterator Itr = Info->conditions.Begin(); Itr.End() == false && Itr.Get(); ++Itr)
			{
				TESConditionItem* Condition = Itr.Get();
				SME_ASSERT(Condition);

				const UInt16 FunctionIndex = Condition->functionIndex & 0x0FFF;
				if (FunctionIndex == kFunction_GetIsID || FunctionIndex == kFunction_CSEGetIsID)
				{
					if (Result.Speaker == nullptr)
					{
						Result.Speaker = ResolveSpeakerNPCFromConditionParam(Condition->param1.form);
					}
				}
				else if (FunctionIndex == kFunction_GetIsRace)
				{
					if (Result.RaceHint == nullptr)
					{
						TESRace* Race = CS_CAST(Condition->param1.form, TESForm, TESRace);
						if (Race)
							Result.RaceHint = Race;
					}
				}
				else if (FunctionIndex == kFunction_GetIsSex)
				{
					if (Result.HasSexHint == false)
					{
						int Sex = static_cast<int>(Condition->comparisonValue);
						if (Sex == 0 || Sex == 1)
						{
							Result.HasSexHint = true;
							Result.IsFemale = Sex != 0;
						}
					}
				}
			}

			return Result;
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
			const char* Value = Input ? Input : "";
			size_t Length = strlen(Value);
			size_t QuoteCount = 0;
			for (const char* Cursor = Value; *Cursor; ++Cursor)
			{
				if (*Cursor == '"')
					QuoteCount++;
			}

			std::string Result;
			Result.reserve(Length + QuoteCount + 2);
			Result.push_back('"');
			for (const char* Cursor = Value; *Cursor; ++Cursor)
			{
				char Ch = *Cursor;
				if (Ch == '\r' || Ch == '\n')
					Ch = ' ';

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

		struct RevoiceCSVRowData
		{
			std::string FormID;
			std::string VoiceID;
			std::string Race;
			std::string Gender;
			std::string SpeakerInfo;
			std::string Emotion;
			std::string OutputPath;
			std::string Dialogue;
		};

		static std::string BuildRevoiceCSVRow(const RevoiceCSVRowData& Row)
		{
			std::string EscapedFormID = EscapeCSVField(Row.FormID.c_str());
			std::string EscapedVoiceID = EscapeCSVField(Row.VoiceID.c_str());
			std::string EscapedRace = EscapeCSVField(Row.Race.c_str());
			std::string EscapedGender = EscapeCSVField(Row.Gender.c_str());
			std::string EscapedSpeakerInfo = EscapeCSVField(Row.SpeakerInfo.c_str());
			std::string EscapedEmotion = EscapeCSVField(Row.Emotion.c_str());
			std::string EscapedOutputPath = EscapeCSVField(Row.OutputPath.c_str());
			std::string EscapedDialogue = EscapeCSVField(Row.Dialogue.c_str());

			std::string Result;
			Result.reserve(EscapedFormID.size() + EscapedVoiceID.size() + EscapedRace.size() + EscapedGender.size() + EscapedSpeakerInfo.size() +
				EscapedEmotion.size() + EscapedOutputPath.size() + EscapedDialogue.size() + 7);
			Result += EscapedFormID;
			Result += ",";
			Result += EscapedVoiceID;
			Result += ",";
			Result += EscapedRace;
			Result += ",";
			Result += EscapedGender;
			Result += ",";
			Result += EscapedSpeakerInfo;
			Result += ",";
			Result += EscapedEmotion;
			Result += ",";
			Result += EscapedOutputPath;
			Result += ",";
			Result += EscapedDialogue;
			return Result;
		}

		static bool WriteRevoiceCSVFile(const std::string& FilePath,
			const std::vector<std::string>& Rows,
			size_t StartIndex,
			size_t EndIndexExclusive)
		{
			std::ofstream Output(FilePath, std::ios::trunc);
			if (Output.good() == false)
				return false;

			Output << "FormID,VoiceID,Race,Gender,SpeakerInfo,Emotion,OutputPath,Dialogue\n";
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

		static std::string BuildRevoiceCategoryFilePath(const std::string& FilePath, const char* CategoryFolder)
		{
			if (CategoryFolder == nullptr || CategoryFolder[0] == 0)
				return FilePath;

			size_t LastSlash = FilePath.find_last_of("\\/");
			if (LastSlash == std::string::npos)
				return std::string(CategoryFolder) + "\\" + FilePath;

			std::string Directory = FilePath.substr(0, LastSlash);
			std::string FileName = FilePath.substr(LastSlash + 1);
			if (Directory.empty())
				return std::string(CategoryFolder) + "\\" + FileName;

			return Directory + "\\" + CategoryFolder + "\\" + FileName;
		}

		static bool EnsureRevoiceCategoryDirectory(const std::string& FilePath, const char* CategoryFolder)
		{
			if (CategoryFolder == nullptr || CategoryFolder[0] == 0)
				return true;

			size_t LastSlash = FilePath.find_last_of("\\/");
			std::string Directory = (LastSlash == std::string::npos) ? "" : FilePath.substr(0, LastSlash);
			std::string CategoryPath = Directory.empty() ? std::string(CategoryFolder) : (Directory + "\\" + CategoryFolder);
			if (CreateDirectoryA(CategoryPath.c_str(), nullptr) != 0)
				return true;

			const DWORD LastError = GetLastError();
			return LastError == ERROR_ALREADY_EXISTS;
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

		static void CollectActivePluginAndMasters(std::vector<TESFile*>& OutFiles)
		{
			OutFiles.clear();
			if (_DATAHANDLER == nullptr || _DATAHANDLER->activeFile == nullptr)
				return;

			OutFiles.push_back(_DATAHANDLER->activeFile);
			for (UInt32 i = 0; i < _DATAHANDLER->activeFile->masterCount; i++)
			{
				TESFile* MasterFile = _DATAHANDLER->activeFile->masterFiles[i];
				if (MasterFile == nullptr)
					continue;

				if (std::find(OutFiles.begin(), OutFiles.end(), MasterFile) == OutFiles.end())
					OutFiles.push_back(MasterFile);
			}
		}

		static bool IsFormFromActivePluginOrMaster(TESForm* Form, const std::vector<TESFile*>& AllowedFiles)
		{
			if (Form == nullptr)
				return false;

			TESFile* Source = Form->GetOverrideFile(-1);
			if (Source == nullptr)
				return false;

			for (auto* File : AllowedFiles)
			{
				if (File == Source)
					return true;
			}
			return false;
		}

		static std::string BuildDefaultRegionAssetCSVPath()
		{
			std::string BaseName = _DATAHANDLER && _DATAHANDLER->activeFile ? _DATAHANDLER->activeFile->fileName : "ActivePlugin.esp";
			size_t LastSlash = BaseName.find_last_of("\\/");
			if (LastSlash != std::string::npos)
				BaseName.erase(0, LastSlash + 1);
			size_t LastDot = BaseName.find_last_of('.');
			if (LastDot != std::string::npos)
				BaseName.erase(LastDot);
			if (BaseName.empty())
				BaseName = "ActivePlugin";
			return std::string("Data\\CSVExports\\Regions\\region_assets_") + BaseName + ".csv";
		}

		struct RegionAssetExportRow
		{
			std::string FormType;
			std::string FormIDHex;
			std::string EditorID;
			std::string Plugin;
			UInt32 FormIDValue = 0;
		};

		static bool IsExportableRegionAssetFormType(UInt8 FormType)
		{
			return FormType == TESForm::kFormType_Tree ||
				FormType == TESForm::kFormType_Flora ||
				FormType == TESForm::kFormType_Grass ||
				FormType == TESForm::kFormType_LandTexture ||
				FormType == TESForm::kFormType_Static ||
				FormType == TESForm::kFormType_Sound;
		}

		static bool StringContainsCaseInsensitive(const char* Haystack, const char* Needle)
		{
			if (Haystack == nullptr || Needle == nullptr || Needle[0] == 0)
				return false;

			std::string UpperHaystack(Haystack);
			std::string UpperNeedle(Needle);
			std::transform(UpperHaystack.begin(), UpperHaystack.end(), UpperHaystack.begin(), [](unsigned char Ch) { return static_cast<char>(toupper(Ch)); });
			std::transform(UpperNeedle.begin(), UpperNeedle.end(), UpperNeedle.begin(), [](unsigned char Ch) { return static_cast<char>(toupper(Ch)); });

			return UpperHaystack.find(UpperNeedle) != std::string::npos;
		}

		static bool IsRockStaticForm(TESForm* Form)
		{
			if (Form == nullptr || Form->formType != TESForm::kFormType_Static)
				return false;

			TESObjectSTAT* StaticForm = CS_CAST(Form, TESForm, TESObjectSTAT);
			if (StaticForm == nullptr)
				return false;

			const char* ModelPath = StaticForm->modelPath.c_str();
			const char* EditorID = Form->GetEditorID();
			return StringContainsCaseInsensitive(ModelPath, "\\ROCKS\\") ||
				StringContainsCaseInsensitive(ModelPath, "/ROCKS/") ||
				StringContainsCaseInsensitive(ModelPath, "ROCK") ||
				StringContainsCaseInsensitive(EditorID, "ROCK");
		}

		static bool TryBuildRegionAssetExportRow(TESForm* Form,
			const std::vector<TESFile*>& AllowedFiles,
			std::set<UInt32>& SeenFormIDs,
			RegionAssetExportRow& OutRow,
			UInt32& SkippedDuplicate,
			UInt32& SkippedUnsupported,
			UInt32& SkippedScope,
			UInt32& SkippedNoSource)
		{
			if (Form == nullptr)
				return false;

			if (IsExportableRegionAssetFormType(Form->formType) == false)
			{
				SkippedUnsupported++;
				return false;
			}

			if (Form->formType == TESForm::kFormType_Static && IsRockStaticForm(Form) == false)
			{
				SkippedUnsupported++;
				return false;
			}

			if (IsFormFromActivePluginOrMaster(Form, AllowedFiles) == false)
			{
				SkippedScope++;
				return false;
			}

			TESFile* Source = Form->GetOverrideFile(-1);
			if (Source == nullptr)
			{
				SkippedNoSource++;
				return false;
			}

			if (SeenFormIDs.insert(Form->formID).second == false)
			{
				SkippedDuplicate++;
				return false;
			}

			char FormID[16] = { 0 };
			FORMAT_STR(FormID, "%08X", Form->formID);

			OutRow.FormType = (Form->formType == TESForm::kFormType_Static) ? "Rock" :
				(Form->formType == TESForm::kFormType_Sound ? "MiscSound" : TESForm::GetFormTypeIDLongName(Form->formType));
			OutRow.FormIDHex = FormID;
			OutRow.EditorID = Form->GetEditorID() ? Form->GetEditorID() : "";
			OutRow.Plugin = Source->fileName;
			OutRow.FormIDValue = Form->formID;
			return true;
		}

		static void ExportRegionAssetFormsCSV(HWND hWnd)
		{
			if (_DATAHANDLER == nullptr || _DATAHANDLER->activeFile == nullptr)
			{
				BGSEEUI->MsgBoxE("An active plugin must be set before exporting region asset CSV.");
				return;
			}

			char SelectPath[MAX_PATH] = { 0 };
			std::string DefaultPath = BuildDefaultRegionAssetCSVPath();
			strncpy_s(SelectPath, DefaultPath.c_str(), _TRUNCATE);
			if (TESDialog::ShowFileSelect(hWnd,
				"Data",
				"CSV Files\0*.csv\0\0",
				"Export Region Asset CSV",
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

			std::vector<TESFile*> AllowedFiles;
			CollectActivePluginAndMasters(AllowedFiles);

			const bool IncludeOblivionMaster = BGSEEUI->MsgBoxI(hWnd,
				MB_YESNO,
				"Do you want to include Oblivion.esm?\n\n"
				"Yes = include Oblivion.esm rows\n"
				"No = exclude Oblivion.esm rows") == IDYES;

			if (IncludeOblivionMaster == false)
			{
				AllowedFiles.erase(
					std::remove_if(AllowedFiles.begin(), AllowedFiles.end(), [](TESFile* File)
					{
						return File && _stricmp(File->fileName, "Oblivion.esm") == 0;
					}),
					AllowedFiles.end());
			}

			std::vector<RegionAssetExportRow> Rows;
			Rows.reserve(1024);
			std::set<UInt32> SeenFormIDs;

			UInt32 SkippedDuplicate = 0;
			UInt32 SkippedUnsupported = 0;
			UInt32 SkippedScope = 0;
			UInt32 SkippedNoSource = 0;

			for (TESObject* Itr = _DATAHANDLER->objects->first; Itr; Itr = Itr->next)
			{
				RegionAssetExportRow Row;
				if (TryBuildRegionAssetExportRow(Itr,
					AllowedFiles,
					SeenFormIDs,
					Row,
					SkippedDuplicate,
					SkippedUnsupported,
					SkippedScope,
					SkippedNoSource))
				{
					Rows.push_back(Row);
				}
			}

			for (tList<TESLandTexture>::Iterator Itr = _DATAHANDLER->landTextures.Begin(); !Itr.End() && Itr.Get(); ++Itr)
			{
				RegionAssetExportRow Row;
				if (TryBuildRegionAssetExportRow(Itr.Get(),
					AllowedFiles,
					SeenFormIDs,
					Row,
					SkippedDuplicate,
					SkippedUnsupported,
					SkippedScope,
					SkippedNoSource))
				{
					Rows.push_back(Row);
				}
			}

			for (tList<TESSound>::Iterator Itr = _DATAHANDLER->sounds.Begin(); !Itr.End() && Itr.Get(); ++Itr)
			{
				RegionAssetExportRow Row;
				if (TryBuildRegionAssetExportRow(Itr.Get(),
					AllowedFiles,
					SeenFormIDs,
					Row,
					SkippedDuplicate,
					SkippedUnsupported,
					SkippedScope,
					SkippedNoSource))
				{
					Rows.push_back(Row);
				}
			}

			std::sort(Rows.begin(), Rows.end(), [](const RegionAssetExportRow& Left, const RegionAssetExportRow& Right)
			{
				if (_stricmp(Left.FormType.c_str(), Right.FormType.c_str()) != 0)
					return _stricmp(Left.FormType.c_str(), Right.FormType.c_str()) < 0;

				if (_stricmp(Left.EditorID.c_str(), Right.EditorID.c_str()) != 0)
					return _stricmp(Left.EditorID.c_str(), Right.EditorID.c_str()) < 0;

				return Left.FormIDValue < Right.FormIDValue;
			});

			CreateDirectoryA("Data", nullptr);
			CreateDirectoryA("Data\\CSVExports", nullptr);
			CreateDirectoryA("Data\\CSVExports\\Regions", nullptr);

			const bool SplitByType = BGSEEUI->MsgBoxI(hWnd,
				MB_YESNO,
				"Do you want to split CSVs?\n\n"
				"Yes = create FILENAME_FLORA.csv, FILENAME_TREE.csv etc\n"
				"No = create one combined CSV") == IDYES;

			size_t LastSlash = FilePath.find_last_of("\\/");
			std::string FileNameOnly = LastSlash == std::string::npos ? FilePath : FilePath.substr(LastSlash + 1);
			size_t LastDot = FileNameOnly.find_last_of('.');
			std::string BaseName = LastDot == std::string::npos ? FileNameOnly : FileNameOnly.substr(0, LastDot);
			if (BaseName.empty())
				BaseName = "region_assets";

			auto WriteRows = [](const std::string& Path, const std::vector<RegionAssetExportRow>& SourceRows) -> bool
			{
				std::ofstream Out(Path, std::ios::trunc);
				if (!Out.good())
					return false;

				Out << "FormType,FormID,EditorID,Plugin\n";
				for (const auto& Row : SourceRows)
				{
					Out << EscapeCSVField(Row.FormType.c_str()) << ","
						<< EscapeCSVField(Row.FormIDHex.c_str()) << ","
						<< EscapeCSVField(Row.EditorID.c_str()) << ","
						<< EscapeCSVField(Row.Plugin.c_str()) << "\n";
				}

				Out.flush();
				return Out.good();
			};

			const std::string OutputDir = "Data\\CSVExports\\Regions";
			if (SplitByType)
			{
				std::map<std::string, std::vector<RegionAssetExportRow>> Buckets;
				for (const auto& Row : Rows)
				{
					std::string TypeToken = "OTHER";
					if (_stricmp(Row.FormType.c_str(), "Flora") == 0) TypeToken = "FLORA";
					else if (_stricmp(Row.FormType.c_str(), "Tree") == 0) TypeToken = "TREE";
					else if (_stricmp(Row.FormType.c_str(), "Grass") == 0) TypeToken = "GRASS";
					else if (_stricmp(Row.FormType.c_str(), "LandTexture") == 0) TypeToken = "LANDTEXTURE";
					else if (_stricmp(Row.FormType.c_str(), "Rock") == 0) TypeToken = "ROCK";
					else if (_stricmp(Row.FormType.c_str(), "MiscSound") == 0) TypeToken = "MISCSOUND";
					Buckets[TypeToken].push_back(Row);
				}

				UInt32 WrittenFiles = 0;
				for (auto& Bucket : Buckets)
				{
					if (Bucket.second.empty())
						continue;
					std::string SplitPath = OutputDir + "\\" + BaseName + "_" + Bucket.first + ".csv";
					if (WriteRows(SplitPath, Bucket.second) == false)
					{
						BGSEEUI->MsgBoxE("Couldn't write split CSV:\n%s", SplitPath.c_str());
						return;
					}
					WrittenFiles++;
				}

				BGSEEUI->MsgBoxI("Region asset export complete (split mode).\nRows exported: %u\nFiles written: %u\nFolder: %s",
					static_cast<UInt32>(Rows.size()),
					WrittenFiles,
					OutputDir.c_str());
			}
			else
			{
				std::string CombinedPath = OutputDir + "\\" + BaseName + ".csv";
				if (WriteRows(CombinedPath, Rows) == false)
				{
					BGSEEUI->MsgBoxE("Couldn't open output CSV:\n%s", CombinedPath.c_str());
					return;
				}

				BGSEEUI->MsgBoxI("Region asset export complete.\nRows exported: %u\nPath: %s",
					static_cast<UInt32>(Rows.size()),
					CombinedPath.c_str());
			}
		}


		static void TrimCSVField(std::string& Value)
		{
			size_t Start = 0;
			while (Start < Value.size() && isspace(static_cast<unsigned char>(Value[Start])))
				Start++;

			size_t End = Value.size();
			while (End > Start && isspace(static_cast<unsigned char>(Value[End - 1])))
				End--;

			if (Start == 0 && End == Value.size())
				return;
			Value = Value.substr(Start, End - Start);
		}

		static bool ParseCSVLine(const std::string& Line, std::vector<std::string>& OutFields)
		{
			OutFields.clear();
			std::string Current;
			bool InQuotes = false;

			for (size_t i = 0; i < Line.size(); i++)
			{
				char Ch = Line[i];
				if (InQuotes)
				{
					if (Ch == '"')
					{
						if (i + 1 < Line.size() && Line[i + 1] == '"')
						{
							Current.push_back('"');
							i++;
						}
						else
						{
							InQuotes = false;
						}
					}
					else
					{
						Current.push_back(Ch);
					}
				}
				else
				{
					if (Ch == ',')
					{
						TrimCSVField(Current);
						OutFields.push_back(Current);
						Current.clear();
					}
					else if (Ch == '"')
					{
						InQuotes = true;
					}
					else
					{
						Current.push_back(Ch);
					}
				}
			}

			if (InQuotes)
				return false;

			TrimCSVField(Current);
			OutFields.push_back(Current);
			return true;
		}

		static bool EqualsCI(const char* Left, const char* Right)
		{
			if (Left == nullptr || Right == nullptr)
				return false;
			return _stricmp(Left, Right) == 0;
		}

		static bool IsSupportedRegionAssetTypeToken(const std::string& FormType)
		{
			return _stricmp(FormType.c_str(), "Tree") == 0 ||
				_stricmp(FormType.c_str(), "Flora") == 0 ||
				_stricmp(FormType.c_str(), "Grass") == 0 ||
				_stricmp(FormType.c_str(), "LandTexture") == 0 ||
				_stricmp(FormType.c_str(), "Static") == 0 ||
				_stricmp(FormType.c_str(), "Rock") == 0 ||
				_stricmp(FormType.c_str(), "MiscSound") == 0 ||
				_stricmp(FormType.c_str(), "Sound") == 0;
		}

		static bool IsSupportedRegionAssetFormType(UInt8 FormType)
		{
			return FormType == TESForm::kFormType_Tree ||
				FormType == TESForm::kFormType_Flora ||
				FormType == TESForm::kFormType_Grass ||
				FormType == TESForm::kFormType_LandTexture ||
				FormType == TESForm::kFormType_Static ||
				FormType == TESForm::kFormType_Sound;
		}

		static void ImportRegionAssetFormsCSV(HWND hWnd)
		{
			if (_DATAHANDLER == nullptr || _DATAHANDLER->activeFile == nullptr)
			{
				BGSEEUI->MsgBoxE("An active plugin must be set before importing region asset CSV.");
				return;
			}

			char SelectPath[MAX_PATH] = { 0 };
			strncpy_s(SelectPath, BuildDefaultRegionAssetCSVPath().c_str(), _TRUNCATE);
			if (TESDialog::ShowFileSelect(hWnd,
				"Data",
				"CSV Files\0*.csv\0\0",
				"Import Region Asset CSV",
				"csv",
				nullptr,
				true,
				false,
				SelectPath,
				MAX_PATH) == false)
				return;

			std::string ImportPath(SelectPath);
			for (auto& Ch : ImportPath)
			{
				if (Ch == '/')
					Ch = '\\';
			}
			if (IsAbsoluteRevoicePath(ImportPath) == false && _strnicmp(ImportPath.c_str(), "Data\\", 5) != 0)
				ImportPath = std::string("Data\\") + ImportPath;

			std::ifstream In(ImportPath);
			if (!In.good())
			{
				BGSEEUI->MsgBoxE("Couldn't open CSV for import:\n%s", ImportPath.c_str());
				return;
			}

			std::vector<TESFile*> AllowedFiles;
			CollectActivePluginAndMasters(AllowedFiles);

			UInt32 TotalRows = 0;
			UInt32 AcceptedRows = 0;
			UInt32 RejectedMalformed = 0;
			UInt32 RejectedHeader = 0;
			UInt32 RejectedSchema = 0;
			UInt32 RejectedTypeToken = 0;
			UInt32 RejectedResolve = 0;
			UInt32 RejectedScope = 0;
			UInt32 RejectedTypeMismatch = 0;
			UInt32 DuplicateRows = 0;

			std::set<UInt32> SeenFormIDs;
			std::vector<std::string> Fields;
			std::string HeaderLine;
			if (!std::getline(In, HeaderLine))
			{
				BGSEEUI->MsgBoxE("CSV is empty:\n%s", SelectPath);
				return;
			}

			if (ParseCSVLine(HeaderLine, Fields) == false)
			{
				BGSEEUI->MsgBoxE("CSV header is malformed:\n%s", SelectPath);
				return;
			}

			SInt32 SchemaIdx = -1, TypeIdx = -1, FormIDIdx = -1, EditorIDIdx = -1;
			for (UInt32 i = 0; i < Fields.size(); i++)
			{
				if (_stricmp(Fields[i].c_str(), "Schema") == 0)
					SchemaIdx = i;
				else if (_stricmp(Fields[i].c_str(), "FormType") == 0)
					TypeIdx = i;
				else if (_stricmp(Fields[i].c_str(), "FormID") == 0)
					FormIDIdx = i;
				else if (_stricmp(Fields[i].c_str(), "EditorID") == 0)
					EditorIDIdx = i;
			}

			if (TypeIdx < 0 || FormIDIdx < 0)
			{
				BGSEEUI->MsgBoxE("CSV header missing required columns (FormType, FormID):\n%s", SelectPath);
				return;
			}

			std::string Line;
			while (std::getline(In, Line))
			{
				if (Line.empty())
					continue;
				TotalRows++;

				if (ParseCSVLine(Line, Fields) == false)
				{
					RejectedMalformed++;
					continue;
				}

				const SInt32 RequiredMaxIdx = std::max(TypeIdx, FormIDIdx);
				const SInt32 OptionalMaxIdx = std::max(SchemaIdx, EditorIDIdx);
				const SInt32 MaxFieldIdx = std::max(RequiredMaxIdx, OptionalMaxIdx);
				if (Fields.size() <= static_cast<UInt32>(MaxFieldIdx))
				{
					RejectedHeader++;
					continue;
				}

				if (SchemaIdx >= 0)
				{
					const std::string& Schema = Fields[SchemaIdx];
					if (Schema.empty() == false && _stricmp(Schema.c_str(), "region-assets-v1") != 0)
					{
						RejectedSchema++;
						continue;
					}
				}

				const std::string& TypeToken = Fields[TypeIdx];
				if (IsSupportedRegionAssetTypeToken(TypeToken) == false)
				{
					RejectedTypeToken++;
					continue;
				}

				const std::string& FormIDToken = Fields[FormIDIdx];
				char* ParseEnd = nullptr;
				UInt32 FormID = strtoul(FormIDToken.c_str(), &ParseEnd, 16);
				if (ParseEnd == FormIDToken.c_str() || (ParseEnd && *ParseEnd != '\0'))
				{
					RejectedMalformed++;
					continue;
				}

				TESForm* Form = TESForm::LookupByFormID(FormID);
				if (Form == nullptr && EditorIDIdx >= 0)
				{
					const std::string& EditorID = Fields[EditorIDIdx];
					if (EditorID.empty() == false)
						Form = TESForm::LookupByEditorID(EditorID.c_str());
				}

				if (Form == nullptr)
				{
					RejectedResolve++;
					continue;
				}

				if (IsFormFromActivePluginOrMaster(Form, AllowedFiles) == false)
				{
					RejectedScope++;
					continue;
				}

				if (IsSupportedRegionAssetFormType(Form->formType) == false)
				{
					RejectedTypeMismatch++;
					continue;
				}

				const bool TokenIsRock = _stricmp(TypeToken.c_str(), "Rock") == 0;
				const bool TokenIsMiscSound = _stricmp(TypeToken.c_str(), "MiscSound") == 0 || _stricmp(TypeToken.c_str(), "Sound") == 0;
				if (TokenIsRock)
				{
					if (Form->formType != TESForm::kFormType_Static || IsRockStaticForm(Form) == false)
					{
						RejectedTypeMismatch++;
						continue;
					}
				}
				else if (TokenIsMiscSound)
				{
					if (Form->formType != TESForm::kFormType_Sound)
					{
						RejectedTypeMismatch++;
						continue;
					}
				}
				else
				{
					const char* ActualTypeName = TESForm::GetFormTypeIDLongName(Form->formType);
					if (EqualsCI(ActualTypeName, TypeToken.c_str()) == false)
					{
						RejectedTypeMismatch++;
						continue;
					}
				}

				if (SeenFormIDs.insert(Form->formID).second == false)
				{
					DuplicateRows++;
					continue;
				}

				AcceptedRows++;
			}

			BGSEEUI->MsgBoxI(
				"Region asset import pipeline complete (validation stage).\n"
				"CSV: %s\n\n"
				"Rows read: %u\n"
				"Accepted rows: %u\n"
				"Duplicate rows: %u\n"
				"Rejected malformed rows: %u\n"
				"Rejected missing/short columns: %u\n"
				"Rejected schema mismatch: %u\n"
				"Rejected unsupported type token: %u\n"
				"Rejected unresolved forms: %u\n"
				"Rejected out-of-scope forms: %u\n"
				"Rejected type mismatch rows: %u",
				ImportPath.c_str(),
				TotalRows,
				AcceptedRows,
				DuplicateRows,
				RejectedMalformed,
				RejectedHeader,
				RejectedSchema,
				RejectedTypeToken,
				RejectedResolve,
				RejectedScope,
				RejectedTypeMismatch);
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

		static bool StringContainsTokenCaseInsensitive(const std::string& Value, const char* Token)
		{
			if (Token == nullptr || Token[0] == 0)
				return false;

			char UpperValue[256] = { 0 };
			char UpperToken[64] = { 0 };
			FORMAT_STR(UpperValue, "%s", Value.c_str());
			FORMAT_STR(UpperToken, "%s", Token);
			_strupr_s(UpperValue, sizeof(UpperValue));
			_strupr_s(UpperToken, sizeof(UpperToken));

			return strstr(UpperValue, UpperToken) != nullptr;
		}

		static const char* ResolveRevoiceVoiceID(TESRace* VoiceRace, bool IsFemale)
		{
			if (VoiceRace == nullptr)
				return IsFemale ? "F-Imperial-Breton" : "M-Imperials";

			const char* EditorID = VoiceRace->GetEditorID() ? VoiceRace->GetEditorID() : "";
			const char* RaceName = VoiceRace->name.c_str();
			std::string ProbeText = std::string(EditorID) + " " + (RaceName ? RaceName : "");

			if (StringContainsTokenCaseInsensitive(ProbeText, "DREMORA"))
				return IsFemale ? "F-Dremora" : "M-Dremora";
			if (StringContainsTokenCaseInsensitive(ProbeText, "KHAJIIT") ||
				StringContainsTokenCaseInsensitive(ProbeText, "ARGONIAN"))
			{
				return IsFemale ? "F-Khajiit-Argonian" : "M-Khajiit-Argonian";
			}
			if (StringContainsTokenCaseInsensitive(ProbeText, "REDGUARD"))
				return IsFemale ? "F-Redguard" : "M-Redguard";
			if (StringContainsTokenCaseInsensitive(ProbeText, "NORD") ||
				StringContainsTokenCaseInsensitive(ProbeText, "ORC"))
			{
				return IsFemale ? "F-Orc-Nord" : "M-Orc-Nord";
			}
			if (StringContainsTokenCaseInsensitive(ProbeText, "IMPERIAL") ||
				StringContainsTokenCaseInsensitive(ProbeText, "BRETON"))
			{
				return IsFemale ? "F-Imperial-Breton" : (StringContainsTokenCaseInsensitive(ProbeText, "BRETON") ? "M-Bretons" : "M-Imperials");
			}
			if (StringContainsTokenCaseInsensitive(ProbeText, "DARK") ||
				StringContainsTokenCaseInsensitive(ProbeText, "WOOD") ||
				StringContainsTokenCaseInsensitive(ProbeText, "HIGH") ||
				StringContainsTokenCaseInsensitive(ProbeText, "ELF"))
			{
				return IsFemale ? "F-High-Dark-Wood Elves" : "M-Dark-High-Wood Elves";
			}

			return IsFemale ? "F-Imperial-Breton" : "M-Imperials";
		}

		static const char* GetDialogueEmotionTypeLabel(UInt32 EmotionType)
		{
			switch (EmotionType)
			{
			case TESTopicInfo::ResponseData::kEmotionType_Anger:
				return "Anger";
			case TESTopicInfo::ResponseData::kEmotionType_Disgust:
				return "Disgust";
			case TESTopicInfo::ResponseData::kEmotionType_Fear:
				return "Fear";
			case TESTopicInfo::ResponseData::kEmotionType_Sad:
				return "Sad";
			case TESTopicInfo::ResponseData::kEmotionType_Happy:
				return "Happy";
			case TESTopicInfo::ResponseData::kEmotionType_Surprise:
				return "Surprise";
			case TESTopicInfo::ResponseData::kEmotionType_Neutral:
			default:
				return "Neutral";
			}
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

			const bool BlastMode = BGSEEUI->MsgBoxI(hWnd,
				MB_YESNO,
				"Blast?\n\n"
				"Yes = export all dialogue lines/topics from selected plugins (no row filtering)\n"
				"No = keep normal filtering") == IDYES;

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
			AllowedParentMasters.reserve(_DATAHANDLER->activeFile->masterCount);
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

			std::vector<TESRace*> LoadedRaces;
			std::vector<TESRace*> OblivionDefaultRaces;
			for (tList<TESRace>::Iterator ItrRace = _DATAHANDLER->races.Begin(); ItrRace.End() == false && ItrRace.Get(); ++ItrRace)
			{
				TESRace* Race = ItrRace.Get();
				if (Race)
				{
					LoadedRaces.push_back(Race);

					TESFile* RaceSource = Race->GetOverrideFile(-1);
					if (RaceSource && _stricmp(RaceSource->fileName, "Oblivion.esm") == 0)
						OblivionDefaultRaces.push_back(Race);
				}
			}

			const bool IncludeOblivionOnlyRaces = BGSEEUI->MsgBoxI(hWnd,
				MB_YESNO,
				"Include Races from Oblivion.esm?\n\n"
				"Yes = Use only Oblivion.esm races for missing-race auto-fix\n"
				"No = Use all loaded races for missing-race auto-fix") == IDYES;

			const std::vector<TESRace*>& AutoFixRaces = IncludeOblivionOnlyRaces ? OblivionDefaultRaces : LoadedRaces;

			UInt32 Rows = 0;
			UInt32 FoundResponses = 0;
			UInt32 SkippedResponses = 0;
			UInt32 SkippedOutOfScope = 0;
			UInt32 SkippedMissingRace = 0;
			UInt32 SkippedEmptyText = 0;
			UInt32 SkippedFixedPathCollisions = 0;
			std::vector<std::string> ExportedRows;
			std::vector<std::string> OutOfScopeRows;
			std::vector<std::string> MissingRaceRows;
			std::vector<std::string> FixedRows;
			std::set<std::string> SeenFixedOutputPaths;
			ExportedRows.reserve(512);
			OutOfScopeRows.reserve(512);
			MissingRaceRows.reserve(512);
			FixedRows.reserve(512);
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
						const bool IsOutOfScope = ShouldExportDialogueFromFile(SourceFile, ExportMode, AllowedParentMasters) == false;
						if (IsOutOfScope)
							SkippedOutOfScope++;

						RevoiceSpeakerContext SpeakerContext = GetSpeakerContextFromTopicInfo(Info);
						TESNPC* Speaker = SpeakerContext.Speaker;
						bool IsFemale = false;
						TESRace* SpeakerRace = nullptr;
						if (Speaker)
						{
							SpeakerRace = Speaker->race;
							IsFemale = (Speaker->actorFlags & TESActorBaseData::kNPCFlag_Female) != 0;
						}
						else
						{
							SpeakerRace = SpeakerContext.RaceHint;
							if (SpeakerContext.HasSexHint)
								IsFemale = SpeakerContext.IsFemale;
						}
						const bool IsMissingRace = SpeakerRace == nullptr;
						if (IsMissingRace)
							SkippedMissingRace++;

						const char* SexToken = IsFemale ? "F" : "M";
						TESRace* VoiceRace = nullptr;
						if (SpeakerRace)
						{
							VoiceRace = IsFemale ? SpeakerRace->femaleVoiceRace : SpeakerRace->maleVoiceRace;
							if (VoiceRace == nullptr)
								VoiceRace = SpeakerRace;
						}

						const char* VoiceID = VoiceRace ? ResolveRevoiceVoiceID(VoiceRace, IsFemale) : "Unknown";
						const char* RaceName = "Unknown";
						if (SpeakerRace)
						{
							RaceName = SpeakerRace->name.c_str();
							if (RaceName == nullptr || strlen(RaceName) == 0)
								RaceName = "Unknown";
						}

						std::string SpeakerInfo = std::string(RaceName) + "\\" + SexToken;

						for (TESTopicInfo::ResponseListT::Iterator ItrResponse = Info->responseList.Begin();
							ItrResponse.End() == false && ItrResponse.Get();
							++ItrResponse)
						{
							TESTopicInfo::ResponseData* Response = ItrResponse.Get();
							if (Response == nullptr)
								continue;

							FoundResponses++;

							const char* ResponseText = Response->responseText.c_str();
							if ((ResponseText == nullptr || strlen(ResponseText) == 0) && BlastMode == false)
							{
								SkippedEmptyText++;
								continue;
							}
							if (ResponseText == nullptr)
								ResponseText = "";

							char Emotion[64] = { 0 };
							FORMAT_STR(Emotion, "%s:%u",
								GetDialogueEmotionTypeLabel(Response->emotionType),
								Response->emotionValue);

							char OutPath[MAX_PATH] = { 0 };
							const char* VoiceFolder = VoiceID;
							if (VoiceFolder == nullptr || strlen(VoiceFolder) == 0)
								VoiceFolder = RaceName;

							const char* QuestToken = GetNonEmptyToken(Quest->editorID.c_str(), "Quest");
							const char* TopicToken = GetNonEmptyToken(Topic->editorID.c_str(), "Topic");

							const char* SourcePlugin = SourceFile ? SourceFile->fileName : (_DATAHANDLER->activeFile ? _DATAHANDLER->activeFile->fileName : "ActivePlugin.esp");

							FORMAT_STR(OutPath, "Sound\\Voice\\%s\\%s\\%s\\%s_%s_%08X_%u.mp3",
								SourcePlugin,
								VoiceFolder,
								SexToken,
								QuestToken,
								TopicToken,
								Info->formID,
								Response->responseNumber);

							char FormID[9] = { 0 };
							FORMAT_STR(FormID, "%08X", Info->formID);

							RevoiceCSVRowData BaseRow;
							BaseRow.FormID = FormID;
							BaseRow.VoiceID = VoiceID;
							BaseRow.Race = RaceName;
							BaseRow.Gender = SexToken;
							BaseRow.SpeakerInfo = SpeakerInfo;
							BaseRow.Emotion = Emotion;
							BaseRow.OutputPath = OutPath;
							BaseRow.Dialogue = ResponseText;

							std::string Row = BuildRevoiceCSVRow(BaseRow);
							if (IsOutOfScope)
								OutOfScopeRows.push_back(Row);
							if (IsMissingRace && IsOutOfScope == false)
							{
								MissingRaceRows.push_back(Row);
								for (auto* LoadedRace : AutoFixRaces)
								{
									if (LoadedRace == nullptr)
										continue;

									const char* FixedRaceName = LoadedRace->name.c_str();
									if (FixedRaceName == nullptr || strlen(FixedRaceName) == 0)
										FixedRaceName = "Unknown";

									TESRace* FixedVoiceRace = IsFemale ? LoadedRace->femaleVoiceRace : LoadedRace->maleVoiceRace;
									if (FixedVoiceRace == nullptr)
										FixedVoiceRace = LoadedRace;
									const char* FixedVoiceID = ResolveRevoiceVoiceID(FixedVoiceRace, IsFemale);
									if (FixedVoiceID == nullptr || strlen(FixedVoiceID) == 0)
										FixedVoiceID = FixedRaceName;

									char FixedOutPath[MAX_PATH] = { 0 };
									FORMAT_STR(FixedOutPath, "Sound\\Voice\\%s\\%s\\%s\\%s_%s_%08X_%u.mp3",
										SourcePlugin,
										FixedVoiceID,
										SexToken,
										QuestToken,
										TopicToken,
										Info->formID,
										Response->responseNumber);

									if (SeenFixedOutputPaths.insert(FixedOutPath).second == false)
									{
										SkippedFixedPathCollisions++;
										continue;
									}

									RevoiceCSVRowData FixedRow = BaseRow;
									FixedRow.VoiceID = FixedVoiceID;
									FixedRow.Race = FixedRaceName;
									FixedRow.SpeakerInfo = std::string(FixedRaceName) + "\\" + SexToken;
									FixedRow.OutputPath = FixedOutPath;
									FixedRows.push_back(BuildRevoiceCSVRow(FixedRow));
								}
							}
							if (BlastMode || (IsOutOfScope == false && IsMissingRace == false))
								ExportedRows.push_back(Row);
							Rows++;
						}
					}
				}
			}

			SkippedResponses = SkippedOutOfScope + SkippedMissingRace + SkippedEmptyText;

			auto WriteCategoryRows = [&](const char* CategoryFolder,
				const std::vector<std::string>& CategoryRows,
				UInt32& OutWrittenFiles,
				std::string& OutFirstFile) -> bool
			{
				if (EnsureRevoiceCategoryDirectory(FilePath, CategoryFolder) == false)
				{
					BGSEEUI->MsgBoxE("Couldn't create output folder:\n%s", CategoryFolder);
					return false;
				}

				std::string CategoryPath = BuildRevoiceCategoryFilePath(FilePath, CategoryFolder);
				if (SplitIntoParts)
				{
					const size_t kPartSize = 24;
					const size_t PartCount = std::max<size_t>(1, (CategoryRows.size() + kPartSize - 1) / kPartSize);
					for (size_t Part = 0; Part < PartCount; Part++)
					{
						const size_t StartIndex = Part * kPartSize;
						const size_t EndIndex = std::min(CategoryRows.size(), StartIndex + kPartSize);
						std::string PartFilePath = BuildRevoicePartFilePath(CategoryPath, static_cast<UInt32>(Part + 1));
						if (WriteRevoiceCSVFile(PartFilePath, CategoryRows, StartIndex, EndIndex) == false)
						{
							BGSEEUI->MsgBoxE("Couldn't open output file for writing:\n%s", PartFilePath.c_str());
							return false;
						}
					}
					OutWrittenFiles += static_cast<UInt32>(PartCount);
					OutFirstFile = BuildRevoicePartFilePath(CategoryPath, 1);
				}
				else
				{
					if (WriteRevoiceCSVFile(CategoryPath, CategoryRows, 0, CategoryRows.size()) == false)
					{
						BGSEEUI->MsgBoxE("Couldn't open output file for writing:\n%s", CategoryPath.c_str());
						return false;
					}
					OutWrittenFiles += 1;
					OutFirstFile = CategoryPath;
				}

				return true;
			};

			UInt32 TotalWrittenFiles = 0;
			std::string ExportedFirstFile;
			if (WriteCategoryRows("Exported", ExportedRows, TotalWrittenFiles, ExportedFirstFile) == false)
				return;

			std::string OutOfScopeFirstFile;
			if (WriteCategoryRows("Out of scope", OutOfScopeRows, TotalWrittenFiles, OutOfScopeFirstFile) == false)
				return;

			std::string MissingRaceFirstFile;
			if (WriteCategoryRows("Missing race data", MissingRaceRows, TotalWrittenFiles, MissingRaceFirstFile) == false)
				return;

			std::string FixedFirstFile;
			if (WriteCategoryRows("Fixed", FixedRows, TotalWrittenFiles, FixedFirstFile) == false)
				return;

			BGSEEUI->MsgBoxI("reVoice CSV export complete.\nFound: %u dialogue responses\nExported: %u dialogue rows\nSkipped: %u responses\n  - Out of scope: %u\n  - Missing race data: %u\n  - Empty response text: %u\n\nAuto-fix from missing race data:\n  - Loaded races used: %u\n  - Fixed rows generated: %u\n  - Fixed rows skipped (path collisions): %u\n\nWrote %u files across folders:\n  - Exported\n  - Out of scope\n  - Missing race data\n  - Fixed\n\nExample files:\n%s\n%s\n%s\n%s",
					FoundResponses,
					Rows,
					SkippedResponses,
					SkippedOutOfScope,
					SkippedMissingRace,
					SkippedEmptyText,
					static_cast<UInt32>(AutoFixRaces.size()),
					static_cast<UInt32>(FixedRows.size()),
					SkippedFixedPathCollisions,
					TotalWrittenFiles,
					ExportedFirstFile.c_str(),
					OutOfScopeFirstFile.c_str(),
					MissingRaceFirstFile.c_str(),
					FixedFirstFile.c_str());


			std::string RaceListReport = "Auto-fix race pool:\n";
			if (AutoFixRaces.empty())
			{
				RaceListReport += "(none)";
			}
			else
			{
				for (auto* AutoFixRace : AutoFixRaces)
				{
					if (AutoFixRace == nullptr)
						continue;

					const char* RaceName = AutoFixRace->name.c_str();
					if (RaceName == nullptr || strlen(RaceName) == 0)
						RaceName = "Unknown";

					RaceListReport += "- ";
					RaceListReport += RaceName;

					TESFile* RaceSource = AutoFixRace->GetOverrideFile(-1);
					if (RaceSource && RaceSource->fileName)
					{
						RaceListReport += " (";
						RaceListReport += RaceSource->fileName;
						RaceListReport += ")";
					}

					RaceListReport += "\n";
				}
			}

			BGSEEUI->MsgBoxI("%s", RaceListReport.c_str());
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
				case 40136: // "CS Preferences..." in the File menu; route to CSE options dialog
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
				case IDC_MAINMENU_EXPORT_REGIONASSETCSV_ACTIVEPLUGIN:
					ExportRegionAssetFormsCSV(hWnd);

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
				case IDC_MAINMENU_MARKERPLACEMENT:
					if (g_MarkerPlacementDialog && IsWindow(g_MarkerPlacementDialog))
					{
						ShowWindow(g_MarkerPlacementDialog, SW_RESTORE);
						SetForegroundWindow(g_MarkerPlacementDialog);
						break;
					}

					g_MarkerPlacementDialog = BGSEEUI->ModelessDialog(BGSEEMAIN->GetExtenderHandle(),
						MAKEINTRESOURCE(IDD_MARKERPLACEMENT),
						hWnd,
						(DLGPROC)MarkerPlacementDlgProc);

					if (g_MarkerPlacementDialog == nullptr)
					{
						g_MarkerPlacementDialog = CreateDialogParam(BGSEEMAIN->GetExtenderHandle(),
							MAKEINTRESOURCE(IDD_MARKERPLACEMENT),
							hWnd,
							(DLGPROC)MarkerPlacementDlgProc,
							0);
					}

					if (g_MarkerPlacementDialog == nullptr)
						BGSEECONSOLE_ERROR("Couldn't create marker placement dialog");
					else
					{
						ShowWindow(g_MarkerPlacementDialog, SW_SHOW);
						SetForegroundWindow(g_MarkerPlacementDialog);
					}

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


		static bool IsBottomDockLikeChildWindow(HWND hWnd)
		{
			if (hWnd == nullptr || IsWindowVisible(hWnd) == FALSE)
				return false;

			char ClassName[64] = { 0 };
			GetClassNameA(hWnd, ClassName, sizeof(ClassName));
			return _stricmp(ClassName, "msctls_statusbar32") == 0 ||
				_stricmp(ClassName, "msctls_progress32") == 0 ||
				_stricmp(ClassName, "ToolbarWindow32") == 0;
		}

		static bool TryGetMainWindowViewportLayoutRect(HWND MainWindow, RECT& OutRect)
		{
			if (MainWindow == nullptr || IsWindow(MainWindow) == FALSE)
				return false;

			if (GetClientRect(MainWindow, &OutRect) == FALSE)
				return false;

			if (*TESCSMain::MainToolbarHandle && IsWindowVisible(*TESCSMain::MainToolbarHandle))
			{
				RECT ToolbarRect = { 0 };
				if (GetWindowRect(*TESCSMain::MainToolbarHandle, &ToolbarRect))
				{
					MapWindowPoints(nullptr, MainWindow, reinterpret_cast<LPPOINT>(&ToolbarRect), 2);
					OutRect.top = std::max<LONG>(OutRect.top, ToolbarRect.bottom);
				}
			}

			LONG BottomDockTop = OutRect.bottom;
			for (HWND Child = GetWindow(MainWindow, GW_CHILD); Child; Child = GetWindow(Child, GW_HWNDNEXT))
			{
				if (IsBottomDockLikeChildWindow(Child) == false)
					continue;

				if (Child == *TESCSMain::MainToolbarHandle || Child == *TESObjectWindow::WindowHandle ||
					Child == *TESCellViewWindow::WindowHandle || Child == *TESRenderWindow::WindowHandle)
				{
					continue;
				}

				RECT ChildRect = { 0 };
				if (GetWindowRect(Child, &ChildRect) == FALSE)
					continue;

				MapWindowPoints(nullptr, MainWindow, reinterpret_cast<LPPOINT>(&ChildRect), 2);
				if (ChildRect.top >= (OutRect.bottom / 2))
					BottomDockTop = std::min(BottomDockTop, ChildRect.top);
			}

			OutRect.bottom = std::max<LONG>(OutRect.top + 120, BottomDockTop);
			return (OutRect.right - OutRect.left) > 300 && (OutRect.bottom - OutRect.top) > 250;
		}

		static void AutoSnapMainViewports(HWND MainWindow)
		{
			if (MainWindow == nullptr || IsWindow(MainWindow) == FALSE)
				return;

			HWND ObjectWindow = *TESObjectWindow::WindowHandle;
			HWND CellViewWindow = *TESCellViewWindow::WindowHandle;
			HWND RenderWindow = *TESRenderWindow::WindowHandle;
			if (ObjectWindow == nullptr || CellViewWindow == nullptr || RenderWindow == nullptr)
				return;

			if (IsWindowVisible(ObjectWindow) == FALSE || IsWindowVisible(CellViewWindow) == FALSE || IsWindowVisible(RenderWindow) == FALSE)
				return;

			RECT LayoutRect = { 0 };
			if (TryGetMainWindowViewportLayoutRect(MainWindow, LayoutRect) == false)
				return;

			const int LayoutWidth = std::max<LONG>(0, LayoutRect.right - LayoutRect.left);
			const int LayoutHeight = std::max<LONG>(0, LayoutRect.bottom - LayoutRect.top);
			const int LeftColumnWidth = std::max(320, std::min(720, (LayoutWidth * 36) / 100));
			const int RightColumnWidth = std::max(320, LayoutWidth - LeftColumnWidth);
			const int ObjectHeight = std::max(220, (LayoutHeight * 66) / 100);
			const int CellHeight = std::max(160, LayoutHeight - ObjectHeight);

			SetWindowPos(ObjectWindow, nullptr,
				LayoutRect.left,
				LayoutRect.top,
				LeftColumnWidth,
				std::max(220, ObjectHeight),
				SWP_NOZORDER | SWP_NOACTIVATE);

			SetWindowPos(CellViewWindow, nullptr,
				LayoutRect.left,
				LayoutRect.top + std::max(220, ObjectHeight),
				LeftColumnWidth,
				std::max(160, CellHeight),
				SWP_NOZORDER | SWP_NOACTIVATE);

			SetWindowPos(RenderWindow, nullptr,
				LayoutRect.left + LeftColumnWidth,
				LayoutRect.top,
				RightColumnWidth,
				LayoutHeight,
				SWP_NOZORDER | SWP_NOACTIVATE);
		}

#define ID_PATHGRIDTOOLBARBUTTION_TIMERID		0x99
#define ID_AUTOSNAPVIEWPORTS_TIMERID		0x9A

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
					SetTimer(hWnd, ID_AUTOSNAPVIEWPORTS_TIMERID, 750, nullptr);
					SubclassParams->Out.MarkMessageAsHandled = true;
				}

				break;
			case WM_DESTROY:
				{
					KillTimer(hWnd, ID_PATHGRIDTOOLBARBUTTION_TIMERID);
					KillTimer(hWnd, ID_AUTOSNAPVIEWPORTS_TIMERID);

					if (g_MarkerPlacementDialog && IsWindow(g_MarkerPlacementDialog))
						DestroyWindow(g_MarkerPlacementDialog);
					g_MarkerPlacementDialog = nullptr;

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
			case WM_SIZE:
				AutoSnapMainViewports(hWnd);
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
				case ID_AUTOSNAPVIEWPORTS_TIMERID:
					AutoSnapMainViewports(hWnd);
					KillTimer(hWnd, ID_AUTOSNAPVIEWPORTS_TIMERID);

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
