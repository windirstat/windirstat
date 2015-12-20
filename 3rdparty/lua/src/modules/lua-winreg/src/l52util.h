#ifndef _LZUTILS_H_
#define _LZUTILS_H_

#include "lua.h"
#include "lauxlib.h"

#if LUA_VERSION_NUM >= 502 // lua 5.2

// lua_rawgetp
// lua_rawsetp
// luaL_setfuncs
// lua_absindex


#define lua_objlen      lua_rawlen

int   luaL_typerror (lua_State *L, int narg, const char *tname);

void luaL_register (lua_State *L, const char *libname, const luaL_Reg *l);

#else                      // lua 5.1

// functions form lua 5.2

# define lua_absindex(L, i) (((i)>0)?(i):((i)<=LUA_REGISTRYINDEX?(i):(lua_gettop(L)+(i)+1)))
# define lua_rawlen  lua_objlen

void  lua_rawgetp   (lua_State *L, int index, const void *p);
void  lua_rawsetp   (lua_State *L, int index, const void *p);
void  luaL_setfuncs  (lua_State *L, const luaL_Reg *l, int nup);

#endif

int   lutil_newmetatablep (lua_State *L, const void *p);
void  lutil_getmetatablep (lua_State *L, const void *p);
void  lutil_setmetatablep (lua_State *L, const void *p);

#define lutil_newudatap(L, TTYPE, TNAME) (TTYPE *)lutil_newudatap_impl(L, sizeof(TTYPE), TNAME)
int   lutil_isudatap      (lua_State *L, int ud, const void *p);
void *lutil_checkudatap   (lua_State *L, int ud, const void *p);
int   lutil_createmetap   (lua_State *L, const void *p, const luaL_Reg *methods);

void *lutil_newudatap_impl     (lua_State *L, size_t size, const void *p);

#endif

