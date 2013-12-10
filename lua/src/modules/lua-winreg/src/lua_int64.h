#ifndef __LUA_INT64_H___
#define __LUA_INT64_H___
#ifdef  __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <stdlib.h> //lua_push?INT64
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

UINT64 lua_checkUINT64(lua_State *L, int i);
INT64 lua_checkINT64(lua_State *L, int i);
int atoUINT64(const char* s, UINT64 * pv);
int atoINT64(const char* s, INT64 *pv);

#ifdef __GNUC__
	#define CONST_9007199254740992 0x20000000000000LL
#else
	#define CONST_9007199254740992 9007199254740992
#endif

#define lua_pushUINT64(L,n)	\
	if( n > CONST_9007199254740992 ){ \
		char buf[24]; \
		if(0 == _ui64toa_s(n, buf, _countof(buf) - 1, 10)) \
			lua_pushstring(L, buf); \
		else \
			lua_dllerror(L, ERROR_INVALID_DATA); \
	}else{ \
		lua_pushnumber(L, (lua_Number)(__int64)n); \
	}

#define lua_pushINT64(L,n)	\
	if(n > 9007199254740992 || n < -9007199254740992){ \
		char buf[24]; \
		lua_pushstring(L, _i64toa_s(n, buf, 10)); \
	}else{ \
		lua_pushnumber(L, (lua_Number)n); \
	}

#ifdef  __cplusplus
}
#endif
#endif //__LUA_INT64_H___
