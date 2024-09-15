@ECHO OFF
TITLE Building WinDirStat Installer
SETLOCAL

:: setup environment variables based on location of this script
CD /D "%~dp0"
SET PX86=%PROGRAMFILES(X86)%
SET BLDDIR=..\build
FOR /F "DELIMS=" %%X IN ('DIR "%PX86%\WiX Toolset*" /B /AD') DO SET PATH=%PATH%;%PX86%\%%~nxX\bin

:: create the installers
FOR %%A IN (arm arm64 x86 x64) DO (
   candle -arch %%A "WinDirStat.wxs" -o "WinDirStat-%%A.wixobj"
   light -ext WixUIExtension -ext WixUtilExtension -sval "WinDirStat-%%A.wixobj" -o "%BLDDIR%\WinDirStat-%%A.msi"
)
DEL /F "*.wixobj"
DEL /F "%BLDDIR%\*.wixpdb"

EXIT /B 0