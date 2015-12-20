local winreg = require "winreg"

function winreg.copykey(src, dst, remove_dst_on_fail)
    hksrc = winreg.openkey(src, "r")
    if not hksrc then
        return nil, string.format("Source key (%s) could not be opened", src)
    end
    hkdst = winreg.openkey(dst) or winreg.createkey(dst)
    if not hkdst then
        if remove_dst_on_fail then hkdst:deletekey() end
        return nil, string.format("Destination key (%s) could not be opened/created", dst)
    end
    for vname, vtype in hksrc:enumvalue() do
        if not hkdst:setvalue(vname, hksrc:getvalue(vname), vtype) then
            if remove_dst_on_fail then hkdst:deletekey() end
            return nil, string.format("Destination key (%s) could not be opened", dst)
        end
    end
    for kname in hksrc:enumkey() do
        ret, err = winreg.copykey(src .. [[\]] .. kname, dst .. [[\]] .. kname)
        if not ret then
            return ret, err
        end
    end
    return true
end

function winreg.movekey(src, dst, remove_dst_on_fail)
    ret, err = winreg.copykey(src, dst, remove_dst_on_fail)
    if not ret then
        return ret, err
    end
    hksrc = winreg.openkey(src)
    if not hksrc then
        return nil, string.format("Could not open %s for removal", src)
    else
        if not hksrc:deletekey() then
            return nil, string.format("Could not remove %s", src)
        end
    end
    return true
end
