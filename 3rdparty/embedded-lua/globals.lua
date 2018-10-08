-- Portions Copyright (c) 2002-2009 Jason Perkins and the Premake project

_G.iif = function(expr, trueval, falseval)
	if (expr) then
		return trueval
	else
		return falseval
	end
end

_G.printf = function(msg, ...)
	_G.print(string.format(msg, unpack(arg)))
end

_G.dbgprintf = function(msg, ...)
	os.dbgprint(string.format(msg, unpack(arg)))
end

-- An extension to type() to identify project object types by reading the
-- "__type" field from the metatable.
local builtin_type = _G.type
_G.type = function(t)
	local mt = getmetatable(t)
	if (mt) then
		if (mt.__type) then
			return mt.__type
		end
	end
	return builtin_type(t)
end
