@ECHO OFF
TITLE Building WinDirStat
SETLOCAL EnableExtensions EnableDelayedExpansion

:: solicit whether this is production or beta build
ECHO Please choose a release type:
ECHO 1. Beta
ECHO 2. Production
SET /p CHOICE=Enter Choice (1 or 2): 
SET RELTYPE=
IF "%CHOICE%" EQU "1" SET RELTYPE=BETA
IF "%CHOICE%" EQU "2" SET RELTYPE=PRODUCTION
IF "%RELTYPE%" EQU "" EXIT /B 1

:: setup environment variables based on location of this script
SET THISDIR=%~dp0
SET THISDIR=%THISDIR:~0,-1%
SET BASEDIR=%THISDIR%\.
SET BLDDIR=%THISDIR%\build
SET PUBDIR=%THISDIR%\publish
SET STOREBLDDIR=%BLDDIR%\store

:: cert info to use for signing
SET "TSAURL=http://time.certum.pl/"
SET "LIBNAME=WinDirStat"
SET "LIBURL=https://github.com/WinDirStat/WinDirStat"

:: prepend 7-Zip and system paths
IF EXIST "%ProgramFiles%\7-Zip" SET PATH=%ProgramFiles%\7-Zip;%PATH%
IF EXIST "%ProgramFiles(x86)%\7-Zip" SET PATH=%ProgramFiles(x86)%\7-Zip;%PATH%
SET PATH=%WINDIR%\system32;%WINDIR%\system32\WindowsPowerShell\v1.0;%PATH%
SET "POWERSHELL=POWERSHELL.EXE -NoProfile -NonInteractive -NoLogo -ExecutionPolicy Unrestricted"

:: create define for build about box
SET PRODUCTION=0
IF /I "%RELTYPE%" EQU "PRODUCTION" SET PRODUCTION=1

:: import vs build tools
IF EXIST "%BLDDIR%" RD /S /Q "%BLDDIR%"
FOR /F "USEBACKQ TOKENS=*" %%X in (`
    "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" ^
        -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
        -property installationPath
`) DO CALL "%%X\Common7\Tools\VsDevCmd.bat"
IF EXIST "%WindowsSdkVerBinPath%\x86" msbuild "%BASEDIR%\windirstat.sln" /p:Configuration=Release /t:Clean;Build /p:Platform=Win32,ExternalCompilerOptions=/DPRODUCTION=%PRODUCTION%
IF EXIST "%WindowsSdkVerBinPath%\x64" msbuild "%BASEDIR%\windirstat.sln" /p:Configuration=Release /t:Clean;Build /p:Platform=x64,ExternalCompilerOptions=/DPRODUCTION=%PRODUCTION%
IF EXIST "%WindowsSdkVerBinPath%\arm64" msbuild "%BASEDIR%\windirstat.sln" /p:Configuration=Release /t:Clean;Build /p:Platform=ARM64,ExternalCompilerOptions=/DPRODUCTION=%PRODUCTION%

:: try signing the main executables
signtool sign /fd sha256 /tr %TSAURL% /td sha256 /d %LIBNAME% /du %LIBURL% "%BLDDIR%\*.exe"
IF ERRORLEVEL 1 ECHO Executable signing failed; continuing without signed executables.

:: build the msi
CALL "%THISDIR%\setup\build.cmd" "%RELTYPE%"

:: sign the msi
signtool sign /fd sha256 /tr %TSAURL% /td sha256 /d %LIBNAME% /du %LIBURL% "%BLDDIR%\*.msi"
IF ERRORLEVEL 1 ECHO MSI signing failed; continuing without a signed MSI.

:: copy the output files
IF EXIST "%PUBDIR%" RD /S /Q "%PUBDIR%"
FOR %%A IN (arm64 x86 x64) DO (
   IF NOT EXIST "%PUBDIR%\%%A" MKDIR "%PUBDIR%\%%A"
   COPY /Y "%BLDDIR%\WinDirStat_%%A.exe" "%PUBDIR%\%%A\WinDirStat.exe"
   COPY /Y "%BLDDIR%\WinDirStat_%%A.pdb" "%PUBDIR%\%%A\WinDirStat.pdb"
   COPY /Y "%BLDDIR%\WinDirStat-%%A.msi" "%PUBDIR%"
)

:: build the Microsoft Store MSIX bundle; Partner Center signs it during submission
%POWERSHELL% -File "%THISDIR%\setup\store\build-store-msix.ps1" -OutDir "%STOREBLDDIR%"
IF ERRORLEVEL 1 EXIT /B 1
%POWERSHELL% -Command "$packages = @(Get-ChildItem -LiteralPath '%STOREBLDDIR%\packages' -File | Where-Object { $_.Extension -in '.msix', '.msixbundle' }); if (-not $packages) { throw 'No Store artifacts were generated.' }; foreach ($package in $packages) { Move-Item -LiteralPath $package.FullName -Destination '%PUBDIR%' -Force }"
IF ERRORLEVEL 1 EXIT /B 1

:: 7-zip executables and debug files
7z.EXE >NUL 2>&1
IF %ERRORLEVEL% NEQ 0 ECHO 7-Zip not found; skipping 7-Zip archive
IF %ERRORLEVEL% EQU 0 7z.EXE a -mx=9 "%PUBDIR%\WinDirStat.7z" "%PUBDIR%\*\*.exe"
IF %ERRORLEVEL% EQU 0 7z.EXE a -mx=9 "%PUBDIR%\WinDirStat-DebugSymbols.7z" "%PUBDIR%\*\*.pdb"
DEL /F /S /Q "%PUBDIR%\*.pdb" >NUL 2>&1

:: zip up executables
FOR %%A IN (arm64 x86 x64) DO %POWERSHELL% -Command "Compress-Archive '%PUBDIR%\%%A' -DestinationPath '%PUBDIR%\WinDirStat.zip' -Update"

:: output hash information
SET HASHFILE=%PUBDIR%\WinDirStat-Hashes.txt
IF EXIST "%HASHFILE%" DEL /F "%HASHFILE%"
%POWERSHELL% -Command "$f = Get-ChildItem -Include @('*.msi','*.exe','*.zip','*.7z','*.msix','*.msixbundle') -Path '%PUBDIR%' -Recurse; 'SHA256','SHA1','MD5' | ForEach-Object { $f | Get-FileHash -Algorithm $_ | Out-File -Append '%HASHFILE%' -Width 256 }"
%POWERSHELL% -Command "$Data = Get-Content '%HASHFILE%'; $Data.Replace((Get-Item -LiteralPath '%PUBDIR%').FullName + '\','').Trim() | Set-Content '%HASHFILE%'"

PAUSE
