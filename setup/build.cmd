@ECHO OFF
TITLE Building WinDirStat Installer
SETLOCAL

:: setup environment variables based on location of this script
CD /D "%~dp0"
SET PX86=%PROGRAMFILES(X86)%
SET BLDDIR=..\build
FOR /F "DELIMS=" %%X IN ('DIR "%PX86%\WiX Toolset*" /B /AD') DO SET PATH=%PATH%;%PX86%\%%~nxX\bin

:: create the installers
candle -arch x86 "WinDirStat.wxs" -o "WinDirStat-x86.wixobj"
light -ext WixUIExtension -ext WixUtilExtension -sval "WinDirStat-x86.wixobj" -o "%BLDDIR%\WinDirStat-x86.msi"
candle -arch x64 "WinDirStat.wxs" -o "WinDirStat-x64.wixobj"
light -ext WixUIExtension -ext WixUtilExtension -sval "WinDirStat-x64.wixobj" -o "%BLDDIR%\WinDirStat-x64.msi"
DEL /F "*.wixobj"
DEL /F "%BLDDIR%\*.wixpdb"

EXIT /B 0