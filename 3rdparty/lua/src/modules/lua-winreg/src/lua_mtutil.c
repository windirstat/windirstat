#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "l52util.h"

/* push metatable to stack, create if not yet reg, the metatable of the table is its self */
int lua_opentablemt(lua_State *L, const char * libname, const luaL_Reg * reg, int upval){
	/*STACK:...<upvals>#*/
	if(luaL_newmetatable(L, libname)){
	/*STACK:...<upvals><tablemt>#*/
		luaL_setfuncs(L, reg, upval);
		lua_pushliteral(L, "__index");
	/*STACK:...<tablemt><"__index">#*/
		lua_pushvalue(L, -2);
	/*STACK:...<tablemt><"__index"><tablemt>#*/
		lua_rawset(L, -3);
	/*STACK:...<tablemt>*/
		return 1;//created
	}
	return 0;//opened, all ready created
}

/* new user data with metamethods table */
void * lua_newuserdatamt(lua_State *L, size_t cdata, const char * mtname, const luaL_Reg * mtreg){
	void* pdata = lua_newuserdata(L, cdata);/*STACK:...<udata># */
	lua_opentablemt(L, mtname, mtreg, 0);	/*STACK:...<udata><tablemt># */
	lua_setmetatable(L, -2);				/*STACK:...<udata># */
	return pdata;
}

/* new user data with metamethods table and upvalues */
void * lua_newuserdatamtuv(lua_State *L, size_t cdata, const char * mtname, const luaL_Reg * mtreg, int upval){
	void* pdata;
	/*STACK:...<upvals>#*/
	lua_opentablemt(L, mtname, mtreg, upval);
	/*STACK:...<tablemt>#*/
	pdata = lua_newuserdata(L, cdata);
	/*STACK:...<tablemt><udata>#*/
	lua_insert(L,-2);
	/*STACK:...<udata><tablemt>#*/
	lua_setmetatable(L, -2);
	/*STACK:...<udata>#*/
	return pdata;
}