#pragma once

#define LNUM_INT64
#define WDS_LUA_NO_LUAC

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
#   include <lua.h>
#   include <lauxlib.h>
#   include <lualib.h>
lua_State* luaWDS_open();
#ifdef __cplusplus
};
#endif // __cplusplus

// Modules/Packages headers
#include "modules/lua-winreg/src/lua_tstring.h"
#include "modules/w32resembed.h"

#ifndef lua_pushtstring_lowercase
    // same as in Lua: s = s:lower() ... with s on the stack afterward
#   define lua_pushtstring_lowercase(L, s) \
        lua_getfield(L, LUA_GLOBALSINDEX, "string"); \
        lua_getfield(L, -1, "lower"); \
        lua_remove(L, -2); \
        lua_pushtstring(L, s); \
        lua_call(L, 1, 1)
#endif // lua_pushtstring_lowercase

EXTERN_C void stackDump(lua_State* L, char* description);
