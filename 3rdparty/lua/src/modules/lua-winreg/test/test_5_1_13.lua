local winreg = require"winreg"
-- Enumerate shell folders
hkey = winreg.openkey([[HKCU\Software\Microsoft\Windows\CurrentVersion]])
skey = hkey:openkey("Explorer\\Shell Folders")
for name, kind in skey:enumvalue(true) do
	print("\nname: " .. name .. " (type " .. kind .. ")")
end
