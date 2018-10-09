#ifndef __WINTRACE_H__
#define __WINTRACE_H__
#pragma once

#ifndef NOP_FUNCTION
	#if (_MSC_VER >= 1210)
		#define NOP_FUNCTION __noop
	#else
		#pragma warning(disable:4505) // unreferenced local function has been removed.
		static void nullfunc(const void * x, ...){FatalAppExit(0,(LPCTSTR)x);}
		#define NOP_FUNCTION 1?((void)0):nullfunc
	#endif
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#include <windows.h>
#ifdef NDEBUG
	#define WIN_TRACET NOP_FUNCTION
	#define WIN_TRACEA NOP_FUNCTION
	#define WIN_TRACEW NOP_FUNCTION
	#define WIN_TRACEA_FT NOP_FUNCTION
	#define WIN_TRACEA_ST NOP_FUNCTION
#else
	void win_traceA(const char *pszFmt, ...);
	void win_traceW(const WCHAR *pszFmt, ...);

	#define WIN_TRACEA win_traceA
	#define WIN_TRACEW win_traceW
	#ifdef UNICODE
		#define WIN_TRACET win_traceW
	#else
		#define WIN_TRACET win_traceA
	#endif

	// trace a FILE_TIME
	#define WIN_TRACEA_FT(msg,pft) if(pft){SYSTEMTIME st;FileTimeToSystemTime(pft, &st); \
		WIN_TRACEA(msg " %s %0.4d-%0.2d-%0.2d %0.2d:%0.2d:%0.2d.%0.3d", #pft, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);}

	// trace a SYSTEMTIME
	#define WIN_TRACEA_ST(msg,pst) if(pst){ \
		WIN_TRACEA(msg " %s %0.4d-%0.2d-%0.2d %0.2d:%0.2d:%0.2d.%0.3d", #pst, pst->wYear, pst->wMonth, pst->wDay, pst->wHour, pst->wMinute, pst->wSecond, pst->wMilliseconds);}

#endif

#ifdef  __cplusplus
}
#endif

#endif //__WINTRACE_H__