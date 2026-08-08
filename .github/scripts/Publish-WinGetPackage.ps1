<#
.SYNOPSIS
    Publishes a multilingual WinDirStat release to WinGet.

.DESCRIPTION
    Generates WinGet manifests with a pinned Komac version, removes the incorrect InstallerLocale emitted from the
    base MSI ProductLanguage, and submits the corrected manifests. WinDirStat's installers embed multiple locales,
    so describing them as en-US-only prevents upgrades from installations registered with another locale.

.PARAMETER PackageIdentifier
    WinGet package identifier to update.

.PARAMETER PackageVersion
    Package version to publish.

.PARAMETER ReleaseRepository
    GitHub repository containing the release assets.

.PARAMETER ReleaseTag
    GitHub release tag containing the installer assets.

.PARAMETER ReleaseNotesUrl
    Optional release notes URL to include in the generated manifests.

.PARAMETER InstallersRegex
    Regular expression used to select installer assets from the release.
#>

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string] $PackageIdentifier,

    [Parameter(Mandatory = $true)]
    [string] $PackageVersion,

    [Parameter(Mandatory = $true)]
    [string] $ReleaseRepository,

    [Parameter(Mandatory = $true)]
    [string] $ReleaseTag,

    [string] $ReleaseNotesUrl,

    [string] $InstallersRegex = "\.(msi|msixbundle)$"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-GitHubCli
{
    param([Parameter(Mandatory = $true)] [string[]] $Arguments)

    $Output = & gh @Arguments 2>&1
    If ($LASTEXITCODE -ne 0)
    {
        Throw "GitHub CLI failed: $($Output -join [Environment]::NewLine)"
    }

    return $Output -join "`n"
}

function Invoke-Komac
{
    param([Parameter(Mandatory = $true)] [string[]] $Arguments)

    & komac @Arguments
    If ($LASTEXITCODE -ne 0)
    {
        Throw "Komac failed with exit code $LASTEXITCODE."
    }
}

$PullRequestsJson = Invoke-GitHubCli -Arguments @(
    "pr", "list", "--repo", "microsoft/winget-pkgs", "--state", "all",
    "--search", "$PackageIdentifier $PackageVersion in:title", "--json", "state,title,url", "--limit", "100"
)
$IdentifierTermPattern = "(?<!\S){0}(?!\S)" -f [regex]::Escape($PackageIdentifier)
$VersionTermPattern = "(?<!\S){0}(?!\S)" -f [regex]::Escape($PackageVersion)
$ExistingPullRequests = @(
    $PullRequestsJson |
        ConvertFrom-Json |
        Where-Object { $_.title -match $IdentifierTermPattern -and $_.title -match $VersionTermPattern }
)
If ($ExistingPullRequests.Count -ne 0)
{
    Write-Host "WinGet already has a pull request for this package version: $($ExistingPullRequests.url -join ', ')"
    return
}

$ReleaseJson = Invoke-GitHubCli -Arguments @(
    "release", "view", $ReleaseTag, "--repo", $ReleaseRepository, "--json", "assets"
)
$Release = $ReleaseJson | ConvertFrom-Json
$InstallerAssets = @(
    $Release.assets |
        Where-Object { $_.name -match $InstallersRegex } |
        Sort-Object -Property name
)
If ($InstallerAssets.Count -eq 0)
{
    Throw "Release '$ReleaseTag' has no installer assets matching '$InstallersRegex'."
}
If (-not ($InstallerAssets.name -match "\.msi$"))
{
    Throw "Release '$ReleaseTag' has no MSI asset."
}

$RunnerTemp = $env:RUNNER_TEMP
If ([string]::IsNullOrWhiteSpace($RunnerTemp))
{
    $RunnerTemp = [IO.Path]::GetTempPath()
}
$OutputDirectory = Join-Path $RunnerTemp ("winget-manifests-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null

Invoke-Komac -Arguments @("sync-fork")
$UpdateArguments = @(
    "update", $PackageIdentifier,
    "--version", $PackageVersion,
    "--output", $OutputDirectory,
    "--dry-run"
)
If (-not [string]::IsNullOrWhiteSpace($ReleaseNotesUrl))
{
    $UpdateArguments += @("--release-notes-url", $ReleaseNotesUrl)
}
$UpdateArguments += "--urls"
$UpdateArguments += @($InstallerAssets.url)
Invoke-Komac -Arguments $UpdateArguments

$InstallerManifests = @(Get-ChildItem -LiteralPath $OutputDirectory -Recurse -File -Filter "*.installer.yaml")
If ($InstallerManifests.Count -ne 1)
{
    Throw "Expected one generated installer manifest, but found $($InstallerManifests.Count)."
}

$InstallerManifest = $InstallerManifests[0]
$ManifestContent = [IO.File]::ReadAllText($InstallerManifest.FullName)
$IdentifierPattern = "(?m)^PackageIdentifier:[ \t]*{0}[ \t]*\r?$" -f [regex]::Escape($PackageIdentifier)
$VersionPattern = "(?m)^PackageVersion:[ \t]*{0}[ \t]*\r?$" -f [regex]::Escape($PackageVersion)
If ($ManifestContent -notmatch $IdentifierPattern -or $ManifestContent -notmatch $VersionPattern)
{
    Throw "Generated installer manifest does not match package '$PackageIdentifier' version '$PackageVersion'."
}

$MissingMsiUrls = @(
    $InstallerAssets |
        Where-Object { $_.name -match "\.msi$" -and -not $ManifestContent.Contains($_.url) } |
        Select-Object -ExpandProperty url
)
If ($MissingMsiUrls.Count -ne 0)
{
    Throw "Generated installer manifest is missing MSI release asset URL(s): $($MissingMsiUrls -join ', ')"
}

$LocaleLinePattern = "(?m)^[ \t]*InstallerLocale:[^\r\n]*\r?$"
$LocaleLines = @([regex]::Matches($ManifestContent, $LocaleLinePattern))
$UnexpectedLocales = @(
    $LocaleLines |
        ForEach-Object { ($_.Value -replace "^[ \t]*InstallerLocale:[ \t]*", "").Trim() } |
        Where-Object { $_ -ne "en-US" }
)
If ($UnexpectedLocales.Count -ne 0)
{
    Throw "Generated installer manifest contains unexpected locale(s): $($UnexpectedLocales -join ', ')"
}

If ($LocaleLines.Count -ne 0)
{
    $LocaleRemovalPattern = "(?m)^[ \t]*InstallerLocale:[^\r\n]*(?:\r?\n|\z)"
    $ManifestContent = [regex]::Replace($ManifestContent, $LocaleRemovalPattern, "")
    $Utf8NoBom = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText($InstallerManifest.FullName, $ManifestContent, $Utf8NoBom)
    Write-Host "Removed $($LocaleLines.Count) incorrect InstallerLocale field(s)."
}
Else
{
    Write-Host "Generated installer manifest does not contain InstallerLocale; no correction is needed."
}

If ([regex]::IsMatch([IO.File]::ReadAllText($InstallerManifest.FullName), "(?m)^[ \t]*InstallerLocale:"))
{
    Throw "InstallerLocale remains in the generated installer manifest."
}

Invoke-Komac -Arguments @("submit", $OutputDirectory, "--yes")
Invoke-Komac -Arguments @("cleanup", "--only-merged")
