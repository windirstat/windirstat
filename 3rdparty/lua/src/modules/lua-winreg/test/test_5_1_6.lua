local winreg = require"winreg"
-- Enumerate start up programs

rkey = "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
hkey = winreg.openkey(rkey)

print('\n'..rkey..":")
for name, kind in hkey:enumvalue() do
	print("\nname: " .. name
	.. "\ncommand: " .. hkey:getvalue(name))
end

rkey = "HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
hkey = winreg.openkey(rkey)

print('\n'..rkey..":")
for name, kind in hkey:enumvalue() do
	print("\nname: " .. name
	.. "\ncommand: " .. hkey:getvalue(name))
end
