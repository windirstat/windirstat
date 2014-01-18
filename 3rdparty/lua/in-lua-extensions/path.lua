--
-- path.lua
-- Path manipulation functions.
-- Copyright (c) 2002-2010 Jason Perkins and the Premake project
--


--
-- Retrieve the filename portion of a path, without any extension.
--

	function path.getbasename(p)
		local name = path.getname(p)
		local i = name:findlast(".", true)
		if (i) then
			return name:sub(1, i - 1)
		else
			return name
		end
	end

--
-- Retrieve the directory portion of a path, or an empty string if 
-- the path does not include a directory.
--

	function path.getdirectory(p)
		local i = p:findlast("/", true)
		if (i) then
			if i > 1 then i = i - 1 end
			return p:sub(1, i)
		else
			return "."
		end
	end


--
-- Retrieve the drive letter, if a Windows path.
--

	function path.getdrive(p)
		local ch1 = p:sub(1,1)
		local ch2 = p:sub(2,2)
		if ch2 == ":" then
			return ch1
		end
	end



--
-- Retrieve the file extension.
--

	function path.getextension(p)
		local i = p:findlast(".", true)
		if (i) then
			return p:sub(i)
		else
			return ""
		end
	end



--
-- Retrieve the filename portion of a path.
--

	function path.getname(p)
		local i = p:findlast("[/\\]")
		if (i) then
			return p:sub(i + 1)
		else
			return p
		end
	end


--
-- Converts from a simple wildcard syntax, where * is "match any"
-- and ** is "match recursive", to the corresponding Lua pattern.
--
-- @param pattern
--    The wildcard pattern to convert.
-- @returns
--    The corresponding Lua pattern.
--

	function path.wildcards(pattern)
		-- Escape characters that have special meanings in Lua patterns
		pattern = pattern:gsub("([%+%.%-%^%$%(%)%%])", "%%%1")

		-- Replace wildcard patterns with special placeholders so I don't
		-- have competing star replacements to worry about
		pattern = pattern:gsub("%*%*", "\001")
		pattern = pattern:gsub("%*", "\002")
		
		-- Replace the placeholders with their Lua patterns
		pattern = pattern:gsub("\001", ".*")
		pattern = pattern:gsub("\002", "[^/]*")
		
		return pattern
	end
