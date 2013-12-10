///////////////////////////////////////////////////////////////////////////////
///
/// Written 2011-2012, Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
///
/// Original filename: w32embed.h
/// Project          : WinDirStat
/// Author(s)        : Oliver Schneider
///
/// Purpose          : Lua package to load Lua code from the resource section
///                    of a PE file. Skips the UTF-8 BOM (3 bytes), if found.
///
///////////////////////////////////////////////////////////////////////////////

#include <Windows.h>
#include <tchar.h>
#include <lua.h>
#include "w32resembed.h"

static int luaC_registerPreloader_(lua_State* L, const int winresidx, LPCTSTR lpszName)
{
    const int oldTop = lua_gettop(L);
    UNREFERENCED_PARAMETER(winresidx);
    // Expects the winres table at -1
    lua_getfield(L, LUA_GLOBALSINDEX, "package");
    if(!lua_istable(L, -1))
    {
        lua_settop(L, oldTop);
        lua_pushfstring(L, "Not a table at index %i. Expected '%s' table here.", -1, "package");
        return FALSE;
    }
    lua_getfield(L, -1, "preload");
    if(!lua_istable(L, -1))
    {
        lua_settop(L, oldTop);
        lua_pushfstring(L, "Not a table at index %i. Expected '%s.%s' table here.", -1, "package", "preload");
        return FALSE;
    }
    lua_pushtstring(L, lpszName);
    lua_getfield(L, -4, W32RES_LOADER); // get registered C function
    if(!lua_isfunction(L, -1))
    {
        lua_settop(L, oldTop);
        lua_pushfstring(L, "Not a C function when fetching field '%s.%s'.", W32RES_MODNAME, W32RES_LOADER);
        return 1;
    }
    lua_rawset(L, -3); // package.preload[lpszName] = winres.c_loader
    // We checked the parameters already
    lua_pushtstring_lowercase(L, lpszName); // lpszName = string:lower(lpszName)
    lua_getfield(L, -4, W32RES_LOADER);
    lua_rawset(L, -3); // package.preload[lpszName] = winres.c_loader
    lua_settop(L, oldTop);
    return 0;
}

static BOOL CALLBACK enumLuaScriptsLanguageCallback(HANDLE hModule, LPCTSTR lpszType, LPCTSTR lpszName, WORD wIDLanguage, lua_State* L)
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(lpszName);
    if((0 == lstrcmp(RT_LUASCRIPT,lpszType)) && (LANG_NEUTRAL == PRIMARYLANGID(wIDLanguage)))
    {
        const int winresidx = -1;
        // Expecting the winres table at the top of the stack here
        if(!lua_istable(L, winresidx))
        {
            lua_pushfstring(L, "Not a table at index %i. Expected '%s' table here.", winresidx, W32RES_MODNAME);
            return FALSE;
        }
        // Now register the preloader
        if(luaC_registerPreloader_(L, winresidx, lpszName))
        {
            // we expect a string on the stack here ... don't do anything additional
            return FALSE;
        }
        lua_getfield(L, -1, W32RES_SCRIPTS);
        if(!lua_istable(L, -1))
        {
            lua_pushfstring(L, "Not a table at index %i. Expected '%s.%s' table here.", winresidx, W32RES_MODNAME, W32RES_SCRIPTS);
            return FALSE;
        }
        // winres.scripts at top of stack
        lua_pushtstring(L, lpszName);
        lua_pushtstring(L, lpszType);
        lua_rawset(L, -3); // winres.scripts[lpszName] = lpszType
        // now pop one ... (winres.scripts)
        lua_pop(L, 1);
    }
    return TRUE;
}

static BOOL CALLBACK enumLuaScriptsNameCallback(HANDLE hModule, LPCTSTR lpszType, LPCTSTR lpszName, LONG_PTR lParam)
{
    lua_State* L = (lua_State*)lParam;
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
        if(NULL != (hRsrc = FindResource((HMODULE)Instance, ResName, ResType)))
        {
            HGLOBAL hGlob;
            if(NULL != (hGlob = LoadResource(Instance, hRsrc)))
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
    if((ret = luaL_loadbuffer(L, scriptBuf, scriptLen, chunkName)))
    {
        luaL_error(L, "Could not load Lua chunk from resource (%d): %s", ret, lua_tostring(L, -1));
        return 0;
    }
    // the loaded script is at the top of the stack
    lua_remove(L, 2); // remove the chunk name now
    lua_pushtstring(L, resName);
    if((ret = lua_pcall(L, 1, LUA_MULTRET, 0)))
    {
        luaL_error(L, "Could not call the newly loaded chunk (%d): %s", ret, lua_tostring(L, -1));
    }
    return ret;
}

int w32res_enumerateEmbeddedLuaScripts(lua_State* L)
{
    const luaL_Reg winres_funcs[] = {
        {W32RES_LOADER, luaC_winres_loader_},
        {NULL, NULL}
    };
    luaL_register(L, W32RES_MODNAME, winres_funcs);
    // winres table at top of stack
    // Name for the contained table
    lua_pushstring(L, W32RES_SCRIPTS);
    // winres.scripts (later)
    lua_newtable(L);
    // Assign the table as winres.scripts
    lua_rawset(L, -3);
    // Enumerate the resource names of type RT_LUASCRIPT in the current module
    // The callback functions add the names of resources to the table at the top of the stack
    (void)EnumResourceNames(getMyModuleHandle(), RT_LUASCRIPT, enumLuaScriptsNameCallback, (LONG_PTR)L);
    if(lua_isstring(L, -1))
    {
        // Leave error message at top of stack
        return 1;
    }
    // leave the winres table on the stack
    return 0;
}
