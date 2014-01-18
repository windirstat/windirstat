#ifndef __ISADMIN_H_VER__
#define __ISADMIN_H_VER__ 2012102414
// $Id: isadmin.h,v 52e6657a08c7 2012/10/24 15:21:39 oliver $
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
    LUA_API const luaL_Reg isadmin_funcs[];
#ifdef __cplusplus
};
#endif // __cplusplus

#endif // __ISADMIN_H_VER__
