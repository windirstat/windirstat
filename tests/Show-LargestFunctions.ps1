#Requires -Version 7.0
<#
.SYNOPSIS
    Shows the largest functions by code size in the WinDirStat Release executable.

.DESCRIPTION
    Builds the default Release configuration with a linker MAP file, then computes
    approximate function sizes from consecutive-RVA differences and prints a ranked
    table of the top N largest functions.

.PARAMETER Platform
    Target platform: x64 (default), Win32, or ARM64.

.PARAMETER Top
    Number of functions to show (default 100).

.PARAMETER SkipBuild
    Skip the build and reuse an existing MAP file in build_analysis\largest\<Platform>\.

.EXAMPLE
    .\Show-LargestFunctions.ps1

.EXAMPLE
    .\Show-LargestFunctions.ps1 -Platform Win32 -Top 50

.EXAMPLE
    .\Show-LargestFunctions.ps1 -SkipBuild
#>
param(
    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string] $Platform = 'x64',

    [int] $Top = 100,

    [switch] $SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-ColoredLine {
    param([Parameter(Mandatory)] [string] $Message, [ConsoleColor] $Color = [ConsoleColor]::Gray)
    Write-Host $Message -ForegroundColor $Color
}

function Write-LabelValue {
    param(
        [Parameter(Mandatory)] [string] $Label,
        [AllowNull()] [object] $Value,
        [ConsoleColor] $ValueColor = [ConsoleColor]::Gray
    )
    Write-Host -NoNewline "${Label}: " -ForegroundColor DarkCyan
    Write-Host $Value -ForegroundColor $ValueColor
}

$root         = Split-Path -Parent $PSScriptRoot
$solution     = Join-Path $root 'windirstat.sln'
$analysisRoot = Join-Path $root 'build_analysis'

$platformShort = @{ 'Win32' = 'x86'; 'x64' = 'x64'; 'ARM64' = 'arm64' }[$Platform]
$exeName       = "WinDirStat_$platformShort"

$outDir      = Join-Path $analysisRoot "largest\$Platform"
$intDir      = Join-Path $analysisRoot "intermediate\largest\$Platform\windirstat\"
$mapFile     = Join-Path $outDir "$exeName.map"
$targetsFile = Join-Path $analysisRoot 'override_largest.targets'
$reportFile  = Join-Path $analysisRoot 'largest_functions.txt'

if (-not (Test-Path -LiteralPath $solution)) { throw "Solution not found: $solution" }
New-Item -ItemType Directory -Force -Path $outDir, $intDir | Out-Null

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere.exe not found — is Visual Studio installed?' }

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild `
    -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild.exe not found' }

$vsRoot  = & $vswhere -latest -property installationPath
$undname = Get-ChildItem -Path "$vsRoot\VC\Tools\MSVC" -Filter 'undname.exe' -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.DirectoryName -match 'Hostx64.x64$|HostX86.x86$' } |
    Select-Object -First 1 -ExpandProperty FullName

try {
    if (-not $SkipBuild) {
        $mapFileXml = $mapFile.Replace('\', '/')
        Set-Content -Path $targetsFile -Encoding UTF8 -Value @"
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <!-- Override injected by Show-LargestFunctions.ps1 -->
  <ItemDefinitionGroup>
    <Link>
      <GenerateMapFile>true</GenerateMapFile>
      <MapFileName>$mapFileXml</MapFileName>
    </Link>
  </ItemDefinitionGroup>
</Project>
"@
        Write-ColoredLine ('─' * 72) DarkGray
        Write-ColoredLine " Building Release|$Platform with MAP file..." Cyan
        Write-ColoredLine ('─' * 72) DarkGray

        & $msbuild $solution `
            /t:Build `
            /p:Configuration=Release `
            "/p:Platform=$Platform" `
            "/p:OutDir=$outDir\" `
            "/p:IntDir=$intDir" `
            "/p:ForceImportAfterCppTargets=$targetsFile" `
            /m

        if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    }
    else {
        Write-ColoredLine 'Skipping build (-SkipBuild)' DarkYellow
    }
}
finally {
    if (Test-Path -LiteralPath $targetsFile) {
        Remove-Item -LiteralPath $targetsFile -Force -ErrorAction SilentlyContinue
    }
}

if (-not (Test-Path -LiteralPath $mapFile)) { throw "MAP file not found: $mapFile  (run without -SkipBuild)" }

function Get-MapSymbols {
    param([Parameter(Mandatory)] [string] $Path)

    $table  = [ordered] @{}
    $active = $false
    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if ($line -match 'Publics by Value|Static symbols') { $active = $true; continue }
        if (-not $active) { continue }
        if ($line -match '^\s+(0001:[0-9A-Fa-f]{8})\s+(\S+)\s+([0-9A-Fa-f]{16})\s+(.+)') {
            $seg, $sym, $rva, $mod = $Matches[1], $Matches[2], $Matches[3], $Matches[4].Trim()
            if ($sym -notmatch '^(__imp_|_NULL_THUNK|__ImageBase|__safe_se)') {
                $table[$sym] = @{ Loc = "$seg  $mod"; RVA = [Convert]::ToInt64($rva, 16) }
            }
        }
    }
    return $table
}

function Get-FunctionSizes {
    param([Parameter(Mandatory)] [object] $Symbols)

    $sorted = @($Symbols.GetEnumerator() | Sort-Object { $_.Value.RVA })
    $sizes  = @{}
    for ($i = 0; $i -lt $sorted.Count; $i++) {
        $cur  = $sorted[$i].Value.RVA
        $next = ($i + 1 -lt $sorted.Count) ? $sorted[$i + 1].Value.RVA : $cur
        $sizes[$sorted[$i].Key] = [Math]::Max(0L, $next - $cur)
    }
    return $sizes
}

function Get-PrettyName {
    param([Parameter(Mandatory)] [string] $Mangled)

    if (-not $undname) { return $null }
    try {
        $out = $Mangled | & $undname 2>$null | Select-Object -Last 1
        if ($out -match 'is "(.+)"') { return $Matches[1] }
    } catch {}
    return $null
}

$symbols  = Get-MapSymbols -Path $mapFile
$sizes    = Get-FunctionSizes -Symbols $symbols
$exeSize  = (Get-Item -LiteralPath (Join-Path $outDir "$exeName.exe")).Length

$showCount = [Math]::Min($Top, $symbols.Count)
$ranked = $symbols.Keys |
    ForEach-Object { [pscustomobject] @{ Sym = $_; Size = $sizes[$_]; Loc = $symbols[$_].Loc } } |
    Sort-Object -Property Size -Descending |
    Select-Object -First $showCount

Write-Host ''
Write-ColoredLine ('═' * 72) Green
Write-ColoredLine " Largest functions  —  $Platform  Release" Green
Write-ColoredLine ('═' * 72) Green
Write-Host ''
Write-LabelValue 'Executable' "$exeName.exe  ($("{0:N0}" -f $exeSize) bytes)"
Write-LabelValue 'Symbols'    "$("{0:N0}" -f $symbols.Count) code-section functions in MAP"
Write-Host ''
Write-ColoredLine ('─' * 72) DarkGray
Write-ColoredLine (' Top {0} functions by size:' -f $showCount) Yellow
Write-ColoredLine ('─' * 72) DarkGray
Write-ColoredLine ('  {0,4}  {1,7}  {2}' -f 'Rank', 'Bytes', 'Function') DarkGray
Write-ColoredLine ('  {0,4}  {1,7}  {2}' -f '────', '───────', '─' * 54) DarkGray

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("Largest functions in WinDirStat Release|$Platform")
$lines.Add("Generated  : $(Get-Date -Format 'yyyy-MM-dd HH:mm')")
$lines.Add("Executable : $exeName.exe  ($("{0:N0}" -f $exeSize) bytes)")
$lines.Add("MAP file   : $mapFile")
$lines.Add('')
$lines.Add("Top $showCount functions by size:")
$lines.Add(('  {0,4}  {1,7}  {2}' -f 'Rank', 'Bytes', 'Function'))
$lines.Add(('  {0,4}  {1,7}  {2}' -f '----', '-------', '-' * 54))

$rank = 1
foreach ($entry in $ranked) {
    $pretty  = Get-PrettyName -Mangled $entry.Sym
    $display = $pretty ?? $entry.Sym
    $row = '  {0,4}  {1,7:N0}  {2}' -f $rank, $entry.Size, $display
    Write-Host $row
    $lines.Add($row)
    $rank++
}

$lines | Set-Content -Path $reportFile -Encoding UTF8

Write-Host ''
Write-LabelValue 'Report' $reportFile DarkGray
Write-LabelValue 'MAP'    $mapFile    DarkGray
Write-Host ''
