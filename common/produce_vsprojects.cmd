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
if "%~1" == "--full"      (set OPTIONS=--resources --sdk71)
if "%~1" == "--sdk71"     (set OPTIONS=--sdk71)
if "%~1" == "--resources" (set OPTIONS=--resources)
if "%~1" == "--dev"       (set OPTIONS=--dev --sdk71)
set DEFAULT_VSVERSIONS=2005 2008 2010 2012 2013 2015 2017
set VSVERSIONS=%*
if "%VSVERSIONS%" == "" set VSVERSIONS=%DEFAULT_VSVERSIONS%
for %%i in (%VSVERSIONS%) do @(
  for %%j in (%DEFAULT_VSVERSIONS%) do @(
    if "%%i" == "%%j" "%COMMON%premake4.exe" --xp %OPTIONS% vs%%i
  )
)
:: Back to normal
popd & endlocal
