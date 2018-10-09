#ifndef __WIN_REGISTRY_H__
#define __WIN_REGISTRY_H__
#ifdef  __cplusplus
extern "C" {
#endif

#include <windows.h>
LONG win_reg_deltree(HKEY hParentKey, const TCHAR * strKeyName);

#ifdef  __cplusplus
}
#endif
#endif //__WIN_REGISTRY_H__