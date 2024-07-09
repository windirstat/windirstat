@ECHO OFF
TITLE Building WinDirStat
SETLOCAL

:: setup environment variables based on location of this script
SET THISDIR=%~dp0
SET THISDIR=%THISDIR:~0,-1%
SET BASEDIR=%THISDIR%\.
SET BLDDIR=%THISDIR%\build
SET PUBDIR=%THISDIR%\publish

:: cert info to use for signing
set TSAURL=http://time.certum.pl/
set LIBNAME=WinDirStat
set LIBURL=https://github.com/WinDirStat/WinDirStat

:: import vs build tools
IF EXIST "%BLDDIR%" RD /S /Q "%BLDDIR%"
FOR /F "DELIMS=" %%X IN ('DIR "%ProgramFiles%\Microsoft Visual Studio\VsDevCmd.bat" /A /S /B') DO SET VS=%%X
CALL "%VS%"
msbuild "%BASEDIR%\windirstat.sln" /p:Configuration=Release /p:Platform=Win32
msbuild "%BASEDIR%\windirstat.sln" /p:Configuration=Release /p:Platform=x64
TIMEOUT /t 3 /nobreak >NUL

:: determine 32-bit program files directory
IF DEFINED ProgramFiles SET PX86=%ProgramFiles%
IF DEFINED ProgramFiles(x86) SET PX86=%ProgramFiles(x86)%

:: setup paths
SET PATH=%WINDIR%\system32;%WINDIR%\system32\WindowsPowerShell\v1.0
FOR /F "DELIMS=" %%X IN ('DIR "%PX86%\Windows Kits\10\bin\signtool.exe" /B /S /A ^| FINDSTR "\\x64\\"') DO SET PATH=%PATH%;%%~dpX

:: sign the main executables 
signtool sign /fd sha256 /tr %TSAURL% /td sha256 /d %LIBNAME% /du %LIBURL% "%BLDDIR%\*.exe"

:: build the msi
CALL "%THISDIR%\setup\build.cmd"

:: sign the msi
signtool sign /fd sha256 /tr %TSAURL% /td sha256 /d %LIBNAME% /du %LIBURL% "%BLDDIR%\*.msi"

:: copy the output files
IF EXIST "%PUBDIR%" RD /S /Q "%PUBDIR%"
IF NOT EXIST "%PUBDIR%\x86" MKDIR "%PUBDIR%\x86"
IF NOT EXIST "%PUBDIR%\x64" MKDIR "%PUBDIR%\x64"
COPY /Y "%BLDDIR%\WinDirStat32.exe" "%PUBDIR%\x86\WinDirStat.exe"
COPY /Y "%BLDDIR%\WinDirStat32.pdb" "%PUBDIR%\x86\WinDirStat.pdb"
COPY /Y "%BLDDIR%\WinDirStat64.exe" "%PUBDIR%\x64\WinDirStat.exe"
COPY /Y "%BLDDIR%\WinDirStat64.pdb" "%PUBDIR%\x64\WinDirStat.pdb"
COPY /Y "%BLDDIR%\WinDirStat-x86.msi" "%PUBDIR%"
COPY /Y "%BLDDIR%\WinDirStat-x64.msi" "%PUBDIR%"

:: zip up executatables
SET POWERSHELL=POWERSHELL.EXE -NoProfile -NonInteractive -NoLogo -ExecutionPolicy Unrestricted
PUSHD "%PUBDIR%"
%POWERSHELL% -Command "$Rev = Get-Date -Format 'yyyy-MM-dd'; Compress-Archive '%PUBDIR%\x86' -DestinationPath ('%PUBDIR%\WinDirStat-2.0.0-' + $Rev + '.zip') -Force"
%POWERSHELL% -Command "$Rev = Get-Date -Format 'yyyy-MM-dd'; Compress-Archive '%PUBDIR%\x64' -DestinationPath ('%PUBDIR%\WinDirStat-2.0.0-' + $Rev + '.zip') -Update"
POPD

PAUSE