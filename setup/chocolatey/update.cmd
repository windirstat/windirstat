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

$ErrorActionPreference = 'Stop'

# Normalize module path
$env:PSModulePath = Join-Path ([System.Environment]::SystemDirectory) 'WindowsPowerShell\v1.0\Modules'

$ScriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$RepoRoot = Resolve-Path -LiteralPath (Join-Path $ScriptRoot '..\..')
$VersionHeader = Join-Path $RepoRoot 'windirstat\Version.h'
$MsiX86 = Join-Path $RepoRoot 'publish\WinDirStat-x86.msi'
$MsiX64 = Join-Path $RepoRoot 'publish\WinDirStat-x64.msi'

ForEach ($Path in @($VersionHeader, $MsiX86, $MsiX64))
{
    If (-not (Test-Path -LiteralPath $Path))
    {
        Throw "Required Chocolatey input file is missing: $Path"
    }
}

$Content = Get-Content -LiteralPath $VersionHeader
$Pattern = "#define\s+PRD_\S+\s+(\d+)"
$VersionMatches = $Content | Select-String -Pattern $Pattern -AllMatches
$VersionParts = $VersionMatches | ForEach-Object { $_.Matches.Groups[1].Value }
$Version = $VersionParts -join '.'

If ([String]::IsNullOrWhiteSpace($Version))
{
    Throw "Could not determine WinDirStat version from $VersionHeader"
}

$ReplaceStrings = @{
    '${VERSION}' = $Version
    '${HASHX86}' = (Get-FileHash -Algorithm SHA256 -LiteralPath $MsiX86).Hash;
    '${HASHX64}' = (Get-FileHash -Algorithm SHA256 -LiteralPath $MsiX64).Hash;
}

$Templates = Get-ChildItem -LiteralPath $ScriptRoot -Filter "*.template" -Recurse -Force
If ($Templates.Count -eq 0)
{
    Throw "No Chocolatey template files found under $ScriptRoot"
}

ForEach ($File in $Templates)
{
    $Content = [System.IO.File]::ReadAllText($File.FullName)
    ForEach ($Key in $ReplaceStrings.Keys)
    {
        $Content = $Content.Replace($Key, $ReplaceStrings[$Key])   
    }

    If ($Content -match '\$\{(VERSION|HASHX86|HASHX64)\}')
    {
        Throw "Unresolved Chocolatey template placeholder remains in $($File.FullName)"
    }

    [System.IO.File]::WriteAllText(($File.FullName -replace '\.template$',''), $Content)
}

Exit 0
