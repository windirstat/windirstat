#include "luawin_dllerror.h"
#include "stdmacro.h"
#include "luamacro.h"
#include "win_trace.h"
#define lgk_dllerror	"__dll_error"

#ifndef NDEBUG
void lua_dllerror(lua_State *L, DWORD dwErr, const char * exp, const char * file, int line){
#else
void lua_dllerror(lua_State *L, DWORD dwErr){
#endif
/*
if lgk_dllerror is present in global env and equal to false dont raise error!
*/
	lua_getglobal(L, lgk_dllerror);

	if(lua_isrealfalse(L, -1)){
		WIN_TRACEA("lua_dllerror:false");
	}else{
		CHAR chbuf[1024];
		int c;
		if(dwErr == 0)dwErr = GetLastError();
		c = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dwErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), chbuf, 1024, NULL);
		// remove trailing spaces
		if (c > 0 && c < 1024) {
			while(--c > 1 && ISSPACE(chbuf[c]))chbuf[c] = '\0';
		}
		lua_pushfstring(L, "%s\ncode: %d\n", chbuf, dwErr);
#ifndef NDEBUG
		if(file){
			lua_pushfstring(L, "file: %s\nline: %d\n", file, line);
			lua_concat(L, 2);
		}
		if(exp){
			lua_pushfstring(L, "expr: %s\n", exp);
			lua_concat(L, 2);
		}
#endif
		if(lua_isfunction(L, -2)){
			WIN_TRACEA("lua_dllerror:function");
			lua_call(L, 1, 0);
		}else{
			WIN_TRACEA("lua_dllerror:true");
			lua_error(L);
		}
	}
}
