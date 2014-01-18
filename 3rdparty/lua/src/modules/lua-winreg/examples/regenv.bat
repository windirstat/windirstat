goto --Lua--
=goto;
io.stdout:seek("set",0)

desc = [[
DESCRIPTION: print some registry environments
COMMAND: regenv
REMARKS: To redirect output to file use this syntax:
   WinNT/Win9x: %comspec%/c regenv > file.out 
   WinNT: regenv > file.out 
]]

print("Registry Environments:")

-- Prints some system environments
-- Save to file via "regenv.bat > out.txt" commnad

require"winreg"

function regopen(k)
	return winreg.openkey(k)
end

_, hkey = pcall(regopen, [[HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders]])
if _ and hkey then
	print("Current User Shell folders:")
	for name in hkey:enumvalue() do
		print("\t"..name.."="..(hkey:getvalue(name) or ""))
	end
end

_, hkey = pcall(regopen, [[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders]])
if _ and hkey then
	print("All User Shell folders:")
	for name in hkey:enumvalue() do
		print("\t"..name.."="..(hkey:getvalue(name) or ""))
	end
end

_, hkey = pcall(regopen, [[HKEY_CURRENT_USER\Environment]])
if _ and hkey then
	print("Current User Environment:")
	for name in hkey:enumvalue() do
		print("\t"..name.."="..(hkey:getvalue(name) or ""))
	end
end

_, hkey = pcall(regopen, [[HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager\Environment]])
if _ and hkey then
	print("System Environment:")
	for name in hkey:enumvalue() do
		print("\t"..name.."="..(hkey:getvalue(name) or ""))
	end
end

_, hkey = pcall(regopen, [[HKEY_CURRENT_USER\Volatile Environment]])
if _ and hkey then
	print("Volatile Environment:")
	for name in hkey:enumvalue() do
		print("\t"..name.."="..(hkey:getvalue(name) or ""))
	end
end

_, hkey = pcall(regopen, [[HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths]])
if _ and hkey then
	print("Application paths:")
	for name in hkey:enumkey() do
		print("\t"..name.."="..(hkey:openkey(name):getvalue() or ""))
	end
end


_, hkey = pcall(regopen, [[HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run]])
if _ and hkey then
	print("Current User Startup programs:")
	for name in hkey:enumvalue() do
		print("\t"..name.."="..(hkey:getvalue(name) or ""))
	end
end

_, hkey = pcall(regopen, [[HKLM\Software\Microsoft\Windows\CurrentVersion\Run]])
if _ and hkey then
	print("System Startup programs:")
	for name in hkey:enumvalue() do
		print("\t"..name.."="..(hkey:getvalue(name) or ""))
	end
end

io.stdout:flush()
--[[
:--Lua--
@echo off
if "%OS%" == "Windows_NT" goto -winnt-
set f=%0
if exist "%f%" goto -run-
set f=%0.bat
if exist "%f%" goto -run-
for %%? in (%path%) do if exist "%%?\%0" set f=%%?\%0
if exist "%f%" goto -run-
for %%? in (%path%) do if exist "%%?\%0.bat" set f=%%?\%0.bat
:-run-
lua "%f%" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto -end-
:-winnt-
lua %~f0 %*
:-end-
::]]
