/*
** $Id$
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/

#ifndef lapi_h
#define lapi_h


#include "lobject.h"


LUAI_FUNC void luaA_pushobject (lua_State *L, const TValue *o);

#ifdef LUA_TINT
  int lua_pushvalue_as_number (lua_State *L, int idx);
#endif

#endif
