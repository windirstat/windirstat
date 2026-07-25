#Requires -Version 7.0
<#
.SYNOPSIS
    Identifies which functions were fully inlined in the WinDirStat Release build.

.DESCRIPTION
    Performs two builds into isolated output/intermediate directories:

      Build A  —  Default Release settings
                  (InlineFunctionExpansion=AnySuitable / /Ob2, /GL, /LTCG)

      Build B  —  Inlining disabled
                  (InlineFunctionExpansion=Disabled / /Ob0, WholeProgramOptimization
                   disabled, LinkTimeCodeGeneration=Default)

    A linker MAP file is generated for each build via a temporary .targets override
    file injected through the ForceImportAfterCppTargets MSBuild hook.  Because this
    file is imported after the project's own ItemDefinitionGroups, its metadata wins.

    Functions present in Build B's MAP but absent from Build A's were completely
    inlined away — no standalone copy survived in the optimised binary.

    Results are printed to the console and saved to build_analysis\inlined.txt.

.PARAMETER Platform
    Target platform: x64 (default), Win32, or ARM64.

.PARAMETER Top
    Number of largest inlined functions to display (default 100).

.PARAMETER SkipBuildA
    Skip the default-settings build and reuse an existing MAP file.

.PARAMETER SkipBuildB
    Skip the no-inline build and reuse an existing MAP file.

.EXAMPLE
    .\Show-InlinedFunctions.ps1

.EXAMPLE
    .\Show-InlinedFunctions.ps1 -SkipBuildA -SkipBuildB
#>
param(
    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string] $Platform = 'x64',

    [int] $Top = 100,

    [switch] $SkipBuildA,

    [switch] $SkipBuildB
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'ScriptSupport.ps1')

$context = New-MapAnalysisContext -Platform $Platform
$analysisRoot = $context.AnalysisRoot
$exeName = $context.ExeName

$outA  = Join-Path $analysisRoot "inline_on\$Platform"
$outB  = Join-Path $analysisRoot "inline_off\$Platform"
$intA  = Join-Path $analysisRoot "intermediate\inline_on\$Platform\windirstat\"
$intB  = Join-Path $analysisRoot "intermediate\inline_off\$Platform\windirstat\"
$mapA  = Join-Path $outA "$exeName.map"
$mapB  = Join-Path $outB "$exeName.map"

$targetsA   = Join-Path $analysisRoot 'override_inline_on.targets'
$targetsB   = Join-Path $analysisRoot 'override_inline_off.targets'
$reportFile = Join-Path $analysisRoot 'inlined.txt'

New-Item -ItemType Directory -Force -Path $outA, $outB, $intA, $intB | Out-Null

$script:cleanupFiles = @()
function Register-Cleanup { param([string] $Path) $script:cleanupFiles += $Path }

try {
    if (-not $SkipBuildA) {
        New-MapOverrideTargets -Path $targetsA -MapFile $mapA
        Register-Cleanup $targetsA
        Invoke-MapBuild -Context $context -Label 'Build A — inlining ENABLED (default Release)' `
            -TargetsFile $targetsA -OutDir $outA -IntDir $intA
    }
    else {
        Write-ColoredLine 'Skipping Build A (-SkipBuildA)' DarkYellow
    }

    if (-not (Test-Path -LiteralPath $mapA)) { throw "MAP file not found after Build A: $mapA" }

    if (-not $SkipBuildB) {
        New-MapOverrideTargets -Path $targetsB -MapFile $mapB `
            -ClCompileExtra @{
                InlineFunctionExpansion  = 'Disabled'
                WholeProgramOptimization = 'false'
            } `
            -LinkExtra @{
                LinkTimeCodeGeneration = 'Default'
            }
        Register-Cleanup $targetsB
        Invoke-MapBuild -Context $context -Label 'Build B — inlining DISABLED (/Ob0, no LTCG)' `
            -TargetsFile $targetsB -OutDir $outB -IntDir $intB
    }
    else {
        Write-ColoredLine 'Skipping Build B (-SkipBuildB)' DarkYellow
    }

    if (-not (Test-Path -LiteralPath $mapB)) { throw "MAP file not found after Build B: $mapB" }

    Write-ColoredLine ''
    Write-ColoredLine ('═' * 72) Green
    Write-ColoredLine " Inline analysis  —  $Platform  Release" Green
    Write-ColoredLine ('═' * 72) Green

    $symA = Get-MapSymbols -Path $mapA
    $symB = Get-MapSymbols -Path $mapB

    $sizeA = (Get-Item -LiteralPath (Join-Path $outA "$exeName.exe")).Length
    $sizeB = (Get-Item -LiteralPath (Join-Path $outB "$exeName.exe")).Length

    Write-Host ''
    Write-Host ('  {0,-42} {1,8} symbols   {2,8:N0} bytes' -f 'Default build (inlining on):', $symA.Count, $sizeA)
    Write-Host ('  {0,-42} {1,8} symbols   {2,8:N0} bytes' -f 'No-inline build:', $symB.Count, $sizeB)
    Write-Host ('  Size increase from disabling inlining: {0:+#,##0;-#,##0;0} bytes ({1:+0.1;-0.1;0}%)' -f `
        ($sizeB - $sizeA), (($sizeB - $sizeA) * 100.0 / $sizeA))

    $eliminated = @($symB.Keys | Where-Object { -not $symA.Contains($_) } | Sort-Object)
    $sizesB     = Get-FunctionSizes -Symbols $symB

    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add("Functions fully inlined in WinDirStat Release|$Platform")
    $lines.Add("Generated     : $(Get-Date -Format 'yyyy-MM-dd HH:mm')")
    $lines.Add("Default MAP   : $mapA  ($($symA.Count) symbols,  $("{0:N0}" -f $sizeA) bytes)")
    $lines.Add("No-inline MAP : $mapB  ($($symB.Count) symbols,  $("{0:N0}" -f $sizeB) bytes)")
    $lines.Add('')

    $showCount = [Math]::Min($Top, $eliminated.Count)
    $ranked = $eliminated |
        ForEach-Object { [pscustomobject] @{ Sym = $_; Size = $sizesB[$_] } } |
        Sort-Object -Property Size -Descending |
        Select-Object -First $showCount

    Write-Host ''
    Write-ColoredLine ('─' * 72) DarkGray
    Write-ColoredLine (' Top {0} inlined functions by size (of {1} total):' -f $showCount, $eliminated.Count) Yellow
    Write-ColoredLine ('─' * 72) DarkGray
    Write-ColoredLine ('  {0,4}  {1,7}  {2}' -f 'Rank', 'Bytes', 'Function') DarkGray
    Write-ColoredLine ('  {0,4}  {1,7}  {2}' -f '────', '───────', '─' * 54) DarkGray

    $lines.Add("Top $showCount inlined functions by size (of $($eliminated.Count) total):")
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

    $lines.Add('')
    $lines.Add("Full list — all $($eliminated.Count) inlined functions:")
    $lines.Add('')

    foreach ($sym in $eliminated) {
        $pretty = Get-PrettyName -Mangled $sym -Undname $context.Undname
        $loc    = $symB[$sym].Loc
        $sz     = $sizesB[$sym]
        if ($pretty) {
            $lines.Add($pretty)
            $lines.Add("    $sz bytes  |  $sym")
            $lines.Add("    $loc")
        }
        else {
            $lines.Add($sym)
            $lines.Add("    $sz bytes  |  $loc")
        }
        $lines.Add('')
    }

    $lines | Set-Content -Path $reportFile -Encoding UTF8

    Write-Host ''
    Write-LabelValue 'Report' $reportFile DarkGray
    Write-LabelValue 'Map A'  $mapA       DarkGray
    Write-LabelValue 'Map B'  $mapB       DarkGray
    Write-Host ''
}
finally {
    foreach ($f in $script:cleanupFiles) {
        if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue }
    }
}
