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

static int luaC_isadmin_(lua_State* L)
{
    static int *pcachedResult = NULL;
    if(!pcachedResult)
    {
        static int cachedResult = 0;
        //pcachedResult = &cachedResult;
    }
    if(!pcachedResult)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushboolean(L, *pcachedResult);
    return 1;
}

static const luaL_Reg isadmin_funcs[] = {
    {"isadmin", luaC_isadmin_},
    {NULL, NULL}
};

LUALIB_API int luaopen_isadmin(lua_State *L)
{
    luaL_register(L, "os", isadmin_funcs);
    return 1;
}
