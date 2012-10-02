#ifndef __WDS_LUA_C_H_VER__
#define __WDS_LUA_C_H_VER__ 2011060922
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support
 
#define WDS_LUA_NO_MATHLIB
#define WDS_LUA_NO_IOLIB
#define WDS_LUA_NO_LOADLIB
#define WDS_LUA_NO_INIT
#define WDS_LUA_NO_OSLIB
#define WDS_LUA_NO_LUAC

#include "modules/lwinreg.h"
#endif // __WDS_LUA_C_H_VER__
