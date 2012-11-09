///////////////////////////////////////////////////////////////////////////////
///
/// Written 2011-2012, Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
///
/// Original filename: w32embed.h
/// Project          : WinDirStat
/// Author(s)        : Oliver Schneider
///
/// Purpose          : Lua package to load Lua code from the resource section
///                    of a PE file.
///
///////////////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <tchar.h>
#include <lua.h>
#include "w32resembed.h"

#define WINRES_MODNAME "winres"

// Forward declaration
static int c_load_chunk_from_resources(lua_State* L);

static BOOL CALLBACK enumLuaScriptsLanguageCallback(HANDLE hModule, LPCTSTR lpszType, LPCTSTR lpszName, WORD wIDLanguage, lua_State* L)
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(lpszName);
    if((0 == lstrcmp(RT_LUASCRIPT,lpszType)) && (LANG_NEUTRAL == PRIMARYLANGID(wIDLanguage)))
    {
        lua_pushtstring(L, lpszName);
        lua_pushcfunction(L, c_load_chunk_from_resources);
        lua_rawset(L, -3);
    }
    return TRUE;
}

static BOOL CALLBACK enumLuaScriptsNameCallback(HANDLE hModule, LPCTSTR lpszType, LPCTSTR lpszName, lua_State* L)
{
    UNREFERENCED_PARAMETER(hModule);
    // First string table entry that we encounter will be grabbed
    if(0 == lstrcmp(RT_LUASCRIPT,lpszType))
    {
        // Now enumerate the languages of this entry ...
        EnumResourceLanguages((HMODULE)hModule, lpszType, lpszName, (ENUMRESLANGPROC)enumLuaScriptsLanguageCallback, (LONG_PTR)L);
    }
    return TRUE;
}

static BOOL getResourcePointer(HINSTANCE Instance, LPCTSTR ResName, LPCTSTR ResType, LPVOID* ppRes, DWORD* pdwResSize)
{
    if(ppRes && pdwResSize)
    {
        HRSRC hRsrc;
        if(hRsrc = FindResource((HMODULE)Instance, ResName, ResType))
        {
            HGLOBAL hGlob;
            if(hGlob = LoadResource(Instance, hRsrc))
            {
                *ppRes = LockResource(hGlob);
                *pdwResSize = SizeofResource(Instance, hRsrc);
                return (*ppRes && *pdwResSize);
            }
        }
    }
    return FALSE;
}

static HMODULE getMyModuleHandle()
{
    static int s_somevar = 0;
    MEMORY_BASIC_INFORMATION mbi;
    if(!VirtualQuery(&s_somevar, &mbi, sizeof(mbi)))
    {
        return NULL;
    }
    return (HMODULE)mbi.AllocationBase;
}

void enumerateEmbeddedLuaScripts(lua_State* L)
{
    lua_newtable(L);
    lua_pushstring(L, "scripts");
    lua_newtable(L);
    EnumResourceNames(getMyModuleHandle(), RT_LUASCRIPT, (ENUMRESNAMEPROC)enumLuaScriptsNameCallback, (LONG_PTR)L);
    lua_rawset(L, -3);
    lua_setglobal(L, WINRES_MODNAME);
}

static const char* callback_resource_lua_Reader(lua_State* L, void* data, size_t* size)
{
    const char* retval = NULL;
    DWORD retsize = 0;
    LPCTSTR resName = lua_totstring(L, 1);
    if(getResourcePointer(getMyModuleHandle(), resName, RT_LUASCRIPT, ((LPVOID*)&retval), &retsize))
    {
        *size = (size_t)retsize;
    }
    else
    {
        *size = 0;
        retval = NULL;
    }
    return retval;
}

static int c_load_chunk_from_resources(lua_State* L)
{
    const char* ref_name = lua_tostring(L, 1);
    return lua_load(L, callback_resource_lua_Reader, NULL, ref_name);
}
