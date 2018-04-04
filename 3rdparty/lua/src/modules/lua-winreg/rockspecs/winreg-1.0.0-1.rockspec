package = "winreg"
version = "1.0.0-1"
source = {
  url = "https://github.com/moteus/lua-winreg/archive/v1.0.0.zip",
  dir = "lua-winreg-1.0.0",
}

description = {
  summary = "Lua module to Access Microsoft(R) Windows(R) Registry",
  homepage = "http://github.com/moteus/lua-winreg",
  license = "MIT/X11",
  detailed = [[
  ]],
}

supported_platforms = {
  "windows"
}

dependencies = {
  "lua >= 5.1, < 5.4",
}

build = {
  type = "builtin",

  copy_directories = {'doc', 'examples', 'test'},

  modules = {
    winreg = {
      sources = {
        "src/l52util.c", "src/lua_int64.c", "src/lua_mtutil.c", "src/lua_tstring.c",
        "src/luawin_dllerror.c", "src/win_privileges.c", "src/win_registry.c",
        "src/win_trace.c", "src/winreg.c",
      },
      defines = {
        "WIN32"; "_WIN32"; "_WINDOWS",
        "WIN32_LEAN_AND_MEAN"; "WINDLL"; "USRDLL",
        "NDEBUG", "_CRT_SECURE_NO_WARNINGS",
        "CRTAPI1=_cdecl", "CRTAPI2=_cdecl",
        "WINREG_EXPORTS", "WINREG_API=__declspec(dllexport)"
      },
      libraries = {"advapi32", "kernel32", "user32"},
    }
  }
}
