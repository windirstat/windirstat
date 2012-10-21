#ifndef __WDS_LUA_C_H_VER__
#define __WDS_LUA_C_H_VER__ 2012102122
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

#define LNUM_INT64
#define WDS_LUA_NO_MATHLIB
#define WDS_LUA_NO_IOLIB
#define WDS_LUA_NO_LOADLIB
#define WDS_LUA_NO_INIT
#define WDS_LUA_NO_OSLIB
#define WDS_LUA_NO_LUAC

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
#   include <lua.h>
#   include <lauxlib.h>
#   include <lualib.h>
#ifdef __cplusplus
};
#endif // __cplusplus

//#include "modules/lwinreg.h"
#endif // __WDS_LUA_C_H_VER__
