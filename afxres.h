#pragma once

// Compatibility shim for environments where MFC is not installed.
// Prefer the canonical Win32 resource header and fall back to winresrc.h.
#if __has_include(<winres.h>)
#include <winres.h>
#else
#include <winresrc.h>
#endif

// MFC's afxres.h also provides IDC_STATIC used by many .rc dialogs.
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif
