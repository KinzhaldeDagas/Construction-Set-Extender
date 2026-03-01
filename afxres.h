#pragma once

// Compatibility shim for environments where MFC is not installed.
// For resource compiler invocations, include the canonical Win32 header directly
// because RC preprocessor support is limited compared to the C/C++ compiler.
#ifdef RC_INVOKED
#include <winres.h>
#else
// Prefer the canonical Win32 resource header and fall back to winresrc.h.
#if defined(__has_include) && __has_include(<winres.h>)
#include <winres.h>
#else
#include <winresrc.h>
#endif
#endif

// MFC's afxres.h also provides IDC_STATIC used by many .rc dialogs.
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif
