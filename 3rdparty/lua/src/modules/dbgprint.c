///////////////////////////////////////////////////////////////////////////////
///
/// Written 2012, Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
///
/// Original filename: dbgprint.c
/// Project          : WinDirStat
/// Author(s)        : Oliver Schneider
///
/// Purpose          : Lua function, implemented in C, to call OutputDebugString
///
///////////////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <tchar.h>
#include <lua.h>
#include "lua-winreg/src/lua_tstring.h"

static int luaC_dbgprint_(lua_State* L)
{
    const TCHAR *str = lua_checkltstring(L,1,NULL);
    OutputDebugString(str);
    return 0;
}

LUA_API const luaL_Reg dbgprint_funcs[] = {
    {"dbgprint", luaC_dbgprint_},
    {NULL, NULL}
};
