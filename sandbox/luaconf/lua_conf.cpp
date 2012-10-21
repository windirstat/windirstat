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
	int nRetCode = 0;
    lua_State* config = luaL_newstate();
    if(config)
    {
    }

	return nRetCode;
}
