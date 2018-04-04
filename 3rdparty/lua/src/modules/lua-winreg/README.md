#winreg

[![Build status](https://ci.appveyor.com/api/projects/status/8xfyu8bow3n51jv2/branch/master?svg=true)](https://ci.appveyor.com/project/moteus/lua-winreg/branch/master)

--------------------------------------------------------------------------------

##Jas Latrix
Copyright © 2005, 2006 Jas Latrix <jastejada at yahoo dot com>

All Rights Deserved. Use at your own risk!. Shake well before using.

--------------------------------------------------------------------------------


#Introduction

winreg is a Lua binary module to Access Microsoft(R) Windows(R) Registry. The registry is a 
system-defined database that applications and Microsoft(R) Windows(R) system components use to 
store and retrieve configuration data.
Load the module via the require function (make sure Lua can find the module), for example: 

```lua
local winreg = require"winreg"

-- prints all the special folders
hkey = winreg.openkey[[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion]]

skey = hkey:openkey([[Explorer\Shell Folders]])
for name in skey:enumvalue() do
  print("\nname: " .. name
     .. "\npath: " .. skey:getvalue(name))
end
```

