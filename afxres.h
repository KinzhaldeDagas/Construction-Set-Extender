#pragma once

// Fallback MFC resource include shim for environments where the MFC/ATL
// headers are unavailable. SME Sundries RC files include <afxres.h>, but the
// project only uses base Win32 resource definitions.
#include <winres.h>
