#include <windows.h>


BOOL win_setprivilege(const TCHAR * privilege, BOOL bEnable, HANDLE hToken){
	TOKEN_PRIVILEGES tpPrevious;
	TOKEN_PRIVILEGES tp;
	DWORD  cbPrevious = sizeof(TOKEN_PRIVILEGES);
	LUID   luid;
	HANDLE hTokenUsed;

	// if no token specified open process token
	if(hToken == 0){
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hTokenUsed)){
			return FALSE;
		}
	}else hTokenUsed = hToken;

	if (!LookupPrivilegeValue(NULL, privilege, &luid )){
		if (hToken == 0)
			CloseHandle(hTokenUsed);
		return FALSE;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = 0;

	if (!AdjustTokenPrivileges(hTokenUsed, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), &tpPrevious, &cbPrevious)){
		if (hToken == 0)
			CloseHandle(hTokenUsed);
		return FALSE;
	}

	tpPrevious.PrivilegeCount = 1;
	tpPrevious.Privileges[0].Luid = luid;

	if (bEnable)
		tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
	else
		tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED & tpPrevious.Privileges[0].Attributes);

	if (!AdjustTokenPrivileges(hTokenUsed, FALSE, &tpPrevious, cbPrevious, NULL, NULL)){
		if (hToken == 0)
			CloseHandle(hTokenUsed);
		return FALSE;
	}
	return TRUE;
}
