@echo off
@if not "%OS%"=="Windows_NT" @(echo This script requires Windows NT 4.0 or later to run properly! & goto :EOF)
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: 2013, 2014, Oliver Schneider (assarbad.net) - PUBLIC DOMAIN/CC0
:::
::: DISCLAIMER: Disclaimer: This software is provided 'as-is', without any
:::             express or implied warranty. In no event will the author be
:::             held liable for any damages arising from the use of this
:::             software.
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
set REPOROOT=%~dp0..
:: Directory in which this script resides ($REPOROOT/common)
set COMMON=%~dp0
:: Change into the repository root
setlocal enableextensions & pushd "%REPOROOT%"
set OPTIONS=--resources
set VSVERSIONS=
for %%i in (%*) do @(
    if "%%~i" == "--full"      call :SetVar OPTIONS "--resources --sdk71"
    if "%%~i" == "--sdk"       call :SetVar OPTIONS "--sdk71"
    if "%%~i" == "--res"       call :SetVar OPTIONS "--resources"
    if "%%~i" == "--dev"       call :SetVar OPTIONS "--dev --sdk71"
    call :AppendVSVer VSVERSIONS "%%~i"
)
set DEFAULT_VSVERSIONS=2005 2017 2019
echo %VSVERSIONS%
if "%VSVERSIONS%" == "" set VSVERSIONS=%DEFAULT_VSVERSIONS%
echo Generating for %VSVERSIONS%, options: %OPTIONS%
for %%i in (%VSVERSIONS%) do @(
  for %%j in (%DEFAULT_VSVERSIONS%) do @(
    if "%%i" == "%%j" "%COMMON%premake4.exe" %OPTIONS% vs%%i
  )
)
:: Back to normal
popd & endlocal
goto :EOF

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: / SetVar subroutine
:::   Param1 == name of the variable, Param2 == value to be set for the variable
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:SetVar
:: Get the name of the variable we are working with
setlocal enableextensions&set VAR_NAME=%1
endlocal & set %VAR_NAME%=%~2
goto :EOF
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: \ SetVar subroutine
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: / AppendVSVer subroutine
:::   Param1 == value to be appended to the variable
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:AppendVSVer
:: Get the name of the variable we are working with
setlocal enableextensions&set ADDVAL= %~1
if not "%ADDVAL:~1,3%" == "vs" set ADDVAL=
endlocal & set VSVERSIONS=%VSVERSIONS%%ADDVAL%
goto :EOF
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
::: \ AppendVSVer subroutine
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
