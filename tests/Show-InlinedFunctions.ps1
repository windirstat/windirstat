<#
.SYNOPSIS
    Identifies which functions were fully inlined in the WinDirStat Release build.

.DESCRIPTION
    Performs two builds into isolated output/intermediate directories:

      Build A  –  Default Release settings
                  (InlineFunctionExpansion=AnySuitable / /Ob2, /GL, /LTCG)

      Build B  –  Inlining disabled
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
    [ValidateSet("x64", "Win32", "ARM64")]
    [string]$Platform = "x64",
    [int]$Top = 100,
    [switch]$SkipBuildA,
    [switch]$SkipBuildB
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Paths ────────────────────────────────────────────────────────────────────────
$root         = Split-Path -Parent $PSScriptRoot           # solution root
$solution     = Join-Path $root "windirstat.sln"
$analysisRoot = Join-Path $root "build_analysis"

$platformShort = @{ "Win32" = "x86"; "x64" = "x64"; "ARM64" = "arm64" }[$Platform]
$exeName       = "WinDirStat_$platformShort"

# Separate output/intermediate trees so MSBuild won't reuse stale .obj files.
$outA  = Join-Path $analysisRoot "inline_on\$Platform"
$outB  = Join-Path $analysisRoot "inline_off\$Platform"
$intA  = Join-Path $analysisRoot "intermediate\inline_on\$Platform\windirstat\"
$intB  = Join-Path $analysisRoot "intermediate\inline_off\$Platform\windirstat\"
$mapA  = Join-Path $outA  "$exeName.map"
$mapB  = Join-Path $outB  "$exeName.map"

# Temporary .targets override files (cleaned up on exit)
$targetsA = Join-Path $analysisRoot "override_inline_on.targets"
$targetsB = Join-Path $analysisRoot "override_inline_off.targets"

$reportFile = Join-Path $analysisRoot "inlined.txt"

if (-not (Test-Path $solution)) { throw "Solution not found: $solution" }
New-Item -ItemType Directory -Force -Path $outA, $outB, $intA, $intB | Out-Null

# ── Locate toolchain ─────────────────────────────────────────────────────────────
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found — is Visual Studio installed?" }

$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild `
    -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild.exe not found" }

# Best-effort: find undname.exe for symbol undecorating
$vsRoot  = & $vswhere -latest -property installationPath
$undname = Get-ChildItem -Path "$vsRoot\VC\Tools\MSVC" -Filter "undname.exe" -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.DirectoryName -match "Hostx64.x64$|HostX86.x86$" } |
    Select-Object -First 1 -ExpandProperty FullName

# ── .targets file generator ───────────────────────────────────────────────────────
# ForceImportAfterCppTargets is imported after the project's own ItemDefinitionGroups,
# so metadata defined here wins over project-level settings.
function New-OverrideTargets {
    param(
        [string]$Path,
        [string]$MapFile,
        [hashtable]$ClCompileExtra = @{},
        [hashtable]$LinkExtra      = @{}
    )

    $clLines   = $ClCompileExtra.GetEnumerator() |
        ForEach-Object { "      <$($_.Key)>$($_.Value)</$($_.Key)>" }
    $linkLines = $LinkExtra.GetEnumerator() |
        ForEach-Object { "      <$($_.Key)>$($_.Value)</$($_.Key)>" }

    # Escape backslashes in the path for XML (backslash is valid XML; no escaping
    # needed for element content, but use forward slashes to be safe with MSBuild)
    $mapFileXml = $MapFile.Replace('\', '/')

    Set-Content -Path $Path -Encoding UTF8 -Value @"
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <!-- Override injected by Show-InlinedFunctions.ps1 -->
  <ItemDefinitionGroup>
    <ClCompile>
$($clLines -join "`n")
    </ClCompile>
    <Link>
      <GenerateMapFile>true</GenerateMapFile>
      <MapFileName>$mapFileXml</MapFileName>
$($linkLines -join "`n")
    </Link>
  </ItemDefinitionGroup>
</Project>
"@
}

# ── MSBuild helper ───────────────────────────────────────────────────────────────
function Invoke-MSBuild([string]$label, [string]$targetsFile, [string]$outDir, [string]$intDir) {
    $sep = "─" * 72
    Write-Host "`n$sep" -ForegroundColor DarkGray
    Write-Host " $label" -ForegroundColor Cyan
    Write-Host "$sep`n" -ForegroundColor DarkGray

    & $msbuild $solution `
        /t:Build `
        /p:Configuration=Release `
        /p:Platform=$Platform `
        "/p:OutDir=$outDir\" `
        "/p:IntDir=$intDir" `
        "/p:ForceImportAfterCppTargets=$targetsFile" `
        /m

    if ($LASTEXITCODE -ne 0) { throw "Build FAILED — $label" }
}

# ── Cleanup helper ───────────────────────────────────────────────────────────────
$script:cleanupFiles = @()
function Register-Cleanup([string]$path) { $script:cleanupFiles += $path }

try {

# ═══════════════════════════════════════════════════════════════════════════════
#  Build A — default Release (inlining ON, LTCG ON)
# ═══════════════════════════════════════════════════════════════════════════════
if (-not $SkipBuildA) {
    New-OverrideTargets -Path $targetsA -MapFile $mapA
    Register-Cleanup $targetsA
    Invoke-MSBuild "Build A — inlining ENABLED (default Release)" $targetsA $outA $intA
} else {
    Write-Host "Skipping Build A (–SkipBuildA)" -ForegroundColor DarkYellow
}

if (-not (Test-Path $mapA)) { throw "MAP file not found after Build A: $mapA" }

# ═══════════════════════════════════════════════════════════════════════════════
#  Build B — inlining disabled
#   InlineFunctionExpansion=Disabled  →  /Ob0 (no expansion, even __forceinline)
#   WholeProgramOptimization=false    →  removes /GL (whole-program opt)
#   LinkTimeCodeGeneration=Default    →  removes /LTCG
# ═══════════════════════════════════════════════════════════════════════════════
if (-not $SkipBuildB) {
    New-OverrideTargets -Path $targetsB -MapFile $mapB `
        -ClCompileExtra @{
            InlineFunctionExpansion  = "Disabled"
            WholeProgramOptimization = "false"
        } `
        -LinkExtra @{
            LinkTimeCodeGeneration = "Default"
        }
    Register-Cleanup $targetsB
    Invoke-MSBuild "Build B — inlining DISABLED (/Ob0, no LTCG)" $targetsB $outB $intB
} else {
    Write-Host "Skipping Build B (–SkipBuildB)" -ForegroundColor DarkYellow
}

if (-not (Test-Path $mapB)) { throw "MAP file not found after Build B: $mapB" }

# ═══════════════════════════════════════════════════════════════════════════════
#  MAP file parser
# ═══════════════════════════════════════════════════════════════════════════════
# Returns an OrderedDictionary: mangled-name -> @{ Loc = "seg  module"; RVA = [int64] }
# Only captures code-section symbols (segment 0001:) to avoid noise from data.
function Get-MapSymbols([string]$path) {
    $table  = [ordered]@{}
    $active = $false
    foreach ($line in [System.IO.File]::ReadLines($path)) {
        if ($line -match 'Publics by Value|Static symbols') { $active = $true; continue }
        if (-not $active) { continue }
        if ($line -match '^\s+(0001:[0-9A-Fa-f]{8})\s+(\S+)\s+([0-9A-Fa-f]{16})\s+(.+)') {
            $seg, $sym, $rva, $mod = $Matches[1], $Matches[2], $Matches[3], $Matches[4].Trim()
            # Skip CRT/linker-generated symbols that are always present
            if ($sym -notmatch '^(__imp_|_NULL_THUNK|__ImageBase|__safe_se)') {
                $table[$sym] = @{
                    Loc = "$seg  $mod"
                    RVA = [Convert]::ToInt64($rva, 16)
                }
            }
        }
    }
    return $table
}

# Approximate function sizes: sort all symbols by RVA, then
# size[i] = RVA[i+1] - RVA[i].  The last symbol gets size 0.
function Get-FunctionSizes($symbols) {
    $sorted = @($symbols.GetEnumerator() | Sort-Object { $_.Value.RVA })
    $sizes  = @{}
    for ($i = 0; $i -lt $sorted.Count; $i++) {
        $cur  = $sorted[$i].Value.RVA
        $next = if ($i + 1 -lt $sorted.Count) { $sorted[$i + 1].Value.RVA } else { $cur }
        $sizes[$sorted[$i].Key] = [Math]::Max(0L, $next - $cur)
    }
    return $sizes
}

# ── Undecorate helper ─────────────────────────────────────────────────────────────
function Get-PrettyName([string]$mangled) {
    if (-not $undname) { return $null }
    try {
        $out = $mangled | & $undname 2>$null | Select-Object -Last 1
        if ($out -match 'is "(.+)"') { return $Matches[1] }
    } catch {}
    return $null
}

# ═══════════════════════════════════════════════════════════════════════════════
#  Analysis
# ═══════════════════════════════════════════════════════════════════════════════
$sep = "═" * 72
Write-Host "`n$sep" -ForegroundColor Green
Write-Host " Inline analysis  —  $Platform  Release" -ForegroundColor Green
Write-Host "$sep" -ForegroundColor Green

$symA = Get-MapSymbols $mapA
$symB = Get-MapSymbols $mapB

$sizeA = (Get-Item (Join-Path $outA "$exeName.exe")).Length
$sizeB = (Get-Item (Join-Path $outB "$exeName.exe")).Length

Write-Host ""
Write-Host ("  {0,-42} {1,8} symbols   {2,8:N0} bytes" -f "Default build (inlining on):", $symA.Count, $sizeA)
Write-Host ("  {0,-42} {1,8} symbols   {2,8:N0} bytes" -f "No-inline build:", $symB.Count, $sizeB)
Write-Host ("  Size increase from disabling inlining: {0:+#,##0;-#,##0;0} bytes ({1:+0.1;-0.1;0}%)" -f `
    ($sizeB - $sizeA), (($sizeB - $sizeA) * 100.0 / $sizeA))

# Functions present in no-inline MAP but absent from default MAP were completely
# eliminated — every call site had the body inlined, so the linker needed no
# standalone copy.
$eliminated = @($symB.Keys | Where-Object { -not $symA.Contains($_) } | Sort-Object)

# Compute approximate code sizes from Build B (consecutive-RVA differences).
$sizesB = Get-FunctionSizes $symB

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("Functions fully inlined in WinDirStat Release|$Platform")
$lines.Add("Generated     : $(Get-Date -Format 'yyyy-MM-dd HH:mm')")
$lines.Add("Default MAP   : $mapA  ($($symA.Count) symbols,  $("{0:N0}" -f $sizeA) bytes)")
$lines.Add("No-inline MAP : $mapB  ($($symB.Count) symbols,  $("{0:N0}" -f $sizeB) bytes)")
$lines.Add("")

# ── Top-N summary table ───────────────────────────────────────────────────────
$showCount = [Math]::Min($Top, $eliminated.Count)
$ranked = $eliminated |
    ForEach-Object { [pscustomobject]@{ Sym = $_; Size = $sizesB[$_] } } |
    Sort-Object -Property Size -Descending |
    Select-Object -First $showCount

Write-Host ""
Write-Host ("─" * 72) -ForegroundColor DarkGray
Write-Host (" Top {0} inlined functions by size (of {1} total):" -f $showCount, $eliminated.Count) -ForegroundColor Yellow
Write-Host ("─" * 72) -ForegroundColor DarkGray
Write-Host ("  {0,4}  {1,7}  {2}" -f "Rank", "Bytes", "Function") -ForegroundColor DarkGray
Write-Host ("  {0,4}  {1,7}  {2}" -f "────", "───────", "─" * 54) -ForegroundColor DarkGray

$lines.Add("Top $showCount inlined functions by size (of $($eliminated.Count) total):")
$lines.Add(("  {0,4}  {1,7}  {2}" -f "Rank", "Bytes", "Function"))
$lines.Add(("  {0,4}  {1,7}  {2}" -f "----", "-------", "-" * 54))

$rank = 1
foreach ($entry in $ranked) {
    $pretty  = Get-PrettyName $entry.Sym
    $display = if ($pretty) { $pretty } else { $entry.Sym }
    $row = "  {0,4}  {1,7:N0}  {2}" -f $rank, $entry.Size, $display
    Write-Host $row
    $lines.Add($row)
    $rank++
}

# ── Full detail list (report file only) ──────────────────────────────────────
$lines.Add("")
$lines.Add("Full list — all $($eliminated.Count) inlined functions:")
$lines.Add("")

foreach ($sym in $eliminated) {
    $pretty = Get-PrettyName $sym
    $loc    = $symB[$sym].Loc
    $sz     = $sizesB[$sym]
    if ($pretty) {
        $lines.Add($pretty)
        $lines.Add("    $sz bytes  |  $sym")
        $lines.Add("    $loc")
    } else {
        $lines.Add($sym)
        $lines.Add("    $sz bytes  |  $loc")
    }
    $lines.Add("")
}

$lines | Set-Content -Path $reportFile -Encoding UTF8

Write-Host ""
Write-Host "  Report : $reportFile" -ForegroundColor DarkGray
Write-Host "  Map A  : $mapA"       -ForegroundColor DarkGray
Write-Host "  Map B  : $mapB"       -ForegroundColor DarkGray
Write-Host ""

} finally {
    # Remove temporary .targets files so they don't interfere with normal builds
    foreach ($f in $script:cleanupFiles) {
        if (Test-Path $f) { Remove-Item $f -Force -ErrorAction SilentlyContinue }
    }
}
