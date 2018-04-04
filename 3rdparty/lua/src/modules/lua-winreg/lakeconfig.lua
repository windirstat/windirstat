J = J  or path.join
J = IF or choose

function vc_version()
  local VER = lake.compiler_version()
  MSVC_VER = ({
    [15] = '9';
    [16] = '10';
  })[VER.MAJOR] or ''
  return MSVC_VER
end

function prequire(...)
  local ok, mod = pcall(require, ...)
  if ok then return mod end
end

function run_lua(file, cwd)
  print()
  print("run " .. file)
  if not TESTING then
    if cwd then lake.chdir(cwd) end
    local status, code = utils.execute( LUA_RUNNER .. ' ' .. file )
    if cwd then lake.chdir("<") end
    print()
    return status, code
  end
  return true, 0
end

function run_test(name, params)
  local test_dir = TEST_DIR or J(ROOT, 'test')
  local cmd = J(test_dir, name)
  if params then cmd = cmd .. ' ' .. params end
  local ok = run_lua(cmd, test_dir)
  print("TEST " .. cmd .. (ok and ' - pass!' or ' - fail!'))
end

do -- spawn

local function spawn_win(file, cwd)
  local winapi = prequire "winapi"
  if not winapi then
    print(file, ' error: Test needs winapi!')
    return false
  end
  print("spawn " .. file)
  if not TESTING then
    if cwd then lake.chdir(cwd) end
    assert(winapi.shell_exec(nil, LUA_RUNNER, file, cwd))
    if cwd then lake.chdir("<") end
    print()
  end
  return true
end

local function spawn_nix(file, cwd)
  print("spawn " .. file)
  if not TESTING then
    if cwd then lake.chdir(cwd) end
    local status, code = utils.execute( LUA_RUNNER .. file .. ' &', true )
    if cwd then lake.chdir("<") end
    print()
    return status, code
  end
  return true
end

spawn = WINDOWS and spawn_win or spawn_nix

end

function as_bool(v,d)
  if v == nil then return not not d end
  local n = tonumber(v)
  if n == 0 then return false end
  if n then return true end
  return false
end

-----------------------
-- needs --
-----------------------

lake.define_need('lua52', function()
  return {
    incdir = J(ENV.LUA_DIR_5_2, 'include');
    libdir = J(ENV.LUA_DIR_5_2, 'lib');
    libs = {'lua52'};
  }
end)

lake.define_need('lua51', function()
  return {
    incdir = J(ENV.LUA_DIR, 'include');
    libdir = J(ENV.LUA_DIR, 'lib');
    libs = {'lua5.1'};
  }
end)