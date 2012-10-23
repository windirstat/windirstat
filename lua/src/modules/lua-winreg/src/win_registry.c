#include <windows.h>

// delete the whole sub key
LONG win_reg_deltree(HKEY hParentKey, const TCHAR * strKeyName){
	TCHAR   szSubKeyName[MAX_PATH];
	HKEY    hCurrentKey;
	LONG   dwResult;
	if ((dwResult = RegOpenKey(hParentKey, strKeyName, &hCurrentKey)) == ERROR_SUCCESS){
		// Remove all subkeys of the key to delete
		while ((dwResult = RegEnumKey(hCurrentKey, 0, szSubKeyName, 255)) == ERROR_SUCCESS){
			if((dwResult = win_reg_deltree(hCurrentKey, szSubKeyName)) != ERROR_SUCCESS)break;
		}
		// If all went well, we should now be able to delete the requested key
		if((dwResult == ERROR_NO_MORE_ITEMS) || (dwResult == ERROR_BADKEY)){
			dwResult = RegDeleteKey(hParentKey, strKeyName);
		}
		RegCloseKey(hCurrentKey);
	}
	return dwResult;
}