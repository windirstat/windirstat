#ifndef __DBGPRINT_H_VER__
#define __DBGPRINT_H_VER__ 2012102423
// $Id: dbgprint.h,v e885f176d70a 2012/10/25 14:16:05 oliver $
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
    LUA_API const luaL_Reg dbgprint_funcs[];
#ifdef __cplusplus
};
#endif // __cplusplus

#endif // __DBGPRINT_H_VER__
