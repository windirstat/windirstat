#ifndef __WDS_LUA_C_H_VER__
#define __WDS_LUA_C_H_VER__ 2012121805
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

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
#include "modules/winreg.h"
#include "modules/lua-winreg/src/lua_tstring.h"
#include "modules/isadmin.h"
#include "modules/wow64.h"
#include "modules/dbgprint.h"
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

#endif // __WDS_LUA_C_H_VER__
