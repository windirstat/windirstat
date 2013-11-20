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
            name_map = {vs2005 = "vs8", vs2008 = "vs9", vs2010 = "vs10", vs2012 = "vs11"}
            if name_map[_ACTION] then
                pattern = pattern:gsub("%%%%", "%%%%." .. name_map[_ACTION])
            else
                pattern = pattern:gsub("%%%%", "%%%%." .. _ACTION)
            end
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
        includedirs     { "", "windirstat", "common", "windirstat/Controls", "windirstat/Dialogs", "lua/src" }
        objdir          (int_dir)
        links           {"htmlhelp", "psapi"}
        resoptions      {"/nologo", "/l409"}
        resincludedirs  {"$(IntDir)"}
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
            "common/buildinc.cmd",
            "common/build_luajit.cmd",
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
            ["Special Files/*"] = { "common/BUILD", "common/buildinc.cmd", "premake4.lua", "*.cmd" },
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
            prelinkcommands {"$(SolutionDir)\common\\build_luajit.cmd NUL debug"}

        configuration {"Release"}
            defines         ("NDEBUG")
            flags           {"Optimize"}
            linkoptions     {"/release"}
            buildoptions    {"/Oi", "/Ot"}
            prelinkcommands {"$(SolutionDir)\common\\build_luajit.cmd NUL"}

        configuration {"vs2005", "windirstat/WDS_Lua_C.c"}
            defines         ("_CRT_SECURE_NO_WARNINGS")
