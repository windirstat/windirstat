// lua_conf.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "lua_conf.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace std;

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
    lua_State* L = lua_open();
    if(L)
    {
        lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialization */
        luaL_openlibs(L);  /* open libraries */
        lua_gc(L, LUA_GCRESTART, 0);
        int ret = luaL_dofile(L, "..\\lua_conf.lua");
        if(ret)
        {
            fprintf(stderr, "%s", lua_tostring(L, -1));
            lua_pop(L, 1); /* pop error message from the stack */
            lua_close(L);
            return EXIT_FAILURE;
        }
        lua_close(L);
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
