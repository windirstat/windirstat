local winreg = require"winreg"
rkey = "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
hkey = winreg.openkey(rkey)
for name, kind in hkey:enumvalue() do
	print("\nname: " .. name
	.. "\ntype: " .. hkey:getvaltype(name))
	assert(kind == hkey:getvaltype(name))
end
