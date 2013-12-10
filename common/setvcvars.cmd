@echo off
@if not "%OS%"=="Windows_NT" @(echo This script requires Windows NT 4.0 or later to run properly! & goto :EOF)
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: 2009-2013, Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
::: Available from: <https://bitbucket.org/assarbad/scripts/>
:::
::: PURPOSE:    This script can be used to run the vcvars32.bat from any of the
:::             existing Visual C++ versions from .NET (2002) through 2013 or
:::             custom given versions (or single version) on the command line.
:::             The script will try to find the newest installed VC version by
:::             iterating over the space-separated (descending) list of versions
:::             in the variable SUPPORTED_VC below.
:::             Call it from another script and after that you will have NMAKE,
:::             DEVENV.EXE and friends available without having to hardcode
:::             their path into a script or makefile.
:::
::: DISCLAIMER: Disclaimer: This software is provided 'as-is', without any
:::             express or implied warranty. In no event will the author be
:::             held liable for any damages arising from the use of this
:::             software.
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:SCRIPT
if defined VCVER_FRIENDLY echo This script expects a clean environment. Don't run it several times in the same instance of CMD!&goto :EOF
setlocal & pushd .
set MIN_VC=7.0
set MAX_VC=12.0
set MIN_NICE=2002
:: Internal representation of the version number
set SUPPORTED_VC=12.0 11.0 10.0 9.0 8.0 7.1 7.0
:: Marketing name of the Visual Studio versions
set SUPPORTED_NICE=2013 2012 2010 2008 2005 2003 2002
:: Toolsets (potentially) supported
set SUPPORTED_TSET=amd64 x86 ia64 x86_ia64 x86_amd64 x86_arm
set DEFAULT_TSET=x86
reg /? > NUL 2>&1 || echo "REG.EXE is a prerequisite but wasn't found!" && goto :EOF
set SETVCV_ERROR=0
:: First parameter may point to a particular toolset ...
if not "%~1" == "" @(
  for %%i in (%SUPPORTED_TSET%) do @(
    if "%~1" == "%%i" shift & call :SetVar VCTGT_TOOLSET %%i
  )
)
:: Fall back to x86 if not given
if not defined VCTGT_TOOLSET set VCTGT_TOOLSET=%DEFAULT_TSET%
:: Make the string appear a bit nicer
set SUPPORTED_PP=%SUPPORTED_NICE: =, %
:: Allow the version to be overridden on the command line
:: ... else find the VC versions in the order given by SUPPORTED_VC
if not "%~1" == "" @(
  for %%i in (%~1) do @(
    call :FindVC "%%i"
  )
) else @(
  echo Trying to auto-detect supported MSVC version ^(%SUPPORTED_PP%^)
  echo HINT: pass one ^(or several^) of %SUPPORTED_PP% on the command line.
  echo       alternatively one ^(or several^) of:  %SUPPORTED_VC%
  echo.
  for %%i in (%SUPPORTED_VC%) do @(
    call :FindVC "%%i"
  )
)
:: Check result and quit with error if necessary
if not defined VCVARS_PATH @(
  if not "%~1" == "" @(
    echo Requested version ^"%~1^" of Visual Studio not found.
  ) else @(
    echo Could not find any supported version ^(%SUPPORTED_PP%^) of Visual C++.
  )
  popd & endlocal & exit /b %SETVCV_ERROR%
)
:: Return and make sure the outside world sees the results (i.e. leave the scope)
popd & endlocal & if not "%VCVARS_PATH%" == "" @(call "%VCVARS_PATH%" %VCTGT_TOOLSET%) & if not "%VCVER_FRIENDLY%" == "" set VCVER_FRIENDLY=%VCVER_FRIENDLY%
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
if defined VCVARS_PATH @( endlocal & goto :EOF )
:: Now let's distinguish the "nice" version numbers (2002, ... 2013) from the internal ones
if "%VCVER%" geq "%MIN_NICE%" call :NICE_%VCVER% > NUL 2>&1
:: Jump over those "subs"
goto :NICE_SET
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
:NICE_SET
:: This is where we intend to find the installation path in the registry
set _VSINSTALLKEY=HKLM\SOFTWARE\Microsoft\VisualStudio\%VCVER%\Setup\VC
echo Trying to find Visual C++ %VCVER% ^(%VCTGT_TOOLSET%^)
for /f "tokens=2*" %%i in ('reg query "%_VSINSTALLKEY%" /v ProductDir 2^> NUL') do @(
  call :SetVar _VCINSTALLDIR "%%j"
)
set _VSINSTALLKEY=HKLM\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\%VCVER%\Setup\VC
:: If we haven't found it by now, try the WOW64 "Software" key
if not defined _VCINSTALLDIR @(
  for /f "tokens=2*" %%i in ('reg query "%_VSINSTALLKEY%" /v ProductDir 2^> NUL') do @(
    call :SetVar _VCINSTALLDIR "%%j"
  )
)
set TEMP_TOOLSET=%VCTGT_TOOLSET%
if defined _VCINSTALLDIR @(
  if EXIST "%_VCINSTALLDIR%\vcvarsall.bat" @(
    call :SetVar VCVARS_PATH "%_VCINSTALLDIR%\vcvarsall.bat"
  )
  if not defined VCVARS_PATH if EXIST "%_VCINSTALLDIR%\bin\vcvars32.bat" @(
    call :SetVar VCVARS_PATH "%_VCINSTALLDIR%\bin\vcvars32.bat"
    call :SetVar VCTGT_TOOLSET
  )
)
:
:: Return, in case nothing was found
if not defined VCVARS_PATH @( endlocal&set SETVCV_ERROR=1&goto :EOF )
:: Replace the . in the version by an underscore
set VCVERLBL=%VCVER:.=_%
:: Try to set a friendlier name for the Visual Studio version
call :FRIENDLY_%VCVERLBL% > NUL 2>&1
:: Jump over those "subs"
goto :FRIENDLY_SET
:FRIENDLY_12_0
    set _VCVER=2013 ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_11_0
    set _VCVER=2012 ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_10_0
    set _VCVER=2010 ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_9_0
    set _VCVER=2008 ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_8_0
    set _VCVER=2005 ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_7_1
    set _VCVER=.NET 2003 ^[%TEMP_TOOLSET%^]
    goto :EOF
:FRIENDLY_7_0
    set _VCVER=.NET 2002 ^[%TEMP_TOOLSET%^]
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
