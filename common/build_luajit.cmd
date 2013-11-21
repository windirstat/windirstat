@echo off
@if not "%OS%"=="Windows_NT" @(echo This script requires Windows NT 4.0 or later to run properly! & goto :EOF)
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: 2013, Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
:::
::: DISCLAIMER: Disclaimer: This software is provided 'as-is', without any
:::             express or implied warranty. In no event will the author be
:::             held liable for any damages arising from the use of this
:::             software.
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:: LuaJIT source directory relative to this script
set LUAJITSRC=%~dp0..\lua\src
:: Change into the LuaJIT src directory
setlocal & pushd "%LUAJITSRC%"
:: First parameter is the target folder where we put the resulting static lib
set TGTDIR=%~dpnx1
:: Shift by one, now %1 is either empty or 'debug'
shift
if exist "%TGTDIR%\lua51.lib" @(
  if "%~1" == "force" @(
    echo Forced rebuild
    del /f "%TGTDIR%\lua51.lib"
    shift
  ) else @(
    echo Skipping build, found static lib %TGTDIR%\lua51.lib
    goto :RETURN
  )
)
:: Purge the files because we build for different configurations
for %%i in (*.obj *.ilk *.pdb *.exe *.dll *.exp *.lib host\buildvm_arch.h jit\vmdef.lua lj_bcdef.h lj_ffdef.h lj_folddef.h lj_libdef.h lj_recdef.h) do @(
  if exist "%%i" @(
    echo Removing file %%i
    del /f "%%i"
  )
)
echo Calling LuaJIT build script: msvcbuild.bat %~1 static
call msvcbuild.bat %~1 static
:: Now move out the resulting .lib file and luajit.exe
for %%i in (lua51.lib luajit.exe) do @(
  echo Moving %%i into %TGTDIR% 
  move "%%i" "%TGTDIR%\"
)
:: Back to normal
:RETURN
popd & endlocal
