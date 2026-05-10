using namespace System.Drawing
using namespace System.Drawing.Drawing2D
using namespace System.Drawing.Imaging

[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet("x86", "x64", "arm64")][string[]] $Platform = @("x86", "x64", "arm64"),

    [string] $Version,
    [string] $IdentityName = "WinDirStat.WinDirStat",
    [string] $Publisher = $(if ($env:WDS_STORE_PUB) { $env:WDS_STORE_PUB } else { "CN=WinDirStat Team" }),
    [string] $PublisherDisplayName = "WinDirStat Team",
    [string] $DisplayName = "WinDirStat",
    [string] $MinVersion = "10.0.17763.0",
    [string] $MaxVersionTested = "10.0.26100.0",
    [string] $OutDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $PSCommandPath
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..\..")).Path
$TemplatePath = Join-Path $ScriptDir "Package.appxmanifest.template"
$SourceLogoPath = Join-Path $RepoRoot "windirstat\logos\logo_256px.png"
$StoreAssets = @(
    @("Square150x150Logo.png", 150, 150),
    @("Square310x310Logo.png", 310, 310),
    @("Square44x44Logo.png", 44, 44),
    @("Square44x44Logo.targetsize-16.png", 16, 16),
    @("Square44x44Logo.targetsize-24.png", 24, 24),
    @("Square44x44Logo.targetsize-32.png", 32, 32),
    @("Square44x44Logo.targetsize-48.png", 48, 48),
    @("Square44x44Logo.targetsize-256.png", 256, 256),
    @("Square71x71Logo.png", 71, 71),
    @("StoreLogo.png", 50, 50),
    @("Wide310x150Logo.png", 310, 150)
)

if (-not $OutDir) { $OutDir = Join-Path $ScriptDir "out" }

function Assert-CommandInPath([string] $Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Could not find $Name in PATH. Run from a Developer PowerShell prompt or add the tool directory to PATH."
    }
}

function Get-VersionFromHeader() {
    $versionHeader = Join-Path $RepoRoot "windirstat\Version.h"
    $content = Get-Content -Raw -Path $versionHeader
    $major = [regex]::Match($content, "#define\s+PRD_MAJVER\s+(\d+)").Groups[1].Value
    $minor = [regex]::Match($content, "#define\s+PRD_MINVER\s+(\d+)").Groups[1].Value
    $patch = [regex]::Match($content, "#define\s+PRD_PATCH\s+(\d+)").Groups[1].Value

    if (-not $major -or -not $minor -or -not $patch) {
        throw "Could not read PRD_MAJVER, PRD_MINVER, and PRD_PATCH from $versionHeader."
    }

    return "$major.$minor.$patch.0"
}

function Assert-PackageVersion([string] $PackageVersion) {
    if ($PackageVersion -notmatch "^\d+\.\d+\.\d+\.\d+$") {
        throw "MSIX package version '$PackageVersion' must use four numeric parts, for example 2.5.9.0."
    }

    foreach ($part in $PackageVersion.Split(".")) {
        if ([int] $part -gt 65535) {
            throw "MSIX package version part '$part' is outside the allowed 0-65535 range."
        }
    }
}

function Invoke-ExternalTool([string] $FilePath,[string[]] $Arguments) {
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE."
    }
}

function Resolve-BuiltExecutable([string] $Arch) {
    $candidates = @(
        Join-Path $RepoRoot "publish\$Arch\WinDirStat.exe"
        Join-Path $RepoRoot "build\WinDirStat_$Arch.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return (Resolve-Path $candidate).Path }
    }

    throw "Could not find a built executable for $Arch. Expected publish\$Arch\WinDirStat.exe or build\WinDirStat_$Arch.exe."
}

function ConvertTo-ManifestValue([AllowNull()][string] $Value) { [System.Security.SecurityElement]::Escape([string] $Value) }

function New-StoreAssets([string] $SourcePath,[string] $DestinationDir) {
    Add-Type -AssemblyName System.Drawing

    if (Test-Path $DestinationDir) { Remove-Item -Recurse -Force -Path $DestinationDir }
    New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null

    $sourceImage = [Image]::FromFile((Resolve-Path $SourcePath).Path)
    $attributes = [ImageAttributes]::new()
    $attributes.SetWrapMode([WrapMode]::TileFlipXY)

    try {
        foreach ($asset in $StoreAssets) {
            $fileName, $width, $height = $asset
            $scale = [Math]::Min($width / $sourceImage.Width, $height / $sourceImage.Height)
            $drawWidth = [Math]::Max(1, [int][Math]::Round($sourceImage.Width * $scale))
            $drawHeight = [Math]::Max(1, [int][Math]::Round($sourceImage.Height * $scale))
            $destination = [Rectangle]::new(
                [int][Math]::Floor(($width - $drawWidth) / 2),
                [int][Math]::Floor(($height - $drawHeight) / 2),
                $drawWidth,
                $drawHeight
            )

            $bitmap = [Bitmap]::new($width, $height, [PixelFormat]::Format32bppArgb)
            $graphics = [Graphics]::FromImage($bitmap)

            try {
                $graphics.Clear([Color]::Transparent)
                $graphics.CompositingQuality = [CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [PixelOffsetMode]::HighQuality
                $graphics.SmoothingMode = [SmoothingMode]::HighQuality
                $graphics.DrawImage($sourceImage, $destination, 0, 0, $sourceImage.Width, $sourceImage.Height, [GraphicsUnit]::Pixel, $attributes)
                $bitmap.Save((Join-Path $DestinationDir $fileName), [ImageFormat]::Png)
            }
            finally {
                $graphics.Dispose()
                $bitmap.Dispose()
            }
        }
    }
    finally {
        $attributes.Dispose()
        $sourceImage.Dispose()
    }
}

if (-not $Version) { $Version = Get-VersionFromHeader }
Assert-PackageVersion -PackageVersion $Version

if (-not (Test-Path $TemplatePath)) { throw "Missing manifest template: $TemplatePath" }
if (-not (Test-Path $SourceLogoPath)) { throw "Missing source logo: $SourceLogoPath" }

Assert-CommandInPath -Name "makeappx.exe"

$tempRoot = Join-Path $OutDir "temp"
$packageDir = Join-Path $OutDir "packages"
$stageRoot = Join-Path $tempRoot "stage"
$bundleInputDir = Join-Path $tempRoot "bundle-input"
$generatedAssetsDir = Join-Path $tempRoot "generated-assets"

if (Test-Path $tempRoot) { Remove-Item -Recurse -Force -Path $tempRoot }
foreach ($path in $tempRoot, $packageDir) { New-Item -ItemType Directory -Force -Path $path | Out-Null }
Get-ChildItem -LiteralPath $packageDir -File |
    Where-Object { $_.Extension -in ".msix", ".msixbundle" } |
    ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }

Write-Host "Generating Store assets from $SourceLogoPath..."
New-StoreAssets -SourcePath $SourceLogoPath -DestinationDir $generatedAssetsDir

$msixPackages = @()
$manifestTemplate = Get-Content -Raw -Path $TemplatePath

foreach ($arch in $Platform) {
    $stageDir = Join-Path $stageRoot $arch
    $stageAssetsDir = Join-Path $stageDir "Assets"

    if (Test-Path $stageDir) { Remove-Item -Recurse -Force -Path $stageDir }
    New-Item -ItemType Directory -Force -Path $stageAssetsDir | Out-Null

    Copy-Item -Path (Resolve-BuiltExecutable -Arch $arch) -Destination (Join-Path $stageDir "WinDirStat.exe") -Force
    Copy-Item -Path (Join-Path $generatedAssetsDir "*") -Destination $stageAssetsDir -Force

    $manifest = $manifestTemplate
    $replacements = @{
        "{{IdentityName}}" = $IdentityName
        "{{Publisher}}" = $Publisher
        "{{Version}}" = $Version
        "{{ProcessorArchitecture}}" = $arch
        "{{DisplayName}}" = $DisplayName
        "{{PublisherDisplayName}}" = $PublisherDisplayName
        "{{MinVersion}}" = $MinVersion
        "{{MaxVersionTested}}" = $MaxVersionTested
    }

    foreach ($key in $replacements.Keys) {
        $manifest = $manifest.Replace($key, (ConvertTo-ManifestValue $replacements[$key]))
    }

    Set-Content -Path (Join-Path $stageDir "AppxManifest.xml") -Value $manifest -Encoding UTF8

    $packagePath = Join-Path $packageDir ("WinDirStat_{0}.msix" -f $arch)
    Write-Host "Packing $packagePath..."
    Invoke-ExternalTool -FilePath "makeappx.exe" -Arguments @(
        "pack", "/d", $stageDir, "/p", $packagePath, "/h", "SHA256", "/o"
    )
    $msixPackages += $packagePath
}

if (Test-Path $bundleInputDir) { Remove-Item -Recurse -Force -Path $bundleInputDir }
New-Item -ItemType Directory -Force -Path $bundleInputDir | Out-Null
$msixPackages | Copy-Item -Destination $bundleInputDir -Force

$bundleArch = ($Platform -join "_")
$bundlePath = Join-Path $packageDir ("WinDirStat_{0}.msixbundle" -f $bundleArch)
Write-Host "Bundling $bundlePath..."
Invoke-ExternalTool -FilePath "makeappx.exe" -Arguments @(
    "bundle", "/d", $bundleInputDir, "/p", $bundlePath, "/o"
)

Write-Host "`nMSIX packages:"
$msixPackages | ForEach-Object { Write-Host "  $_" }
Write-Host "MSIX bundle:`n  $bundlePath"
