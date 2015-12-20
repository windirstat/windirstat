#ifdef _LUAMSVC
#	include <luamsvc.h>
#endif

#include <assert.h>
#ifndef lua_assert
#   define lua_assert assert
#endif // lua_assert

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "l52util.h"

#include "stdmacro.h"
#include "luamacro.h"
#include "luawinmacro.h"

#include "lua_tstring.h"
#include "lua_int64.h"
#include "lua_mtutil.h"
#include "luawin_dllerror.h"

#include "win_trace.h"
#include "win_privileges.h"
#include "win_registry.h"

#define LUA_REG_DEFINE_EXTERNS
#include "luareg.h"

#define lua_error_invalid_option(L, i) luaL_argerror(L, (i), "invalid option")
#define lrk_hkey	 "{afx/hkey}"

#ifndef HKEY_PERFORMANCE_TEXT
#define HKEY_PERFORMANCE_TEXT       (( HKEY )((LONG)0x80000050) )
#define HKEY_PERFORMANCE_NLSTEXT    (( HKEY )((LONG)0x80000060) )
#endif
#ifndef REG_QWORD
#define REG_QWORD                   ( 11 )  // 64-bit number
#endif


#ifdef LUA_REG_AS_DLL
BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call,  LPVOID lpReserved){
	UNUSED(hModule);
	UNUSED(lpReserved);
	UNUSED(ul_reason_for_call);
	return TRUE;
}

__declspec(dllexport)
#else
LUA_API
#endif // LUA_REG_AS_DLL
int luaopen_winreg(lua_State *L){
	luaL_register(L, "winreg", lreg_reglib);
	return 1;
}
#define reg_aux_gethkey(L,index) (*((PHKEY)luaL_checkudata(L, index, lrk_hkey)))
#define reg_aux_getphkey(L,index) ((PHKEY)luaL_checkudata(L, index, lrk_hkey))

typedef struct HKEY_DATA{
	HKEY hkey;
	DWORD opt;
} HKEY_DATA, *PHKEY_DATA;

int reg_aux_newhkey(lua_State *L, PHKEY pHKey){
	PHKEY phKey = (PHKEY)lua_newuserdatamt(L, sizeof(HKEY), lrk_hkey, lreg_regobj);
	return phKey && pHKey && (NULL != (*phKey = *pHKey));
}

int reg_aux_newkey(lua_State *L,  HKEY HKey,  LPCTSTR szKey, LPCTSTR szClass, REGSAM samDesired, BOOL bCreate){
	DWORD dwDis = (bCreate==FALSE) ? REG_OPENED_EXISTING_KEY : 0;
	HKEY hKey   = NULL;
	LONG ret    = (bCreate
			? RegCreateKeyEx(HKey, szKey, 0, (PTSTR)szClass, 0, samDesired, NULL, &hKey, &dwDis) 
			: RegOpenKeyEx(HKey, szKey, 0, samDesired, &hKey)
			);
	//WIN_TRACET(_T("%hs: hkey<%x>, sub<%s>, cls<%s>, acc<%d>;"), bCreate?"createkey":"openkey", HKey, szKey, szClass, samDesired);
	
	if(ret == ERROR_SUCCESS){
		reg_aux_newhkey(L, &hKey); 
		return dwDis;
	}else{
		// if we're opening a key, dont shout the error
		// just return nil
		// this will test if the key exist
		if(bCreate){
			LUA_CHECK_DLL_ERROR(L,ret);
		}
		lua_pushnil(L);
		return 0;
	}
}

REGSAM reg_aux_getaccess(lua_State *L, int i){
	REGSAM acc = 0;
	if(lua_isnumber(L, i) || lua_isnoneornil(L, i)){
		acc = lua_optint(L, i, KEY_ALL_ACCESS);
	}else{
		const char * psz = lua_checkstring(L, i);
		for(;*psz;psz++){
			switch(*psz){
			case 'w': acc |= KEY_WRITE; break;
			case 'r': acc |= KEY_READ ; break;
			case 'a': acc |= KEY_ALL_ACCESS ; break;
			case 'X': acc |= KEY_WOW64_64KEY ; break;
			case 'x': acc |= KEY_WOW64_32KEY ; break;
			default : lua_error_invalid_option(L, i);
			}
		}
	}
	return acc;
}

typedef struct KVDATA{
	const char * name;
	size_t data;
} KVDATA, *PKVDATA;

HKEY reg_aux_strtohkey(lua_State *L, const char * psz){
	static KVDATA ph[] = {
		{"HKEY_CLASSES_ROOT", (size_t)HKEY_CLASSES_ROOT},
		{"HKEY_CURRENT_USER", (size_t)HKEY_CURRENT_USER},
		{"HKEY_LOCAL_MACHINE", (size_t)HKEY_LOCAL_MACHINE},
		{"HKEY_USERS", (size_t)HKEY_USERS},
		{"HKEY_PERFORMANCE_DATA", (size_t)HKEY_PERFORMANCE_DATA},
		{"HKEY_PERFORMANCE_TEXT", (size_t)HKEY_PERFORMANCE_TEXT},
		{"HKEY_PERFORMANCE_NLSTEXT", (size_t)HKEY_PERFORMANCE_NLSTEXT},
		{"HKEY_CURRENT_CONFIG", (size_t)HKEY_CURRENT_CONFIG},
		{"HKEY_DYN_DATA", (size_t)HKEY_DYN_DATA},
		{"HKCR", (size_t)HKEY_CLASSES_ROOT},
		{"HKCU", (size_t)HKEY_CURRENT_USER},
		{"HKLM", (size_t)HKEY_LOCAL_MACHINE},
		{"HKU", (size_t)HKEY_USERS},
		{"HKPD", (size_t)HKEY_PERFORMANCE_DATA},
		{"HKPT", (size_t)HKEY_PERFORMANCE_TEXT},
		{"HKPN", (size_t)HKEY_PERFORMANCE_NLSTEXT},
		{"HKCC", (size_t)HKEY_CURRENT_CONFIG},
		{"HKDD", (size_t)HKEY_DYN_DATA},
		{0,0}
	};
	PKVDATA pph;
	INT64 x;
	if(atoINT64(psz, &x)){
		WIN_TRACEA("DIGIT ROOTKEY %s", psz);
		return (HKEY)(size_t)x;	        
	}else{
		for(pph = ph; pph->name && _stricmp(psz, pph->name); pph++);
		if(!pph->data)luaL_error(L, "invalid prefix key '%s'", psz);
		return (HKEY)pph->data;
	}
}


void reg_aux_splitkey(lua_State *L, const char * psz){
	const char * szpre;
	// skip space
	while(*psz && ISSPACE(*psz))psz++;
	// remember prefix
	szpre = psz;
	// skip prefix
	while(*psz && *psz != '\\')psz++;
	lua_pushlstring(L, szpre, (int)(psz - szpre)); //@ -2 (rootkey)
	while(*psz && *psz == '\\')psz++;
	lua_pushstring(L, psz); //@ -1 (subkey)
}
//docok
int reglib_createkey(lua_State *L){//reglib.createkey
	//reglib.createkey("<ROOT>\\SUBKEY","access","class")
	HKEY hkey = NULL;
	const char * szPath = lua_checkstring(L, 1);
	REGSAM Access = reg_aux_getaccess(L, 2);
	const TCHAR * pszCls = lua_opttstring(L, 3, NULL);
	const TCHAR * pszSubKey;
	const char * pszRootKey;
	reg_aux_splitkey(L, szPath);
	pszSubKey = lua_totstring(L,-1);
	pszRootKey = lua_tostring(L,-2);
	// get hkey of root-key-string
	hkey = reg_aux_strtohkey(L, pszRootKey);
	reg_aux_newkey(L, hkey, pszSubKey, pszCls, Access, TRUE);
	return 1;
}
//docok
int reglib_openkey(lua_State *L){//reglib.openkey
	HKEY hkey = NULL;
	LONG ret = 0;
	const char * szPath = lua_checkstring(L, 1);
	REGSAM Access = reg_aux_getaccess(L, 2);

	if(ISSLASH(szPath[0]) && ISSLASH(szPath[1])){
		HKEY HKey = NULL;
		const TCHAR * pszMachine;
		const char * pszRootKey;
		// \\computer_name\hkxx
		while(szPath[0] && ISSLASH(szPath[0]) )szPath++;// skip the beginning slashes
		reg_aux_splitkey(L, szPath);
		pszMachine = lua_totstring(L,-1);
		pszRootKey = lua_tostring(L,-2);

		hkey = reg_aux_strtohkey(L, pszRootKey);
		if((ret = RegConnectRegistry(pszMachine, hkey, &HKey)) == ERROR_SUCCESS){
			reg_aux_newhkey(L, &HKey);
		}else{
			lua_pushnil(L);
			LUA_CHECK_DLL_ERROR(L, ret);
		}
	}else{
		const TCHAR * pszSubKey;
		const char * pszRootKey;
		reg_aux_splitkey(L, szPath);
		pszSubKey = lua_totstring(L,-1);
		pszRootKey = lua_tostring(L,-2);
		hkey = reg_aux_strtohkey(L, pszRootKey);
		reg_aux_newkey(L, hkey, pszSubKey, NULL, Access, FALSE);
	}
	return 1;
}
static const KVDATA reg_type_table[] = {
	{"none", REG_NONE},
	{"sz", REG_SZ },
	{"expand_sz", REG_EXPAND_SZ},
	{"binary", REG_BINARY},
	{"dword", REG_DWORD},
	{"dword_big_endian", REG_DWORD_BIG_ENDIAN},
	{"link", REG_LINK},
	{"multi_sz", REG_MULTI_SZ},
	{"resource_list", REG_RESOURCE_LIST},
	{"full_resource_descriptor", REG_FULL_RESOURCE_DESCRIPTOR},
	{"resource_requirements_list", REG_RESOURCE_REQUIREMENTS_LIST},
	{"qword", REG_QWORD},
	{0,0}
};
void reg_aux_pushdatatype(lua_State *L, DWORD dwType){
/*	const KVDATA * pkvd; 
	for(pkvd = &reg_type_table[0]; pkvd->name && pkvd->data != dwType; pkvd++);
	if(pkvd->name){
		lua_pushstring(L, pkvd->name);
	}else*/{
		lua_pushint(L, dwType);
	}
}

int reg_aux_getdatatype(lua_State *L, int i){
	if(lua_isrealstring(L, i)){
		const KVDATA * pkvd;
		const char * psz = lua_tostring(L, i);
		for(pkvd = &reg_type_table[0]; pkvd->name && strcmp(psz, pkvd->name); pkvd++);
		return (pkvd->name)?(int)pkvd->data:-1;
	}else{
		return lua_optint(L, i, -1);
	}
}

BOOL reg_aux_setvalue(lua_State *L, HKEY hKey, const TCHAR * pszVal, int type, int i){
	PBYTE pdata = NULL;
	size_t cdata = 0;
	LONG ret = ERROR_SUCCESS;
	DWORD32 dw32;
	DWORD64 dw64;

	if(type < 0){
		switch(lua_type(L, i)){
		break;case LUA_TTABLE :type = REG_MULTI_SZ;
		break;case LUA_TNUMBER:type = REG_DWORD;
		break;case LUA_TSTRING:type = REG_SZ;
		}
	}
	WIN_TRACEA("reg_aux_setvalue val<%s> type<%d>", pszVal, type);
	switch(type){
	case REG_DWORD:	case REG_DWORD_BIG_ENDIAN:{
		dw32  = lua_checkDWORD(L, i);
		pdata = (PBYTE)&dw32;
		cdata = sizeof(DWORD32);
	}
	break; case REG_QWORD:{
		if(lua_isuserdata(L, i)){
			memcpy(&dw64, lua_touserdata(L, i), sizeof(DWORD64));
		}else{
			dw64  = lua_checkUINT64(L, i);
		}
		pdata = (PBYTE)&dw64;
		cdata = sizeof(DWORD64);
	}
	break; case REG_MULTI_SZ:{
		size_t len = 0;
		luaL_Buffer B;
		luaL_buffinit(L, &B);
		if(lua_istable(L, i)){
			int n;
			int last = (int)lua_objlen(L, i);
			for (n = 1; n <= last; n++) {
				lua_rawgeti(L, i, n);
				luaL_addstring(&B, lua_checkstring(L, -1));
				luaL_addchar(&B, 0);
			}
		}else{
			luaL_checktype(L, i, LUA_TSTRING);
			lua_pushvalue(L, i);
			luaL_addvalue(&B);
			luaL_addchar(&B, 0);
		}
		luaL_addchar(&B, 0);
		luaL_pushresult(&B);
		pdata = (PBYTE)lua_checkltstring(L, -1, &len);
		cdata = len*sizeof(TCHAR);
	}
	break; case REG_SZ: case REG_EXPAND_SZ:{
		pdata = (PBYTE)lua_checkltstring(L, i, &cdata);
		cdata = (cdata+1)*sizeof(TCHAR);
	}
	break; default:
#if LUA_VERSION_NUM >= 501
		if(lua_isfulluserdata(L, i)){
			pdata = (PBYTE)lua_touserdata(L, i);
			cdata = lua_objlen(L, i);
		}else
#endif
		{
			pdata = (PBYTE)luaL_checklstring(L, i, &cdata);
		}
	}
	ret = RegSetValueEx(hKey, pszVal, 0, type, pdata, (DWORD)cdata);
	LUA_CHECK_DLL_ERROR(L, ret);
	return ERROR_SUCCESS == ret;
}
void reg_aux_pusheregstrdata(lua_State *L, PVOID pdata, size_t cdata, DWORD dwType, int expand){
	if(pdata == NULL){lua_pushnil(L);return;}
	switch(dwType){
		case REG_DWORD: case REG_DWORD_BIG_ENDIAN: case REG_QWORD:
		{
			char buf[24];
			DWORD64 n;
			if(dwType == REG_QWORD){
				n = *((PDWORD64)pdata);
			}else{
				n = (DWORD64)(*((PDWORD32)pdata));
			}
			if(0 == _ui64toa_s(n, buf, _countof(buf) - 1, 10))
				lua_pushstring(L, buf);
			else
				LUA_CHECK_DLL_ERROR(L, ERROR_INVALID_DATA);
		}
		break; case REG_MULTI_SZ:
		{
			lua_pushltstring(L, (const TCHAR *)pdata, cdata);
		}
		break; case REG_EXPAND_SZ: case REG_SZ:
		{
			DWORD dwLen;
			PTSTR pbuf;
			if(	dwType == REG_EXPAND_SZ && expand
			&&	(dwLen = ExpandEnvironmentStrings((const TCHAR *)pdata, NULL, 0)) > 0
			&&	NULL != (pbuf = lua_alloc_tchar(L, ++dwLen))
			&&	(dwLen = ExpandEnvironmentStrings((const TCHAR *)pdata, pbuf, dwLen)) > 0){
				lua_pushtstring(L, pbuf);
			}else{
				lua_pushtstring(L, (const TCHAR *)pdata);
			}
		}
		break;default:
			lua_pushlstring(L, (const char *)pdata, cdata);
	}
}

void reg_aux_pusheregluadata(lua_State *L, PVOID pdata, size_t cdata, DWORD dwType){
	if(pdata == NULL){lua_pushnil(L);return;}
	switch(dwType){
		case REG_DWORD: case REG_DWORD_BIG_ENDIAN:
			lua_pushnumber(L, *((PDWORD32)pdata));
		break; case REG_QWORD:
			lua_pushUINT64(L, *((PDWORD64)pdata), "reg_aux_pusheregluadata(REG_QWORD)", __FILE__, __LINE__); 
		break; case REG_MULTI_SZ:
		{
			int c = 1;
			lua_newtable(L);
			DZTS_ITER_INIT(PTSTR, ptoken, (PTSTR)pdata)
				lua_pushinteger(L, c);
				lua_pushtstring(L, ptoken);//TCHAR!!
				lua_rawset(L, -3);
				c++;
			DZTS_ITER_CONT(ptoken)
		}
		break; case REG_SZ: case REG_EXPAND_SZ:
			lua_pushtstring(L, (const TCHAR *)pdata);
		break;default:
			lua_pushlstring(L, (const char *)pdata, cdata);
	}
}
typedef struct _REG_ENUM_TAG {    // rc  
	HKEY hKey; 
	DWORD dwIndex;
	PTSTR buffer;
	DWORD bchlen;
} REG_ENUM_TAG;
#define REG_ENUM_MINBUFLEN	((LUAL_BUFFERSIZE-sizeof(REG_ENUM_TAG))/sizeof(TCHAR))
#define REG_ENUM_DATALEN(n) (sizeof(REG_ENUM_TAG)+((n)*sizeof(TCHAR)))

int reg_aux_enumkey_closure(lua_State *L){
	REG_ENUM_TAG* pret = (REG_ENUM_TAG*)lua_touserdata(L, lua_upvalueindex(1));
	if(pret){
		DWORD dwcbSubKey = pret->bchlen;
		PTSTR pszSubKey = pret->buffer;
		LONG ret = RegEnumKeyEx(pret->hKey, pret->dwIndex, pszSubKey, &dwcbSubKey, 0, NULL, NULL, NULL);

		if(ERROR_SUCCESS == ret	){
			lua_pushtstring(L, pszSubKey);
			pret->dwIndex++;
			return 1;
		}
		if(ret != ERROR_NO_MORE_ITEMS){
			WIN_TRACEA("pret->bchlen=%d,dwcbSubKey=%d,Subkey='%s'", pret->bchlen, dwcbSubKey, pszSubKey);
			LUA_CHECK_DLL_ERROR(L, ret);
		}
	}
	lua_pushnil(L);
	return 1;
}
int reg_aux_enumvalue_closure(lua_State *L){
	REG_ENUM_TAG* pret = (REG_ENUM_TAG*)lua_touserdata(L, lua_upvalueindex(1));
	if(pret){
		LONG ret = 0;
		DWORD dwType = 0;
		DWORD dwccValue = pret->bchlen;
		PTSTR pszValue = pret->buffer;

		ret = RegEnumValue(pret->hKey, pret->dwIndex, pszValue, &dwccValue, 0, &dwType, NULL, NULL);

		if(ERROR_SUCCESS == ret	){
			lua_pushtstring(L, pszValue);
			reg_aux_pushdatatype(L, dwType);
			pret->dwIndex++;
			return 2;
		}
		if(ret != ERROR_NO_MORE_ITEMS){
			LUA_CHECK_DLL_ERROR(L, ret);
		}
	}
	lua_pushnil(L);
	return 1;
}
//docok
int reg_close(lua_State *L){//regobj.__gc, regobj.close
	PHKEY phKey = (PHKEY)lua_touserdata(L, 1);
	lua_assert(phKey);
	//WIN_TRACEA("reg_close<%x>;", *phKey);
	if(phKey && *phKey){
		LUA_CHECK_DLL_ERROR(L, RegCloseKey(*phKey));
		*phKey = NULL;
	}
	return 0;
}
//docok
int reg_createkey(lua_State *L){//regobj.createkey
	reg_aux_newkey(L, reg_aux_gethkey(L, 1),
		lua_checktstring(L, 2), (PTSTR)lua_opttstring(L, 4, NULL), reg_aux_getaccess(L, 3), TRUE);
	return 1;
}
//docok
int reg_deletekey(lua_State *L){//regobj.deletekey
	LONG ret;
    // the key name is optional
	if(reg_aux_gethkey(L,1) && lua_isnoneornil(L, 2))
	{
		lua_pushtstring(L, TEXT(""));
	}
	ret = win_reg_deltree(reg_aux_gethkey(L,1), lua_checktstring(L, 2)); 
	LUA_CHECK_DLL_ERROR(L, ret);
	LUA_CHECK_RETURN_OBJECT(L, ret == ERROR_SUCCESS);
}
//docok
int reg_deletevalue(lua_State *L){//regobj.deletevalue
	LONG ret = RegDeleteValue(reg_aux_gethkey(L,1), lua_checktstring(L, 2)); 
	LUA_CHECK_DLL_ERROR(L, ret);
	LUA_CHECK_RETURN_OBJECT(L, ret == ERROR_SUCCESS);
}
//docok
int reg_enumkey(lua_State *L){//regobj.enumkey
	HKEY hKey = reg_aux_gethkey(L, 1);
	DWORD dwNameLen = 0;
	REG_ENUM_TAG* pret;
	LONG ret = RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, &dwNameLen, NULL, NULL, NULL, NULL, NULL, NULL);
	dwNameLen = max(REG_ENUM_MINBUFLEN,dwNameLen+1);
	pret = (REG_ENUM_TAG*)lua_newuserdata(L,REG_ENUM_DATALEN(dwNameLen));

	pret->hKey = hKey;
	pret->dwIndex = 0;
	pret->bchlen = dwNameLen;
	pret->buffer = (PTSTR)(&pret[1]);

	WIN_TRACEA("reg_enumkey bchlen=%d datlen=%d", pret->bchlen, REG_ENUM_DATALEN(dwNameLen));
	LUA_CHECK_DLL_ERROR(L, ret);
	lua_pushcclosure(L, reg_aux_enumkey_closure, 1);
	return 1;
}
//docok
int reg_enumvalue(lua_State *L){//regobj.enumvalue
	HKEY hKey = reg_aux_gethkey(L, 1);
	REG_ENUM_TAG* pret;
	DWORD dwNameLen = 0;
	LONG ret = RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &dwNameLen, NULL, NULL, NULL);
	dwNameLen = max(REG_ENUM_MINBUFLEN,dwNameLen+1);
	pret = (REG_ENUM_TAG*)lua_newuserdata(L,REG_ENUM_DATALEN(dwNameLen));

	pret->hKey = hKey;
	pret->dwIndex = 0;
	pret->bchlen = dwNameLen;
	pret->buffer = (PTSTR)(&pret[1]);

	WIN_TRACEA("reg_enumkey bchlen=%d datlen=%d", pret->bchlen, REG_ENUM_DATALEN(dwNameLen));
	LUA_CHECK_DLL_ERROR(L, ret);

	lua_pushcclosure(L, reg_aux_enumvalue_closure, 1);
	return 1;
}
//docok
int reg_flushkey(lua_State *L){//"regobj.flushkey"
	LONG ret = RegFlushKey(reg_aux_gethkey(L, 1));
	LUA_CHECK_DLL_ERROR(L, ret);
	LUA_CHECK_RETURN_OBJECT(L, ret == ERROR_SUCCESS); 
}
//docok
int reg_getinfo(lua_State *L){//regobj.getinfo
	HKEY hKey = reg_aux_gethkey(L,1);
	FILETIME ftLastWriteTime = {0,};
	DWORD dwcbClass = 64, dwcSubKeys = 0, dwcbMaxSubKeyLen = 0, dwcbMaxClassLen = 0, dwcValues = 0, dwcbMaxValueNameLen = 0, dwcbMaxValueLen = 0, dwcbSecurityDescriptor = 0;
	LONG ret = RegQueryInfoKey (hKey, NULL, NULL, NULL, &dwcSubKeys, &dwcbMaxSubKeyLen,	&dwcbMaxClassLen, &dwcValues,	&dwcbMaxValueNameLen, &dwcbMaxValueLen, &dwcbSecurityDescriptor, &ftLastWriteTime);
	PTSTR psz = lua_alloc_tchar(L, ++dwcbClass);

	if(ERROR_MORE_DATA == RegQueryInfoKey(hKey, psz, &dwcbClass, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) ){
		psz = lua_alloc_tchar(L, ++dwcbClass);
		RegQueryInfoKey(hKey, psz, &dwcbClass, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	}

	LUA_CHECK_DLL_ERROR(L, ret);
	if(ret == ERROR_SUCCESS){
		lua_newtable(L);
		lua_rawset_st(L, -3, "class",psz);
		lua_rawset_sn(L, -3, "subkeys",dwcSubKeys);
		lua_rawset_sn(L, -3, "values",dwcValues);
		lua_rawset_sn(L, -3, "maxsubkeylen",dwcbMaxSubKeyLen);
		lua_rawset_sn(L, -3, "maxclasslen",dwcbMaxClassLen);
		lua_rawset_sn(L, -3, "maxvaluelen",dwcbMaxValueLen);
		lua_rawset_sn(L, -3, "maxvaluenamelen",dwcbMaxValueNameLen);
		lua_rawset_sn(L, -3, "maxsecuritydescriptor",dwcbSecurityDescriptor);
		//lua_pushstring(L, "lastwritetime");
		//aux_pushftime(L, &ftLastWriteTime);
		//lua_rawset(L, -3);
	}else{
		lua_pushnil(L);
	}
	return 1;
}
//docok
int reg_getvalue(lua_State *L){//regobj.getvalue
	HKEY hKey = reg_aux_gethkey(L, 1);
	const TCHAR * pszVal = lua_opttstring(L, 2, NULL);
	DWORD dwType = 0;
	DWORD dwLen = 0;
	PVOID pvd = 0;
	LONG ret = ERROR_SUCCESS;
	// open the key
	if(	ERROR_SUCCESS == (ret = RegQueryValueEx(hKey, pszVal, NULL, &dwType, NULL, &dwLen))
	// alloc
	&&	(pvd = lua_newuserdata(L, dwLen)) != NULL
	// query a-again
	&&	ERROR_SUCCESS == (ret = RegQueryValueEx(hKey, pszVal, NULL, &dwType, pvd, &dwLen))
	){
		reg_aux_pusheregluadata(L, pvd, dwLen, dwType);
		reg_aux_pushdatatype(L, dwType);
	}else{
		lua_pushnil(L);
		lua_pushnil(L);
		//LUA_CHECK_DLL_ERROR(L, ret);
	}
	return 2;
}
#ifndef LUA_REG_NO_HIVEOPS
//docok
int reg_loadkey(lua_State *L){//regobj.load
	LONG ret;
	win_setprivilege(SE_RESTORE_NAME, 1, NULL);
	ret = RegLoadKey(reg_aux_gethkey(L,1), lua_checktstring(L, 2), lua_checktstring(L, 3));
	LUA_CHECK_DLL_ERROR(L, ret);
	LUA_CHECK_RETURN_OBJECT(L, ret == ERROR_SUCCESS);
}
#endif // LUA_REG_NO_HIVEOPS
//docok
int reg_openkey(lua_State *L){//regobj.openkey
	reg_aux_newkey(L, reg_aux_gethkey(L, 1),
		lua_checktstring(L, 2), NULL, reg_aux_getaccess(L, 3), FALSE);
	return 1;
}
#ifndef LUA_REG_NO_HIVEOPS
//docok
int reg_replacekey(lua_State *L){//regobj.replace
	LONG ret;
	win_setprivilege(SE_RESTORE_NAME, 1, NULL);
	ret = RegReplaceKey(reg_aux_gethkey(L,1), lua_checktstring(L, 2), lua_checktstring(L, 3), lua_checktstring(L, 4));
	LUA_CHECK_DLL_ERROR(L, ret);
	LUA_CHECK_RETURN_OBJECT(L, ret == ERROR_SUCCESS);
}
//docok
int reg_restorekey(lua_State *L){//regobj.restore
	LONG ret;
	DWORD dwFlags = 0;
	win_setprivilege(SE_RESTORE_NAME, 1, NULL);
	if(lua_isboolean(L,3)){
		dwFlags = lua_toboolean(L,3)?REG_WHOLE_HIVE_VOLATILE:0;
	}else{
		dwFlags = lua_optDWORD(L, 3, 0);
	}
	ret = RegRestoreKey(reg_aux_gethkey(L,1), lua_checktstring(L, 2), dwFlags);
	LUA_CHECK_DLL_ERROR(L, ret);
	LUA_CHECK_RETURN_OBJECT(L, ret == ERROR_SUCCESS);
}
//docok
int reg_savekey(lua_State *L){//regobj.save
	LONG ret;
	win_setprivilege(SE_BACKUP_NAME, 1, NULL);
	ret = RegSaveKey(reg_aux_gethkey(L,1), lua_checktstring(L, 2), NULL);
	LUA_CHECK_DLL_ERROR(L, ret);
	LUA_CHECK_RETURN_OBJECT(L, ret == ERROR_SUCCESS);
}
#endif // LUA_REG_NO_HIVEOPS
//docok
int reg_setvalue(lua_State *L){//regobj.setvalue
	LUA_CHECK_RETURN_OBJECT(L, 
		reg_aux_setvalue(L, reg_aux_gethkey(L, 1), lua_opttstring(L, 2, NULL), reg_aux_getdatatype(L, 4), 3)
		);
}
#ifndef LUA_REG_NO_HIVEOPS
//docok
int reg_unloadkey(lua_State *L){//regobj.unload
	LONG ret;
	win_setprivilege(SE_RESTORE_NAME, 1, NULL);
	ret = RegUnLoadKey(reg_aux_gethkey(L,1), lua_checktstring(L, 2));
	LUA_CHECK_DLL_ERROR(L, ret);
	LUA_CHECK_RETURN_OBJECT(L, ret == ERROR_SUCCESS);
}
#endif // LUA_REG_NO_HIVEOPS

int reg_handle(lua_State *L){//regobj.handle
	HKEY hKey = reg_aux_gethkey(L, 1);
	lua_pushlightuserdata(L, hKey);
	return 1;
}

int reg_detach(lua_State *L){//regobj.detach
	PHKEY phKey = reg_aux_getphkey(L, 1);
	lua_pushlightuserdata(L, *phKey);
	*phKey = NULL;
	return 1;
}

int reg_getstrval(lua_State *L){//regobj.getstrval
	HKEY hKey = reg_aux_gethkey(L, 1);
	const TCHAR * pszVal = lua_opttstring(L, 2, NULL);
	int expand = lua_optbool(L,3,1);
	DWORD dwType = 0;
	DWORD dwLen = 0;
	PVOID pvd = 0;
	LONG ret = ERROR_SUCCESS;
	// open the key
	if(	ERROR_SUCCESS == (ret = RegQueryValueEx(hKey, pszVal, NULL, &dwType, NULL, &dwLen))
	// alloc
	&&	(pvd = lua_newuserdata(L, dwLen)) != NULL
	// query a-again
	&&	ERROR_SUCCESS == (ret = RegQueryValueEx(hKey, pszVal, NULL, &dwType, pvd, &dwLen))
	){
		reg_aux_pusheregstrdata(L, pvd, dwLen, dwType, expand);
	}else{
		lua_pushnil(L);
	}
	return 1;
}

int reg_getvaltype(lua_State *L){//regobj.getvaltype
	HKEY hKey = reg_aux_gethkey(L, 1);
	const TCHAR * pszVal = lua_opttstring(L, 2, NULL);
	DWORD dwType = 0;
	LONG ret = ERROR_SUCCESS;
	if(ERROR_SUCCESS == (ret = RegQueryValueEx(hKey, pszVal, NULL, &dwType, NULL, NULL)) ){
		reg_aux_pushdatatype(L, dwType);
	}else{
		// value does not exist!
		lua_pushnil(L);
	}
	return 1;
}