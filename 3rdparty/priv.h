///////////////////////////////////////////////////////////////////////////////
///
/// Written by Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
///
/// Purpose          : Functions to deal with NT privileges.
///
///////////////////////////////////////////////////////////////////////////////
#ifndef __PRIV_H_VER__
#define __PRIV_H_VER__ 2018070910
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif /* Check for "#pragma once" support */

#include <Windows.h>
#include <winnt.h>
#include <tchar.h>

EXTERN_C HANDLE PrivGetProcessToken(DWORD dwAdditionalAccess);
EXTERN_C HANDLE PrivGetThreadToken(DWORD dwAdditionalAccess);
EXTERN_C BOOL PrivSetTokenPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
EXTERN_C BOOL PrivSetContextPrivilege(LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
EXTERN_C BOOL PrivSetProcessPrivilege(LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
EXTERN_C BOOL PrivSetThreadPrivilege(LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
EXTERN_C BOOL PrivHasTokenPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, LPDWORD lpdwAttributes);
EXTERN_C BOOL PrivHasContextTokenPrivilege(LPCTSTR lpszPrivilege, LPDWORD lpdwAttributes);
EXTERN_C BOOL PrivHasProcessTokenPrivilege(LPCTSTR lpszPrivilege, LPDWORD lpdwAttributes);
EXTERN_C BOOL PrivHasThreadTokenPrivilege(LPCTSTR lpszPrivilege, LPDWORD lpdwAttributes);
EXTERN_C BOOL PrivIsContextTokenPrivilegeEnabled(LPCTSTR lpszPrivilege);
EXTERN_C BOOL PrivIsProcessTokenPrivilegeEnabled(LPCTSTR lpszPrivilege);
EXTERN_C BOOL PrivIsThreadTokenPrivilegeEnabled(LPCTSTR lpszPrivilege);

#ifdef __cplusplus
class CSnapEnableAssignedPrivilege
{
    BOOL m_bEnabled, m_bNoErrors;
    LPTSTR m_lpszPrivilege;
public:
    CSnapEnableAssignedPrivilege(LPCTSTR lpszPrivilege, BOOL bNoErrors = TRUE)
        : m_bEnabled(FALSE)
        , m_bNoErrors(bNoErrors)
        , m_lpszPrivilege((lpszPrivilege) ? _tcsdup(lpszPrivilege) : NULL)
    {
        if (PrivHasContextTokenPrivilege(m_lpszPrivilege, NULL))
        {
            m_bEnabled = PrivSetContextPrivilege(m_lpszPrivilege, TRUE);
            if (!m_bEnabled)
            {
                if (!m_bNoErrors)
                {
                    _ftprintf(stderr, _T("Could not enable %s\n"), lpszPrivilege);
                }
            }
        }
    }

    virtual ~CSnapEnableAssignedPrivilege()
    {
        if (m_bEnabled)
        {
            (void)PrivSetContextPrivilege(m_lpszPrivilege, FALSE);
        }
        free(m_lpszPrivilege);
    }
private:
    CSnapEnableAssignedPrivilege(CSnapEnableAssignedPrivilege&);
    CSnapEnableAssignedPrivilege& operator=(CSnapEnableAssignedPrivilege&);
};
#endif

#endif /* __PRIV_H_VER__ */
