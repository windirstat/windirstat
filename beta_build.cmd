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
msbuild "%BASEDIR%\windirstat.sln" /p:Configuration=Release /t:Clean;Build /p:Platform=Win32
msbuild "%BASEDIR%\windirstat.sln" /p:Configuration=Release /t:Clean;Build /p:Platform=x64
msbuild "%BASEDIR%\windirstat.sln" /p:Configuration=Release /t:Clean;Build /p:Platform=ARM
msbuild "%BASEDIR%\windirstat.sln" /p:Configuration=Release /t:Clean;Build /p:Platform=ARM64
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
FOR %%A IN (arm arm64 x86 x64) DO (
   IF NOT EXIST "%PUBDIR%\%%A" MKDIR "%PUBDIR%\%%A"
   COPY /Y "%BLDDIR%\WinDirStat_%%A.exe" "%PUBDIR%\%%A\WinDirStat.exe"
   COPY /Y "%BLDDIR%\WinDirStat_%%A.pdb" "%PUBDIR%\%%A\WinDirStat.pdb"
   COPY /Y "%BLDDIR%\WinDirStat-%%A.msi" "%PUBDIR%"
)

:: zip up executatables
SET POWERSHELL=POWERSHELL.EXE -NoProfile -NonInteractive -NoLogo -ExecutionPolicy Unrestricted
PUSHD "%PUBDIR%"
FOR %%A IN (arm arm64 x86 x64) DO (
   %POWERSHELL% -Command "$Rev = Get-Date -Format 'yyyy-MM-dd'; Compress-Archive '%PUBDIR%\%%A' -DestinationPath ('%PUBDIR%\WinDirStat-2.0.0-' + $Rev + '.zip') -Update"
)
POPD

:: output hash information
SET HASHFILE=%PUBDIR%\WinDirStat-Hashes.txt
IF EXIST "%HASHFILE%" DEL /F "%HASHFILE%"
%POWERSHELL% -Command "Get-ChildItem -Include @('*.msi','*.exe','*.zip') -Path '%PUBDIR%' -Recurse | Get-FileHash -Algorithm SHA256 | Out-File -Append '%HASHFILE%' -Width 256"
%POWERSHELL% -Command "Get-ChildItem -Include @('*.msi','*.exe','*.zip') -Path '%PUBDIR%' -Recurse | Get-FileHash -Algorithm SHA1 | Out-File -Append '%HASHFILE%' -Width 256"
%POWERSHELL% -Command "Get-ChildItem -Include @('*.msi','*.exe','*.zip') -Path '%PUBDIR%' -Recurse | Get-FileHash -Algorithm MD5 | Out-File -Append '%HASHFILE%' -Width 256"
%POWERSHELL% -Command "$Data = Get-Content '%HASHFILE%'; $Data.Replace((Get-Item -LiteralPath '%PUBDIR%').FullName + '\','').Trim() | Set-Content '%HASHFILE%'"

PAUSE