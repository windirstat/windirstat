#ifndef __LUAMACWIN_H__
#define __LUAMACWIN_H__
/* Lua useful macros for Windows */

#define lua_toLONG(L,i)		  ((LONG)lua_tointeger(L,(i)))
#define lua_toDWORD(L,i)	  ((DWORD)lua_tonumber(L,(i)))
#define lua_toWORD(L,i)		  ((WORD)lua_tointeger(L,(i)))
#define lua_checkLONG(L,i)    ((LONG)luaL_checkinteger(L,(i)))
#define lua_checkDWORD(L,i)   ((DWORD)luaL_checknumber(L,(i)))
#define lua_checkUINT(L,i)   ((UINT)luaL_checknumber(L,(i)))
#define lua_checkWORD(L,i)	  ((WORD)luaL_checkinteger(L,(i)))
#define lua_checkSHORT(L,i)   ((SHORT)luaL_checkinteger(L,(i)))
#define lua_optUINT(L,i,d)    ((UINT)luaL_optnumber(L,(i),(d)))
#define lua_optDWORD(L,i,d)   ((DWORD)luaL_optnumber(L,(i),(d)))
#define lua_optSHORT(L,i,d)	  ((SHORT)luaL_optinteger(L,(i),(d)))
#define lua_optWORD(L,i,d)	  ((WORD)luaL_optinteger(L,(i),(d)))

#endif //__LUAMACWIN_H__


