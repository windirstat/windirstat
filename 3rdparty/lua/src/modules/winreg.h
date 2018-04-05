#ifndef __WINREG_H_VER__
#define __WINREG_H_VER__ 2018040518
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

#ifdef  __cplusplus
extern "C" {
#endif
    extern luaL_Reg lreg_regobj[];
    extern luaL_Reg lreg_reglib[];
    extern int luaopen_winreg(lua_State *L);
#ifdef  __cplusplus
}
#endif

#define LUA_WINREGNAME "winreg"
#define LUA_REG_NO_WINTRACE
#define LUA_REG_NO_HIVEOPS
#define LUA_REG_NO_DLL

#endif // __LWINREG_H_VER__
