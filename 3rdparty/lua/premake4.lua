-- The below is used to insert the .vs(2005|2008|2010|2012|2013) into the file names for projects and solutions
local standalone = false
local action = _ACTION or ""
if premake.CurrentContainer == nil then
    -- Name the project files after their VS version
    local orig_getbasename = premake.project.getbasename
    premake.project.getbasename = function(prjname, pattern)
        if _ACTION then
            -- Using vc instead of vs here to avoid collisions
            name_map = {vs2005 = "8", vs2008 = "9", vs2010 = "10", vs2012 = "11", vs2013 = "12"}
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
end
do
    -- Override the project creation to suppress unnecessary configurations
    -- these get invoked by sln2005.generate per project ...
    -- ... they depend on the values in the sln.vstudio_configs table
    local mprj = {["minilua"] = {["Release|Win32"] = 0}, ["buildvm"] = {["Release|Win32"] = 0, ["Release|x64"] = 0}, ["luajit2"] = {["Release|Win32"] = 0, ["Release|x64"] = 0}, ["lua"] = {["Release|Win32"] = 0, ["Release|x64"] = 0}}
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
    -- Allow us to set the project configuration to Release|Win32 for minilua
    -- no matter what the global solution project is.
    local orig_project_platforms_sln2prj_mapping = premake.vstudio.sln2005.project_platforms_sln2prj_mapping
    premake.vstudio.sln2005.project_platforms_sln2prj_mapping = function(sln, prj, cfg, mapped)
        if prj.name:find('minilua') then
            _p('\t\t{%s}.%s.ActiveCfg = Release|Win32', prj.uuid, cfg.name)
            _p('\t\t{%s}.%s.Build.0 = Release|Win32',  prj.uuid, cfg.name)
        elseif prj.name:find('buildvm') or prj.name:find('luajit2') or prj.name:find('lua') then
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
local function fmt(msg, ...)
    return string.format(msg, unpack(arg))
end

function create_luajit_projects(basedir)
    local bd = ""
    local offs = "" -- relative path, calculated based on slashes and backslashes in bd (basedir after normalization)
    if basedir ~= nil then
        bd = basedir:gsub("[\\/]+$", "") .. "\\"
        offs = bd:gsub("[^\\/]+", ""):gsub(".", "..\\")
    end
    local oldcurr = premake.CurrentContainer
    local int_dir           = fmt("intermediate\\%s_$(%s)_$(%s)\\$(ProjectName)", action, transformMN("Platform"), transformMN("Configuration"))
    local inc_dir           = fmt("intermediate\\%s_$(%s)", action, transformMN("Platform"))
    -- Single minilua for all configurations and platforms
    project ("minilua") -- required to build LuaJIT
        uuid                ("531911BC-0023-4EC6-A2CE-6C3F5C182647")
        language            ("C")
        kind                ("ConsoleApp")
        location            (bd.."src")
        targetname          ("minilua")
        targetdir           (bd.."src")
        flags               {"Optimize", "StaticRuntime", "NoManifest", "NoMinimalRebuild", "NoIncrementalLink", "NoEditAndContinue"}
        linkoptions         {"/release"}
        objdir              (int_dir)
        libdirs             {"$(IntDir)"}
        defines             {"NDEBUG", "_CRT_SECURE_NO_DEPRECATE"}
        vpaths              {["Header Files/*"] = { "src/host/*.h" }, ["Source Files/*"] = { "src/host/*.c" },}
        files               {bd.."src/host/minilua.c"}
    project ("buildvm") -- required to build LuaJIT
        uuid                ("F949C208-7A2E-4B1C-B74D-956E88542A26")
        language            ("C")
        kind                ("ConsoleApp")
        location            (bd.."src")
        targetname          ("buildvm")
        targetdir           (bd.."src")
        includedirs         {"$(ProjectDir)", "$(ProjectDir)..\\dynasm", inc_dir}
        flags               {"Optimize", "StaticRuntime", "NoManifest", "NoMinimalRebuild", "NoIncrementalLink", "NoEditAndContinue"}
        objdir              (int_dir)
        libdirs             {"$(IntDir)"}
        defines             {"NDEBUG", "_CRT_SECURE_NO_DEPRECATE"}
        links               ("minilua") -- make sure we have minilua.exe
        vpaths              {["Header Files/*"] = { bd.."src/host/*.h" }, ["Source Files/*"] = { bd.."src/host/*.c" },}
        files               {bd.."src/host/buildvm*.c", bd.."src/host/buildvm*.h",}
        -- Add the pre-build steps required to compile and link the static library
        local prebuild_table= {[32] = "", [64] = " -D P64"}
        for k,v in pairs(prebuild_table) do
            configuration {fmt("x%d", k)}
                targetsuffix    (fmt("%d", k))
                prebuildcommands(fmt("if not exist \"%s..\\%s\" md \"%s..\\%s\"", offs, inc_dir, offs, inc_dir))
                prebuildcommands(fmt("minilua ..\\dynasm\\dynasm.lua -LN -D WIN -D JIT -D FFI%s -o \"%s..\\%s\\buildvm_arch.h\" vm_x86.dasc", prebuild_table[k], offs, inc_dir))
        end
    project ("luajit2") -- actual LuaJIT2 static lib
        uuid                ("9F35C2BB-DF1E-400A-A829-AE34E1C91A70")
        language            ("C")
        kind                ("StaticLib")
        location            (bd.."src")
        targetname          (fmt("luajit2_$(%s)", transformMN("Platform")))
        targetdir           (offs.."build")
        includedirs         {"$(ProjectDir)", "$(ProjectDir)..\\dynasm", inc_dir}
        flags               {"StaticRuntime", "Optimize", "No64BitChecks"}
        objdir              (int_dir)
        libdirs             {"$(IntDir)"}
        defines             {"NDEBUG", "_CRT_SECURE_NO_DEPRECATE"}
        links               {"minilua", "buildvm"} -- make sure we have minilua.exe
        linkoptions         {"/nodefaultlib"}
        vpaths              {["Header Files/*"] = { bd.."src/*.h" }, ["Source Files/*"] = { bd.."src/*.c" },}
        files               {bd.."src/lib_*.c", bd.."src/lj_*.c", bd.."src/*.h",}
        -- Add the pre-build steps required to compile and link the static library
        local prebuild_table= {[32] = 0, [64] = 0}
        for k,v in pairs(prebuild_table) do
            local ALL_LIB       = "lib_base.c lib_math.c lib_bit.c lib_string.c lib_table.c lib_io.c lib_os.c lib_package.c lib_debug.c lib_jit.c lib_ffi.c"
            configuration {fmt("x%d", k)}
                prebuildcommands(fmt("if not exist \"%s..\\%s\" md \"%s..\\%s\"", offs, inc_dir, offs, inc_dir))
                prebuildcommands(fmt("buildvm%d -m peobj -o \"$(IntDir)\\lj_vm%d.obj\"", k, k))
                linkoptions     {fmt("\"$(IntDir)\\lj_vm%d.obj\"", k)}
                prebuildcommands(fmt("buildvm%d -m bcdef -o \"%s..\\%s\\lj_bcdef.h\" %s", k, offs, inc_dir, ALL_LIB))
                prebuildcommands(fmt("buildvm%d -m ffdef -o \"%s..\\%s\\lj_ffdef.h\" %s", k, offs, inc_dir, ALL_LIB))
                prebuildcommands(fmt("buildvm%d -m libdef -o \"%s..\\%s\\lj_libdef.h\" %s", k, offs, inc_dir, ALL_LIB))
                prebuildcommands(fmt("buildvm%d -m recdef -o \"%s..\\%s\\lj_recdef.h\" %s", k, offs, inc_dir, ALL_LIB))
                prebuildcommands(fmt("buildvm%d -m folddef -o \"%s..\\%s\\lj_folddef.h\" lj_opt_fold.c", k, offs, inc_dir))
        end
    if standalone then
        project ("lua") -- actual Lua executable that statically links LuaJIT2
            uuid                ("3A806ACF-62B5-4597-B934-ED2F98A4F115")
            language            ("C")
            kind                ("ConsoleApp")
            location            (bd.."src")
            targetname          ("luajit")
            targetdir           (offs.."build")
            includedirs         {"$(ProjectDir)"}
            flags               {"StaticRuntime", "Optimize", "NoMinimalRebuild", "NoIncrementalLink", "NoEditAndContinue", "No64BitChecks", "Symbols"}
            objdir              (int_dir)
            libdirs             {"$(IntDir)"}
            defines             {"NDEBUG", "_CRT_SECURE_NO_DEPRECATE"}
            links               {"luajit2"}
            linkoptions         {"/pdbaltpath:%_PDB%"}
            vpaths              {["Header Files/*"] = { bd.."src/*.h" }, ["Source Files/*"] = { bd.."src/*.c" },}
            files               {bd.."src/luajit.c",}
            configuration {"Release", "x32"}
                targetsuffix    ("32")
            configuration {"Release", "x64"}
                targetsuffix    ("64")
    end
    premake.CurrentContainer = oldcurr -- restore container before this function was called
end

if premake.CurrentContainer == nil then
    standalone = true
    solution ("luajit")
        configurations  {"Release"}
        platforms       {"x32", "x64"}
        location        ('.')
        create_luajit_projects(nil)
end
