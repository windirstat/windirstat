-- The below is used to insert the .vs(2005|2008|2010|2012|2013) into the file names for projects and solutions
local action = _ACTION or ""
do
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
    -- Make sure we do not incremental linking for the resource DLLs
    local orig_config_isincrementallink = premake.config.isincrementallink
    premake.config.isincrementallink = function(cfg)
        if cfg.project.name:find('wdsr') and cfg.flags.NoIncrementalLink then
            return false
        end
        return orig_config_isincrementallink(cfg)
    end
    -- Override the project creation to suppress unnecessary configurations
    -- these get invoked by sln2005.generate per project ...
    -- ... they depend on the values in the sln.vstudio_configs table
    local mprj = {["wdsr%x*"] = {["Release|Win32"] = 0}}
    local function prjgen_override_factory(orig_prjgen)
        return function(prj)
            local function prjmap()
                for k,v in pairs(mprj) do
                    if prj.name:find(k) or prj.name:match(k) then
                        return v
                    end
                end
                return nil
            end
            if prjmap() and type(prj.solution.vstudio_configs) == "table" then
                local cfgs = prj.solution.vstudio_configs
                local faked_cfgs = {}
                local prjmap = prjmap()
                for k,v in pairs(cfgs) do
                    if prjmap[v['name']] then
                        faked_cfgs[#faked_cfgs+1] = v
                    end
                end
                prj.solution.vstudio_configs = faked_cfgs
                retval = orig_prjgen(prj)
                prj.solution.vstudio_configs = cfgs
                return retval
            end
            return orig_prjgen(prj)
        end
    end
    premake.vs2010_vcxproj = prjgen_override_factory(premake.vs2010_vcxproj)
    premake.vstudio.vc200x.generate = prjgen_override_factory(premake.vstudio.vc200x.generate)
    -- Allow us to set the project configuration to Release|Win32 for the resource DLL projects,
    -- no matter what the global solution project is.
    local orig_project_platforms_sln2prj_mapping = premake.vstudio.sln2005.project_platforms_sln2prj_mapping
    premake.vstudio.sln2005.project_platforms_sln2prj_mapping = function(sln, prj, cfg, mapped)
        if prj.name:find('wdsr') then
            _p('\t\t{%s}.%s.ActiveCfg = Release|Win32', prj.uuid, cfg.name)
            _p('\t\t{%s}.%s.Build.0 = Release|Win32',  prj.uuid, cfg.name)
        else
            _p('\t\t{%s}.%s.ActiveCfg = %s|%s', prj.uuid, cfg.name, cfg.buildcfg, mapped)
            if mapped == cfg.platform or cfg.platform == "Mixed Platforms" then
                _p('\t\t{%s}.%s.Build.0 = %s|%s',  prj.uuid, cfg.name, cfg.buildcfg, mapped)
            end
        end
    end
end
local function transformMN(input) -- transform the macro names for older Visual Studio versions
    local new_map   = { vs2002 = 0, vs2003 = 0, vs2005 = 0, vs2008 = 0 }
    local replacements = { Platform = "PlatformName", Configuration = "ConfigurationName" }
    if new_map[action] ~= nil then
        for k,v in pairs(replacements) do
            if input:find(k) then
                input = input:gsub(k, v)
            end
        end
    end
    return input
end
newoption { trigger = "resources", description = "Also create projects for the resource DLLs." }
newoption { trigger = "sdk71", description = "Applies to VS 2005 and 2008. If you have the Windows 7 SP1\n                   SDK, use this to create projects for a feature-complete\n                   WinDirStat." }
newoption { trigger = "release", description = "Creates a solution suitable for a release build." }
if _OPTIONS["resources"] then
    print "INFO: Creating projects for resource DLLs."
end
local release = false
local slnname = "wds_release"
if _OPTIONS["release"] then
    print "INFO: Creating release build solution."
    _OPTIONS["resources"] = ""
    _OPTIONS["sdk71"] = ""
    release = true
end
local function inc(inc_dir)
    include(inc_dir)
    create_luajit_projects(inc_dir, iif(release, slnname .. "_", ""))
end

solution (iif(release, slnname, "windirstat"))
    configurations  (iif(release, {"Release"}, {"Debug", "Release"}))
    platforms       {"x32", "x64"}
    location        ('.')

    -- Include the LuaJIT projects
    inc("3rdparty\\lua")

    -- Main WinDirStat project
    project (iif(release, slnname, "windirstat"))
        local int_dir   = "intermediate/" .. action .. "_$(" .. transformMN("Platform") .. ")_$(" .. transformMN("Configuration") .. ")\\$(ProjectName)"
        uuid            ("BD11B94C-6594-4477-9FDF-2E24447D1F14")
        language        ("C++")
        kind            ("WindowedApp")
        location        ("windirstat")
        targetname      ("wds")
        flags           {"StaticRuntime", "Unicode", "MFC", "NativeWChar", "ExtraWarnings", "NoRTTI", "WinMain", "NoMinimalRebuild", "NoIncrementalLink", "NoEditAndContinue"} -- "No64BitChecks", "NoManifest", "NoExceptions" ???
        targetdir       ("build")
        includedirs     {".", "windirstat", "common", "windirstat/Controls", "windirstat/Dialogs", "3rdparty/lua/src"}
        objdir          (int_dir)
        libdirs         {"$(IntDir)"}
        links           {"htmlhelp", "psapi", "delayimp", iif(release, slnname .. "_luajit2", "luajit2")}
        resoptions      {"/nologo", "/l409"}
        resincludedirs  {".", "$(IntDir)"}
        linkoptions     {"/delayload:psapi.dll", "/pdbaltpath:%_PDB%"}
        if release then
            postbuildcommands
            {
                "signtool.exe sign /v /a /ph /d \"WinDirStat\" /du \"http://windirstat.info\" /tr http://www.startssl.com/timestamp \"$(TargetPath)\""
            }
        end
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
            "*.txt", "*.rst",
            "common/BUILD",
            "common/*.cmd",
            "premake4.lua",
        }

        excludes
        {
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
            ["*"] = { "*.txt", "*.rst" },
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
            flags           {"Optimize", "Symbols"}
            linkoptions     {"/release"}
            buildoptions    {"/Oi", "/Ot"}

        configuration {"vs2005", "windirstat/WDS_Lua_C.c"}
            defines         ("_CRT_SECURE_NO_WARNINGS") -- _CRT_SECURE_NO_DEPRECATE, _SCL_SECURE_NO_WARNINGS, _AFX_SECURE_NO_WARNINGS and _ATL_SECURE_NO_WARNINGS???

        configuration {"vs2013"}
            defines         {"WINVER=0x0501"}

        configuration {"vs2002 or vs2003 or vs2005 or vs2008 or vs2010 or vs2012"}
            defines         {"WINVER=0x0500"}

        if _OPTIONS["sdk71"] then
            configuration {"vs2005 or vs2008"}
                defines         {"HAVE_WIN7_SDK=1"}
                if action == "vs2005" or action == "vs2008" then
                    print "INFO: Assuming Windows 7 SP1 SDK is installed (#define HAVE_WIN7_SDK=1)."
                end
        end

    -- Add the resource DLL projects, if requested
    if _OPTIONS["resources"] then
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
                prj = project(iif(release, slnname .. "_" .. nm, nm))
                    local int_dir   = "intermediate/" .. action .. "_$(ProjectName)_" .. nm
                    uuid            (guid)
                    language        ("C++")
                    kind            ("SharedLib")
                    location        (nm)
                    flags           {"NoImportLib", "Unicode", "NoManifest", "NoExceptions", "NoPCH", "NoIncrementalLink"}
                    objdir          (int_dir)
                    targetdir       ("build")
                    targetextension (".wdslng")
                    resoptions      {"/nologo", "/l409"}
                    resincludedirs  {".", nm, "$(IntDir)"} -- ATTENTION: FAULTY IN premake-stable ... needs to be addressed
                    linkoptions     {"/noentry"}
                    files
                    {
                        nm .. "/*.txt", nm .. "/*.rst",
                        nm .. "/windirstat.rc",
                        nm .. "/res/windirstat.rc2",
                        "common/version.h",
                        "windirstat/res/*.bmp",
                        "windirstat/res/*.cur",
                        "windirstat/res/*.ico",
                        "windirstat/res/*.txt",
                        "windirstat/resource.h",
                    }
                    vpaths
                    {
                        ["Header Files/*"] = { "windirstat/*.h", "common/*.h", nm .. "/*.h" },
                        ["Resource Files/*"] = { nm .. "/windirstat.rc", nm .. "/res/windirstat.rc2" },
                        ["Resource Files/embedded/*"] = { "windirstat/res/*" },
                        ["*"] = { nm .. "/*.txt", nm .. "/*.rst" },
                    }
            end
            premake.CurrentContainer = oldcurr
        end
    end
