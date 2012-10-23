local winreg = require"winreg"

-- prints all the special folders
hkey = winreg.openkey[[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion]]

skey = hkey:openkey([[Explorer\Shell Folders]])
for name in skey:enumvalue() do
	print("\nname: " .. name
	   .. "\npath: " .. skey:getvalue(name))
end
