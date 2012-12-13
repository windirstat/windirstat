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
    const BYTE utf8bom[3] = { 0xEF, 0xBB, 0xBF };
    const char* scriptBuf = NULL;
    size_t scriptLen = 0;
    DWORD dwScriptLen = 0;
    int ret;
    LPCTSTR resName;
    LPCSTR chunkName;

    // Expecting char string here (used as chunk name)
    chunkName = lua_checkstring(L, 1);
    // Take a copy on the stack
    lua_pushstring(L, chunkName);
    // Converts the copy to a wchar_t string
    resName = lua_checktstring(L, 2);

    // Get pointer to script contents and size of script from current module
    if(
        !getResourcePointer(getMyModuleHandle(), resName, RT_LUASCRIPT, ((LPVOID*)&scriptBuf), &dwScriptLen)
        || !dwScriptLen
        )
    {
        luaL_error(L, "Could not load the Lua script from the resources: %s", chunkName);
        return 0;
    }
    scriptLen = (size_t)dwScriptLen;
    // Skip the UTF-8 BOM, if any
    if((scriptLen > sizeof(utf8bom)) && 0 == memcmp(utf8bom, scriptBuf, sizeof(utf8bom)))
    {
        scriptBuf += sizeof(utf8bom);
        scriptLen -= sizeof(utf8bom);
        dwScriptLen -= sizeof(utf8bom);
    }
    // Load the script into the Lua state
    if(ret = luaL_loadbuffer(L, scriptBuf, scriptLen, chunkName))
    {
        luaL_error(L, "Could not load Lua chunk from resource (%d): %s", ret, lua_tostring(L, -1));
        return 0;
    }
    // the loaded script is at the top of the stack
    lua_remove(L, 2); // remove the chunk name now
    lua_pushtstring(L, resName);
    if(ret = lua_pcall(L, 1, LUA_MULTRET, 0))
    {
        luaL_error(L, "Could not call the newly loaded chunk (%d): %s", ret, lua_tostring(L, -1));
    }
    return ret;
}

int enumerateEmbeddedLuaScripts(lua_State* L)
{
    const luaL_Reg winres_funcs[] = {
        {"c_loader", luaC_winres_loader_},
        {NULL, NULL}
    };
    luaL_register(L, WINRES_MODNAME, winres_funcs);
    // winres table at top of stack
    // Name for the contained table
    lua_pushstring(L, "scripts");
    // winres.scripts (later)
    lua_newtable(L);
    // scripts at top of stack
    // Enumerate the resource names of type RT_LUASCRIPT in the current module
    // The callback functions add the names of resources to the table at the top of the stack
    EnumResourceNames(getMyModuleHandle(), RT_LUASCRIPT, (ENUMRESNAMEPROC)enumLuaScriptsNameCallback, (LONG_PTR)L);
    // Assign the table as winres.scripts
    lua_rawset(L, -3);
    // winres table at top of stack again
    lua_pop(L, 1);
    // back to previous stack top (before calling this function)
    return 0;
}
