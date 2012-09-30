-- The below is used to insert the .vs(2005|2008|2010|2012) into the file names for projects and solutions
local action = _ACTION or ""
do
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
    local orig_getbasename = premake.project.getbasename
    premake.project.getbasename = function(prjname, pattern)
        if _ACTION then
            pattern = pattern:gsub("%%%%", "%%%%." .. _ACTION)
        end
        return orig_getbasename(prjname, pattern)
    end
end

solution ("windirstat")
    configurations  {"Debug", "Release"}
    platforms       {"x32", "x64"}
    location        ('.')
    
    project ("windirstat")
        local int_dir   = "intermediate/" .. action .. "_" .. "$(PlatformName)_$(ConfigurationName)"
        uuid            ("BD11B94C-6594-4477-9FDF-2E24447D1F14")
        language        ("C++")
        kind            ("WindowedApp")
        location        ("windirstat")
        targetname      ("wds")
        flags           {"StaticRuntime", "Unicode", "MFC", "NativeWChar", "ExtraWarnings", "NoRTTI", "WinMain", "NoMinimalRebuild"}
        defines         {"WINVER=0x0500"}
        targetdir       ("build")
        objdir          (int_dir)
        -- pchheader       ("windirstat/stdafx.h")
        -- pchsource       ("windirstat/stdafx.cpp")

        files
        {
            "common/*.cpp",
            "windirstat/*.cpp",
            "windirstat/*.c",
            "windirstat/windirstat.rc",
        }

        excludes
        {
            "lua/src/premake.lua",
            "lua/src/lua.c",
            "lua/src/luac.c",
            "lua/src/print.c",
            "lua/src/**.lua",
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
        configuration {"Release"}
            defines         ("NDEBUG")
            flags           {"Optimize"}
            linkoptions     {"/release"}
            buildoptions    {"/Oi", "/Ot"}
        configuration {"vs*"}
            -- buildoptions {}
            links           { "htmlhelp", "psapi" }
            resoptions      {"/nologo", "/l409"}
            resincludedirs  {"$(IntDir)"}
            includedirs     {".", "lua/src"}
            linkoptions     {"/delayload:psapi.dll"}
--        configuration {"WDS_Lua_C.c"}
--          defines         ("_CRT_SECURE_NO_WARNINGS")
