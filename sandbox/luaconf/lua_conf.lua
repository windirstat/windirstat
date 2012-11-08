print "Hello world"

if os.isadmin() then
  print "I'm admin"
else
  print "I'm NOT admin"
end

if os.iswow64() then
  print "I'm a WOW64 process"
else
  print "I'm NOT a WOW64 process"
end

print "-----------------"

function test_loader(...)
    print(...)
    local mod = {}
    package.loaded[...] = mod
    return mod
end


package.preload["mytest"] = test_loader
require "mytest"

for k,v in pairs(package.loaded)do print(k,v) end
--[[
local function load(modulename)
  local errmsg = ""
  -- Find source
  local modulepath = string.gsub(modulename, "%.", "/")
  for path in string.gmatch(package.path, "([^;]+)") do
    local filename = string.gsub(path, "%?", modulepath)
    local file = io.open(filename, "rb")
    if file then
      -- Compile and return the module
      return assert(loadstring(assert(file:read("*a")), filename))
    end
    errmsg = errmsg.."\n\tno file '"..filename.."' (checked with custom loader)"
  end
  return errmsg
end
]]