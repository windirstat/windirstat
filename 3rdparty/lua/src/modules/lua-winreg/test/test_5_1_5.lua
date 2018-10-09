local winreg = require"winreg"
-- Enumerate Application paths
rkey = [[HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths]]
hkey = winreg.openkey(rkey, 'r')
for k in hkey:enumkey() do
	print(k,(hkey:openkey(k, 'r'):getvalue()))
	collectgarbage()
end
