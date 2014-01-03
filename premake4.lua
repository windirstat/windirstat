-- The below is used to insert the .vs(2005|2008|2010|2012|2013) into the file names for projects and solutions
local action = _ACTION or ""
do
    -- This is mainly to support older premake4 builds
    if not premake.project.getbasename then
        print "Magic happens ..."
        -- override the function to establish the behavior we'd get after patching Premake to have premake.project.getbasename
        premake.project.getbasename = function(prjname, pattern)
            return pattern:gsub("%%%%", prjname)
        end
        -- obviously we also need to overwrite the following to generate functioning VS solution files
        premake.vstudio.projectfile = function(prj)
            local pattern
            if prj.language == "C#" then
                pattern = "%%.csproj"
            else
                pattern = iif(_ACTION > "vs2008", "%%.vcxproj", "%%.vcproj")
            end

            local fname = premake.project.getbasename(prj.name, pattern)
            fname = path.join(prj.location, fname)
            return fname
        end
        -- we simply overwrite the original function on older Premake versions
        premake.project.getfilename = function(prj, pattern)
            local fname = premake.project.getbasename(prj.name, pattern)
            fname = path.join(prj.location, fname)
            return path.getrelative(os.getcwd(), fname)
        end
    end
    -- Name the project files after their VS version
    local orig_getbasename = premake.project.getbasename
    premake.project.getbasename = function(prjname, pattern)
        if _ACTION then
            name_map = {vs2005 = "vs8", vs2008 = "vs9", vs2010 = "vs10", vs2012 = "vs11", vs2013 = "vs12"}
            if name_map[_ACTION] then
                pattern = pattern:gsub("%%%%", "%%%%." .. name_map[_ACTION])
            else
                pattern = pattern:gsub("%%%%", "%%%%." .. _ACTION)
            end
        end
        return orig_getbasename(prjname, pattern)
    end
    -- Override the object directory paths ... don't make them "unique" inside premake4
    local orig_gettarget = premake.gettarget
    premake.gettarget = function(cfg, direction, pathstyle, namestyle, system)
        local r = orig_gettarget(cfg, direction, pathstyle, namestyle, system)
        if (cfg.objectsdir) and (cfg.objdir) then
            cfg.objectsdir = cfg.objdir
        end
        return r
    end
    -- Silently suppress generation of the .user files ...
    local orig_generate = premake.generate
    premake.generate = function(obj, filename, callback)
        if filename:find('.vcproj.user') or filename:find('.vcxproj.user') then
            return
        end
        orig_generate(obj, filename, callback)
    end
    --[[
    dofile("lua/datadumper.lua")
    for _,v in ipairs{"vs2005", "vs2008", "vs2010", "vs2012", "vs2013"} do
        if _ACTION then
            if _ACTION == v then
                do
                    orig_onsolution = premake.action.list[_ACTION].onsolution
                    premake.action.list[_ACTION].onsolution = function(sln)
                        orig_onsolution(sln)
                    end
                end
            end
        end
    end]]
end
local function transformMacroNames(action, input)
    local new_map   = { vs2010 = 0, vs2012 = 0, vs2013 = 0 }
    local replacements = { PlatformName = "Platform", ConfigurationName = "Configuration" }
    if new_map[action] ~= nil then
        for k,v in pairs(replacements) do
            if input:find(k) then
                input = input:gsub(k, v)
            end
        end
    end
    return input
end

solution ("windirstat")
    configurations  {"Debug", "Release"}
    platforms       {"x32", "x64"}
    location        ('.')

    project ("windirstat")
        local int_dir   = transformMacroNames(action, "intermediate/" .. action .. "_" .. "$(PlatformName)_$(ConfigurationName)")
        uuid            ("BD11B94C-6594-4477-9FDF-2E24447D1F14")
        language        ("C++")
        kind            ("WindowedApp")
        location        ("windirstat")
        targetname      ("wds")
        flags           {"StaticRuntime", "Unicode", "MFC", "NativeWChar", "ExtraWarnings", "NoRTTI", "WinMain", "NoMinimalRebuild"} -- "No64BitChecks", "NoEditAndContinue", "NoManifest", "NoExceptions" ???
        defines         {"WINVER=0x0500"}
        targetdir       ("build")
        includedirs     {".", "windirstat", "common", "windirstat/Controls", "windirstat/Dialogs", "lua/src"}
        objdir          (int_dir)
        libdirs         {"$(IntDir)"}
        links           {"htmlhelp", "psapi", "lua51", "delayimp"}
        resoptions      {"/nologo", "/l409"}
        resincludedirs  {".", "$(IntDir)"}
        linkoptions     {"/delayload:psapi.dll"}

        files
        {
            "common/*.h",
            "common/*.cpp",
            "windirstat/*.cpp",
            "windirstat/Controls/*.cpp",
            "windirstat/Dialogs/*.cpp",
            "windirstat/*.c",
            "windirstat/*.h",
            "windirstat/Controls/*.h",
            "windirstat/Dialogs/*.h",
            "windirstat/windirstat.rc",
            "windirstat/res/*.*",
            "*.txt",
            "common/BUILD",
            "common/*.cmd",
            "premake4.lua",
        }

        excludes
        {
            "lua/src/premake.lua",
            "lua/src/lua.c",
            "lua/src/luac.c",
            "lua/src/print.c",
            "lua/src/**.lua",
            "windirstat/stdafx.cpp",
        }
        
        vpaths
        {
            ["Header Files/Common/*"] = { "common/*.h" },
            ["Header Files/Controls/*"] = { "windirstat/Controls/*.h" },
            ["Header Files/Dialogs/*"] = { "windirstat/Dialogs/*.h" },
            ["Header Files/*"] = { "windirstat/*.h" },
            ["Resource Files/*"] = { "windirstat/*.rc" },
            ["Resource Files/Resources/*"] = { "windirstat/res/*.*" },
            ["Source Files/Common/*"] = { "common/*.cpp" },
            ["Source Files/Lua/*"] = { "windirstat/WDS_Lua_C.c" },
            ["Source Files/Controls/*"] = { "windirstat/Controls/*.cpp" },
            ["Source Files/Dialogs/*"] = { "windirstat/Dialogs/*.cpp" },
            ["Source Files/*"] = { "windirstat/*.cpp" },
            ["Special Files/*"] = { "common/BUILD", "common/*.cmd", "premake4.lua", "*.cmd" },
            ["*"] = { "*.txt" },
        }

        configuration {"Debug", "x32"}
            targetsuffix    ("32D")

        configuration {"Debug", "x64"}
            targetsuffix    ("64D")

        configuration {"Release", "x32"}
            targetsuffix    ("32")

        configuration {"Release", "x64"}
            targetsuffix    ("64")

        configuration {"Debug"}
            defines         ("_DEBUG")
            flags           {"Symbols"}
            prelinkcommands {"$(SolutionDir)\common\\build_luajit.cmd \"$(ProjectDir)$(IntDir)\" debug"}

        configuration {"Release"}
            defines         ("NDEBUG")
            flags           {"Optimize"}
            linkoptions     {"/release"}
            buildoptions    {"/Oi", "/Ot"}
            prelinkcommands {"$(SolutionDir)\common\\build_luajit.cmd \"$(ProjectDir)$(IntDir)\""}

        configuration {"vs2005", "windirstat/WDS_Lua_C.c"}
            defines         ("_CRT_SECURE_NO_WARNINGS") -- _CRT_SECURE_NO_DEPRECATE, _SCL_SECURE_NO_WARNINGS, _AFX_SECURE_NO_WARNINGS and _ATL_SECURE_NO_WARNINGS???
--[[
        do
            local oldcurr = premake.CurrentContainer
            local resource_dlls = {
                ["wdsr0405"] = "C3F39C58-7FC4-4243-82B2-A3572235AE02", -- Czech
                ["wdsr0407"] = "C8D9E4F9-7051-4B41-A5AB-F68F3FCE42E8", -- German
                ["wdsr040a"] = "23B76347-204C-4DE6-A311-F562CEF5D89C", -- Spanish
                ["wdsr040b"] = "C7A5D1EC-35D3-4754-A815-2C527CACD584", -- Finnish
                ["wdsr040c"] = "DA4DDD24-67BC-4A9D-87D3-18C73E5CAF31", -- French
                ["wdsr040e"] = "2A75AA20-BFFE-4D1C-8AEC-274823223919", -- Hungarian
                ["wdsr0410"] = "FD4194A7-EA1E-4466-A80B-AB4D8D17F33C", -- Italian
                ["wdsr0413"] = "70A55EB7-E109-41DE-81B4-0DF2B72DCDE9", -- Dutch
                ["wdsr0415"] = "70C09DAA-6F6D-4AAC-955F-ACD602A667CE", -- Polish
                ["wdsr0419"] = "7F06AAC4-9FBE-412F-B1D7-CB37AB8F311D", -- Russian
                ["wdsr0425"] = "2FADC62C-C670-4963-8B69-70ECA7987B93", -- Estonian
                }
            for nm,guid in pairs(resource_dlls) do
                premake.CurrentContainer = oldcurr
                prj = project(nm)
                    local int_dir   = transformMacroNames(action, "intermediate/" .. action .. "_" .. "$(PlatformName)_$(ConfigurationName)")
                    uuid            (guid)
                    language        ("C++")
                    kind            ("WindowedApp")
                    location        (nm)
                    flags           {"NoImportLib"}
                    targetdir       ("build")
                    resoptions      {"/nologo", "/l409"}
                    resincludedirs  {".", nm, "$(IntDir)"}
                    linkoptions     {"/noentry"}
                    files
                    {
                        nm .. "/.*.txt",
                        nm .. "/windirstat.rc",
                        nm .. "/res/windirstat.rc2",
                        "windirstat/res/*.*",
                    }
                    vpaths
                    {
                        ["Header Files/*"] = { "*.h" },
                        ["Resource Files/*"] = { "*.rc", "*.rc2", "windirstat/res/*" },
                    }
            end
            premake.CurrentContainer = oldcurr
        end
]]