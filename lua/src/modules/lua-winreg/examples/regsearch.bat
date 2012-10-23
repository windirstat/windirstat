goto --Lua--
=goto;
io.stdout:seek("set",0)
--[[
This batch file will work if:
* The Lua interpreter is in the directory listed
  in the %PATH% environment variable. Or in the
  current directory.
* The Lua interpreter name is "lua.exe"
* Lua can search the required modules
]]

desc = [[
DESCRIPTION: searches the registry key and value name for a pattern
COMMAND: regsearch PATTERN [/l] [/u] [/subkey:path]
			[/hkcu] [/hklm] [/hkcr] [/hku] [/hkcc]
   PATTERN - Lua pattern
   /l       - convert to lower case before matching
   /u       - convert to upper case before matching
   /hkcu    - search HKEY_CURRENT_USER 
   /hkcu    - search HKEY_CURRENT_USER
   /hklm    - search HKEY_LOCAL_MACHINE
   /hkcr    - search HKEY_CLASSES_ROOT
   /hku     - search HKEY_USERS
   /hkcc    - search HKEY_CURRENT_CONFIG
   /hkall   - same as /hkcc /hku /hkcr /hkcu /hklm
   /subkey:path - e.g /subkey:Software\Microsoft
EXAMPLE: regsearch HKCU "[Mm][Rr][Uu]"
OUTPUTS: key or value name match
   key match syntax is : ROOTKEY\Subkey\Key
   value match syntax  : ROOTKEY\Subkey\Key\\Value
REMARKS: write '%%' or '\37' for character '%'
   to avoid environment expansion.
   To redirect output to file use this syntax:
   WinNT/Win9x: %comspec%/c regsearch [ROOTKEY] [PATTERN] > file.out
   WinNT: %comspec%/q /c regsearch [ROOTKEY] [PATTERN] > file.out
   WinNT: regsearch [ROOTKEY] [PATTERN] > file.out
]]
local tins = table.insert
local gmatch = string.gfind or string.gmatch
local opt = {}
local mpm = {['+']=true,['-']=false,['']=true}
for i,v in ipairs(arg) do
	local k, d = gmatch(v, "/(%w+)%:(.+)$")()
	if k and d then
		opt[k] = d
	else
		k, d = gmatch(v, "/(%w+)([%-%+]?)$")()
		if k then opt[k] = mpm[d] end
	end
	if not k then tins(opt, v) end
end


require"winreg"
local find = string.find
local gsub = string.gsub
local fmt  = string.format
local lwr  = string.lower
local upr  = string.upper
local quot = function(s) return fmt("%q",s) end
local pats = {}
local rky0 = {hkcu=1,hklm=1,hkcr=1,hku=1,hkcc=1}
local rkys = {}
local case = opt.l and lwr or (opt.u and upr or function(x)return x end);
local skey = nil;

function regopen(xkey,k)
	return xkey:openkey(k)
end

function joinkey(a,b)
	return gsub(a, "(\\+)$", "")..'\\'..b
end

function regsrch(robj,rstr,pat)
	if robj then
		for v in robj:enumvalue() do
			v = case(v)
			if find(v,pat) then
				print(rstr.."\\\\"..v)
			end
		end
		for k in robj:enumkey() do
			k = case(k)
			local s = joinkey(rstr, k)
			if find(k,pat) then
				print(s)
			end
			local r, o = pcall(regopen, robj, k)
			if r and o then regsrch(o,s, pat)
			--else
			--	print("CANT OPEN:",s)
			end
			collectgarbage()
		end
	end
end

local function main()
	for i,v in ipairs(opt) do
		tins(pats,v)
	end
	if opt.hkall then
		for k,v in pairs(rky0) do
			tins(rkys, upr(k))
		end
	else
		for k,v in pairs(opt) do
			if rky0[k] and v then
				tins(rkys, upr(k))
			end
		end
	end
	
	if pats[1] then
		if table.getn(rkys) < 1  then tins(rkys,"HKCU") end
		skey = opt.subkey or ""
		for _,key in ipairs(rkys) do
			for _,pat in ipairs(pats) do
				local x = joinkey(key, skey)
				regsrch(winreg.openkey(x,'r'), case(x),pat)
			end
		end
	else
		print(desc)
	end
end

ok, msg = pcall(main)
return ok or error(msg)

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