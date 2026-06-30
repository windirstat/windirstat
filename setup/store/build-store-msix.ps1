using namespace System.Drawing
using namespace System.Drawing.Drawing2D
using namespace System.Drawing.Imaging

[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet("x86", "x64", "arm64")][string[]] $Platform = @("x86", "x64", "arm64"),
    [string] $IdentityName = "WinDirStatTeam.WinDirStat",
    [string] $Publisher = "CN=BACE0F4C-E0CC-4A1D-945E-61E8D6D94180",
    [string] $CertificateThumbprint,
    [string] $PublisherDisplayName = "WinDirStat Team",
    [string] $DisplayName = "WinDirStat",
    [string] $MinVersion = "10.0.17763.0",
    [string] $MaxVersionTested = "10.0.29999.0",
    [string] $OutDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $PSCommandPath
$RepoRoot = (Resolve-Path (Join-Path $ScriptDir "..\..")).Path
$TemplatePath = Join-Path $ScriptDir "Package.appxmanifest.template"
$SourceLogoPath = Join-Path $RepoRoot "windirstat\logos\logo_256px.png"

# Asset sizes for Windows Store
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

# Read version from header
$VersionHeader = Join-Path $RepoRoot "windirstat\Version.h"
$Content = Get-Content -Raw -Path $VersionHeader
$Major = [regex]::Match($Content, "#define\s+PRD_MAJVER\s+(\d+)").Groups[1].Value
$Minor = [regex]::Match($Content, "#define\s+PRD_MINVER\s+(\d+)").Groups[1].Value
$Patch = [regex]::Match($Content, "#define\s+PRD_PATCH\s+(\d+)").Groups[1].Value
$Version = "$Major.$Minor.$Patch.0"

# Get code signing certificate
$CodeSigningEkuOid = "1.3.6.1.5.5.7.3.3"

function Test-HasCodeSigningEku
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Security.Cryptography.X509Certificates.X509Certificate2] $Certificate
    )

    ForEach ($Extension in $Certificate.Extensions)
    {
        If ($Extension -is [System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension])
        {
            ForEach ($Oid in $Extension.EnhancedKeyUsages)
            {
                If ($Oid.Value -eq $CodeSigningEkuOid) { return $true }
            }

            return $false
        }
    }

    return $false
}

function Test-IsTrustedCertificate
{
    param(
        [Parameter(Mandatory = $true)]
        [System.Security.Cryptography.X509Certificates.X509Certificate2] $Certificate
    )

    $Chain = [System.Security.Cryptography.X509Certificates.X509Chain]::new()
    Try
    {
        $Chain.ChainPolicy.RevocationMode = [System.Security.Cryptography.X509Certificates.X509RevocationMode]::NoCheck
        $Chain.ChainPolicy.RevocationFlag = [System.Security.Cryptography.X509Certificates.X509RevocationFlag]::EntireChain
        $Chain.ChainPolicy.VerificationFlags = [System.Security.Cryptography.X509Certificates.X509VerificationFlags]::NoFlag
        $Chain.ChainPolicy.VerificationTime = Get-Date

        $null = $Chain.Build($Certificate)
        $Status = @($Chain.ChainStatus | Where-Object { $_.Status -ne [System.Security.Cryptography.X509Certificates.X509ChainStatusFlags]::NoError })
        return $Status.Count -eq 0
    }
    Finally
    {
        $Chain.Dispose()
    }
}

$CodeSigningCerts = @(Get-ChildItem Cert:\CurrentUser\My |
    Where-Object {
        $_.HasPrivateKey -and
        (Test-HasCodeSigningEku -Certificate $_) -and
        (Test-IsTrustedCertificate -Certificate $_)
    } |
    Sort-Object NotAfter -Descending)

$Cert = $null
If ($CertificateThumbprint)
{
    $RequestedThumbprint = ($CertificateThumbprint -replace "\s", "").ToUpperInvariant()
    $MatchingCerts = @($CodeSigningCerts | Where-Object { $_.Thumbprint.ToUpperInvariant() -eq $RequestedThumbprint })
    If ($MatchingCerts.Count -eq 0)
    {
        Throw "Code-signing certificate '$CertificateThumbprint' was not found in Cert:\CurrentUser\My as a trusted certificate with private key and EKU $CodeSigningEkuOid."
    }

    $Cert = $MatchingCerts[0]
}
ElseIf ($CodeSigningCerts.Count -eq 1)
{
    $Cert = $CodeSigningCerts[0]
}
ElseIf ($CodeSigningCerts.Count -eq 0)
{
    Write-Warning "No trusted code-signing certificate found in Cert:\CurrentUser\My. Continuing with store/unsigned MSIX only. Install/import one with a private key and EKU $CodeSigningEkuOid, or pass -CertificateThumbprint to also build local signed packages."
}
Else
{
    $CertList = $CodeSigningCerts | ForEach-Object { "  - $($_.Subject) | Thumbprint=$($_.Thumbprint) | Expires=$($_.NotAfter.ToString('yyyy-MM-dd'))" }
    Write-Warning "Multiple trusted code-signing certificates found in Cert:\CurrentUser\My. Continuing with store/unsigned MSIX only.`n$($CertList -join "`n")`nPass -CertificateThumbprint to select one for local signed packages."
}

$StorePublisher = If ($Cert) { $Cert.Subject } Else { $Publisher }

If (-not $Cert)
{
    Write-Host "Skipping local signed MSIX build because no single trusted code-signing certificate was selected."
}
$UnsignedOutDir = Join-Path $OutDir "unsigned"
$StoreTempRoot = Join-Path $OutDir "temp-store"
$StorePackageDir = $OutDir
$LocalTempRoot = Join-Path $UnsignedOutDir "temp"
$LocalPackageDir = $UnsignedOutDir

# Initialize output and temp directories
ForEach ($Dir in @($UnsignedOutDir, $StoreTempRoot, $LocalTempRoot))
{
    If (Test-Path $Dir) { Remove-Item -Recurse -Force -Path $Dir }
    New-Item -ItemType Directory -Force -Path $Dir | Out-Null
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$ManifestTemplate = Get-Content -Raw -Path $TemplatePath
ForEach ($BuildMode in @("store", "local"))
{
    If ($BuildMode -eq "local" -and -not $Cert) { Continue }
    If ($BuildMode -eq "store")
    {
        $CurrentOutDir = $UnsignedOutDir
        $TempRoot = $StoreTempRoot
        $PackageDir = $StorePackageDir
        $ManifestPublisher = $StorePublisher
    }
    Else
    {
        $CurrentOutDir = $OutDir
        $TempRoot = $LocalTempRoot
        $PackageDir = $LocalPackageDir
        $ManifestPublisher = $Publisher
    }

    $StageRoot = Join-Path $TempRoot "stage"
    $BundleInputDir = Join-Path $TempRoot "bundle-input"
    $GeneratedAssetsDir = Join-Path $TempRoot "generated-assets"
    New-Item -ItemType Directory -Force -Path $StageRoot | Out-Null
    New-Item -ItemType Directory -Force -Path $BundleInputDir | Out-Null
    New-Item -ItemType Directory -Force -Path $GeneratedAssetsDir | Out-Null

    Add-Type -AssemblyName System.Drawing
    $SourceImage = [Image]::FromFile((Resolve-Path $SourceLogoPath).Path)
    $Attributes = [ImageAttributes]::new()
    $Attributes.SetWrapMode([WrapMode]::TileFlipXY)

    # Generate resized asset images
    Try
    {
        ForEach ($Asset in $StoreAssets)
        {
            $FileName, $Width, $Height = $Asset
            $Scale = [Math]::Min($Width / $SourceImage.Width, $Height / $SourceImage.Height)
            $DrawWidth = [Math]::Max(1, [int][Math]::Round($SourceImage.Width * $Scale))
            $DrawHeight = [Math]::Max(1, [int][Math]::Round($SourceImage.Height * $Scale))
            $Destination = [Rectangle]::new(
                [int][Math]::Floor(($Width - $DrawWidth) / 2),
                [int][Math]::Floor(($Height - $DrawHeight) / 2),
                $DrawWidth, $DrawHeight)

            $Bitmap = [Bitmap]::new($Width, $Height, [PixelFormat]::Format32bppArgb)
            $Graphics = [Graphics]::FromImage($Bitmap)
            Try
            {
                $Graphics.Clear([Color]::Transparent)
                $Graphics.CompositingQuality = [CompositingQuality]::HighQuality
                $Graphics.InterpolationMode = [InterpolationMode]::HighQualityBicubic
                $Graphics.PixelOffsetMode = [PixelOffsetMode]::HighQuality
                $Graphics.SmoothingMode = [SmoothingMode]::HighQuality
                $Graphics.DrawImage($SourceImage, $Destination,
                    0, 0, $SourceImage.Width, $SourceImage.Height,
                    [GraphicsUnit]::Pixel, $Attributes)
                $Bitmap.Save((Join-Path $GeneratedAssetsDir $FileName), [ImageFormat]::Png)
            }
            Finally
            {
                $Graphics.Dispose()
                $Bitmap.Dispose()
            }
        }
    }
    Finally
    {
        $Attributes.Dispose()
        $SourceImage.Dispose()
    }
    # Pack MSIX packages for each architecture
    $MsixPackages = @()
    ForEach ($Arch in $Platform)
    {
        $StageDir = Join-Path $StageRoot $Arch
        $StageAssetsDir = Join-Path $StageDir "Assets"
        New-Item -ItemType Directory -Force -Path $StageAssetsDir | Out-Null

        $ExePath = $null
        ForEach ($Candidate in @(
            (Join-Path $RepoRoot "publish\$Arch\WinDirStat.exe"),
            (Join-Path $RepoRoot "build\WinDirStat_$Arch.exe")
        ))
        {
            If (Test-Path $Candidate)
            {
                $ExePath = (Resolve-Path $Candidate).Path
                Break
            }
        }

        Copy-Item -Path $ExePath -Destination (Join-Path $StageDir "WinDirStat.exe") -Force
        Copy-Item -Path (Join-Path $GeneratedAssetsDir "*") -Destination $StageAssetsDir -Force

        $Manifest = $ManifestTemplate
        $Replacements = @{
            "{{IdentityName}}" = $IdentityName
            "{{Publisher}}" = $ManifestPublisher
            "{{Version}}" = $Version
            "{{ProcessorArchitecture}}" = $Arch
            "{{DisplayName}}" = $DisplayName
            "{{PublisherDisplayName}}" = $PublisherDisplayName
            "{{MinVersion}}" = $MinVersion
            "{{MaxVersionTested}}" = $MaxVersionTested
        }

        ForEach ($Key in $Replacements.Keys)
        {
            $Escaped = [System.Security.SecurityElement]::Escape([string] $Replacements[$Key])
            $Manifest = $Manifest.Replace($Key, $Escaped)
        }

        Set-Content -Path (Join-Path $StageDir "AppxManifest.xml") -Value $Manifest -Encoding UTF8

        $PackagePath = Join-Path $PackageDir ("WinDirStat_{0}.msix" -f $Arch)
        Write-Host "Packing $PackagePath..."
        & makeappx.exe pack /d $StageDir /p $PackagePath /h SHA256 /o
        If ($LASTEXITCODE -ne 0) { Throw "makeappx.exe failed while packing $PackagePath" }
        $MsixPackages += $PackagePath
    }

    # Create MSIX bundle
    $MsixPackages | Copy-Item -Destination $BundleInputDir -Force
    $BundleArch = ($Platform -join "_")
    $BundlePath = Join-Path $PackageDir ("WinDirStat_{0}.msixbundle" -f $BundleArch)
    Write-Host "Bundling $BundlePath..."
    & makeappx.exe bundle /d $BundleInputDir /p $BundlePath /o
    If ($LASTEXITCODE -ne 0)
    {
        Throw "makeappx.exe failed while bundling $BundlePath"
    }

    Get-ChildItem $PackageDir -Include *.msix, *.msixbundle |
        Copy-Item -Destination $CurrentOutDir -Force
}

# Clean up temporary directories
ForEach ($Dir in @($StoreTempRoot, $LocalTempRoot))
{
    If (Test-Path $Dir) { Remove-Item -Recurse -Force -Path $Dir }
}
