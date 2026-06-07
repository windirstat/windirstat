@ECHO OFF
TITLE Building WinDirStat Installer
SETLOCAL ENABLEDELAYEDEXPANSION

:: setup release type
SET RELTYPE=BETA
IF "%~1" EQU "PRODUCTION" SET RELTYPE=PRODUCTION

:: setup environment variables based on location of this script
CD /D "%~dp0"
SET BLDDIR=..\..\build

:: test for wix being installed (WiX 4 uses wix.exe instead of candle.exe)
wix --version >NUL 2>&1
IF %ERRORLEVEL% NEQ 0 (
  ECHO WiX Toolset v4 not found; skipping MSI build
  ECHO Install via: dotnet tool install --global wix
  EXIT /B 0
)

:: grab current version information from source
FOR /F "TOKENS=2,3 DELIMS=	 " %%A IN ('FINDSTR "#define.PRD_" ..\..\windirstat\version.h') DO SET %%A=%%B


:: grab current data for installer build version
FOR /F %%X in ('git -C ..\.. rev-list --count --all') DO SET PRD_BUILD=%%X

:: create version string based on git and source
SET VERSTRING=-d MAJVER=%PRD_MAJVER% -d MINVER=%PRD_MINVER% -d PATCH=%PRD_PATCH% -d BUILD=%PRD_BUILD%

:: generate localized WiX .wxl files
POWERSHELL -NoLogo -NoProfile -ExecutionPolicy Bypass -File "make-multilingual.ps1" -GenerateOnly
IF !ERRORLEVEL! NEQ 0 (
   ECHO ERROR: wxl generation failed
   EXIT /B 1
)


:: create a temporary license RTF with smaller font sizes for the installer dialog (8pt body / 10pt headings)
SET LICENSERTF=license-build.rtf
POWERSHELL -NoLogo -NoProfile -Command ^
   "$c = [IO.File]::ReadAllText('license.rtf'); $c = $c.Replace('\afs24','\afs16').Replace('\fs24','\fs16').Replace('\fs28','\fs20'); [IO.File]::WriteAllText('%LICENSERTF%',$c)"

:: create the installers
FOR %%A IN (arm64 x86 x64) DO (
   FOR /F %%S in ('POWERSHELL -NoLogo -NoProfile "[int] ((Get-Item ..\..\build\windirstat_%%A.exe).Length / 1024)"') DO SET SIZE=%%S

   :: Generate one ProductCode per architecture, shared by the base MSI and all language transforms
   FOR /F %%G in ('POWERSHELL -NoLogo -NoProfile "[guid]::NewGuid().ToString().ToUpper()"') DO SET PRODCODE=%%G

   :: Build base English MSI
   wix build -arch %%A "WinDirStat.wxs" -loc "temp_wxl\WinDirStat_en.wxl" -culture en-US -o "%BLDDIR%\WinDirStat-%%A.msi" -d RELTYPE=%RELTYPE% %VERSTRING% -d EstimatedSize=!SIZE! -d ProductCode=!PRODCODE! -d LicenseRtf=!LICENSERTF! -ext WixToolset.UI.wixext -ext WixToolset.Util.wixext
   IF !ERRORLEVEL! NEQ 0 (
      ECHO ERROR: base MSI build failed for %%A
      EXIT /B 1
   )

   :: Embed language transforms for all other supported locales
   POWERSHELL -NoLogo -NoProfile -ExecutionPolicy Bypass -File "make-multilingual.ps1" ^
      -MsiPath "%BLDDIR%\WinDirStat-%%A.msi" -Arch "%%A" ^
      -RelType "%RELTYPE%" -MajVer "%PRD_MAJVER%" -MinVer "%PRD_MINVER%" ^
      -Patch "%PRD_PATCH%" -Build "%PRD_BUILD%" -EstimatedSize "!SIZE!" -ProductCode "!PRODCODE!"
   IF !ERRORLEVEL! NEQ 0 (
      ECHO ERROR: multilingual embedding failed for %%A
      EXIT /B 1
   )
)

:: cleanup temporary files
IF EXIST "%LICENSERTF%" DEL "%LICENSERTF%"
IF EXIST "temp_wxl" RD /S /Q "temp_wxl"

EXIT /B 0
