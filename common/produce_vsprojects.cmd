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
setlocal & pushd "%REPOROOT%"
if "%~1" == "full"      shift&set OPTIONS=--resources --sdk71
if "%~1" == "sdk71"     shift&set OPTIONS=--sdk71
if "%~1" == "resources" shift&set OPTIONS=--resources
set VSVERSIONS=%*
if "%VSVERSIONS%" == "" set VSVERSIONS=vs2005 vs2008 vs2010 vs2012 vs2013
for %%i in (%VSVERSIONS%) do @(
  "%COMMON%\premake4.exe" %OPTIONS% %%i
)
:: Back to normal
popd & endlocal
