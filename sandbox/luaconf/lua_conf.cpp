// lua_conf.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "lua_conf.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    static void l_message (const char *pname, const char *msg)
    {
        if (pname) fprintf(stderr, "%s: ", pname);
        fprintf(stderr, "%s\n", msg);
        fflush(stderr);
    }


    static int report (lua_State *L, int status)
    {
        if (status && !lua_isnil(L, -1)) {
            const char *msg = lua_tostring(L, -1);
            if (msg == NULL) msg = "(error object is not a string)";
            l_message(__FILE__, msg);
            lua_pop(L, 1);
        }
        return status;
    }
}

using namespace std;

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
    lua_State* L = luaWDS_open();
    if(L)
    {
        int ret = w32res_enumerateEmbeddedLuaScripts(L);
        if(ret)
        {
            luaL_error(L, "Failed to enum the resources (%d)", report(L, ret));
            lua_close(L);
            return EXIT_FAILURE;
        }
        if(lua_istable(L, -1))
        {
            lua_pop(L, 1);
        }
        ret = luaL_dofile(L, "lua_conf.lua");
        if(ret)
        {
            luaL_error(L, "Failed to load lua_conf.lua (%d)", report(L, ret));
            lua_close(L);
            return EXIT_FAILURE;
        }
        lua_close(L);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
