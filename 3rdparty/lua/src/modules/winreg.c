#define __LUA_WINREG_C__
#define LUA_REG_DEFINE_EXTERNS
#include "lua-winreg/src/luareg.h"
#include "winreg.h"

#ifdef LUA_REG_NO_WINTRACE
#   define __WINTRACE_H__
#   ifndef NOP_FUNCTION
#       if (_MSC_VER >= 1210)
#           define NOP_FUNCTION __noop
#       else
#           pragma warning(disable:4505) // unreferenced local function has been removed.
            static void nullfunc(const void * x, ...){FatalAppExit(0,(LPCTSTR)x);}
#           define NOP_FUNCTION 1?((void)0):nullfunc
#       endif
#   endif
#   define WIN_TRACET NOP_FUNCTION
#   define WIN_TRACEA NOP_FUNCTION
#   define WIN_TRACEW NOP_FUNCTION
#   define WIN_TRACEA_FT NOP_FUNCTION
#   define WIN_TRACEA_ST NOP_FUNCTION
#endif // LUA_REG_NO_WINTRACE

#include "lua-winreg/src/l52util.h"
#include "lua-winreg/src/stdmacro.h"
#include "lua-winreg/src/luamacro.h"
#include "lua-winreg/src/luawinmacro.h"
#include "lua-winreg/src/lua_tstring.h"
#include "lua-winreg/src/lua_int64.h"
#include "lua-winreg/src/lua_mtutil.h"
#include "lua-winreg/src/luawin_dllerror.h"
#include "lua-winreg/src/win_trace.h"
#include "lua-winreg/src/win_privileges.h"
#include "lua-winreg/src/win_registry.h"

#include "lua-winreg/src/l52util.c"
#include "lua-winreg/src/luawin_dllerror.c"
#include "lua-winreg/src/lua_int64.c"
#include "lua-winreg/src/lua_mtutil.c"
#include "lua-winreg/src/lua_tstring.c"
#include "lua-winreg/src/win_privileges.c"
#include "lua-winreg/src/win_registry.c"
#include "lua-winreg/src/win_trace.c"
#include "lua-winreg/src/winreg.c"

