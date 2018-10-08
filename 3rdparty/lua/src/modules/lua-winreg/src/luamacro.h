#ifndef __LUAMACRO_H__
#define __LUAMACRO_H__
/* Compatibility between Lua 5.1+ and Lua 5.0 */
#ifndef LUA_VERSION_NUM
#	define LUA_VERSION_NUM 0
#endif
#if LUA_VERSION_NUM < 501
#	define luaL_register(a, b, c) luaL_openlib((a), (b), (c), 0)
#	define lua_Integer		ptrdiff_t
#	define lua_pushinteger	lua_pushnumber
#	define luaL_optinteger	luaL_optnumber
#	define luaL_checkinteger luaL_checknumber
#	define luaL_addchar		luaL_putchar
#	define lua_createtable(L,a,b) lua_newtable(L)
#	define lua_tointeger(L,i)	((lua_Integer)lua_tonumber((L),(i)))
#	define lua_recycle(L)	lua_setgcthreshold(L,0)
#else
#	define lua_recycle(L)	lua_gc(L,LUA_GCCOLLECT,0)
#endif
#ifndef lua_boxpointer
#	define lua_boxpointer(L,u) (*(void **)(lua_newuserdata(L, sizeof(void *))) = (u))
#endif
#ifndef lua_unboxpointer
#	define lua_unboxpointer(L,i)	(*(void **)(lua_touserdata(L, i)))
#endif
/* convert a stack index to positive */
#define lua_absindex(L, i)		(((i)>0)?(i):((i)<=LUA_REGISTRYINDEX?(i):(lua_gettop(L)+(i)+1)))




#define lua_leavenil(L)		(lua_settop(L,0),lua_pushnil(L))

#define lua_checkint(L,i)	((int)luaL_checkinteger(L,(i)))
#define lua_optint(L,i,d)	((int)luaL_optinteger(L,(i),(d)))
#define lua_pushint(L, n)	(lua_pushinteger(L, (lua_Integer)(n)))

#define lua_checkstring		luaL_checkstring
#define lua_checklstring	luaL_checklstring

#define lua_isfulluserdata(L, n)(lua_type(L, (n)) == LUA_TUSERDATA)
#define lua_isrealstring(L, n)	(lua_type(L, (n)) == LUA_TSTRING)
#define lua_isrealnumber(L, n)	(lua_type(L, (n)) == LUA_TNUMBER)
#define lua_isrealtrue(L, n)	(lua_isboolean(L, (n)) && lua_toboolean(L, (n)))
#define lua_isrealfalse(L, n)	(lua_isboolean(L, (n)) && (lua_toboolean(L, (n)) == 0))

#define lua_optbool(L,i,d)		(lua_isnone(L,(i))?(d):lua_toboolean(L,(i)))
#define lua_pushfasle(L)		(lua_pushboolean(L, 0))
#define lua_pushtrue(L)			(lua_pushboolean(L, 1))
/* rawset's  shortcuts */
#define lua_rawset_index_assert(t)	assert((t) >= 0 || (t) == -3 || (t) <= LUA_REGISTRYINDEX)
#define lua_rawset_lf(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushliteral(L, k), lua_pushcfunction(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_lb(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushliteral(L, k), lua_pushboolean(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_lv(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushliteral(L, k), lua_pushvalue(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_ll(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushliteral(L, k), lua_pushliteral(L, v), lua_rawset(L, (t)))
#define lua_rawset_ln(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushliteral(L, k), lua_pushnumber(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_li(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushliteral(L, k), lua_pushinteger(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_sn(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushstring(L,(k)), lua_pushnumber(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_vv(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushvalue(L, (k)), lua_pushvalue(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_vi(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushvalue(L, (k)), lua_pushinteger(L, (v)), lua_rawset(L, (t)))
/* rawseti's  shortcuts */
#define lua_rawset_ii(L,t,i,j)  (assert((t) >= 0 || (t) == -2 || (t) <= LUA_REGISTRYINDEX), lua_pushinteger(L, (j)), lua_rawseti(L, (t), (i)))
#define lua_rawset_is(L,t,i,v)  (assert((t) >= 0 || (t) == -2 || (t) <= LUA_REGISTRYINDEX), lua_pushstring(L, (v)), lua_rawseti(L, (t), (i)))
#define lua_rawset_in(L,t,i,j)  (assert((t) >= 0 || (t) == -2 || (t) <= LUA_REGISTRYINDEX), lua_pushnumber(L, (j)), lua_rawseti(L, (t), (i)))

/* traversing table */
#define lua_tabletraverse(L, i) for(lua_pushnil(L); lua_next(L, (i)); lua_pop(L, 1))

#define LUA_LEAVE_OBJECT(L)			 (lua_settop(L,1))
#define LUA_CHECK_LEAVE_OBJECT(L,b)	 ((b)?LUA_LEAVE_OBJECT(L):(lua_settop(L,0),lua_pushnil(L)))
#define LUA_CHECK_RETURN_OBJECT(L,b) return(LUA_CHECK_LEAVE_OBJECT(L,b),1)
/* allocation charaters */
#define lua_alloc_wchar(L, charlen) ((wchar_t*)lua_newuserdata(L,(charlen)*sizeof(wchar_t)))
#define lua_alloc_char(L, charlen)	((char*)lua_newuserdata(L,(charlen)*sizeof(char)))
#define lua_alloc_uchar(L, charlen) ((unsigned char*)lua_newuserdata(L,(charlen)*sizeof(unsigned char)))
/* luacheia support */
#define LUACHEIA(m) __declspec(dllexport) int luaLM_import(lua_State *L){luaopen_##m(L);return 0;} __declspec(dllexport) const char * luaLM_version(void) {return LUA_VERSION;}
/* gettable shortcuts */
#define lua_gettable_s(L,i,s)	  (lua_pushstring(L,s),lua_gettable(L,i))
#define lua_gettable_l(L,i,l)	  (lua_pushliteral(L,l),lua_gettable(L,i))
#define lua_gettable_i(L,i,j)	  (lua_pushinteger(L,j),lua_gettable(L,i))
#define lua_chktable_s(L,i,s)	  (lua_gettable_s(L,i,s),!lua_isnoneornil(L,-1))
#define lua_chktable_l(L,i,l)	  (lua_gettable_l(L,i,l),!lua_isnoneornil(L,-1))
#define lua_chktable_i(L,i,j)	  (lua_gettable_i(L,i,j),!lua_isnoneornil(L,-1))

/* Garbage !
#define lua_push_lj(L,s)		(lua_pushliteral(L, s), lua_absindex(L, -1))
#define lua_push_fj(L,f)		(lua_pushcfunction(L, f), lua_absindex(L, -1))

#define lua_picktablep(L,i,p)	  (lua_pushlightuserdata(L,(p)),lua_rawget(L,(i)))
#define lua_picktablev(L,i,j)	  (lua_pushvalue(L, (j)), lua_rawget(L,(i)))
#define lua_picktablel(L,i,s)	  (lua_pushliteral(L, s), lua_rawget(L,(i)))
#define lua_picktablei(L,i,n)	  (lua_rawgeti(L,i,n))

#define lua_gettablel(L,i,s)	  (lua_pushliteral(L, s), lua_gettable(L,i))
#define lua_gettablei(L,i,n)	  (lua_pushinteger(L,(n)),lua_gettable(L,i))
#define lua_gettables(L,i,s)	  (lua_getfield(L,(i),(s)))


#define lua_gettable_l(L,i,s)	  (lua_gettablel(L,i,s), !lua_isnoneornil(L,-1))
#define lua_gettable_i(L,i,n)	  (lua_gettablei(L,i,n), !lua_isnoneornil(L,-1))
#define lua_gettable_s(L,i,s)	  (lua_gettables(L,i,s), !lua_isnoneornil(L,-1))

#define lua_checkktable_li(L,i,s) (lua_gettablel(L,i,s), luaL_checkinteger(L, -1))

#define lua_gettable_lb(L,i,s)	  (lua_gettablel(L,i,s), lua_toboolean(L, -1))

#define lua_picktable_l(L,i,s)	  (lua_picktablel(L,i,s), !lua_isnoneornil(L,-1))
#define lua_picktable_v(L,i,j)	  (lua_picktablev(L,i,j), !lua_isnoneornil(L,-1))
#define lua_picktable_i(L,i,n)	  (lua_picktablei(L,i,n), !lua_isnoneornil(L,-1))
#define lua_picktable_p(L,i,p)	  (lua_picktablep(L,i,p), !lua_isnoneornil(L,-1))

#define lua_picktable_lb(L,i,s)	  (lua_picktablel(L,i,s), lua_toboolean(L, -1))
#define lua_picktable_vb(L,i,j)	  (lua_picktablev(L,i,j), lua_toboolean(L, -1))

#define lua_picktabletype_l(L,i,s)(lua_picktablel(L,i,s), lua_type(L, -1))

#define lua_querytable_vs(L,i,j)  (lua_picktablev(L,i,j), luaL_checkstring(L, -1))

#define lua_picktable_vj(L,i,j)	  (lua_picktablev(L,i,j), lua_absindex(L, -1))
#define lua_picktable_lj(L,i,s)	  (lua_picktablel(L,i,s), lua_absindex(L, -1))
*/

#endif /*__LUAMACRO_H__*/