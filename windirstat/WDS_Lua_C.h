#ifndef __WDS_LUA_C_H_VER__
#define __WDS_LUA_C_H_VER__ 2012102414
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

#endif // __WDS_LUA_C_H_VER__
