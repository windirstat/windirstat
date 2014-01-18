#ifndef __W32RESEMBED_H_VER__
#define __W32RESEMBED_H_VER__ 2012121805
// $Id: w32resembed.h,v c46495b518e1 2012/12/18 05:27:02 oliver $
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

#ifdef RC_INVOKED
#   include <WinNT.rh>
#   define RT_LUASCRIPT "LUA"
#else
#   include <Windows.h>
#   define RT_LUASCRIPT TEXT("LUA")

#   define W32RES_MODNAME "winres"
#   define W32RES_LOADER  "c_loader"
#   define W32RES_SCRIPTS "scripts"

    EXTERN_C int w32res_enumerateEmbeddedLuaScripts(lua_State* L);

#   ifndef lua_pushtstring
#       define lua_pushtstring lua_pushstring
#   endif
#   ifndef lua_pushtstring_lowercase
    // same as in Lua: s = s:lower() ... with s on the stack afterward
#       define lua_pushtstring_lowercase(L, s) \
            lua_getfield(L, LUA_GLOBALSINDEX, "string"); \
            lua_getfield(L, -1, "lower"); \
            lua_remove(L, -2); \
            lua_pushtstring(L, s); \
            lua_call(L, 1, 1)
#   endif // lua_pushtstring_lowercase

#endif

#endif // __W32RESEMBED_H_VER__
