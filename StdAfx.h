// Precompiled header
//	>> Include headers with stable code
//	!! Recompile header after modifications
//
#pragma once
// 4018 - signed/unsigned mismatch
// 4244 - loss of data by assignment
// 4267 - possible loss of data (truncation)
// 4305 - truncation by assignment
// 4288 - disable warning for crap Microsoft extension screwing up the scope of variables defined in for loops
// 4311 - pointer truncation
// 4312 - pointer extension
#pragma warning(disable: 4018 4244 4267 4305 4288 4312 4311 4800)

// Header Version - Win7+
#define WINVER			0x0601
#define _WIN32_WINNT	0x0601
#define DIRECTINPUT_VERSION 0x0800
#define DPSAPI_VERSION	1
#define NOMINMAX 1

// WIN32
#include <windows.h>
#include <windowsx.h>
#include <atltypes.h>
#include <commctrl.h>
#include <richedit.h>
#include <shlobj.h>
#include <Rpc.h>
#include <Dbghelp.h>
#include <uxtheme.h>
#include <Objbase.h>
#include <Psapi.h>
#include <Shellapi.h>
#include <vssym32.h>

// CRT
#include <time.h>
#include <intrin.h>
#include <errno.h>
#include <crtdefs.h>

// STL
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdarg>
#include <cmath>
#include <cwchar>
#include <algorithm>
#include <list>
#include <map>
#include <vector>
#include <set>
#include <queue>
#include <stack>
#include <string>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <exception>
#include <sstream>
#include <memory>
#include <iostream>
#include <complex>
#include <iomanip>
#include <numeric>
#include <functional>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <array>

// RPC
#include <Rpc.h>

// DIRECTX
#include <d3d9.h>
#include <d3d9types.h>
#include <d3dx9.h>
#include <d3dx9tex.h>
#include <dinput.h>


// xSE Common
#include <ITypes.h>
#include <IErrors.h>
#include <IDynamicCreate.h>
#include <ISingleton.h>
#include <IDirectoryIterator.h>
#include <IFileStream.h>

// SME
#include <MemoryHandler.h>
#include <INIManager.h>
#include <INIEditGUI.h>
#include <Functors.h>
#include <StringHelpers.h>
#include <UIHelpers.h>
#include <MiscGunk.h>
#include <MersenneTwister.h>

using namespace SME;
using namespace SME::INI;
using namespace SME::Functors;
using namespace SME::MemoryHandler;

// xOBSE / editor-safe declarations and interfaces
#include "obse_common\obse_version.h"

// Pull authoritative OBSE interfaces/types from xOBSE headers.
#include <PluginAPI.h>

// xOBSE templates may reference FormHeap allocators via declarations.
void* FormHeap_Allocate(UInt32 Size);
void  FormHeap_Free(void* Ptr);

// xOBSE/xSE compatibility shim
// Keep this active across BGSEEBase headers so any transitive xOBSE includes
// get renamed and cannot collide with CSE editor-side classes.
#define BSFile             OBSEShim_BSFile
#define BSRenderedTexture  OBSEShim_BSRenderedTexture
#define BSTextureManager   OBSEShim_BSTextureManager
#define TESChildCell       OBSEShim_TESChildCell

// BGSEEBASE
#include "[BGSEEBase]\Main.h"
#include "[BGSEEBase]\Console.h"
#include "[BGSEEBase]\UIManager.h"
#include "[BGSEEBase]\HookUtil.h"

// End shim BEFORE CSE editor headers
#undef TESChildCell
#undef BSTextureManager
#undef BSRenderedTexture
#undef BSFile

// CSE
#include "Main.h"
#include "EditorAPI\TESEditorAPI.h"
#include "Settings.h"
#include "EventSources.h"
#include "EventSinks.h"
#include "Serialization.h"

#define PI						3.151592653589793

#undef SME_ASSERT
#define SME_ASSERT(_Expression) (void)( (!!(_Expression)) || (BGSEECONSOLE->LogAssertion("CSE", "ASSERTION FAILED (%s, %d): %s", __FILE__, __LINE__, #_Expression), _wassert(_CRT_WIDE(#_Expression), _CRT_WIDE(__FILE__), __LINE__), 0) )


// required for assertions in d'tors (for static instances) as we don't want it to trigger the crash handler recursively
#ifdef NDEBUG
#define DEBUG_ASSERT(_Expression)		(void)( (!!(_Expression)) || (BGSEECONSOLE->LogAssertion("CSE", "ASSERTION FAILED (%s, %d): %s", __FILE__, __LINE__, #_Expression), 0) )
#else
#define DEBUG_ASSERT(expr)		SME_ASSERT(expr)
#endif
