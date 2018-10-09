local winreg = require"winreg"
rkey = [[HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer]]
info = winreg.openkey(rkey):getinfo()
print("number of subkeys : '" .. info.subkeys .. "'")
print("longest subkey name length : '" .. info.maxsubkeylen .. "'")
print("longest class string length : '" .. info.maxclasslen .. "'")
print("number of value entries : '" .. info.values .. "'")
print("longest value name length : '" .. info.maxvaluelen .. "'")
print("class string : '" .. info.class .. "'")
