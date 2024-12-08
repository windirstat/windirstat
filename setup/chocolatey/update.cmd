<# ::
@ECHO OFF
TITLE Updating WinDirStat Release Info
CD /D "%~dp0"
SET PSSCRIPT=%~dpnx0
SET PSSCRIPT=%PSSCRIPT:.cmd=.ps1%
COPY /Y "%~dpnx0" "%PSSCRIPT%" > NUL
POWERSHELL.EXE -ExecutionPolicy Bypass -NoLogo -NoProfile -File "%PSSCRIPT%" %*
SET ERR=%ERRORLEVEL%
DEL /F "%PSSCRIPT%" > NUL
EXIT /B %ERR%
#>

$Content = Get-Content '..\..\windirstat\Version.h'
$Pattern = "#define\s+PRD_\S+\s+(\d+)"
$VersionMatches = $Content | Select-String -Pattern $Pattern -AllMatches
$VersionParts = $VersionMatches | ForEach-Object { $_.Matches.Groups[1].Value }
$Version = $VersionParts -join '.'

$SpecContent = Get-Content 'WinDirStat.template'
$SpecContent = $SpecContent -replace "<version>.*?</version>","<version>${Version}</version>"
$SpecContent | Set-Content 'WinDirStat.nuspec' -Force

$JsonDataHash = @{
   'version' = $Version;
   'hashX86' = (Get-FileHash -Algorithm SHA256 -LiteralPath '..\..\publish\WinDirStat-x86.msi').Hash;
   'hashX64' = (Get-FileHash -Algorithm SHA256 -LiteralPath '..\..\publish\WinDirStat-x64.msi').Hash;
}

$JsonDataHash | ConvertTo-Json | Set-Content 'tools\chocolateymetadata.json' -Force

Exit 0