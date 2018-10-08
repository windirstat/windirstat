local winreg = require "winreg"

local p = [[HKEY_LOCAL_MACHINE\SOFTWARE]]

local key64 = assert(winreg.openkey(p, 'r64'))
local key32 = assert(winreg.openkey(p, 'r32'))

function DumpKeys(hkey)
  for name in hkey:enumvalue() do
    result, type = hkey:getvalue(name)
    print(name, result, type)
  end
end

print('X64:')
DumpKeys(key64)
print("---------------------------------")
print('X32:')
DumpKeys(key32)
print("---------------------------------")

