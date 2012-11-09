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
#define WINRES_LOADER  "c_loader"

// Forward declaration
static int luaC_winres_loader_(lua_State* L);

static BOOL CALLBACK enumLuaScriptsLanguageCallback(HANDLE hModule, LPCTSTR lpszType, LPCTSTR lpszName, WORD wIDLanguage, lua_State* L)
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(lpszName);
    if((0 == lstrcmp(RT_LUASCRIPT,lpszType)) && (LANG_NEUTRAL == PRIMARYLANGID(wIDLanguage)))
    {
        lua_pushtstring(L, lpszName);
        lua_pushtstring(L, lpszType);
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

static int luaC_winres_loader_(lua_State* L)
{
    int x;
    const char* scriptBuf = NULL;
    size_t scriptLen = 0;
    DWORD dwScriptLen = 0;
    LPCTSTR resName = lua_totstring(L, 1); // we _require_ a string, not ordinal resource IDs
    // Get pointer to script contents and size of script from current module
    if(getResourcePointer(getMyModuleHandle(), resName, RT_LUASCRIPT, ((LPVOID*)&scriptBuf), &dwScriptLen))
    {
        scriptLen = (size_t)dwScriptLen;
    }
    x = luaL_loadbuffer(L, scriptBuf, scriptLen, lua_tostring(L, 1));
    printf("X == %d\n", x);
    return x;
}

void enumerateEmbeddedLuaScripts(lua_State* L)
{
    const luaL_Reg winres_funcs[] = {
        {"c_loader", luaC_winres_loader_},
        {NULL, NULL}
    };
    luaL_register(L, WINRES_MODNAME, winres_funcs);
    // Get the table
    lua_getglobal(L, WINRES_MODNAME);
    // Name for the contained table
    lua_pushstring(L, "scripts");
    // winres.scripts
    lua_newtable(L);
    // Enumerate the resource names of type RT_LUASCRIPT in the current module
    // The callback functions add the names of resources to the table at the top of the stack
    EnumResourceNames(getMyModuleHandle(), RT_LUASCRIPT, (ENUMRESNAMEPROC)enumLuaScriptsNameCallback, (LONG_PTR)L);
    // Assign the table as winres.scripts
    lua_rawset(L, -3);
}
