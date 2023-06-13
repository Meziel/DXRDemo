// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif
// C RunTime Header Files
#include <wrl.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
