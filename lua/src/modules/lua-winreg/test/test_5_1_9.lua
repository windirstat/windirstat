local winreg = require"winreg"
rkey = "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
hkey = winreg.openkey(rkey)
for name, kind in hkey:enumvalue() do
	print("\nname: " .. name
	.. "\nvalue: " .. hkey:getstrval(name))
end
