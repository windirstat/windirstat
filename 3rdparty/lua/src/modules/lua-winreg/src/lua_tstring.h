#ifndef __LUA_TSTRING_H___
#define __LUA_TSTRING_H___
#ifdef  __cplusplus
extern "C" {
#endif
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

size_t lua_utf8towcsZ(lua_State *L, const char *s, int len);
size_t lua_utf8towcs(lua_State *L, const char *s, int len);

size_t lua_wcstoutf8(lua_State *L, const wchar_t *s, size_t cchSrc);
size_t lua_wcstochar(lua_State *L, const wchar_t *s, size_t cchSrc);

void lua_pushutf8_from_wchar(lua_State *L, wchar_t ch);
void lua_addutf8_from_wchar(luaL_Buffer * pB, wchar_t ch);
wchar_t lua_checkwchar_from_utf8(lua_State *L, int i);
const wchar_t *lua_tolwcs_from_utf8(lua_State *L, int narg, size_t* l);
const wchar_t *lua_checklwcs_from_utf8(lua_State *L, int narg, size_t* l);
const wchar_t *lua_optlwcs_from_utf8(lua_State *L, int narg, const wchar_t *def, size_t *len);
void lua_pushutf8_from_wcs(lua_State *L, const wchar_t *s);
#define lua_checkwcs_from_utf8(L,n)	(lua_checklwcs_from_utf8(L, (n), NULL))
#define lua_optwcs_from_utf8(L,n,d)	(lua_optlwcs_from_utf8(L, (n), (d), NULL))
#define lua_towcs_from_utf8(L,i)	(lua_tolwcs_from_utf8(L, (i), NULL))
#define lua_pushlutf8_from_wcs		lua_wcstoutf8

const wchar_t *lua_checklwcs_from_char(lua_State *L, int narg, size_t* l);
const wchar_t *lua_optlwcs_from_char(lua_State *L, int narg, const wchar_t *def, size_t *len);
#define lua_checkwcs_from_char(L,n)	(lua_checklwcs_from_char(L, (n), NULL))
#define lua_optwcs_from_char(L,n,d)	(lua_optlwcs_from_char(L, (n), (d), NULL))
#define lua_pushchar_from_wchar(L,c){char _c;if(c>0x00FF){_c='?';}else{_c=(char)c;}lua_pushlstring(L,&_c,1);}
#define lua_addchar_from_wchar(B,c)	luaL_addchar(B,c>0x00ff?'?':c)
#define lua_pushlwcs_from_char		lua_wcstochar

#ifdef UNICODE
/* all lua*wstring function will use utf8 <--> wide convertion */
#define luaL_checklwstring		lua_checklwcs_from_utf8
#define luaL_checkwstring		lua_checkwcs_from_utf8
#define luaL_optlwstring		lua_optlwcs_from_utf8
#define luaL_optwstring			lua_optwcs_from_utf8

#define lua_checklwstring		lua_checklwcs_from_utf8
#define lua_checkwstring		lua_checkwcs_from_utf8
#define lua_optlwstring			lua_optlwcs_from_utf8
#define lua_optwstring			lua_optwcs_from_utf8

#define lua_tolwstring			lua_tolwcs_from_utf8
#define lua_towstring			lua_towcs_from_utf8
#define lua_pushlwstring		lua_pushlutf8_from_wcs
#define lua_pushwstring			lua_pushutf8_from_wcs
#define lua_pushwchar			lua_pushutf8_from_wchar
#define lua_addwchar			lua_addutf8_from_wchar
#else
/* all lua*wstring function will use char <--> wide convertion */
#define lua_checkwstring		lua_checkwcs_from_char
#define lua_optlwstring			lua_optlwcs_from_char
#define lua_optwstring			lua_optwcs_from_char
#define lua_pushwchar			lua_pushchar_from_wchar
#define lua_addwchar			lua_addchar_from_wchar
#define lua_pushlwstring		lua_pushlwcs_from_char
#endif // !UNICODE

#ifdef UNICODE
#	define lua_checkltstring	 luaL_checklwstring
#	define lua_checktstring(L,n) (luaL_checklwstring(L, (n), NULL))
#	define lua_optltstring		 luaL_optlwstring
#	define lua_opttstring(L,n,d) (luaL_optlwstring(L, (n), (d), NULL))
#	define lua_pushtstring		 lua_pushwstring
#	define lua_totstring		 lua_towstring
#	define lua_pushtchar		 lua_pushwchar
#	define lua_addtchar			 lua_addwchar
#	define LUA_TCHAR			 wchar_t
#	define lua_pushltstring		 lua_pushlwstring
#else
#	define lua_checkltstring	 luaL_checklstring
#	define lua_checktstring		 luaL_checkstring
#	define lua_optltstring		 luaL_optlstring
#	define lua_opttstring		 luaL_optstring
#	define lua_pushtstring		 lua_pushstring
#	define lua_totstring		 lua_tostring
#	define LUA_TCHAR			 char
#	define lua_pushtchar		 lua_pushchar_from_wchar
#	define lua_addtchar			 luaL_addchar
#	define lua_pushltstring		 lua_pushlstring
#endif // !UNICODE

#define lua_alloc_tchar(L, charlen) ((LUA_TCHAR*)lua_newuserdata(L,(charlen)*sizeof(LUA_TCHAR)))
#define lua_rawset_lt(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushliteral(L, k), lua_pushtstring(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_st(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushstring(L,(k)), lua_pushtstring(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_tt(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushtstring(L,(k)), lua_pushtstring(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_vt(L,t,k,v)  (lua_rawset_index_assert(t), lua_pushvalue(L, (k)), lua_pushtstring(L, (v)), lua_rawset(L, (t)))
#define lua_rawset_it(L,t,i,v)  (assert((t) >= 0 || (t) == -2 || (t) <= LUA_REGISTRYINDEX), lua_pushtstring(L, (v)), lua_rawseti(L, (t), (i)))

#ifdef  __cplusplus
}
#endif
#endif //__LUA_TSTRING_H___