///////////////////////////////////////////////////////////////////////////////
///
/// Written 2012, Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
///
/// Original filename: wow64.c
/// Project          : WinDirStat
/// Author(s)        : Oliver Schneider
///
/// Purpose          : Lua function, implemented in C, to check whether the
///                    current process is running as WOW64 process.
///
///////////////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <tchar.h>
#include <lua.h>

static int luaC_iswow64_(lua_State* L)
{
    static int *pcachedResult = NULL;
    if(!pcachedResult)
    {
        typedef BOOL (WINAPI *TFNIsWow64Process) (HANDLE, PBOOL);
        static int cachedResult = 0;
        TFNIsWow64Process pfnIsWow64Process = (TFNIsWow64Process)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "IsWow64Process");
        if(pfnIsWow64Process)
        {
            BOOL bIsWow64 = FALSE;
            // Ignore result, instead presume it's not WOW64
            (void)pfnIsWow64Process(GetCurrentProcess(), &bIsWow64);
            cachedResult = (bIsWow64) ? 1 : 0;
        }
        pcachedResult = &cachedResult;
    }
    if(!pcachedResult)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushboolean(L, *pcachedResult);
    return 1;
}

LUA_API const luaL_Reg wow64_funcs[] = {
    {"iswow64", luaC_iswow64_},
    {NULL, NULL}
};
