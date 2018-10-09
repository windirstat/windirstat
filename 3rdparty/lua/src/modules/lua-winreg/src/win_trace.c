#include <windows.h>

/*
BOOL CALLBACK IsDebuggerPresentStub(VOID);
BOOL (CALLBACK *IsDebuggerPresent)(VOID) = IsDebuggerPresentStub;

BOOL CALLBACK IsDebuggerPresentStub(VOID){
    HINSTANCE hinst = GetModuleHandleA("KERNEL32.DLL");
    FARPROC fp = GetProcAddress(hinst, "IsDebuggerPresent");
    if (fp) {
		*(FARPROC *)&IsDebuggerPresent = fp;
		return IsDebuggerPresent();
	}
	return 0;
}
*/

void win_traceA(const char *pszFmt, ...){
	CHAR tchbuf[1024] = {'$',0};
	va_list argList = NULL;
	va_start(argList, pszFmt);
	wvsprintfA(&tchbuf[1], pszFmt, argList);
	va_end(argList);
	lstrcatA(tchbuf, "\r\n");
	if(IsDebuggerPresent()){
		OutputDebugStringA(tchbuf);
	}else{
		DWORD dwWrt;
		WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), tchbuf, lstrlenA(tchbuf), &dwWrt, NULL);
	}
}
void win_traceW(const WCHAR *pszFmt, ...){
	WCHAR tchbuf[1024];
	va_list argList = NULL;
	va_start(argList, pszFmt);
	wvsprintfW(tchbuf, pszFmt, argList);
	va_end(argList);
	if(IsDebuggerPresent()){
		OutputDebugStringW(tchbuf);
	}else{
		DWORD dwWrt;
		WriteConsoleW(GetStdHandle(STD_ERROR_HANDLE), tchbuf, lstrlenW(tchbuf), &dwWrt, NULL);
	}
}
