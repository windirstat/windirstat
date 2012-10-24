#ifndef __LUAWIN_DLLERROR_H__
#define __LUAWIN_DLLERROR_H__
#ifdef  __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <lua.h>


#ifdef NDEBUG
void lua_dllerror(lua_State *L, DWORD dwErr);
#	define	LUA_LAST_DLL_ERROR_ASSERT(L,exp)((exp)||(lua_dllerror(L, 0),0))
#	define	LUA_CHECK_DLL_ERROR(L, code)	((void)((code)&&(lua_dllerror(L, (code)),0)))
#	define	LUA_CHECK_LAST_DLL_ERROR(L)		lua_dllerror(L, 0)
#else
void lua_dllerror(lua_State *L, DWORD dwErr, const char * exp, const char * file, int line);
#	define	LUA_LAST_DLL_ERROR_ASSERT(L,exp)((exp)||(lua_dllerror(L, 0, #exp, __FILE__, __LINE__),0))
#	define	LUA_CHECK_DLL_ERROR(L, code)	((void)((code)&&(lua_dllerror(L, (code), NULL, __FILE__, __LINE__),0)))
#	define	LUA_CHECK_LAST_DLL_ERROR(L)		lua_dllerror(L, 0, NULL, __FILE__, __LINE__)
#endif
#define LUA_LAST_DLL_ERROR_ASSERT_RETURN_OBJECT(L,exp)	LUA_CHECK_RETURN_OBJECT(L, LUA_LAST_DLL_ERROR_ASSERT(L,exp))
#define LUA_LAST_DLL_ERROR_ASSERT_LEAVE_OBJECT(L,exp)	LUA_CHECK_LEAVE_OBJECT(L, LUA_LAST_DLL_ERROR_ASSERT(L,exp))
#define LUA_LAST_DLL_ERROR_ASSERT_PUSHBOOL(L,exp)	lua_pushboolean(L, LUA_LAST_DLL_ERROR_ASSERT(L, exp)?1:0)
#define LUA_LAST_DLL_ERROR_ASSERT_RETBOOL(L,exp)	return(LUA_LAST_DLL_ERROR_ASSERT_PUSHBOOL(L,exp), 1)


#ifdef  __cplusplus
}
#endif
#endif //__LUAWIN_DLLERROR_H__
