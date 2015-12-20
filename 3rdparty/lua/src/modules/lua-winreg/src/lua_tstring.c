#include <limits.h>
#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "luamacro.h"

size_t lua_utf8towcsZ(lua_State *L, const char *s, int len){
	wchar_t UNCH = 0;
    size_t cchWC = 0;                 // # of wchar_t code points generated
    const unsigned char * pUTF8 = (const unsigned char *)s;
    const unsigned char * pEnd = (const unsigned char *)(s + len);
	luaL_Buffer b;
	luaL_buffinit(L, &b);
    while (pUTF8 < pEnd){
        //  See if there are any trail bytes.
        if (*pUTF8 < 0xC0) {// 192
            //  Found ASCII.
			UNCH = pUTF8[0];
        }else if (pUTF8[0] < 0xE0){ //224
			if ((pUTF8[1] & 0xC0) == 0x80) {
				/* A two-byte-character lead-byte not followed by trail-byte represents itself.*/
				UNCH = (wchar_t) (((pUTF8[0] & 0x1F) << 6) | (pUTF8[1] & 0x3F));
				pUTF8 += 1;
			}else{
				/* A two-byte-character lead-byte not followed by trail-byte represents itself.	 */
				UNCH = pUTF8[0];
			}
		}else if (pUTF8[0] < 0xF0) {//240
			if (((pUTF8[1] & 0xC0) == 0x80) && ((pUTF8[2] & 0xC0) == 0x80)) {
				/* Three-byte-character lead byte followed by two trail bytes.*/
				UNCH = (wchar_t) (((pUTF8[0] & 0x0F) << 12) | ((pUTF8[1] & 0x3F) << 6) | (pUTF8[2] & 0x3F));
				pUTF8 += 2;
			}else{
				/* Three-byte-character lead byte followed by two trail bytes.*/
				UNCH = pUTF8[0];
			}
		}else{
			UNCH = pUTF8[0];
		}
		luaL_addlstring(&b, (const char*)&UNCH, sizeof(wchar_t));
        cchWC++;
        pUTF8++;
    }
	luaL_addchar(&b, '\0');// the other '\0' will be added by lua
	luaL_pushresult(&b);
    //  Return the number of wchar_t characters written.
    return (cchWC);
}

size_t lua_utf8towcs(lua_State *L, const char *s, int len){
	wchar_t UNCH = 0;
    size_t cchWC = 0;                 // # of wchar_t code points generated
    const unsigned char * pUTF8 = (const unsigned char *)s;
    const unsigned char * pEnd = (const unsigned char *)(s + len);
	luaL_Buffer b;
	luaL_buffinit(L, &b);
    while (pUTF8 < pEnd){
        //  See if there are any trail bytes.
        if (*pUTF8 < 0xC0) {// 192
            //  Found ASCII.
			UNCH = pUTF8[0];
        }else if (pUTF8[0] < 0xE0){ //224
			if ((pUTF8[1] & 0xC0) == 0x80) {
				/* A two-byte-character lead-byte not followed by trail-byte represents itself.*/
				UNCH = (wchar_t) (((pUTF8[0] & 0x1F) << 6) | (pUTF8[1] & 0x3F));
				pUTF8 += 1;
			}else{
				/* A two-byte-character lead-byte not followed by trail-byte represents itself.	 */
				UNCH = pUTF8[0];
			}
		}else if (pUTF8[0] < 0xF0) {//240
			if (((pUTF8[1] & 0xC0) == 0x80) && ((pUTF8[2] & 0xC0) == 0x80)) {
				/* Three-byte-character lead byte followed by two trail bytes.*/
				UNCH = (wchar_t) (((pUTF8[0] & 0x0F) << 12) | ((pUTF8[1] & 0x3F) << 6) | (pUTF8[2] & 0x3F));
				pUTF8 += 2;
			}else{
				/* Three-byte-character lead byte followed by two trail bytes.*/
				UNCH = pUTF8[0];
			}
		}else{
			UNCH = pUTF8[0];
		}
		luaL_addlstring(&b, (const char*)&UNCH, sizeof(wchar_t));
        cchWC++;
        pUTF8++;
    }
	luaL_pushresult(&b);
    //  Return the number of wchar_t characters written.
    return (cchWC);
}

//  Constant Declarations.
#define ASCII             0x007f
#define UTF8_2_MAX        0x07ff  // max UTF8 2-byte sequence (32 * 64 = 2048)
#define UTF8_1ST_OF_2     0xc0    // 110x xxxx
#define UTF8_1ST_OF_3     0xe0    // 1110 xxxx
#define UTF8_TRAIL        0x80    // 10xx xxxx
#define HIGER_6_BIT(u)    ((u) >> 12)
#define MIDDLE_6_BIT(u)   (((u) & 0x0fc0) >> 6)
#define LOWER_6_BIT(u)    ((u) & 0x003f)

#ifndef HIBYTE
#define BYTE unsigned char
#define HIBYTE(w)   ((BYTE) (((wchar_t) (w) >> 8) & 0xFF))  
#define LOBYTE(w)   ((BYTE) (w))  
#endif

//  Maps a wchar_t character string to its UTF-8 string counterpart.
size_t lua_wcstoutf8(lua_State *L, const wchar_t *s, size_t cchSrc){
	const wchar_t * lpWC = s;
    size_t cchU8 = 0;                // # of UTF8 chars generated

	luaL_Buffer b;
	luaL_buffinit(L, &b);

    while (cchSrc--)   {
        if (*lpWC <= ASCII){
            //  Found ASCII.
			luaL_addchar(&b, (char)*lpWC);
            cchU8++;
		}else if (*lpWC <= UTF8_2_MAX){
            //  Found 2 byte sequence if < 0x07ff (11 bits).
            //  Use upper 5 bits in first byte.
            //  Use lower 6 bits in second byte.
			luaL_addchar(&b, (char)(UTF8_1ST_OF_2 | (*lpWC >> 6)));
			luaL_addchar(&b, (char)(UTF8_TRAIL    | LOWER_6_BIT(*lpWC)));
			cchU8 += 2;
        }else{
            //  Found 3 byte sequence.
            //  Use upper  4 bits in first byte.
            //  Use middle 6 bits in second byte.
            //  Use lower  6 bits in third byte.
			luaL_addchar(&b, (char)(UTF8_1ST_OF_3 | (*lpWC >> 12)));
			luaL_addchar(&b, (char)(UTF8_TRAIL    | MIDDLE_6_BIT(*lpWC)));
			luaL_addchar(&b, (char)(UTF8_TRAIL    | LOWER_6_BIT(*lpWC)));
			cchU8 += 3;
        }
        lpWC++;
    }
	luaL_pushresult(&b);
    //  Return the number of UTF-8 characters written.
    return (cchU8);
}

size_t lua_chartowcsZ(lua_State *L, const char *s, int len){
	luaL_Buffer b;
    size_t cchWC = 0;
	wchar_t UNCH = 0;
    const unsigned char * pCur = (const unsigned char *)s;
    const unsigned char * pEnd = (const unsigned char *)(s + len);
	luaL_buffinit(L, &b);
    while (pCur < pEnd){
		UNCH = pCur[0];
		luaL_addlstring(&b, (const char*)&UNCH, sizeof(wchar_t));
        cchWC++;
        pCur++;
	}
	luaL_addchar(&b, '\0');// the other '\0' will be added by lua
	luaL_pushresult(&b);
    //  Return the number of wchar_t characters written.
    return (cchWC);
}
int wc2utf8(char *d, wchar_t ch){
	if (ch < 0x80) {
		*d++ = (char)ch;
		return 1;
	}
	if (ch < 0x800) {
		*d++ = (char)(( ch >>  6)         | 0xc0);
		*d++ = (char)(( ch        & 0x3f) | 0x80);
		return 2;
	}
	{
		*d++ = (char)(( ch >> 12)         | 0xe0);
		*d++ = (char)(((ch >>  6) & 0x3f) | 0x80);
		*d++ = (char)(( ch        & 0x3f) | 0x80);
		return 3;
	}
}
// push a wchar_t character value convert equiv utf8 chars
void lua_pushutf8_from_wchar(lua_State *L, wchar_t ch){
	char buf[4] = {0,0,0,0};
	lua_pushlstring(L, buf, wc2utf8(buf, ch));
}
// add a wchar_t character to buffer converted to equiv utf8 chars
void lua_addutf8_from_wchar(luaL_Buffer * pB, wchar_t ch){
	luaL_addsize(pB, wc2utf8(luaL_prepbuffer (pB), ch));
}
// gets an int character value, if string is >= 2 chars checks if utf
wchar_t lua_checkwchar_from_utf8(lua_State *L, int i){
	const unsigned char* psz = (const unsigned char *)luaL_checkstring(L, i);
    if ((psz[0] && psz[1] == 0) || psz[0] == 0) {
		return psz[0];// single character
    }else if ((psz[0] >= 0xC0) && (psz[0] < 0xE0)
		&&	(psz[1] & 0xC0) == 0x80
		&&	(psz[2] == 0)// Two-byte-character lead-byte followed by a trail-byte.
	){	return  (wchar_t)(((psz[0] & 0x1F) << 6) | (psz[1] & 0x3F));
	}else if ((psz[0] >= 0xE0) && (psz[0] < 0xF0)
		&&	((psz[1] & 0xC0) == 0x80)
		&&	((psz[2] & 0xC0) == 0x80)
		&&	(psz[3] == 0)// Three-utf-character lead utf followed by two trail bytes.
	){	return (wchar_t)(((psz[0] & 0x0F) << 12) | ((psz[1] & 0x3F) << 6) | (psz[2] & 0x3F));
	}else{
		luaL_argerror(L, i, "character expected");
	}
	return 0;
}
// lua utf8 string to wide string
const wchar_t *lua_tolwcs_from_utf8(lua_State *L, int narg, size_t* l){
	size_t ulen = 0;
	const char * psz;
	narg = lua_absindex(L, narg);
#if LUA_VERSION_NUM >= 501
	psz = lua_tolstring(L, narg, &ulen);
#else
	psz = lua_tostring(L, narg);
	ulen = lua_strlen(L, narg);
#endif
	if(psz){
		ulen = (size_t)lua_utf8towcsZ(L, psz, (int)ulen);
		if(l)*l = ulen;
		lua_replace (L, narg);
		return (const wchar_t *)lua_tostring(L, narg);
	}else{
		return NULL;
	}
}
// lua utf8 string to wide string, with len
const wchar_t *lua_checklwcs_from_utf8(lua_State *L, int narg, size_t* l){
	size_t ulen = 0;
	const char * psz;
	narg = lua_absindex(L, narg);
	psz = luaL_checklstring(L, narg, &ulen);
	ulen = (size_t)lua_utf8towcsZ(L, psz, (int)ulen);
	if(l)*l = ulen;
	lua_replace (L, narg);
	return (const wchar_t *)lua_tostring(L, narg);
}
// lua utf8 string to wide string, with len, optional
const wchar_t *lua_optlwcs_from_utf8(lua_State *L, int narg, const wchar_t *def, size_t *len){
	if (lua_isnoneornil(L, narg)) {
		if (len)
			*len = (def ? wcslen(def) : 0);
		return def;
	}
	else return lua_checklwcs_from_utf8(L, narg, len);
}
// lua push wide string, convert to utf8
void lua_pushutf8_from_wcs (lua_State *L, const wchar_t *s) {
  if (s == NULL)
    lua_pushnil(L);
  else
    lua_wcstoutf8(L, s, wcslen(s));
}


size_t lua_cstowcsZ(lua_State *L, const char *s, int len){
	wchar_t UNCH = 0;
    size_t cchWC = 0;                 // # of wchar_t code points generated
    const unsigned char * pStr = (const unsigned char *)s;
    const unsigned char * pEnd = (const unsigned char *)(s + len);
	luaL_Buffer b;
	luaL_buffinit(L, &b);

    while (pStr < pEnd){
		UNCH = *pStr;
		luaL_addlstring(&b, (const char*)&UNCH, sizeof(wchar_t));
        cchWC++;
        pStr++;
    }
	luaL_addchar(&b, '\0');// the other '\0' will be added by lua
	luaL_pushresult(&b);
    //  Return the number of wchar_t characters written.
    return (cchWC);
}
// Lua char string to wide string, with len
const wchar_t *lua_checklwcs_from_char(lua_State *L, int narg, size_t* l){
	size_t ulen = 0;
	const char * psz;
	narg = lua_absindex(L, narg);
	psz = luaL_checklstring(L, narg, &ulen);
	ulen = (size_t)lua_cstowcsZ(L, psz, (int)ulen);
	if(l)*l = ulen;
	lua_replace (L, narg);
	return (const wchar_t *)lua_tostring(L, narg);
}
// Lua char string to wide string, with len, optional
const wchar_t *lua_optlwcs_from_char (lua_State *L, int narg, const wchar_t *def, size_t *len){
	if (lua_isnoneornil(L, narg)) {
		if (len)
			*len = (def ? wcslen(def) : 0);
		return def;
	}
	else return lua_checklwcs_from_char(L, narg, len);
}
//  Maps a wchar_t character string to its char string counterpart.
size_t lua_wcstochar(lua_State *L, const wchar_t *s, size_t cchSrc){
	const wchar_t * lpWC = s;
    size_t cch = 0;                // # of UTF8 chars generated

	luaL_Buffer b;
	luaL_buffinit(L, &b);

    while (cchSrc--)   {
        if (*lpWC <= 0x00ff){
			luaL_addchar(&b, (char)*lpWC);
		}else{
			luaL_addchar(&b, '?');// no choice
        }
		cch++;
        lpWC++;
    }
	luaL_pushresult(&b);
    //  Return the number of char characters written.
    return (cch);
}







/*
size_t lua_utf8towcsZ2(lua_State *L, const char *s, int len){
    size_t cchWC = 0;                 // # of wchar_t code points generated
    const unsigned char * pUTF8 = (const unsigned char *)s;
    const unsigned char * pEnd = (const unsigned char *)(s + len);
    wchar_t * pBuf = NULL;
	const wchar_t * pBuf0 = NULL;
	const wchar_t * pBuf1 = NULL;
	luaL_Buffer b;
	luaL_buffinit(L, &b);
    while (pUTF8 < pEnd){
		pBuf = (wchar_t *)luaL_prepbuffer(&b);
		pBuf0 = pBuf;
		pBuf1 = pBuf + (LUAL_BUFFERSIZE-sizeof(wchar_t));
		while(pUTF8 < pEnd && pBuf <= pBuf1){
			//  See if there are any trail bytes.
			if (*pUTF8 < 0xC0) {// 192
				//  Found ASCII.
				*pBuf++ = pUTF8[0];
			}else if (pUTF8[0] < 0xE0){ //224
				if ((pUTF8[1] & 0xC0) == 0x80) {
					// A two-byte-character lead-byte not followed by trail-byte represents itself.
					*pBuf++ = (wchar_t) (((pUTF8[0] & 0x1F) << 6) | (pUTF8[1] & 0x3F));
					pUTF8 += 1;
				}else{
					// A two-byte-character lead-byte not followed by trail-byte represents itself.
					*pBuf++ = pUTF8[0];
				}
			}else if (pUTF8[0] < 0xF0) {//240
				if (((pUTF8[1] & 0xC0) == 0x80) && ((pUTF8[2] & 0xC0) == 0x80)) {
					// Three-byte-character lead byte followed by two trail bytes.
					*pBuf++ = (wchar_t) (((pUTF8[0] & 0x0F) << 12) | ((pUTF8[1] & 0x3F) << 6) | (pUTF8[2] & 0x3F));
					pUTF8 += 2;
				}else{
					// Three-byte-character lead byte followed by two trail bytes.
					*pBuf++ = pUTF8[0];
				}
			}else{
				*pBuf++ = pUTF8[0];
			}
			cchWC++;
			pUTF8++;
		}
		luaL_addsize(&b, (size_t)(pBuf0 - pBuf));
    }
	luaL_addchar(&b, '\0');// the other '\0' will be added by lua
	luaL_pushresult(&b);
    //  Return the number of wchar_t characters written.
    return (cchWC);
}//*/