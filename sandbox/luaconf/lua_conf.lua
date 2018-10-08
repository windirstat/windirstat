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

function dumptable(T,t)
    local __genOrderedIndex = function( t )
        local orderedIndex = {}
        for key in pairs(t) do
            table.insert( orderedIndex, key )
        end
        table.sort( orderedIndex )
        return orderedIndex
    end
    local orderedNext = function (t, state)
        -- Equivalent of the next function, but returns the keys in the alphabetic
        -- order. We use a temporary ordered key table that is stored in the
        -- table being iterated.
        local key = nil
        --print("orderedNext: state = "..tostring(state) )
        if state == nil then
            -- the first time, generate the index
            t.__orderedIndex = __genOrderedIndex( t )
            key = t.__orderedIndex[1]
        else
            -- fetch the next value
            for i = 1,table.getn(t.__orderedIndex) do
                if t.__orderedIndex[i] == state then
                    key = t.__orderedIndex[i+1]
                end
            end
        end
        if key then
            return key, t[key]
        end
        -- no more value to return, cleanup
        t.__orderedIndex = nil
        return
    end
    local orderedPairs = function(t)
        -- Equivalent of the pairs() function on tables. Allows to iterate
        -- in order
        return orderedNext, t, nil
    end
    print '--------------------------'
    print(T)
    print '--------------------------'
    for k,v in orderedPairs(t) do
        print(k,v)
    end
    print '=========================='
end

if winres then
    dumptable('winres', winres)
    if winres.scripts then
        dumptable('winres.scripts', winres.scripts)
    end
end

--[[
for k,v in pairs(winres.scripts) do
    package.preload[k:lower()] = function(...)
        return winres.c_loader(k:upper())
    end
    --package.preload[k:upper()] = package.preload[k:lower()]
end
--]]
dumptable('package.preload', package.preload)
dumptable('winreg', winreg)
hello = require "hello"
dumptable('package.loaded', package.loaded)
dumptable('_G', _G)

hello.hello()