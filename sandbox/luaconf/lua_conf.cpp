// lua_conf.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "lua_conf.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    static int traceback (lua_State *L) {
        if (!lua_isstring(L, 1))  /* 'message' not a string? */
            return 1;  /* keep it intact */
        lua_getfield(L, LUA_GLOBALSINDEX, "debug");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return 1;
        }
        lua_getfield(L, -1, "traceback");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            return 1;
        }
        lua_pushvalue(L, 1);  /* pass error message */
        lua_pushinteger(L, 2);  /* skip this function and traceback */
        lua_call(L, 2, 1);  /* call debug.traceback */
        return 1;
    }

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

    static int docall (lua_State *L, int narg, int clear)
    {
        int status;
        int base = lua_gettop(L) - narg;  /* function index */
        lua_pushcfunction(L, traceback);  /* push traceback function */
        lua_insert(L, base);  /* put it under chunk and args */
        status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
        lua_remove(L, base);  /* remove traceback function */
        /* force a complete garbage collection in case of errors */
        if (status != 0) lua_gc(L, LUA_GCCOLLECT, 0);
        return status;
    }

    static int dostring (lua_State *L, const char *s) {
        const char *name = "dostring";
        int status = luaL_loadbuffer(L, s, strlen(s), name) || docall(L, 0, 1);
        return report(L, status);
    }
}

using namespace std;

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
    lua_State* L = luaWDS_open();
    if(L)
    {
        const char* _dostring = "function dumptable(T,t)\n\
                      print '--------------------------'\n\
                      print(T)\n\
                      print '--------------------------'\n\
                      for k,v in pairs(t)do print(k,v) end\n\
                      end\n\
                      \n\
                      dumptable('package', package)\n\
                      dumptable('package.loaded', package.loaded)\n\
                      dumptable('package.loaders', package.loaders)\n\
                      dumptable('package.loaded._G', package.loaded._G)\n\
                      print '--------------------------'\n\
                      print('_G = ', _G)\n\
                      print('package.loaded._G = ', package.loaded._G)\n\
                      ";
        dostring(L, _dostring);
        fprintf(stderr, "---------------------------\n");
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
