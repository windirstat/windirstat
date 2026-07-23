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
. (Join-Path $PSScriptRoot 'ScriptSupport.ps1')

$context = New-MapAnalysisContext -Platform $Platform
$analysisRoot = $context.AnalysisRoot
$exeName = $context.ExeName

$outDir      = Join-Path $analysisRoot "largest\$Platform"
$intDir      = Join-Path $analysisRoot "intermediate\largest\$Platform\windirstat\"
$mapFile     = Join-Path $outDir "$exeName.map"
$targetsFile = Join-Path $analysisRoot 'override_largest.targets'
$reportFile  = Join-Path $analysisRoot 'largest_functions.txt'

New-Item -ItemType Directory -Force -Path $outDir, $intDir | Out-Null

try {
    if (-not $SkipBuild) {
        New-MapOverrideTargets -Path $targetsFile -MapFile $mapFile
        Invoke-MapBuild -Context $context -Label "Building Release|$Platform with MAP file" `
            -TargetsFile $targetsFile -OutDir $outDir -IntDir $intDir
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
    $pretty  = Get-PrettyName -Mangled $entry.Sym -Undname $context.Undname
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
