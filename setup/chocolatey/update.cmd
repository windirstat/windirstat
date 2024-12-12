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

$ReplaceStrings = @{
    '${VERSION}' = $Version
    '${HASHX86}' = (Get-FileHash -Algorithm SHA256 -LiteralPath '..\..\publish\WinDirStat-x86.msi').Hash;
    '${HASHX64}' = (Get-FileHash -Algorithm SHA256 -LiteralPath '..\..\publish\WinDirStat-x64.msi').Hash;
}

ForEach ($File in (Get-ChildItem ".\*.template" -Recurse -Force))
{
    $Content = [System.IO.File]::ReadAllText($File.FullName)
    ForEach ($Key in $ReplaceStrings.Keys)
    {
        $Content = $Content.Replace($Key, $ReplaceStrings[$Key])   
    }
    [System.IO.File]::WriteAllText(($File.FullName -replace '.template$',''), $Content)
}

Exit 0