#ifndef __LUA_REG__
#define __LUA_REG__
#ifdef  __cplusplus
extern "C" {
#endif

int reglib_createkey(lua_State *L);
int reglib_openkey(lua_State *L);
int reg_close(lua_State *L);
int reg_createkey(lua_State *L);
int reg_deletekey(lua_State *L);
int reg_deletevalue(lua_State *L);
int reg_enumkey(lua_State *L);
int reg_enumvalue(lua_State *L);
int reg_flushkey(lua_State *L);
int reg_getinfo(lua_State *L);
int reg_getvalue(lua_State *L);
int reg_loadkey(lua_State *L);
int reg_openkey(lua_State *L);
int reg_replacekey(lua_State *L);
int reg_restorekey(lua_State *L);
int reg_savekey(lua_State *L);
int reg_setvalue(lua_State *L);
int reg_unloadkey(lua_State *L);
int reg_handle(lua_State *L);
int reg_detach(lua_State *L);
int reg_getstrval(lua_State *L);
int reg_getvaltype(lua_State *L);
/* Total:22 */

extern luaL_Reg lreg_regobj[];
#ifdef LUA_REG_DEFINE_EXTERNS
luaL_Reg lreg_regobj[] = {
{"__gc",reg_close},
{"close",reg_close},
{"createkey",reg_createkey},
{"deletekey",reg_deletekey},
{"deletevalue",reg_deletevalue},
{"enumkey",reg_enumkey},
{"enumvalue",reg_enumvalue},
{"flushkey",reg_flushkey},
{"getinfo",reg_getinfo},
{"getvalue",reg_getvalue},
{"load",reg_loadkey},
{"openkey",reg_openkey},
{"replace",reg_replacekey},
{"restore",reg_restorekey},
{"save",reg_savekey},
{"setvalue",reg_setvalue},
{"unload",reg_unloadkey},
{"handle",reg_handle},
{"detach",reg_detach},
{"getstrval",reg_getstrval},
{"getvaltype",reg_getvaltype},
{0,0}};/* Total:21 */
#endif

extern luaL_Reg lreg_reglib[];
#ifdef LUA_REG_DEFINE_EXTERNS
luaL_Reg lreg_reglib[] = {
{"createkey",reglib_createkey},
{"openkey",reglib_openkey},
{0,0}};/* Total:2 */
#endif

#ifdef  __cplusplus
}
#endif
#endif //__LUA_REG__
