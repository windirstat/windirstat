#ifndef __LUA_MTUTIL_H___
#define __LUA_MTUTIL_H___
#ifdef  __cplusplus
extern "C" {
#endif
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

int lua_opentablemt(lua_State *L, const char * libname, const luaL_Reg * reg);
void * lua_newuserdatamt(lua_State *L, size_t cdata, const char * mtname, const luaL_Reg * mtreg);
void * lua_newuserdatamtuv(lua_State *L, size_t cdata, const char * mtname, const luaL_Reg * mtreg, int upval);
#ifdef  __cplusplus
}
#endif
#endif //__LUA_MTUTIL_H___
