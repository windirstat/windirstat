@echo off
@if not "%OS%"=="Windows_NT" (echo This script requires Windows NT 4.0 or later to run properly! & goto :EOF)
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: 2009-2023, Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
::: Available from Git mirror: <https://github.com/assarbad/scripts>
:::
::: PURPOSE:    This script can be used to run the vcvars32.bat/vcvarsall.bat
:::             from any of the existing Visual C++ versions starting with .NET
:::             (2002) through 2022 or versions (or a single version) given on
:::             the command line.
:::             The script will try to find the newest installed VC version by
:::             iterating over the space-separated (descending) list of versions
:::             in the variable SUPPORTED_VC below, by default.
:::             Call it from another script and after that you will have NMAKE,
:::             DEVENV.EXE and friends available without having to hardcode
:::             their paths into a script or makefile.
:::
::: DISCLAIMER: Disclaimer: This software is provided 'as-is', without any
:::             express or implied warranty. In no event will the author be
:::             held liable for any damages arising from the use of this
:::             software.
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:SCRIPT
setlocal & pushd .
:: Toolsets (potentially) supported
set SUPPORTED_TSET=amd64 x86 ia64 x86_ia64 x86_amd64 amd64_x86 x86_arm amd64_arm
:: Internal representation of the version number
set SUPPORTED_VC=17.0 ^
16.0 ^
15.0 ^
14.0 ^
12.0 ^
11.0 ^
10.0 ^
9.0 ^
8.0 ^
7.1 ^
7.0
:: Marketing name of the Visual Studio versions
set SUPPORTED_NICE=2022 ^
2019 ^
2017 ^
2015 ^
2013 ^
2012 ^
2010 ^
2008 ^
2005 ^
2003 ^
2002
set DEFAULT_TSET=x86
:: When the script runs on AMD64, we use that as default
if "%PROCESSOR_ARCHITECTURE%" == "AMD64" set DEFAULT_TSET=amd64
if not "%~1" == "" (
  if "%~1" == "/?"     goto :Help
  if "%~1" == "-?"     goto :Help
  if "%~1" == "/h"     goto :Help
  if "%~1" == "-h"     goto :Help
  if "%~1" == "/help"  goto :Help
  if "%~1" == "--help" goto :Help
)
if defined VCVER_FRIENDLY echo This script expects a clean environment. Don't run it several times in the same instance of CMD! Or use setlocal and endlocal in your own script to limit the effect of this one.&popd&endlocal&goto :EOF
set MIN_VC=7.0
set MAX_VC=16.0
set MIN_NICE=2002
reg /? > NUL 2>&1
if not ERRORLEVEL 0 echo "REG.EXE is a prerequisite but wasn't found!" & goto :EOF
set SETVCV_ERROR=0
:: First parameter may point to a particular toolset ...
if not "%~1" == "" (
  for %%i in (%SUPPORTED_TSET%) do (
    if "%~1" == "%%i" shift & call :SetVar VCTGT_TOOLSET %%i
  )
)
:: Fall back to default if not explicitly given
if not defined VCTGT_TOOLSET set VCTGT_TOOLSET=%DEFAULT_TSET%
:: Make the string appear a bit nicer, i.e. comma-separated
set SUPPORTED_PP=%SUPPORTED_NICE: =, %
:: Allow the version to be overridden on the command line
:: ... else find the VC versions in the order given by SUPPORTED_VC
if not "%~1" == "" (
  for %%i in (%~1) do (
    call :FindVC "%%i"
  )
) else (
  echo Trying to auto-detect supported MSVC version ^(%SUPPORTED_PP%^)
  echo HINT: pass one ^(or several^) of %SUPPORTED_PP% on the command line.
  echo       alternatively one ^(or several^) of:  %SUPPORTED_VC%
  echo.
  for %%i in (%SUPPORTED_VC%) do (
    call :FindVC "%%i"
  )
)
:: Check result and quit with error if necessary
if not defined VCVARS_PATH (
  if not "%~1" == "" (
    echo Requested version ^"%~1^" of Visual Studio not found.
  ) else (
    echo Could not find any supported version ^(%SUPPORTED_PP%^) of Visual C++.
  )
  popd & endlocal & exit /b %SETVCV_ERROR%
)
set ALLORNOT=%VCVARS_PATH:~-7,3%
if "%ALLORNOT%" == "all" set VCTGT_TOOLSET_ARG=%VCTGT_TOOLSET%
set WINSDK_TOOLSET=%VCTGT_TOOLSET:~0,5%
if "%WINSDK_TOOLSET%" == "amd64" set WINSDK_TOOLSET=x64
set WINSDK_TOOLSET=%WINSDK_TOOLSET:~0,3%
:: ... make sure the outside world sees the results (i.e. leave the scope)
popd & endlocal & if not "%VCVARS_PATH%" == "" @call "%VCVARS_PATH%" %VCTGT_TOOLSET_ARG% & if not "%VCVER_FRIENDLY%" == "" set VCVER_FRIENDLY=%VCVER_FRIENDLY%&set WINSDK_TOOLSET=%WINSDK_TOOLSET%
:: ... and make sure the resource compiler (rc.exe) can be used (by default it can't on VS2015)
rc.exe /? > NUL 2>&1
:: Didn't fail, so nothing for us to fix
if not ERRORLEVEL 1 goto :EOF
:: We need those to locate rc.exe
if not defined WindowsSdkDir goto :EOF
if not defined WindowsSDKVersion goto :EOF
"%WindowsSdkDir%\bin\%WindowsSDKVersion%%WINSDK_TOOLSET%\rc.exe" > NUL 2>&1
if ERRORLEVEL 1 goto :EOF
call :SetVar PATH "%PATH%;%WindowsSdkDir%\bin\%WindowsSDKVersion%%WINSDK_TOOLSET%"
goto :EOF

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: / FindVC subroutine
:::   Param1 == version identifier for VC
:::
:::   Sets the global variable VCVARS_PATH if it finds the installation.
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:FindVC
setlocal ENABLEEXTENSIONS & set VCVER=%~1
:: We're not interested in overwriting an already existing value
if defined VCVARS_PATH ( endlocal & goto :EOF )
:: Now let's distinguish the "nice" version numbers (2002, ... 2022) from the internal ones
set VCVER=%VCVER:vs=%
:: Not a "real" version number, but the marketing one (2002, ... 2022)?
if %VCVER% GEQ %MIN_NICE% call :NICE_%VCVER% > NUL 2>&1
set NUMVER=%VCVER:.0=%
set NUMVER=%NUMVER:.1=%
:: Figure out the set of supported toolsets
set VCVERLBL=%VCVER:.=_%
call :PRETTY_%VCVERLBL% > NUL 2>&1
call :TSET_%VCVERLBL% > NUL 2>&1
if not defined NICEVER ( echo ERROR: This script does not know the given version Visual C++ version&endlocal&set SETVCV_ERROR=1&goto :EOF )
:: Jump over those "subs"
goto :NICE_SET
:PRETTY_17_0
    set NICEVER=2022
    goto :EOF
:PRETTY_16_0
    set NICEVER=2019
    goto :EOF
:PRETTY_15_0
    set NICEVER=2017
    goto :EOF
:PRETTY_14_0
    set NICEVER=2015
    goto :EOF
:PRETTY_12_0
    set NICEVER=2013
    goto :EOF
:PRETTY_11_0
    set NICEVER=2012
    goto :EOF
:PRETTY_10_0
    set NICEVER=2010
    goto :EOF
:PRETTY_9_0
    set NICEVER=2008
    goto :EOF
:PRETTY_8_0
    set NICEVER=2005
    goto :EOF
:PRETTY_7_1
    set NICEVER=2003
    goto :EOF
:PRETTY_7_0
    set NICEVER=2002
    goto :EOF
:NICE_2022
    set VCVER=17.0
    set NEWVS=1
    goto :EOF
:NICE_2019
    set VCVER=16.0
    set NEWVS=1
    goto :EOF
:NICE_2017
    set VCVER=15.0
    set NEWVS=1
    goto :EOF
:NICE_2015
    set VCVER=14.0
    goto :EOF
:NICE_2013
    set VCVER=12.0
    goto :EOF
:NICE_2012
    set VCVER=11.0
    goto :EOF
:NICE_2010
    set VCVER=10.0
    goto :EOF
:NICE_2008
    set VCVER=9.0
    goto :EOF
:NICE_2005
    set VCVER=8.0
    goto :EOF
:NICE_2003
    set VCVER=7.1
    goto :EOF
:NICE_2002
    set VCVER=7.0
    goto :EOF
:TSET_16_0
:TSET_15_0
:TSET_14_0
:TSET_12_0
    set SUPPORTED_TSET=x86 amd64 arm x86_amd64 x86_arm amd64_x86 amd64_arm
    goto :EOF
:TSET_11_0
    set SUPPORTED_TSET=x86 amd64 arm x86_amd64 x86_arm
    goto :EOF
:TSET_10_0
:TSET_9_0
:TSET_8_0
    set SUPPORTED_TSET=x86 ia64 amd64 x86_amd64 x86_ia64
    goto :EOF
:NICE_SET
echo Trying to locate Visual C++ %NICEVER% ^(= %VCVER%, %VCTGT_TOOLSET%^)
:: Is it a version below 15? Then we use the old registry keys
if %NUMVER% LSS 15 goto :OLDVS
set /a NUMVERNEXT=%NUMVER%+1
echo Modern (^>=2017) Visual Studio
:: This is where we intend to find the installation path in the registry
set _VSWHERE_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer
set _VSWHERE=%_VSWHERE_PATH%\vswhere.exe
"%_VSWHERE%" -? > NUL 2>&1 || set _VSWHERE=vswhere.exe
"%_VSWHERE%" -? > NUL 2>&1 || goto :SKIPVSWHERE
set PATH=%_VSWHERE_PATH%;%PATH%
::@echo Trying to call vswhere.exe from ^(%_VSWHERE_PATH%^) with:
@echo Running: vswhere -products * -format value -property installationPath -version ^"^[%NUMVER%.0,%NUMVERNEXT%.0^)^"
if not defined _VCINSTALLDIR (
  for /f "usebackq tokens=*" %%i in (`vswhere -products * -format value -property installationPath -version "[%NUMVER%.0,%NUMVERNEXT%.0)"`) do (
    call :SetVar _VCINSTALLDIR "%%i\VC\"
  )
)
if defined _VCINSTALLDIR goto :DETECTION_FINISHED
:SKIPVSWHERE
if not "%NICEVER%" == "2017" goto :DETECTION_FINISHED
set _VSINSTALLKEY=HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7
if not defined _VCINSTALLDIR (
  for /f "tokens=2*" %%i in ('reg query "%_VSINSTALLKEY%" /v %VCVER% 2^> NUL') do (
    call :SetVar _VCINSTALLDIR "%%j"
  )
)
set _VSINSTALLKEY=HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7
:: If we haven't found it by now, try the WOW64 "Software" key
if not defined _VCINSTALLDIR (
  for /f "tokens=2*" %%i in ('reg query "%_VSINSTALLKEY%" /v %VCVER% 2^> NUL') do (
    call :SetVar _VCINSTALLDIR "%%j"
  )
)
goto :DETECTION_FINISHED
:OLDVS
:: echo Old (^<2017) Visual Studio
:: The versions of Visual Studio prior to 2017 were all using this key
set _VSINSTALLKEY=HKLM\SOFTWARE\Microsoft\VisualStudio\%VCVER%\Setup\VC
if not defined _VCINSTALLDIR (
  for /f "tokens=2*" %%i in ('reg query "%_VSINSTALLKEY%" /v ProductDir 2^> NUL') do (
    call :SetVar _VCINSTALLDIR "%%j"
  )
)
set _VSINSTALLKEY=HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\%VCVER%\Setup\VC
:: If we haven't found it by now, try the WOW64 "Software" key
if not defined _VCINSTALLDIR (
  for /f "tokens=2*" %%i in ('reg query "%_VSINSTALLKEY%" /v ProductDir 2^> NUL') do (
    call :SetVar _VCINSTALLDIR "%%j"
  )
)
:DETECTION_FINISHED
set TEMP_SUPPORTED=
echo _VCINSTALLDIR=%_VCINSTALLDIR%
if not defined _VCINSTALLDIR goto :SKIP_VCVARS
if "%VCTGT_TOOLSET%" == "x86" set BATCHNAME=vcvars32.bat
if "%VCTGT_TOOLSET%" == "amd64" set BATCHNAME=vcvars64.bat
if not defined BATCHNAME set BATCHNAME=vcvars%VCTGT_TOOLSET%.bat
for %%i in (Auxiliary\Build\%BATCHNAME% bin\%VCTGT_TOOLSET%\%BATCHNAME% bin\%BATCHNAME% Auxiliary\Build\vcvarsall.bat vcvarsall.bat) do (
  call :FindVCVarsScript "%_VCINSTALLDIR%" "%%i" "%VCTGT_TOOLSET%"
)
:SKIP_VCVARS
if not "%VCTGT_TOOLSET%" == "" (
  for %%i in (%SUPPORTED_TSET%) do (
    if "%VCTGT_TOOLSET%" == "%%i" call :SetVar TEMP_SUPPORTED yes
  )
)
if not "%TEMP_SUPPORTED%" == "yes" ( echo ERROR: Invalid toolset %TEMP_TOOLSET% for version %VCVER% of Visual C++&endlocal&set SETVCV_ERROR=2&goto :EOF )
:: Return, in case nothing was found
if not defined VCVARS_PATH ( endlocal&set SETVCV_ERROR=3&goto :EOF )
:: Replace the . in the version by an underscore
set VCVERLBL=%VCVER:.=_%
:: Try to set a friendlier name for the Visual Studio version
call :FRIENDLY_%VCVERLBL% > NUL 2>&1
:: Jump over those "subs"
goto :FRIENDLY_SET
:FRIENDLY_16_0
    set _VCVER=%NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_15_0
    set _VCVER=%NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_14_0
    set _VCVER=%NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_12_0
    set _VCVER=%NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_11_0
    set _VCVER=%NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_10_0
    set _VCVER=%NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_9_0
    set _VCVER=%NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_8_0
    set _VCVER=%NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_7_1
    set _VCVER=.NET %NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_7_0
    set _VCVER=.NET %NICEVER% ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_SET
if not defined _VCVER call :SetVar _VCVER "%VCVER%"
echo   -^> Found Visual C++ %_VCVER%
endlocal & set VCVARS_PATH=%VCVARS_PATH%&set VCVER_FRIENDLY=Visual C++ %_VCVER%
goto :EOF
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: \ FindVC subroutine
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: / Help subroutine
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:Help
echo.
echo Syntax: setvcvars ^[toolset^] ^[^'store^'^] ^[VS versions^]
echo.
echo     The toolset can be one of %SUPPORTED_TSET%
echo     depending on the version of Visual Studio you ask for.
echo     Unless explicitly given it defaults to %DEFAULT_TSET%.
echo.
echo     'store' is the literal string 'store' for Visual Studio 2015 and newer.
echo.
echo     VS versions can be one of %SUPPORTED_VC%
echo     or %SUPPORTED_NICE%
echo.
echo     To verify success when calling this script from another, check that VCVER_FRIENDLY is
echo     defined. If it isn't, something failed.
echo     Use setlocal/endlocal inside your script to limit the scope of variables.
popd&endlocal&exit /b 0
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: \ Help subroutine
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: / SetVar subroutine
:::   Param1 == name of the variable, Param2 == value to be set for the variable
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:SetVar
:: Get the name of the variable we are working with
setlocal ENABLEEXTENSIONS&set VAR_NAME=%1
endlocal & set %VAR_NAME%=%~2
goto :EOF
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: \ SetVar subroutine
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: / FindVCVarsScript subroutine
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:FindVCVarsScript
setlocal ENABLEEXTENSIONS
:: Do not overwrite any pre-existing path
if defined VCVARS_PATH endlocal&goto :EOF
set VCINSTALLDIR=%~1
set VCVARSGUESS=%~2
set VCTGT_TOOLSET=%~3
set VCVARS_CURRENT_GUESS=%VCINSTALLDIR%%VCVARSGUESS%
:: Use the guess passed as second argument, after building a path based on VCINSTALLDIR
if EXIST "%VCVARS_CURRENT_GUESS%" set VCVARS_PATH=%VCVARS_CURRENT_GUESS%
if defined VCVARS_PATH goto :VCVarsFound
:: Just in case the install directory path contains no trailing backslash
set VCVARS_CURRENT_GUESS=%VCINSTALLDIR%\%VCVARSGUESS%
if EXIST "%VCVARS_CURRENT_GUESS%" set VCVARS_PATH=%VCVARS_CURRENT_GUESS%
if defined VCVARS_PATH goto :VCVarsFound
endlocal & goto :EOF
:VCVarsFound
endlocal & set VCVARS_PATH=%VCVARS_PATH%
goto :EOF
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: \ FindVCVarsScript subroutine
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
