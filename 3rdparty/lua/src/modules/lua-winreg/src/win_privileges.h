#ifndef __WIN_PRIVILEGES_H__
#define __WIN_PRIVILEGES_H__
#ifdef  __cplusplus
extern "C" {
#endif

#include <windows.h>
BOOL win_setprivilege(const TCHAR * privilege, BOOL bEnable, HANDLE hToken);

#ifdef  __cplusplus
}
#endif
#endif //__WIN_PRIVILEGES_H__
