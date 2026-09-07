<#
.SYNOPSIS
    Builds a WinDirStat GitHub release body from the assets already attached to a release.

.DESCRIPTION
    Populates .github/release-template.md with an immutable CHANGELOG link and a download table. Debug symbols are
    omitted. Known public assets also receive descriptions and versioned-filename links. Existing release-specific
    notes are retained after the generated section.

.PARAMETER Version
    Release version, such as 2.7.0. The release tag is expected to be release/v2.7.0.

.PARAMETER Repository
    GitHub repository containing the release.

.PARAMETER OutputPath
    Destination Markdown file. Defaults to publish/WinDirStat-<version>-release.md, which is gitignored.

.PARAMETER UpdateRelease
    Updates the existing GitHub release description after writing the Markdown file.

.EXAMPLE
    .\.github\scripts\New-ReleaseBody.ps1 -Version 2.8.0

    Generates publish/WinDirStat-2.8.0-release.md from the uploaded release assets.

.EXAMPLE
    .\.github\scripts\New-ReleaseBody.ps1 -Version 2.8.0 -UpdateRelease

    Generates the Markdown file and updates the existing release/v2.8.0 release description.
#>

[CmdletBinding(SupportsShouldProcess = $true, PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string] $Version,

    [string] $Repository = "windirstat/windirstat",

    [string] $OutputPath,

    [switch] $UpdateRelease
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

function Get-NormalizedBody
{
    param([AllowEmptyString()] [string] $Body)

    return (($Body -replace "\r\n?", "`n").TrimEnd() + "`n")
}

function Get-PreservedReleaseNotes
{
    param([AllowEmptyString()] [string] $Body)

    $StartMarker = "<!-- BEGIN WINDIRSTAT GENERATED RELEASE DOWNLOADS -->"
    $EndMarker = "<!-- END WINDIRSTAT GENERATED RELEASE DOWNLOADS -->"
    $Text = $Body -replace "\r\n?", "`n"
    $StartCount = [regex]::Matches($Text, [regex]::Escape($StartMarker)).Count
    $EndCount = [regex]::Matches($Text, [regex]::Escape($EndMarker)).Count
    If ($StartCount -ne $EndCount -or $StartCount -gt 1)
    {
        Throw "Release body contains malformed generated-section markers."
    }
    If ($StartCount -eq 1)
    {
        $ManagedPattern = "(?s)" + [regex]::Escape($StartMarker) + ".*?" + [regex]::Escape($EndMarker)
        return ([regex]::new($ManagedPattern).Replace($Text, "", 1)).Trim()
    }

    $ModernPattern = "(?s)\A\s*See the \[CHANGELOG for WinDirStat [^\]]+\]\([^\n]+\) " +
        "for release details\.\n+## Downloads\n+\| Description \| Direct Link \| Versioned Download \|\n" +
        "\| --- \| --- \| --- \|\n(?:\|[^\n]*\|\n?)+"
    $Text = [regex]::new($ModernPattern).Replace($Text, "", 1).Trim()

    $LegacyChangelog = "See the [CHANGELOG](https://github.com/windirstat/windirstat/blob/master/CHANGELOG.md) " +
        "for release details."
    $LegacyDownloads = "Use the links below to download or visit https://windirstat.net/download.html for a " +
        "description of the downloads."
    If ($Text.StartsWith($LegacyChangelog, [System.StringComparison]::Ordinal))
    {
        $Text = $Text.Substring($LegacyChangelog.Length).TrimStart()
    }
    If ($Text.StartsWith($LegacyDownloads, [System.StringComparison]::Ordinal))
    {
        $Text = $Text.Substring($LegacyDownloads.Length).TrimStart()
    }

    return $Text.Trim()
}

If ($Version -match "^release/v(.+)$") { $Version = $Matches[1] }
If ($Version -notmatch "^\d+\.\d+\.\d+$")
{
    Throw "Version must use the X.Y.Z format."
}
If (-not (Get-Command gh -ErrorAction SilentlyContinue))
{
    Throw "GitHub CLI (gh) is required."
}

$Tag = "release/v$Version"
$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$TemplatePath = Join-Path $PSScriptRoot "..\release-template.md"
If (-not $OutputPath) { $OutputPath = Join-Path $RepoRoot "publish\WinDirStat-$Version-release.md" }
If (-not [System.IO.Path]::IsPathRooted($OutputPath)) { $OutputPath = Join-Path $RepoRoot $OutputPath }
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

$AssetDescriptions = [ordered] @{
    "WinDirStat-x64.msi" = "Windows x64 installer"
    "WinDirStat-x86.msi" = "Windows x86 installer"
    "WinDirStat-arm64.msi" = "Windows ARM64 installer"
    "WinDirStat-arm.msi" = "Windows ARM (32-bit) installer"
    "WinDirStat.zip" = "Portable ZIP archive"
    "WinDirStat.7z" = "Portable 7z archive"
    "WinDirStat-Hashes.txt" = "File checksums"
    "WinDirStat_x86_x64_arm64.msixbundle" = "Windows MSIX bundle (x86, x64, and ARM64)"
    "WinDirStat_x64.msix" = "Windows x64 MSIX package"
    "WinDirStat_x86.msix" = "Windows x86 MSIX package"
    "WinDirStat_arm64.msix" = "Windows ARM64 MSIX package"
}
$HiddenAssets = @("WinDirStat-DebugSymbols.7z")
$Release = Invoke-GitHubCli -Arguments @(
    "release", "view", $Tag, "--repo", $Repository, "--json", "assets,body,name"
) | ConvertFrom-Json
$OriginalBody = [string] $Release.body
$Assets = @($Release.assets | Where-Object { $_.state -eq "uploaded" -and $_.name -notin $HiddenAssets })
If ($Assets.Count -eq 0)
{
    Throw "Release '$Tag' has no public assets. Upload the release files before generating its description."
}

$AssetsByName = @{}
ForEach ($Asset in $Assets) { $AssetsByName[$Asset.name] = $Asset }
$Assets = @(
    ForEach ($Name in $AssetDescriptions.Keys)
    {
        If ($AssetsByName.ContainsKey($Name)) { $AssetsByName[$Name] }
    }
    ForEach ($Asset in $Release.assets)
    {
        If ($Asset.state -eq "uploaded" -and $Asset.name -notin $HiddenAssets -and
            -not $AssetDescriptions.Contains($Asset.name))
        {
            $Asset
        }
    }
)

$Rows = ForEach ($Asset in $Assets)
{
    $Description = If ($AssetDescriptions.Contains($Asset.name))
    {
        $AssetDescriptions[$Asset.name]
    }
    Else
    {
        "Additional release file"
    }
    $DirectLink = "[$($Asset.name)]($($Asset.url))"
    $VersionedLink = [char] 0x2014
    If ($AssetDescriptions.Contains($Asset.name))
    {
        $VersionedName = $Asset.name -replace "^WinDirStat", "WinDirStat-$Version"
        $EncodedName = [System.Uri]::EscapeDataString($VersionedName)
        $PublicUrl = "https://windirstat.net/downloads/#/v$Version/$EncodedName"
        $VersionedLink = "[$VersionedName]($PublicUrl)"
    }

    "| $Description | $DirectLink | $VersionedLink |"
}

$AnchorName = "windirstat-$Version"
$AnchorMarkup = "<a name=`"$AnchorName`"></a>"
$TagChangelog = & git -C $RepoRoot show "$Tag`:CHANGELOG.md" 2>$null
$TagHasAnchor = $LASTEXITCODE -eq 0 -and ($TagChangelog -join "`n").Contains($AnchorMarkup)
$MasterChangelogPath = Join-Path $RepoRoot "CHANGELOG.md"
$MasterHasAnchor = (Test-Path -LiteralPath $MasterChangelogPath) -and
    [System.IO.File]::ReadAllText($MasterChangelogPath).Contains($AnchorMarkup)
$ChangelogUrl = "https://github.com/$Repository/blob/$Tag/CHANGELOG.md"
If ($TagHasAnchor)
{
    $ChangelogUrl += "#$AnchorName"
}
ElseIf ($MasterHasAnchor)
{
    $ChangelogUrl = "https://github.com/$Repository/blob/master/CHANGELOG.md#$AnchorName"
}

$PreservedNotes = Get-PreservedReleaseNotes -Body $OriginalBody
$ReleaseNotes = If ($PreservedNotes) { "`n`n$PreservedNotes" } Else { "" }
$Template = [System.IO.File]::ReadAllText($TemplatePath)
$Body = $Template.Replace("{{VERSION}}", $Version).Replace("{{CHANGELOG_URL}}", $ChangelogUrl).
    Replace("{{DOWNLOAD_ROWS}}", ($Rows -join "`n")).Replace("{{RELEASE_NOTES}}", $ReleaseNotes)
$Body = ($Body -replace "\r?\n", [Environment]::NewLine).TrimEnd() + [Environment]::NewLine
$BodyChanged = (Get-NormalizedBody -Body $Body) -cne (Get-NormalizedBody -Body $OriginalBody)
$OutputDirectory = Split-Path -Parent $OutputPath
$OutputReady = $false

If ($PSCmdlet.ShouldProcess($OutputPath, "Write GitHub release body"))
{
    If (-not (Test-Path -LiteralPath $OutputDirectory))
    {
        New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
    }

    [System.IO.File]::WriteAllText($OutputPath, $Body, [System.Text.UTF8Encoding]::new($false))
    $OutputReady = $true
    Write-Host "Wrote $OutputPath"
}

If ($UpdateRelease -and $OutputReady -and
    $PSCmdlet.ShouldProcess("$Repository $Tag", "Update GitHub release description"))
{
    If (-not $BodyChanged)
    {
        Write-Host "Release $Tag is already current."
        return
    }

    $CurrentRelease = Invoke-GitHubCli -Arguments @(
        "release", "view", $Tag, "--repo", $Repository, "--json", "body"
    ) | ConvertFrom-Json
    If ([string] $CurrentRelease.body -cne $OriginalBody)
    {
        Throw "Release '$Tag' changed after it was read; refusing to overwrite its current notes."
    }

    Invoke-GitHubCli -Arguments @(
        "release", "edit", $Tag, "--repo", $Repository, "--notes-file", $OutputPath
    ) | Out-Null
    $UpdatedRelease = Invoke-GitHubCli -Arguments @(
        "release", "view", $Tag, "--repo", $Repository, "--json", "body"
    ) | ConvertFrom-Json
    If ((Get-NormalizedBody -Body ([string] $UpdatedRelease.body)) -cne (Get-NormalizedBody -Body $Body))
    {
        Throw "Release '$Tag' did not retain the generated description."
    }

    Write-Host "Updated https://github.com/$Repository/releases/tag/$Tag"
}
