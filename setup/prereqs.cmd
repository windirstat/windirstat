@ECHO OFF
TITLE Installing WinDirStat Build Prerequisites
SETLOCAL EnableExtensions EnableDelayedExpansion

NET SESSION >NUL 2>NUL
IF ERRORLEVEL 1 (
    ECHO This script must be run from an elevated Administrator command prompt.
    EXIT /B 1
)

SET "PF86=%ProgramFiles(x86)%"
SET "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
SET "VSSETUP=%PF86%\Microsoft Visual Studio\Installer\setup.exe"
SET "VSPRODUCTS=Microsoft.VisualStudio.Product.Enterprise Microsoft.VisualStudio.Product.Professional Microsoft.VisualStudio.Product.Community"
SET "V143BUILD=14.44.17.14"
SET "WINGET_ARGS=--exact --silent --accept-package-agreements --accept-source-agreements"

IF NOT EXIST "%VSWHERE%" (
    ECHO vswhere.exe was not found. Install Visual Studio 2022 or later first.
    EXIT /B 1
)
IF NOT EXIST "%VSSETUP%" (
    ECHO Visual Studio Installer setup.exe was not found.
    EXIT /B 1
)

SET "VSPATH="
SET "VSCHANNEL="
PUSHD "%PF86%\Microsoft Visual Studio\Installer" >NUL
FOR /F "DELIMS=" %%I IN ('vswhere.exe -latest -version "[17.0,)" -products %VSPRODUCTS% -property installationPath') DO SET "VSPATH=%%I"
FOR /F "DELIMS=" %%I IN ('vswhere.exe -latest -version "[17.0,)" -products %VSPRODUCTS% -property channelId') DO SET "VSCHANNEL=%%I"
POPD >NUL

IF NOT DEFINED VSPATH (
    ECHO Visual Studio 2022 or later was not found. Install Visual Studio first.
    EXIT /B 1
)

ECHO Found Visual Studio:
ECHO   %VSPATH%
ECHO.
ECHO Installing C++ workload, VS 2022 v143 platform toolset, MFC, ARM64 tools, and recommended Windows SDK...

START /WAIT "" "%VSSETUP%" modify --installPath "%VSPATH%" --channelId "%VSCHANNEL%" ^
    --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended ^
    --add Microsoft.VisualStudio.ComponentGroup.VC.Tools.143.x86.x64 ^
    --add Microsoft.VisualStudio.Component.VC.%V143BUILD%.MFC --add Microsoft.VisualStudio.Component.VC.%V143BUILD%.MFC.ARM64 ^
    --add Microsoft.VisualStudio.Component.VC.%V143BUILD%.ATL --add Microsoft.VisualStudio.Component.VC.%V143BUILD%.ATL.ARM64 ^
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 ^
    --add Microsoft.VisualStudio.Component.VC.ATL --add Microsoft.VisualStudio.Component.VC.ATLMFC ^
    --add Microsoft.VisualStudio.Component.VC.ATL.ARM64 --add Microsoft.VisualStudio.Component.VC.MFC.ARM64 ^
    --removeOos true --norestart --quiet
SET "RC=!ERRORLEVEL!"
IF "%RC%"=="3010" SET "VS_REBOOT_REQUIRED=1"
IF NOT "%RC%"=="0" IF NOT "%RC%"=="3010" (
    ECHO Visual Studio Installer failed with exit code %RC%.
    EXIT /B %RC%
)
IF "%VS_REBOOT_REQUIRED%"=="1" ECHO A reboot is required before all prerequisites are usable.

WHERE winget.exe >NUL 2>NUL
IF ERRORLEVEL 1 (
    ECHO winget.exe was not found. Install command-line prerequisites manually, then rerun this script.
    EXIT /B 1
)

FOR %%P IN (Microsoft.DotNet.SDK.10 Git.Git 7zip.7zip) DO (
    ECHO.
    ECHO Installing or updating %%P...
    winget install --id %%P %WINGET_ARGS%
    IF ERRORLEVEL 1 EXIT /B !ERRORLEVEL!
)
IF EXIST "%ProgramFiles%\dotnet\dotnet.exe" SET "PATH=%ProgramFiles%\dotnet;%PATH%"

ECHO.
ECHO Installing or updating WiX Toolset...
SET "DOTNET_TOOLS=%USERPROFILE%\.dotnet\tools"
IF NOT EXIST "%DOTNET_TOOLS%" MKDIR "%DOTNET_TOOLS%"
SET "PATH=%DOTNET_TOOLS%;%PATH%"

POWERSHELL.EXE -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$p = $env:DOTNET_TOOLS; $path = [Environment]::GetEnvironmentVariable('Path', 'User'); $parts = @($path -split ';' | Where-Object { $_ }); if (-not ($parts | Where-Object { $_ -ieq $p })) { [Environment]::SetEnvironmentVariable('Path', (($parts + $p) -join ';'), 'User') }"
IF ERRORLEVEL 1 EXIT /B 1

dotnet tool update --global wix
IF ERRORLEVEL 1 dotnet tool install --global wix
IF ERRORLEVEL 1 (
    ECHO WiX Toolset installation failed.
    EXIT /B 1
)

wix eula accept wix7
IF ERRORLEVEL 1 (
    ECHO WiX OSMF EULA acceptance failed.
    EXIT /B 1
)

SET "WIX_VERSION="
FOR /F "DELIMS=" %%V IN ('wix --version') DO IF NOT DEFINED WIX_VERSION SET "WIX_VERSION=%%V"
FOR /F "TOKENS=1 DELIMS=+ " %%V IN ("%WIX_VERSION%") DO SET "WIX_VERSION=%%V"

FOR %%E IN (WixToolset.UI.wixext WixToolset.Util.wixext) DO (
    ECHO Installing WiX extension %%E/%WIX_VERSION%...
    wix extension list --global 2>NUL | FINDSTR /I /C:"%%E" >NUL
    IF NOT ERRORLEVEL 1 wix extension remove --global "%%E" >NUL 2>NUL
    wix extension add --global "%%E/%WIX_VERSION%"
    IF ERRORLEVEL 1 EXIT /B 1
)

ECHO.
ECHO WinDirStat build prerequisites are installed.
ECHO If this is a new shell, reopen it so PATH changes are visible.
EXIT /B 0
