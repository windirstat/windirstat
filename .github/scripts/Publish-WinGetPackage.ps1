<#
.SYNOPSIS
    Publishes a multilingual WinDirStat release to WinGet.

.DESCRIPTION
    Validates the release installers and generates WinGet manifests with Komac. Each multilingual MSI has one
    entry without InstallerLocale so installations registered in other languages can upgrade. The manifests
    must pass WinGet validation before submission.

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

.PARAMETER DryRun
    Generates and validates manifests without syncing the fork or submitting a pull request. Returns their folder.
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

    [string] $InstallersRegex = "\.(msi|msixbundle)$",

    [switch] $DryRun
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

If (-not $DryRun)
{
    $PullRequestsJson = Invoke-GitHubCli -Arguments @(
        "pr", "list", "--repo", "microsoft/winget-pkgs", "--state", "all",
        "--search", "$PackageIdentifier $PackageVersion in:title", "--json", "state,title,url", "--limit", "100"
    )
    $IdentifierTermPattern = "(?<!\S){0}(?!\S)" -f [regex]::Escape($PackageIdentifier)
    $VersionTermPattern = "(?<!\S){0}(?!\S)" -f [regex]::Escape($PackageVersion)
    $ExistingPullRequests = @(
        $PullRequestsJson |
            ConvertFrom-Json |
            Where-Object {
                $_.state -in @('OPEN', 'MERGED') -and
                $_.title -match $IdentifierTermPattern -and $_.title -match $VersionTermPattern
            }
    )
    If ($ExistingPullRequests.Count -ne 0)
    {
        Write-Host "WinGet already has a pull request for this package version: $($ExistingPullRequests.url -join ', ')"
        return
    }
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

# Verify the uploaded binaries before generating a submission.
$AssetDirectory = Join-Path $OutputDirectory "installers"
$ManifestDirectory = Join-Path $OutputDirectory "manifests"
$MsiAssets = @($InstallerAssets | Where-Object { $_.name -match "\.msi$" })
$DownloadArguments = @("release", "download", $ReleaseTag, "--repo", $ReleaseRepository, "--dir", $AssetDirectory)
ForEach ($Asset in $MsiAssets)
{
    $DownloadArguments += @("--pattern", $Asset.name)
}
Invoke-GitHubCli -Arguments $DownloadArguments | Out-Null
$MsiPaths = @($MsiAssets | ForEach-Object { Join-Path $AssetDirectory $_.name })
$InstallerChecks = @{
    MsiPath = $MsiPaths
    ExpectedVersion = $PackageVersion
    OutputFolder = (Join-Path $OutputDirectory "installer-checks")
}
If ($ReleaseTag -match '^beta/v?(?<Version>\d+\.\d+\.\d+)/(?<Build>\d+|\d{4}-\d{2}-\d{2})$')
{
    $BetaVersion = $Matches.Version
    $BetaBuild = $Matches.Build
    If ($PackageVersion -ne "$BetaVersion.$BetaBuild")
    {
        Throw "Package version does not match beta release tag '$ReleaseTag'."
    }
    If ($BetaBuild.Contains('-'))
    {
        $InstallerChecks.ExpectedVersion = $BetaVersion
        $InstallerChecks.AllowBuildNumber = $true
    }
}
& (Join-Path $PSScriptRoot "..\..\tests\Test-Installer.ps1") @InstallerChecks

$UpdateArguments = @(
    "update", $PackageIdentifier,
    "--version", $PackageVersion,
    "--output", $ManifestDirectory,
    "--dry-run"
)
If (-not [string]::IsNullOrWhiteSpace($ReleaseNotesUrl))
{
    $UpdateArguments += @("--release-notes-url", $ReleaseNotesUrl)
}
$UpdateArguments += "--urls"
$UpdateArguments += @($InstallerAssets.url)
Invoke-Komac -Arguments $UpdateArguments

$InstallerManifests = @(Get-ChildItem -LiteralPath $ManifestDirectory -Recurse -File -Filter "*.installer.yaml")
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

# Keep MSI entries unrestricted while preserving locale metadata for other installer types.
$NewLine = If ($ManifestContent.Contains("`r`n")) { "`r`n" } Else { "`n" }
$RootLocale = [regex]::Match($ManifestContent, "(?m)^InstallerLocale:[ \t]*(?<Locale>[^\r\n]+)\r?$")
If ($RootLocale.Success -and $RootLocale.Groups['Locale'].Value.Trim() -ne "en-US")
{
    Throw "Generated installer manifest has an unexpected root locale."
}
$ManifestContent = [regex]::Replace($ManifestContent, "(?m)^InstallerLocale:[^\r\n]*\r?\n", "")
$InstallerBlocks = @([regex]::Matches($ManifestContent, "(?ms)^- Architecture:.*?(?=^- Architecture:|^ManifestType:)"))
If ($InstallerBlocks.Count -eq 0)
{
    Throw "Generated installer manifest has an unsupported installer layout."
}
$MsiHashes = @{}
ForEach ($Asset in $MsiAssets)
{
    $MsiHashes[$Asset.url] = (Get-FileHash -LiteralPath (Join-Path $AssetDirectory $Asset.name) -Algorithm SHA256).Hash
}
$InstallerEntries = [Collections.Generic.List[string]]::new()
$SeenMsiEntries = @{}
ForEach ($Block in $InstallerBlocks)
{
    $Entry = $Block.Value
    $Url = [regex]::Match($Entry, "(?m)^  InstallerUrl:[ \t]*(?<Url>[^\r\n]+)\r?$").Groups['Url'].Value.Trim()
    If ($Url -notin $InstallerAssets.url)
    {
        Throw "Generated installer entry does not reference a selected release asset: $Url"
    }
    If (-not $MsiHashes.ContainsKey($Url))
    {
        If ($RootLocale.Success -and $Entry -notmatch "(?m)^  InstallerLocale:")
        {
            $Entry = [regex]::Replace($Entry, "(?m)^- Architecture:[^\r\n]*\r?\n",
                ('$0' + "  InstallerLocale: en-US" + $NewLine))
        }
        $InstallerEntries.Add($Entry)
        continue
    }
    $Hash = [regex]::Match($Entry, "(?m)^  InstallerSha256:[ \t]*(?<Hash>[A-Fa-f0-9]{64})\r?$")
    If (-not $Hash.Success -or $Hash.Groups['Hash'].Value -ne $MsiHashes[$Url])
    {
        Throw "Generated installer hash does not match the release asset: $Url"
    }
    $Locale = [regex]::Match($Entry, "(?m)^  InstallerLocale:[ \t]*(?<Locale>[^\r\n]+)\r?$")
    If ($Locale.Success -and $Locale.Groups['Locale'].Value.Trim() -ne "en-US")
    {
        Throw "Generated MSI entry has an unexpected locale: $Url"
    }
    $MsiEntry = [regex]::Replace($Entry, "(?m)^  InstallerLocale:[^\r\n]*\r?\n", "")
    If ($SeenMsiEntries.ContainsKey($Url))
    {
        If ($SeenMsiEntries[$Url] -ne $MsiEntry)
        {
            Throw "Generated MSI entries have conflicting metadata: $Url"
        }
        continue
    }
    $SeenMsiEntries[$Url] = $MsiEntry
    $InstallerEntries.Add($MsiEntry)
}
If ($SeenMsiEntries.Count -ne $MsiAssets.Count)
{
    Throw "Generated installer entries do not cover every MSI release asset."
}
$FirstBlock = $InstallerBlocks[0]
$LastBlock = $InstallerBlocks[-1]
$ManifestContent = $ManifestContent.Substring(0, $FirstBlock.Index) + ($InstallerEntries -join "") +
    $ManifestContent.Substring($LastBlock.Index + $LastBlock.Length)
If ([regex]::IsMatch($ManifestContent, "(?m)^InstallerLocale:") -or
    @($SeenMsiEntries.Values | Where-Object { $_ -match "(?m)^  InstallerLocale:" }).Count -ne 0)
{
    Throw "InstallerLocale remains in the generated MSI entries."
}
[IO.File]::WriteAllText($InstallerManifest.FullName, $ManifestContent, [Text.UTF8Encoding]::new($false))
Write-Host "Generated one unrestricted entry per MSI release asset."

& winget validate --manifest $InstallerManifest.DirectoryName --disable-interactivity
If ($LASTEXITCODE -ne 0)
{
    Throw "WinGet manifest validation failed with exit code $LASTEXITCODE."
}
If ($DryRun)
{
    Write-Host "Validated manifests: $($InstallerManifest.DirectoryName)"
    return $InstallerManifest.DirectoryName
}

Invoke-Komac -Arguments @("sync-fork")
Invoke-Komac -Arguments @("submit", $ManifestDirectory, "--yes")
Invoke-Komac -Arguments @("cleanup", "--only-merged")
