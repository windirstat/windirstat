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

print '--------------------------'

function test_loader(...)
    print("LOADER FUNC: ", ...)
    local mod = {}
    package.loaded[...] = mod
    return mod
end

package.preload["mytest"] = test_loader
require "mytest"

function dumptable(T,t)
    print '--------------------------'
    print(T)
    print '--------------------------'
    for k,v in pairs(t)do print(k,v) end
end

dumptable('package', package)
dumptable('package.preload', package.preload)
dumptable('package.loaded', package.loaded)
dumptable('package.loaders', package.loaders)
dumptable('package.loaded._G', package.loaded._G)
dumptable('package.loaded.winreg', package.loaded.winreg)
if winres then
    dumptable('winres', winres)
    if winres.scripts then
        dumptable('winres.scripts', winres.scripts)
    end
end
print '--------------------------'
print('_G = ', _G)
print('package.loaded._G = ', package.loaded._G)
