///////////////////////////////////////////////////////////////////////////////
///
/// Written 2012, Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
///
/// Original filename: isadmin.c
/// Project          : WinDirStat
/// Author(s)        : Oliver Schneider
///
/// Purpose          : Lua function, implemented in C, to check whether the
///                    user is privileged or not.
///
///////////////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <tchar.h>
#include <lua.h>

BOOL static IsAdmin_()
{
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID pSid;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pSid))
    {
        BOOL bResult = FALSE;
        if (!CheckTokenMembership( NULL, pSid, &bResult))
        {
            FreeSid(pSid);
            return FALSE;
        }
        FreeSid(pSid);
        return bResult;
    }

    return FALSE;
}

static int luaC_isadmin_(lua_State* L)
{
    static int *pcachedResult = NULL;
    if(!pcachedResult)
    {
        static int cachedResult = 0;
        cachedResult = (IsAdmin_()) ? 1 : 0;
        pcachedResult = &cachedResult;
    }
    lua_pushboolean(L, *pcachedResult);
    return 1;
}

LUA_API const luaL_Reg isadmin_funcs[] = {
    {"isadmin", luaC_isadmin_},
    {NULL, NULL}
};
