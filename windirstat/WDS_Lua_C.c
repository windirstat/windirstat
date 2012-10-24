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
#if _MSC_VER >= 1400
#   pragma push_macro("LoadString")
#   undef LoadString
#   define LoadString lundump_LoadString
#else
#   error "LoadString from lundump.c will conflict with name from Win32 API"
#endif
#include "lundump.c"
#if _MSC_VER >= 1400
#   pragma pop_macro("LoadString")
#endif
#include "lvm.c"
#include "lzio.c"

// Patches
#include "lnum.c"

// Libraries
#include "lauxlib.c"
#include "lbaselib.c"
#include "ldblib.c"
#ifndef WDS_LUA_NO_IOLIB
#   include "liolib.c"
#endif // WDS_LUA_NO_IOLIB
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

// Modules/Packages, individual functions
#include "modules/winreg.c"
#include "modules/isadmin.c"
#include "modules/wow64.c"

// Cheat a bit to redefine the list of "default" libraries ...
#ifndef WDS_LUA_NO_INIT
static const luaL_Reg lualibs[] = {
    {"", luaopen_base},
    {LUA_LOADLIBNAME, luaopen_package},
    {LUA_TABLIBNAME, luaopen_table},
    {LUA_IOLIBNAME, luaopen_io},
    {LUA_OSLIBNAME, luaopen_os},
    {LUA_STRLIBNAME, luaopen_string},
    {LUA_MATHLIBNAME, luaopen_math},
    {LUA_DBLIBNAME, luaopen_debug},
    {LUA_WINREGNAME, luaopen_winreg},
    {NULL, NULL},
};

static void luaWDS_openlibs_(lua_State *L)
{
    const luaL_Reg *lib = lualibs;
    for (; lib->func; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}
#endif // WDS_LUA_NO_INIT

lua_State* luaWDS_open()
{
    lua_State* L = lua_open();
    if(L)
    {
        lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialization */
        luaWDS_openlibs_(L);  /* open libraries */
        luaL_register(L, "os", wow64_funcs);
        luaL_register(L, "os", isadmin_funcs);
        lua_gc(L, LUA_GCRESTART, 0); /* resume GC */
    }
    return L;
}

#ifdef _WIN64
#   pragma warning(pop)
#endif
