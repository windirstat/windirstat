#define luaall_c
#define LUA_CORE
#include "WDS_Lua_C.h"

#ifdef _WIN64
#   pragma warning(push)
#   pragma warning(disable:4324)
#   pragma warning(disable:4334)
#endif

// Core
#include "lapi.c"
#include "lcode.c"
#include "ldebug.c"
#include "ldo.c"
#include "ldump.c"
#include "lfunc.c"
#include "lgc.c"
#include "llex.c"
#include "lmem.c"
#include "lobject.c"
#include "lopcodes.c"
#include "lparser.c"
#include "lstate.c"
#include "lstring.c"
#include "ltable.c"
#include "ltm.c"
#include "lundump.c"
#include "lvm.c"
#include "lzio.c"
#include "lnum.c"

#include "lauxlib.c"
#include "lbaselib.c"
#include "ldblib.c"
#ifndef WDS_LUA_NO_IOLIB
#   include "liolib.c"
#endif // WDS_LUA_NO_IOLIB
#ifndef WDS_LUA_NO_INIT
#   include "linit.c"
#endif // WDS_LUA_NO_INIT
#ifndef WDS_LUA_NO_MATHLIB
#   include "lmathlib.c"
#endif // WDS_LUA_NOMATH
#ifndef WDS_LUA_NO_LOADLIB
#   include "loadlib.c"
#endif // WDS_LUA_NO_LOADLIB
#ifndef WDS_LUA_NO_OSLIB
#   include "loslib.c"
#endif // WDS_LUA_NO_OSLIB
#include "lstrlib.c"
#include "ltablib.c"
#ifndef WDS_LUA_NO_LUAC
#   include "lua.c"
#endif // WDS_LUA_NO_LUAC

//#include "modules/lwinreg.c"

#ifdef _WIN64
#   pragma warning(pop)
#endif
