#include <windows.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "luamacro.h"
#include "stdmacro.h"

// from NSIS source code

int atoINT64(const char* s, INT64 *pv){
	*pv = 0;
	if (*s == '0' && (s[1] == 'x' || s[1] == 'X') && ISXDIGIT(s[2])){
		s++;
		for(;;){
			int c = *(++s);
			if (c >= '0' && c <= '9') c -= '0';
			else if (c >= 'a' && c <= 'f') c -= 'a' - 10;
			else if (c >= 'A' && c <= 'F') c -= 'A' - 10;
			else if (c == 0) break;
			else return 0;
			*pv <<= 4;
			*pv += c;
		}
	}else if (*s == '0' && ISODIGIT(s[1])){
		for(;;){
			int c = *(++s);
			if (c >= '0' && c <= '7')c -= '0';
			else if (c == 0) break;
			else return 0;
			*pv <<= 3;
			*pv += c;
		}
	}else if ((*s == '-' && ISDIGIT(s[1])) || ISDIGIT(s[0])){
		int sign = 0;
		if (*s == '-') sign++; else s--;
		for (;;){
			int c = *(++s);
			if (c >= '0' && c <= '9')c -= '0';
			else if (c == 0) break;
			else return 0;
			*pv *= 10;
			*pv += c;
		}
		if (sign) *pv = -*pv;
	}else{
		return 0;
	}
	return 1;
}

int atoUINT64(const char* s, UINT64 * pv){
	*pv = 0;
	if (*s == '0' && (s[1] == 'x' || s[1] == 'X') && ISXDIGIT(s[2])){
		s++;
		for(;;){
			int c = *(++s);
			if (c >= '0' && c <= '9') c -= '0';
			else if (c >= 'a' && c <= 'f') c -= 'a' - 10;
			else if (c >= 'A' && c <= 'F') c -= 'A' - 10;
			else if (c == 0) break;
			else return 0;
			*pv <<= 4;
			*pv += c;
		}
	}else if (*s == '0' && ISODIGIT(s[1])){
		for(;;){
			int c = *(++s);
			if (c >= '0' && c <= '7')c -= '0';
			else if (c == 0) break;
			else return 0;
			*pv <<= 3;
			*pv += c;
		}
	}else if ((*s == '-' && ISDIGIT(s[1])) || ISDIGIT(s[0])){
		if(*s == '-'){
			INT64 iv;
			if(atoINT64(s, &iv)){
				*pv = (UINT64)iv;
			}else{
                return 0;
			}
		}else{
			for (;;){
				int c = *(++s);
				if (c >= '0' && c <= '9')c -= '0';
				else if (c == 0) break;
				else return 0;
				*pv *= 10;
				*pv += c;
			}
		}
	}else{
		return 0;
	}
	return 1;
}

INT64 lua_checkINT64(lua_State *L, int i){
	INT64 v;
	if(lua_isrealstring(L, i) && atoINT64(lua_tostring(L, i), &v)){
		return v;
	}else{
		return (INT64)luaL_checknumber(L, i);
	}
}

UINT64 lua_checkUINT64(lua_State *L, int i){
	UINT64 v;
	if(lua_isrealstring(L, i) && atoUINT64(lua_tostring(L, i), &v)){
		return v;
	}else{
		return (UINT64)luaL_checknumber(L, i);
	}
}