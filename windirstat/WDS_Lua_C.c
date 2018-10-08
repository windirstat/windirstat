#define luaall_c
#define LUA_CORE
#include "WDS_Lua_C.h"

#ifdef _WIN64
#pragma warning(push)
#pragma warning(disable : 4324)
#pragma warning(disable : 4334)
#endif

// Modules/Packages, individual functions
#include "modules/winreg.c"
#include "modules/isadmin.c"
#include "modules/wow64.c"
#include "modules/dbgprint.c"
#include "modules/w32resembed.c"
#include "../../priv.c"

// Cheat a bit to redefine the list of "default" libraries ...
#ifndef WDS_LUA_NO_INIT
static const luaL_Reg lualibs[] = {
    { "", luaopen_base },
    { LUA_LOADLIBNAME, luaopen_package },
    { LUA_TABLIBNAME, luaopen_table },
    { LUA_IOLIBNAME, luaopen_io },
    { LUA_OSLIBNAME, luaopen_os },
    { LUA_STRLIBNAME, luaopen_string },
    { LUA_MATHLIBNAME, luaopen_math },
    { LUA_DBLIBNAME, luaopen_debug },
    { LUA_WINREGNAME, luaopen_winreg },
    { NULL, NULL },
};

static void luaWDS_openlibs_(lua_State* L)
{
    const luaL_Reg* lib = lualibs;
    for (; lib->func; lib++)
    {
        lua_pushcfunction(L, lib->func);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}
#endif // WDS_LUA_NO_INIT // otherwise the implementer needs to define her own version ;)

static void printOther_(lua_State *L, int i)
{
    char const* str = NULL;
    lua_getglobal(L, "tostring");
    lua_pushvalue(L, i); /* value to convert to string representation */
    lua_call(L, 1, 1);
    str = lua_tostring(L, -1); /* get result */
    printf("<%s>", str ? str : "<null>");
    lua_pop(L, 1);  /* pop result */
}

EXTERN_C void stackDump(lua_State *L, char* description) /* from the Programming Lua book */
{
    int i;
    int top = lua_gettop(L);
    printf("[STACK:% 3i] ", lua_gettop(L));
    if (description)
    {
        printf("===== %s =====\n", description);
    }
    for (i = 1; i <= top; i++)
    {
        /* repeat for each level */
        int t = lua_type(L, i);
        switch (t) {

        case LUA_TSTRING:  /* strings */
            printf("\"%s\"", lua_tostring(L, i));
            break;

        case LUA_TBOOLEAN:  /* booleans */
            printf(lua_toboolean(L, i) ? "true" : "false");
            break;

        case LUA_TNUMBER:  /* numbers */
            printf("%g", lua_tonumber(L, i));
            break;

        default:  /* other values */
            printOther_(L, i);
            break;
        }
        printf("  ");  /* put a separator */
    }
    printf("\n");  /* end the listing */
}

lua_State* luaWDS_open()
{
    lua_State* L = lua_open();
    if (L)
    {
        lua_gc(L, LUA_GCSTOP, 0); /* stop collector during initialization */
        luaWDS_openlibs_(L);      /* open libraries */
        luaL_register(L, LUA_OSLIBNAME, wow64_funcs);
        luaL_register(L, LUA_OSLIBNAME, isadmin_funcs);
        luaL_register(L, LUA_OSLIBNAME, dbgprint_funcs);
        lua_pop(L, 3); /* pop the "os" table three times, one for each luaL_register */
        lua_gc(L, LUA_GCRESTART, 0); /* resume GC */
    }
    return L;
}

#ifdef _WIN64
#pragma warning(pop)
#endif
