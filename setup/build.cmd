@ECHO OFF
TITLE Building WinDirStat Installer
SETLOCAL ENABLEDELAYEDEXPANSION

:: setup release type
SET RELTYPE=BETA
IF "%~1" EQU "PRODUCTION" SET RELTYPE=PRODUCTION

:: setup environment variables based on location of this script
CD /D "%~dp0"
SET PX86=%PROGRAMFILES(X86)%
SET BLDDIR=..\build
FOR /F "DELIMS=" %%X IN ('DIR "%PX86%\WiX Toolset*" /B /AD') DO SET PATH=%PATH%;%PX86%\%%~nxX\bin

:: test for wix being installed
candle -help >NUL 2>&1
IF %ERRORLEVEL% NEQ 0 (
  ECHO Wix Toolset not found; skipping MSI build
  EXIT /B 0
)

:: grab current version information from source
FOR /F "TOKENS=2,3 DELIMS=	 " %%A IN ('FINDSTR "#define.PRD_" ..\windirstat\version.h') DO SET %%A=%%B

:: grab current data for installer build version
FOR /F %%X in ('git -C .. rev-list --count --all') DO SET PRD_BUILD=%%X

:: create version string based on git and source
SET VERSTRING=-dMAJVER=%PRD_MAJVER% -dMINVER=%PRD_MINVER% -dPATCH=%PRD_PATCH% -dBUILD=%PRD_BUILD%

:: create the installers
FOR %%A IN (arm64 x86 x64) DO (
   FOR /F %%S in ('POWERSHELL -NoLogo -NoProfile "[int] ((Get-Item ..\build\windirstat_%%A.exe).Length / 1024)"') DO SET SIZE=%%S
   candle -arch %%A "WinDirStat.wxs" -o "WinDirStat-%%A.wixobj" -dRELTYPE=%RELTYPE% %VERSTRING% -dEstimatedSize=!SIZE!
   light -ext WixUIExtension -ext WixUtilExtension -sval "WinDirStat-%%A.wixobj" -o "%BLDDIR%\WinDirStat-%%A.msi"
)
DEL /F "*.wixobj"
DEL /F "%BLDDIR%\*.wixpdb"

EXIT /B 0