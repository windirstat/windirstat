#ifndef __W32RESEMBED_H_VER__
#define __W32RESEMBED_H_VER__ 2012121304
// $Id$
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

#ifdef RC_INVOKED
#   include <WinNT.rh>
#   define RT_LUASCRIPT "LUA"
#else
#   define RT_LUASCRIPT TEXT("LUA")

EXTERN_C int enumerateEmbeddedLuaScripts(lua_State* L);
#endif

#endif // __W32RESEMBED_H_VER__
