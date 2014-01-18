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
    -- Override the project creation to suppress unnecessary configurations
    -- these get invoked by sln2005.generate per project ...
    -- ... they depend on the values in the sln.vstudio_configs table
    local function prjgen_override_factory(orig_prjgen)
        return function(prj)
            if prj.name:find('minilua') and type(prj.solution.vstudio_configs) == "table" then
                local cfgs = prj.solution.vstudio_configs
                local faked_cfgs = {}
                for k,v in pairs(cfgs) do
                    if v['name'] == "Release|Win32" then
                        faked_cfgs[1] = v
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
    -- Allow us to set the project configuration to Release|Win32 for minilua
    -- no matter what the global solution project is.
    local orig_project_platforms_sln2prj_mapping = premake.vstudio.sln2005.project_platforms_sln2prj_mapping
    premake.vstudio.sln2005.project_platforms_sln2prj_mapping = function(sln, prj, cfg, mapped)
        if prj.name:find('minilua') then
            _p('\t\t{%s}.%s.ActiveCfg = Release|Win32', prj.uuid, cfg.name)
            _p('\t\t{%s}.%s.Build.0 = Release|Win32',  prj.uuid, cfg.name)
        elseif prj.name:find('buildvm') then
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
--[[
function create_luajit_projects(basedir)
    local oldcurr = premake.CurrentContainer
    -- Define the solution
    premake.CurrentContainer = oldcurr -- restore container before this function was called
end
]]

solution ("luajit")
    configurations  {"Debug", "Release"}
    platforms       {"x32", "x64"}
    location        ('.')
    -- Single minilua for all configurations and platforms
    project ("minilua") -- required to build LuaJIT
        local int_dir       = "intermediate/" .. action .. "_$(" .. transformMN("Platform") .. ")_$(" .. transformMN("Configuration") .. ")"
        uuid                ("531911BC-0023-4EC6-A2CE-6C3F5C182647")
        language            ("C")
        kind                ("ConsoleApp")
        location            ("src")
        targetname          ("minilua")
        targetdir           ("src")
        flags               {"Optimize", "StaticRuntime", "NoManifest", "NoMinimalRebuild", "NoIncrementalLink", "NoEditAndContinue"}
        linkoptions         {"/release"}
        objdir              (int_dir)
        libdirs             {"$(IntDir)"}
        defines             {"NDEBUG", "_CRT_SECURE_NO_DEPRECATE"}
        files               {"src/host/minilua.c"}
    project ("buildvm") -- required to build LuaJIT
        local int_dir       = "intermediate/" .. action .. "_$(" .. transformMN("Platform") .. ")_$(" .. transformMN("Configuration") .. ")"
        local inc_dir       = "$(ProjectDir)host_$(" .. transformMN("Platform") .. ")"
        uuid                ("F949C208-7A2E-4B1C-B74D-956E88542A26")
        language            ("C")
        kind                ("ConsoleApp")
        location            ("src")
        targetname          ("buildvm")
        targetdir           ("src")
        includedirs         {"$(ProjectDir)", "$(ProjectDir)..\\dynasm", inc_dir}
        flags               {"Optimize", "StaticRuntime", "NoManifest", "NoMinimalRebuild", "NoIncrementalLink", "NoEditAndContinue"}
        objdir              (int_dir)
        libdirs             {"$(IntDir)"}
        defines             {"NDEBUG", "_CRT_SECURE_NO_DEPRECATE"}
        links               ("minilua") -- make sure we have minilua.exe
        files
        {
            "src/host/buildvm*.c",
            "src/host/buildvm*.h",
        }
        prebuildcommands    {"if not exist \""..inc_dir.."\" md \""..inc_dir.."\""}
        configuration {"x32"}
            targetsuffix    ("32")
            prebuildcommands{"minilua ..\\dynasm\\dynasm.lua -LN -D WIN -D JIT -D FFI -o \""..inc_dir.."\\buildvm_arch.h\" vm_x86.dasc"}
        configuration {"x64"}
            targetsuffix    ("64")
            prebuildcommands{"minilua ..\\dynasm\\dynasm.lua -LN -D WIN -D JIT -D FFI -D P64 -o \"" .. inc_dir .. "\\buildvm_arch.h\" vm_x86.dasc"}
