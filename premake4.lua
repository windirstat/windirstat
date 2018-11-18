--[[
        This premake4.lua _requires_ windirstat/premake-stable to work properly.
        If you don't want to use the code-signed build that can be found in the
        ./common/ subfolder, you can build from the WDS-branch over at:

        https://bitbucket.org/windirstat/premake-stable
  ]]
local assemblyName = "WinDirStat_Team.WinDirStat.windirstat"
local programVersion = "1.3" -- until we find a clever way to put this into an environment variable or so ...
local publicKeyToken = "db89f19495b8f232" -- the token for the code-signing, TODO/FIXME: verify!
local action = _ACTION or ""
local release = false
local slnname = ""
local pfx = ""
if _OPTIONS["resources"] then
    print "INFO: Creating projects for resource DLLs."
end
if _OPTIONS["release"] then
    print "INFO: Creating release build solution."
    _OPTIONS["resources"] = ""
    _OPTIONS["sdk71"] = ""
    release = true
    slnname = "wds_release"
    pfx = slnname .. "_"
    _OPTIONS["release"] = pfx
end
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
    -- Make UUID generation for filters deterministic
    if os.str2uuid ~= nil then
        local vc2010 = premake.vstudio.vc2010
        vc2010.filteridgroup = function(prj)
            local filters = { }
            local filterfound = false

            for file in premake.project.eachfile(prj) do
                -- split the path into its component parts
                local folders = string.explode(file.vpath, "/", true)
                local path = ""
                for i = 1, #folders - 1 do
                    -- element is only written if there *are* filters
                    if not filterfound then
                        filterfound = true
                        _p(1,'<ItemGroup>')
                    end

                    path = path .. folders[i]

                    -- have I seen this path before?
                    if not filters[path] then
                        local seed = path .. (prj.uuid or "")
                        local deterministic_uuid = os.str2uuid(seed)
                        filters[path] = true
                        _p(2, '<Filter Include="%s">', path)
                        _p(3, '<UniqueIdentifier>{%s}</UniqueIdentifier>', deterministic_uuid)
                        _p(2, '</Filter>')
                    end

                    -- prepare for the next subfolder
                    path = path .. "\\"
                end
            end
            
            if filterfound then
                _p(1,'</ItemGroup>')
            end
        end
    end
    -- Name the project files after their VS version
    local orig_getbasename = premake.project.getbasename
    premake.project.getbasename = function(prjname, pattern)
        -- The below is used to insert the .vs(8|9|10|11|12|14|15) into the file names for projects and solutions
        if _ACTION then
            name_map = {vs2005 = "vs8", vs2008 = "vs9", vs2010 = "vs10", vs2012 = "vs11", vs2013 = "vs12", vs2015 = "vs14", vs2017 = "vs15"}
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
        if cfg.project.name:find(pfx..'wdsr') and cfg.flags.NoIncrementalLink then
            return false
        end
        return orig_config_isincrementallink(cfg)
    end
    -- We need full debug info for VS2017
    if action == "vs2017" then
        local orig_vc2010_link = premake.vstudio.vc2010.link
        premake.vstudio.vc2010.link = function(cfg)
            if cfg.flags.Symbols ~= nil and cfg.flags.Symbols then
                local old_captured = io.captured -- save io.captured state
                io.capture() -- empties io.captured
                orig_vc2010_link(cfg)
                local captured = io.endcapture()
                local indent = io.indent .. io.indent .. io.indent
                assert(io.indent ~= nil, "io.indent must not be nil at this point!")
                captured = captured:gsub("true(</GenerateDebugInformation>)", "DebugFull%1\n" .. string.format("%s<FullProgramDatabaseFile>%s</FullProgramDatabaseFile>", indent, tostring(cfg.flags.Symbols ~= nil)))
                if old_captured ~= nil then
                    io.captured = old_captured .. captured -- restore outer captured state, if any
                else
                    io.write(captured)
                end
            else
                orig_vc2010_link(cfg)
            end
        end
    end
    -- We want to output the file with UTF-8 BOM
    local orig_vc2010_header = premake.vstudio.vc2010.header
    premake.vstudio.vc2010.header = function(targets)
        local old_captured = io.captured -- save io.captured state
        io.capture() -- empties io.captured
        orig_vc2010_header(targets)
        local captured = io.endcapture()
        if old_captured ~= nil then
            io.captured = old_captured .. "\239\187\191" .. captured -- restore outer captured state, if any
        else
            io.write("\239\187\191")
            io.write(captured)
        end
    end
    -- Make sure we can generate XP-compatible projects for newer Visual Studio versions
    local orig_vc2010_configurationPropertyGroup = premake.vstudio.vc2010.configurationPropertyGroup
    premake.vstudio.vc2010.configurationPropertyGroup = function(cfg, cfginfo)
        local old_captured = io.captured -- save io.captured state
        io.capture() -- empties io.captured
        orig_vc2010_configurationPropertyGroup(cfg, cfginfo)
        local captured = io.endcapture()
        local toolsets = { vs2012 = "v110", vs2013 = "v120", vs2015 = "v140", vs2017 = "v141" }
        local toolset = toolsets[_ACTION]
        if toolset then
            if _OPTIONS["xp"] then
                toolset = toolset .. "_xp"
                captured = captured:gsub("(</PlatformToolset>)", "_xp%1")
            end
        end
        if old_captured ~= nil then
            io.captured = old_captured .. captured -- restore outer captured state, if any
        else
            io.write(captured)
        end
    end
    -- Override the project creation to suppress unnecessary configurations
    -- these get invoked by sln2005.generate per project ...
    -- ... they depend on the values in the sln.vstudio_configs table
    --[[
    local mprj = {[pfx.."wdsr%x*"] = {["Release|Win32"] = 0}, [pfx.."minilua"] = {["Release|Win32"] = 0}, [pfx.."buildvm"] = {["Release|Win32"] = 0, ["Release|x64"] = 0}, [pfx.."luajit2"] = {["Release|Win32"] = 0, ["Release|x64"] = 0}, [pfx.."lua"] = {["Release|Win32"] = 0, ["Release|x64"] = 0}}
    if _OPTIONS["dev"] then
        mprj = {[pfx.."wdsr%x*"] = {["Release|Win32"] = 0},}
    end
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
                    if prjmap[ v['name'] ] then
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
    --]]
    -- Borrowed from setLocal() at https://stackoverflow.com/a/22752379
    local function getLocal(stkidx, name)
        local index = 1
        while true do
            local var_name, var_value = debug.getlocal(stkidx, index)
            if not var_name then break end
            if var_name == name then 
                return var_value
            end
            index = index + 1
        end
    end
    -- resdefines takes no effect in VS201x solutions, let's fix that.
    local orig_premake_vs2010_vcxproj = premake.vs2010_vcxproj
    premake.vs2010_vcxproj = function(prj)
        -- The whole stunt below is necessary in order to modify the resource_compile()
        -- output. Given it's a local function we have to go through hoops.
        local orig_p = _G._p
        local besilent = false
        -- We patch the global _p() function
        _G._p = function(indent, msg, ...)
            -- Look for indent values of 2
            if indent == 2 and msg ~= nil then
                -- ... with msg value of <ResourceCompile>
                if msg == "<ResourceCompile>" then
                    local cfg = getLocal(3, "e") -- with LuaSrcDiet
                    if cfg == nil then
                        cfg = getLocal(3, "cfg") -- without LuaSrcDiet
                    end
                    assert(type(cfg) == "table" and cfg["resdefines"] ~= nil)
                    orig_p(indent, msg, ...) -- spit the original line out
                    local indent = indent + 1
                    if #cfg.defines > 0 or #cfg.resdefines then
                        local defines = table.join(cfg.defines, cfg.resdefines)
                        orig_p(indent,'<PreprocessorDefinitions>%s;%%(PreprocessorDefinitions)</PreprocessorDefinitions>'
                            ,premake.esc(table.concat(premake.esc(defines), ";")))
                    else
                        orig_p(indent,'<PreprocessorDefinitions></PreprocessorDefinitions>')
                    end
                    if #cfg.includedirs > 0 or #cfg.resincludedirs > 0 then
                        local dirs = table.join(cfg.includedirs, cfg.resincludedirs)
                        orig_p(indent,'<AdditionalIncludeDirectories>%s;%%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>'
                                ,premake.esc(path.translate(table.concat(dirs, ";"), '\\')))
                    end
                    besilent = true
                end
                -- ... or msg value of <ResourceCompile>
                if msg == "</ResourceCompile>" then
                    besilent = false
                    -- fall through
                end
            end
            if not besilent then -- should we be silent?
                orig_p(indent, msg, ...)
            end
        end
        orig_premake_vs2010_vcxproj(prj)
        _G._p = orig_p -- restore in any case
    end
    -- Allow us to set the project configuration to Release|Win32 for the resource DLL projects,
    -- no matter what the global solution project is.
    local orig_project_platforms_sln2prj_mapping = premake.vstudio.sln2005.project_platforms_sln2prj_mapping
    premake.vstudio.sln2005.project_platforms_sln2prj_mapping = function(sln, prj, cfg, mapped)
        if prj.name:find(pfx..'wdsr') then
            _p('\t\t{%s}.%s.ActiveCfg = Release|Win32', prj.uuid, cfg.name)
            if release and mapped == "Win32" and cfg.name == "Release|Win32" then
                _p('\t\t{%s}.%s.Build.0 = Release|Win32',  prj.uuid, cfg.name)
            end
        elseif prj.name:find(pfx..'minilua') then
            _p('\t\t{%s}.%s.ActiveCfg = Release|Win32', prj.uuid, cfg.name)
            _p('\t\t{%s}.%s.Build.0 = Release|Win32',  prj.uuid, cfg.name)
        elseif prj.name:find(pfx..'buildvm') or prj.name:find(pfx..'luajit2') or prj.name:find(pfx..'lua') then
            _p('\t\t{%s}.%s.ActiveCfg = Release|%s', prj.uuid, cfg.name, mapped)
            _p('\t\t{%s}.%s.Build.0 = Release|%s',  prj.uuid, cfg.name, mapped)
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
local function inc(inc_dir)
    include(inc_dir)
    create_luajit_projects(inc_dir)
end
newoption { trigger = "resources", description = "Also create projects for the resource DLLs." }
newoption { trigger = "sdk71", description = "Applies to VS 2005 and 2008. If you have the Windows 7 SP1\n                   SDK, use this to create projects for a feature-complete\n                   WinDirStat." }
newoption { trigger = "release", description = "Creates a solution suitable for a release build." }
newoption { trigger = "dev", description = "Add projects only relevant during development." }
newoption { trigger = "xp", description = "Enable XP-compatible build for newer Visual Studio versions." }

--_CRT_SECURE_NO_WARNINGS, _CRT_SECURE_NO_DEPRECATE, _SCL_SECURE_NO_WARNINGS, _AFX_SECURE_NO_WARNINGS and _ATL_SECURE_NO_WARNINGS???
solution (iif(release, slnname, "windirstat"))
    configurations  (iif(release, {"Release"}, {"Debug", "Release"}))
    platforms       {"x32", "x64"}
    location        ('.')

    -- Include the LuaJIT projects
    inc("3rdparty\\lua")

    -- Main WinDirStat project
    project (iif(release, slnname, "windirstat"))
        local int_dir   = pfx.."intermediate/" .. action .. "_$(" .. transformMN("Platform") .. ")_$(" .. transformMN("Configuration") .. ")\\$(ProjectName)"
        uuid            ("BD11B94C-6594-4477-9FDF-2E24447D1F14")
        language        ("C++")
        kind            ("WindowedApp")
        location        ("windirstat")
        targetname      ("wds")
        flags           {"StaticRuntime", "Unicode", "MFC", "NativeWChar", "ExtraWarnings", "NoRTTI", "WinMain",}
        targetdir       (iif(release, slnname, iif(action == "vs2005", "build", "build." .. action)))
        includedirs     {".", "windirstat", "common", "windirstat/Controls", "windirstat/Dialogs", "3rdparty/lua/src"}
        objdir          (int_dir)
        links           {"psapi", "delayimp", pfx.."luajit2"}
        resoptions      {"/nologo", "/l409"}
        resincludedirs  {".", "$(IntDir)"}
        linkoptions     {"/delayload:psapi.dll", "/pdbaltpath:%_PDB%"}
        prebuildcommands{"if not exist \"$(SolutionDir)common\\hgid.h\" call \"$(SolutionDir)\\common\\hgid.cmd\"",}
        if release then
            postbuildcommands
            {
                "ollisign.cmd -a \"$(TargetPath)\" \"https://windirstat.net\" \"WinDirStat\""
            }
        end
        files
        {
            "common/hgid.h",
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
            "*.txt", "*.md",
            "common/version.rc",
            "common/*.cmd",
            "premake4.lua",
        }

        excludes
        {
            "common/tracer.cpp", -- this one gets an #include via windirstat.cpp
        }
        
        vpaths
        {
            ["Header Files/Common/*"] = { "common/*.h" },
            ["Header Files/Controls/*"] = { "windirstat/Controls/*.h" },
            ["Header Files/Dialogs/*"] = { "windirstat/Dialogs/*.h" },
            ["Header Files/*"] = { "windirstat/*.h" },
            ["Resource Files/Resources/*"] = { "windirstat/res/*.*" },
            ["Resource Files/*"] = { "common/*.rc", "windirstat/*.rc" },
            ["Source Files/Common/*"] = { "common/*.cpp" },
            ["Source Files/Lua/*"] = { "windirstat/WDS_Lua_C.c" },
            ["Source Files/Controls/*"] = { "windirstat/Controls/*.cpp" },
            ["Source Files/Dialogs/*"] = { "windirstat/Dialogs/*.cpp" },
            ["Source Files/*"] = { "windirstat/*.cpp" },
            ["Special Files/*"] = { "common/BUILD", "common/*.cmd", "premake4.lua", "*.cmd" },
            ["*"] = { "*.txt", "*.md" },
        }

        configuration {"Debug", "x32"}
            resdefines      {"MODNAME=wds32D"}
            targetsuffix    ("32D")

        configuration {"Debug", "x64"}
            resdefines      {"MODNAME=wds64D"}
            targetsuffix    ("64D")

        configuration {"Release", "x32"}
            resdefines      {"MODNAME=wds32"}
            targetsuffix    ("32")

        configuration {"Release", "x64"}
            resdefines      {"MODNAME=wds64"}
            targetsuffix    ("64")

        configuration {"Debug"}
            defines         {"_DEBUG", "VTRACE_TO_CONSOLE=1", "VTRACE_DETAIL=2"}
            flags           {"Symbols"}
            linkoptions     {"/nodefaultlib:libcmt",}

        configuration {"Release"}
            defines         ("NDEBUG")
            flags           {"Optimize", "Symbols", "NoMinimalRebuild", "NoIncrementalLink", "NoEditAndContinue"}
            linkoptions     {"/release"}
            buildoptions    {"/Oi", "/Ot"}

        configuration {"vs*"}
            defines         {"WINVER=0x0501", "LUA_REG_NO_WINTRACE", "LUA_REG_NO_HIVEOPS", "LUA_REG_NO_DLL"}

        configuration {"vs2015 or vs2017"}
            defines         {"_ALLOW_RTCc_IN_STL"}

        if _OPTIONS["sdk71"] then
            configuration {"vs2005 or vs2008"}
                defines         {"HAVE_WIN7_SDK=1"}
                if action == "vs2005" or action == "vs2008" then
                    print "INFO: Assuming Windows 7 SP1 SDK is installed (#define HAVE_WIN7_SDK=1)."
                end
        end

    if _OPTIONS["dev"] then
        project (pfx.."luaconf")
            local int_dir   = pfx.."intermediate/" .. action .. "_$(" .. transformMN("Platform") .. ")_$(" .. transformMN("Configuration") .. ")\\$(ProjectName)"
            uuid            ("66A24518-ACE0-4C57-96B0-FF9F324E0985")
            language        ("C++")
            kind            ("ConsoleApp")
            location        ("sandbox/luaconf")
            targetname      ("luaconf")
            flags           {"StaticRuntime", "Unicode", "MFC", "NativeWChar", "ExtraWarnings", "NoRTTI", "WinMain", "NoMinimalRebuild", "NoIncrementalLink", "NoEditAndContinue"}
            targetdir       (iif(release, slnname, iif(action == "vs2005", "build", "build." .. action)))
            includedirs     {"windirstat", "common", "3rdparty/lua/src", "sandbox/luaconf"}
            objdir          (int_dir)
            libdirs         {"$(IntDir)"}
            links           {pfx.."luajit2"}
            resoptions      {"/nologo", "/l409"}
            resincludedirs  {".", "$(IntDir)"}
            linkoptions     {"/pdbaltpath:%_PDB%"}
            postbuildcommands{"xcopy /f /y \"$(ProjectDir)lua_conf.lua\" \"$(TargetDir)\""}

            files
            {
                "windirstat/WDS_Lua_C.c",
                "sandbox/luaconf/*.h",
                "sandbox/luaconf/*.rc",
                "sandbox/luaconf/*.cpp",
                "sandbox/luaconf/*.txt", "sandbox/luaconf/*.md",
            }

            vpaths
            {
                ["Header Files/*"] = { "sandbox/luaconf/*.h" },
                ["Resource Files/*"] = { "sandbox/luaconf/*.rc" },
                ["Source Files/*"] = { "sandbox/luaconf/*.cpp", "windirstat/WDS_Lua_C.c" },
                ["*"] = { "sandbox/luaconf/*.txt", "sandbox/luaconf/*.md" },
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
                defines         {"_DEBUG"}
                flags           {"Symbols"}
                linkoptions     {"/nodefaultlib:libcmt",}

            configuration {"Release"}
                defines         ("NDEBUG")
                flags           {"Optimize", "Symbols"}
                linkoptions     {"/release"}
                buildoptions    {"/Oi", "/Ot"}

            configuration {"vs*"}
                defines         {"WINVER=0x0501", "LUA_REG_NO_WINTRACE", "LUA_REG_NO_HIVEOPS", "LUA_REG_NO_DLL"}
    end

    -- Add the resource DLL projects, if requested
    if _OPTIONS["resources"] then
        do
            local oldcurr = premake.CurrentContainer
            local resource_dlls = {
                ["wdslng0405"] = "C3F39C58-7FC4-4243-82B2-A3572235AE02", -- Czech
                ["wdslng0407"] = "C8D9E4F9-7051-4B41-A5AB-F68F3FCE42E8", -- German
                ["wdslng040a"] = "23B76347-204C-4DE6-A311-F562CEF5D89C", -- Spanish
                ["wdslng040b"] = "C7A5D1EC-35D3-4754-A815-2C527CACD584", -- Finnish
                ["wdslng040c"] = "DA4DDD24-67BC-4A9D-87D3-18C73E5CAF31", -- French
                ["wdslng040e"] = "2A75AA20-BFFE-4D1C-8AEC-274823223919", -- Hungarian
                ["wdslng0410"] = "FD4194A7-EA1E-4466-A80B-AB4D8D17F33C", -- Italian
                ["wdslng0413"] = "70A55EB7-E109-41DE-81B4-0DF2B72DCDE9", -- Dutch
                ["wdslng0415"] = "70C09DAA-6F6D-4AAC-955F-ACD602A667CE", -- Polish
                ["wdslng0416"] = "025A9D90-4C61-4D34-8BEC-8A1A044B80EB", -- Portuguese (Brazil)
                ["wdslng0419"] = "7F06AAC4-9FBE-412F-B1D7-CB37AB8F311D", -- Russian
                ["wdslng0425"] = "2FADC62C-C670-4963-8B69-70ECA7987B93", -- Estonian
                }
            for nm,guid in pairs(resource_dlls) do
                local nmpfx = string.sub(nm, 7) -- hex prefix for the language resource directory
                local resdir = "windirstat/res/"
                local nmdirs = os.matchdirs(resdir .. nmpfx .. ".*") -- match the directory with the given prefix
                assert(#nmdirs == 1, "There can be only one directory per language ID")
                local nmdir = nmdirs[1] -- the relative path to the project, e.g. windirstat/res/0405.Czech
                local nmbase = string.sub(nmdir, #resdir + 1) -- just the basename, e.g. 0405.Czech
                premake.CurrentContainer = oldcurr
                project(pfx .. "wdsr" .. nmbase)
                    local int_dir   = pfx.."intermediate/" .. action .. "_$(ProjectName)_" .. nm
                    uuid            (guid)
                    language        ("C")
                    kind            ("SharedLib")
                    location        (nmdir)
                    flags           {"NoImportLib", "Unicode", "NoManifest", "NoExceptions", "NoPCH", "NoIncrementalLink"}
                    objdir          (int_dir)
                    targetdir       (iif(release, slnname, iif(action == "vs2005", "build", "build." .. action)))
                    targetname      ("wdsr" .. nmpfx)
                    targetextension (".wdslng")
                    resdefines      {"WDS_RESLANG=0x" .. nmpfx, "MODNAME=wdsr" .. nmpfx}
                    resoptions      {"/nologo", "/l409"}
                    resincludedirs  {".", "$(ProjectDir)", "$(IntDir)"}
                    linkoptions     {"/noentry"}
                    if release then
                        postbuildcommands
                        {
                            "ollisign.cmd -a \"$(TargetPath)\" \"https://windirstat.net\" \"WinDirStat\""
                        }
                    else
                        prebuildcommands{"if not exist \"$(SolutionDir)common\\hgid.h\" call \"$(SolutionDir)\\common\\hgid.cmd\"",}
                    end
                    files
                    {
                        nmdir .. "/*.rst",
                        nmdir .. "/windirstat.rc",
                        nmdir .. "/res/windirstat.rc2",
                        "common/version.rc",
                        "common/version.h",
                        "windirstat/res/*.bmp",
                        "windirstat/res/*.cur",
                        "windirstat/res/*.ico",
                        "windirstat/res/*.txt",
                        "windirstat/resource.h",
                    }
                    vpaths
                    {
                        ["Header Files/*"] = { "windirstat/*.h", "common/*.h", "windirstat/res/" .. nm .. "/*.h" },
                        ["Resource Files/*"] = { "common/version.rc" },
                        ["Resource Files/" .. nmbase .. "/*"] = { nmdir .. "/windirstat.rc", nmdir .. "/res/windirstat.rc2" },
                        ["Resource Files/embedded/*"] = { "windirstat/res/*" },
                        ["*"] = { nmdir .. "/*.rst" },
                    }
            end
            premake.CurrentContainer = oldcurr
        end
    end
