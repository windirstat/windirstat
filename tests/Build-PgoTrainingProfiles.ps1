<#
.SYNOPSIS
    Drives an instrumented WinDirStat build through a representative set of
    workloads to produce Profile Guided Optimization (PGO) data.

.DESCRIPTION
    Visual C++ PGO is a three-step pipeline:

      1. Build with /LTCG:PGINSTRUMENT (the linker emits a seed .pgd next to
         the .exe).
      2. Run the instrumented .exe through realistic scenarios. Each run
         flushes one or more <name>!N.pgc files into the .exe directory.
      3. Merge the .pgc files into the .pgd with pgomgr.exe; rebuild with
         /LTCG:PGOPTIMIZE.

    This script automates step 2 (and the per-scenario merge cleanup so
    .pgc files do not accumulate). It exercises:

      * accelerated (NTFS USN/MFT) scanning of C:\
      * legacy/per-file scanning of C:\
      * a duplicate scan of C:\Windows
      * a restrictive include / minor exclude scan of C:\
      * regex-mode filtering, size and age filters
      * hidden / protected / unknown inclusion
      * single- vs many-thread scanning
      * hash-algorithm variations for the dupe engine
      * CSV and JSON save round-trips, plus load-from-file paths
      * privilege / reparse / hardlink toggles

    Settings are written to a portable WinDirStat.ini next to the executable
    so the user's HKCU\Software\WinDirStat hive is never touched. The .ini
    is rewritten before every scenario.

.PARAMETER ExePath
    Path to the instrumented WinDirStat .exe (must have a sibling .pgd seed
    file produced by the PGINSTRUMENT link).

.PARAMETER WorkDir
    Where the .ini, .pgc, .pgd and scenario artifacts live. Defaults to the
    directory containing ExePath (which is where the runtime drops .pgc).

.PARAMETER PgoMgrPath
    Optional explicit path to pgomgr.exe. Auto-detected via vswhere otherwise.

.PARAMETER ScanRoot
    Root of the full-disk scan scenarios. Defaults to C:\.

.PARAMETER DupeRoot
    Root of the duplicate-detection scenarios. Defaults to C:\Windows.

.PARAMETER RestrictiveIncludeRoot
    Folder used as the anchor for the restrictive-include scenarios. Defaults
    to C:\Windows\System32 (a deeply nested, file-rich path).

.PARAMETER PerScenarioTimeoutSec
    Hard cap on a single scenario; the process is force-killed if it exceeds
    this. Default 1800 (30 min).

.PARAMETER LoadSettleSeconds
    How long to leave a /loadfrom session open before terminating it (loaded
    scans do not self-exit). Default 12.

.PARAMETER OnlyScenarios
    If supplied, only the named scenarios run (case-insensitive match against
    the scenario Name).

.PARAMETER SkipScenarios
    Scenarios whose Name matches anything in this list are skipped.

.PARAMETER List
    Print the scenario catalog and exit; no execution, no merges.

.PARAMETER KeepArtifacts
    Keep the per-scenario CSV/JSON outputs. Without this they are deleted
    after each scenario to save space.

.PARAMETER NoMerge
    Skip the pgomgr merge step (leaves all .pgc files in WorkDir for manual
    inspection).

.EXAMPLE
    .\Build-PgoTrainingProfiles.ps1 -ExePath C:\PGO\WinDirStat_x64.exe

.EXAMPLE
    .\Build-PgoTrainingProfiles.ps1 -ExePath C:\PGO\WinDirStat_x64.exe `
        -OnlyScenarios ScanFastEngine,DupeScanWindowsJson -KeepArtifacts
#>

#requires -Version 7.2

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $ExePath,

    [string] $WorkDir,

    [string] $PgoMgrPath,

    [string] $ScanRoot = 'C:\',

    [string] $DupeRoot = 'C:\Windows',

    [string] $RestrictiveIncludeRoot = 'C:\Windows\System32',

    [int] $PerScenarioTimeoutSec = 1800,

    [int] $LoadSettleSeconds = 12,

    [string[]] $OnlyScenarios,

    [string[]] $SkipScenarios,

    [switch] $List,

    [switch] $KeepArtifacts,

    [switch] $NoMerge
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# --- Constants -------------------------------------------------------------

# MFC's Setting<std::wstring> stores embedded newlines as ASCII 0x1E.
# Filtering.cpp splits the decoded value on \n, so we have to write 0x1E
# as the inter-pattern delimiter inside the .ini.
$RS = [char] 0x1E
$ScenarioStart = [DateTime]::UtcNow

# --- Logging helpers -------------------------------------------------------

function Write-Section {
    param([Parameter(Mandatory)] [string] $Title)
    Write-Host ''
    Write-Host ('=' * 78) -ForegroundColor DarkCyan
    Write-Host $Title -ForegroundColor Cyan
    Write-Host ('=' * 78) -ForegroundColor DarkCyan
}

function Write-Step {
    param([Parameter(Mandatory)] [string] $Message)
    Write-Host "[$(Get-Date -Format HH:mm:ss)] $Message" -ForegroundColor Gray
}

function Write-Ok {
    param([Parameter(Mandatory)] [string] $Message)
    Write-Host "[$(Get-Date -Format HH:mm:ss)] $Message" -ForegroundColor Green
}

function Write-Warn {
    param([Parameter(Mandatory)] [string] $Message)
    Write-Host "[$(Get-Date -Format HH:mm:ss)] $Message" -ForegroundColor Yellow
}

function Write-ErrorLine {
    param([Parameter(Mandatory)] [string] $Message)
    Write-Host "[$(Get-Date -Format HH:mm:ss)] $Message" -ForegroundColor Red
}

# --- Pre-flight ------------------------------------------------------------

function Resolve-ExeContext {
    if (!(Test-Path -LiteralPath $ExePath -PathType Leaf)) {
        throw "ExePath '$ExePath' does not exist or is not a file."
    }
    $script:ExeFull = (Resolve-Path -LiteralPath $ExePath).ProviderPath
    $script:ExeDir  = Split-Path -Parent $script:ExeFull
    $script:ExeBase = [System.IO.Path]::GetFileNameWithoutExtension($script:ExeFull)
    $script:IniPath = Join-Path $script:ExeDir 'WinDirStat.ini'
    $script:PgdPath = Join-Path $script:ExeDir ($script:ExeBase + '.pgd')

    $script:WorkDirFull = if ([string]::IsNullOrWhiteSpace($WorkDir)) {
        $script:ExeDir
    } else {
        New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
        (Resolve-Path -LiteralPath $WorkDir).ProviderPath
    }
    $script:ArtifactDir = Join-Path $script:WorkDirFull 'pgo-artifacts'
    New-Item -ItemType Directory -Force -Path $script:ArtifactDir | Out-Null

    if (!(Test-Path -LiteralPath $PgdPath)) {
        Write-Warn "Seed .pgd '$PgdPath' not found. Either the binary is not PGINSTRUMENT-linked or the .pgd lives elsewhere. Continuing; merges will fail without it."
    }
}

function Resolve-PgoMgr {
    if ($PgoMgrPath) {
        if (!(Test-Path -LiteralPath $PgoMgrPath -PathType Leaf)) {
            throw "PgoMgrPath '$PgoMgrPath' does not exist."
        }
        $script:PgoMgrFull = (Resolve-Path -LiteralPath $PgoMgrPath).ProviderPath
        return
    }

    # Try PATH first
    $cmd = Get-Command pgomgr.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        $script:PgoMgrFull = $cmd.Source
        return
    }

    # Try vswhere → MSVC toolchain. Pick the host/target folder that matches
    # the executable's architecture.
    $vswhere = ${env:ProgramFiles(x86)} ?? $env:ProgramFiles |
        ForEach-Object { Join-Path $_ 'Microsoft Visual Studio\Installer\vswhere.exe' } |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1

    # Also try the standard install location explicitly in case neither env
    # var resolved a hit above.
    $vswhere ??= @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path  $env:ProgramFiles      'Microsoft Visual Studio\Installer\vswhere.exe')
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

    if (-not $vswhere) {
        throw 'pgomgr.exe was not on PATH and vswhere.exe was not found. Pass -PgoMgrPath explicitly.'
    }

    $vsRoot = & $vswhere -latest -products '*' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsRoot) {
        throw 'vswhere did not report a Visual Studio install with the C++ tools.'
    }

    $msvcRoot = Join-Path $vsRoot 'VC\Tools\MSVC'
    $msvcVer = Get-ChildItem -LiteralPath $msvcRoot -Directory |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if (-not $msvcVer) {
        throw "No MSVC toolchain found under '$msvcRoot'."
    }

    $arch = Get-ExeArchitecture -Path $script:ExeFull
    $hostDir = [Environment]::Is64BitOperatingSystem ? 'Hostx64' : 'Hostx86'
    $targetDir = switch ($arch) {
        'x86'   { 'x86' }
        'x64'   { 'x64' }
        'arm64' { 'arm64' }
        default { 'x64' }
    }

    $candidate = @(
        (Join-Path $msvcVer.FullName "bin\$hostDir\$targetDir\pgomgr.exe"),
        (Join-Path $msvcVer.FullName "bin\$hostDir\x64\pgomgr.exe")
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

    if (-not $candidate) {
        throw "Could not locate pgomgr.exe under '$($msvcVer.FullName)'. Pass -PgoMgrPath explicitly."
    }

    $script:PgoMgrFull = $candidate
}

function Get-ExeArchitecture {
    param([Parameter(Mandatory)] [string] $Path)

    # Read PE header to determine machine type
    $bytes = [byte[]]::new(0x1000)
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $read = $stream.Read($bytes, 0, $bytes.Length)
        if ($read -lt 0x40) { return 'unknown' }
        $peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
        if ($peOffset -lt 0 -or $peOffset -gt ($read - 6)) { return 'unknown' }
        if ($bytes[$peOffset] -ne 0x50 -or $bytes[$peOffset + 1] -ne 0x45) { return 'unknown' }
        $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
    } finally {
        $stream.Dispose()
    }
    switch ($machine) {
        0x014C  { return 'x86' }
        0x8664  { return 'x64' }
        0xAA64  { return 'arm64' }
        default { return 'unknown' }
    }
}

# --- INI authoring ---------------------------------------------------------

# Sections mirror the constants in Options.cpp.
$SectionGeneral     = 'Options'
$SectionTreeMap     = 'TreeMapView'
$SectionFileTree    = 'FileTreeView'
$SectionDupeTree    = 'DupeView'
$SectionExtView     = 'ExtView'
$SectionTopView     = 'TopView'
$SectionSearch      = 'SearchView'
$SectionWatcher     = 'Watcher'
$SectionDriveSelect = 'DriveSelect'

function Convert-BoolToIni {
    param([Parameter(Mandatory)] [bool] $Value)
    if ($Value) { '1' } else { '0' }
}

function Format-MultiPattern {
    # Joins multiple regex / glob patterns with the record separator MFC uses.
    param([Parameter(Mandatory)] [AllowEmptyCollection()] [string[]] $Patterns)
    return ($Patterns -join $RS)
}

function New-BaselineSettings {
    # Quiet, deterministic baseline that every scenario inherits.
    # Disables every UI prompt and every settings persistence side effect that
    # could deviate between runs.
    [ordered]@{
        $SectionGeneral = [ordered]@{
            # No prompts, no elevation pop-ups
            ShowElevationPrompt        = '0'
            AutoElevate                = '0'
            ShowDeleteWarning          = '0'
            ShowDupeDetectionCloudLinksWarning = '0'

            # Predictable scanning behavior
            UseFastScanEngine          = '1'
            UseBackupRestore           = '1'
            ProcessHardlinks           = '1'
            ScanningThreads            = '4'

            # Filtering off by default (scenarios opt in)
            FilteringUseRegex          = '0'
            FilteringSizeMinimum       = '0'
            FilteringSizeUnits         = '0'
            FilteringMaxAgeDays        = '0'

            # Reparse / hidden inclusion off by default
            ExcludeJunctions           = '1'
            ExcludeSymbolicLinksDirectory = '1'
            ExcludeVolumeMountPoints   = '1'
            ExcludeHiddenDirectory     = '0'
            ExcludeProtectedDirectory  = '0'
            ExcludeSymbolicLinksFile   = '1'
            ExcludeHiddenFile          = '0'
            ExcludeProtectedFile       = '0'
            FollowVolumeMountPoints    = '0'

            # Deterministic columns / rendering surface
            ListGrid                   = '0'
            ListStripes                = '0'
            ListFullRowSelection       = '1'
            ShowToolBar                = '1'
            ShowStatusBar              = '1'
            ShowFreeSpace              = '0'
            ShowUnknown                = '0'
            ShowFileTypes              = '1'
            UseSizeSuffixes            = '1'
            UseDrawTextCache           = '1'
            UseWindowsLocaleSetting    = '1'
            PacmanAnimation            = '1'

            # Hash algorithm: HASH_SHA512 default (3 in the enum range MD5..SHA512)
            FileHashAlgorithm          = '3'

            LanguageId                 = '0'
            LargeFileCount             = '50'
            MinimizeViewThreshold      = '10'
        }

        $SectionDupeTree = [ordered]@{
            ScanForDuplicates          = '0'
        }

        $SectionFileTree = [ordered]@{
            ShowColumnAttributes       = '1'
            ShowColumnFiles            = '1'
            ShowColumnFolders          = '1'
            ShowColumnItems            = '1'
            ShowColumnLastChange       = '1'
            ShowColumnOwner            = '0'
            ShowColumnSizeLogical      = '1'
            ShowColumnSizePhysical     = '1'
            ShowTimeSpent              = '1'
        }

        $SectionTreeMap = [ordered]@{
            ShowTreeMap                = '1'
            TreeMapStyle               = '0'
            TreeMapGrid                = '0'
            TreeMapShowExtensions      = '1'
            TreeMapUseLogicalSize      = '0'
        }

        $SectionDriveSelect = [ordered]@{
            FilteringExcludeDirs       = ''
            FilteringExcludeFiles      = ''
            FilteringIncludeDirs       = ''
            FilteringIncludeFiles      = ''
            SelectDrivesRadio          = '0'
        }

        $SectionSearch = [ordered]@{
            SearchWholePhrase          = '0'
            SearchCase                 = '0'
            SearchRegex                = '0'
            SearchMaxResults           = '10000'
        }
    }
}

function Merge-Settings {
    # Deep-merge $Override into $Base (sections + keys). Returns a new
    # ordered map so the caller can keep mutating the base safely.
    param(
        [Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Base,
        [Parameter(Mandatory)] [hashtable] $Override
    )

    $result = [ordered]@{}
    foreach ($section in $Base.Keys) {
        $result[$section] = [ordered]@{}
        foreach ($k in $Base[$section].Keys) {
            $result[$section][$k] = $Base[$section][$k]
        }
    }

    foreach ($section in $Override.Keys) {
        if (!$result.Contains($section)) { $result[$section] = [ordered]@{} }
        foreach ($k in $Override[$section].Keys) {
            $result[$section][$k] = [string] $Override[$section][$k]
        }
    }

    return $result
}

function Write-IniFile {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Sections
    )

    $sb = [System.Text.StringBuilder]::new()
    foreach ($section in $Sections.Keys) {
        [void] $sb.Append('[').Append($section).Append("]`r`n")
        foreach ($k in $Sections[$section].Keys) {
            [void] $sb.Append($k).Append('=').Append($Sections[$section][$k]).Append("`r`n")
        }
        [void] $sb.Append("`r`n")
    }

    # UTF-16 LE with BOM — what WritePrivateProfileStringW reads natively.
    $utf16 = [System.Text.Encoding]::Unicode
    [System.IO.File]::WriteAllText($Path, $sb.ToString(), $utf16)
}

# --- Process orchestration -------------------------------------------------

function Invoke-WinDirStatScenario {
    param(
        [Parameter(Mandatory)] [string] $Name,
        [Parameter(Mandatory)] [string[]] $Arguments,
        [int] $TimeoutSec = $PerScenarioTimeoutSec,
        [switch] $KillAfterSettle,
        [int] $SettleSec = $LoadSettleSeconds
    )

    Write-Step "Launching: `"$script:ExeFull`" $($Arguments -join ' ')"

    # ArgumentList (PS 7+/.NET Core) avoids hand-rolling CRT quoting rules.
    $psi = [System.Diagnostics.ProcessStartInfo]@{
        FileName         = $script:ExeFull
        WorkingDirectory = $script:ExeDir
        UseShellExecute  = $false
        CreateNoWindow   = $true
    }
    foreach ($a in $Arguments) { $psi.ArgumentList.Add($a) }

    $proc = [System.Diagnostics.Process]::Start($psi)

    if ($KillAfterSettle) {
        # /loadfrom and other UI-only paths do not auto-exit. Give the
        # binary time to finish loading and exercise its UI init paths,
        # then end it cleanly. CloseMainWindow is friendlier than Kill
        # (we've already disabled state saving so it has nothing to flush).
        Start-Sleep -Seconds $SettleSec
        if (-not $proc.HasExited) {
            try {
                $closed = $proc.CloseMainWindow() -and $proc.WaitForExit(5000)
                if (-not $closed) { $proc.Kill($true) }
            } catch {
                try { $proc.Kill($true) } catch { }
            }
        }
    } elseif (-not $proc.WaitForExit($TimeoutSec * 1000)) {
        Write-Warn "Scenario '$Name' exceeded ${TimeoutSec}s; force-killing."
        try { $proc.Kill($true) } catch { }
        return -1
    }

    return $proc.ExitCode
}

function Get-PgcSnapshot {
    # Snapshot existing .pgc names so the per-scenario new-file diff only
    # reports files produced by *this* run.
    Get-ChildItem -LiteralPath $script:ExeDir -Filter ($script:ExeBase + '!*.pgc') -ErrorAction SilentlyContinue
}

function Get-NewPgcFiles {
    param([System.IO.FileInfo[]] $Before)

    $beforeSet = @{}
    foreach ($f in @($Before)) { $beforeSet[$f.FullName] = $true }

    Get-ChildItem -LiteralPath $script:ExeDir -Filter ($script:ExeBase + '!*.pgc') -ErrorAction SilentlyContinue |
        Where-Object { -not $beforeSet.ContainsKey($_.FullName) }
}

function Invoke-PgoMerge {
    param(
        [string] $Label
    )

    if ($NoMerge) { return }
    if (!(Test-Path -LiteralPath $script:PgdPath)) {
        Write-Warn "[$Label] No seed .pgd at '$script:PgdPath'; skipping merge."
        return
    }

    $pgcFiles = Get-ChildItem -LiteralPath $script:ExeDir -Filter ($script:ExeBase + '!*.pgc') -ErrorAction SilentlyContinue
    if (!$pgcFiles -or $pgcFiles.Count -eq 0) {
        Write-Warn "[$Label] No .pgc files were produced."
        return
    }

    Write-Step "[$Label] Merging $($pgcFiles.Count) .pgc file(s) into $($script:ExeBase).pgd"

    # pgomgr signature:  pgomgr /merge[:n] [<pgc-prefix>] <pgd-file>
    # With no prefix it derives one from the .pgd basename, which is what
    # we want — the .pgc files are <basename>!N.pgc.
    $mergeArgs = @('/merge', $script:PgdPath)

    Push-Location $script:ExeDir
    try {
        $output = & $script:PgoMgrFull @mergeArgs 2>&1
        $exit = $LASTEXITCODE
    } finally {
        Pop-Location
    }

    if ($exit -ne 0) {
        Write-ErrorLine "[$Label] pgomgr exit $exit"
        $output | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkYellow }
        throw "pgomgr /merge failed for scenario '$Label'."
    }

    # Stamp + remove merged .pgc so the next scenario starts fresh.
    foreach ($f in $pgcFiles) {
        $stamped = Join-Path $script:ArtifactDir ("$Label.{0}" -f $f.Name)
        try { Move-Item -LiteralPath $f.FullName -Destination $stamped -Force } catch {
            Remove-Item -LiteralPath $f.FullName -Force -ErrorAction SilentlyContinue
        }
    }
    Write-Ok "[$Label] Merge complete."
}

# --- Scenario catalog ------------------------------------------------------

function New-ScenarioList {
    $scanRootArg = $ScanRoot.TrimEnd('\') + '\'
    $dupeRootArg = $DupeRoot.TrimEnd('\') + '\'

    # Restrictive include/exclude scenario:
    #   * Anchor scan inside System32 (deeply nested, file-rich).
    #   * Include all *.dll and *.exe under that anchor.
    #   * Exclude a few well-known noisy subfolders.
    $restrictiveAnchor = $RestrictiveIncludeRoot.TrimEnd('\')
    $includeDirs = Format-MultiPattern @(
        ($restrictiveAnchor.Replace('\','\\') + '.*')
    )
    $includeFiles = Format-MultiPattern @('.*\.dll$', '.*\.exe$', '.*\.sys$')
    $excludeDirs = Format-MultiPattern @(
        '.*\\DriverStore\\.*',
        '.*\\WinSxS\\Backup\\.*'
    )
    $excludeFiles = Format-MultiPattern @('.*\.tmp$', '.*\.log$')

    $artifactCsv  = Join-Path $script:ArtifactDir 'scan-fast.csv'
    $artifactCsv2 = Join-Path $script:ArtifactDir 'scan-legacy.csv'
    $artifactJson = Join-Path $script:ArtifactDir 'scan-fast.json'
    $dupeJson     = Join-Path $script:ArtifactDir 'dupes-windows.json'
    $dupeCsv      = Join-Path $script:ArtifactDir 'dupes-windows.csv'

    @(
        # 1. Headline scan: accelerated NTFS engine, save to CSV.
        @{
            Name = 'ScanFastEngine'
            Description = "Accelerated scan of $ScanRoot (UseFastScanEngine=1) → CSV"
            Settings = @{
                $SectionGeneral = @{ UseFastScanEngine = '1'; ScanningThreads = '4' }
            }
            Arguments = @($scanRootArg, '/saveto', $artifactCsv)
        }

        # 2. Same scan with the legacy per-file walker (covers the alternate
        # FinderBasic code path).
        @{
            Name = 'ScanLegacyEngine'
            Description = "Legacy walker scan of $ScanRoot (UseFastScanEngine=0) → CSV"
            Settings = @{
                $SectionGeneral = @{ UseFastScanEngine = '0'; ScanningThreads = '4' }
            }
            Arguments = @($scanRootArg, '/saveto', $artifactCsv2)
        }

        # 3. Save to JSON to exercise SaveResultsJson and the wide-string
        # encoder.
        @{
            Name = 'ScanFastEngineJson'
            Description = "Accelerated scan of $ScanRoot → JSON (covers JSON writer)"
            Settings = @{
                $SectionGeneral = @{ UseFastScanEngine = '1' }
            }
            Arguments = @($scanRootArg, '/saveto', $artifactJson)
        }

        # 4. Restrictive includes + minor excludes (regex). Exercises the
        # filter compile, anchor extraction, and the include-fast-path.
        @{
            Name = 'ScanRestrictiveIncludes'
            Description = "Restrictive include/exclude scan rooted at $RestrictiveIncludeRoot"
            Settings = @{
                $SectionGeneral = @{
                    UseFastScanEngine = '1'
                    FilteringUseRegex = '1'
                }
                $SectionDriveSelect = @{
                    FilteringIncludeDirs  = $includeDirs
                    FilteringIncludeFiles = $includeFiles
                    FilteringExcludeDirs  = $excludeDirs
                    FilteringExcludeFiles = $excludeFiles
                }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-restrictive.csv'))
        }

        # 5. Glob filtering (FilteringUseRegex=0). Covers the second compile path.
        @{
            Name = 'ScanGlobFilters'
            Description = "Glob (non-regex) filtering, *.dll/*.exe under $RestrictiveIncludeRoot"
            Settings = @{
                $SectionGeneral = @{
                    UseFastScanEngine = '1'
                    FilteringUseRegex = '0'
                }
                $SectionDriveSelect = @{
                    FilteringIncludeDirs  = Format-MultiPattern @($restrictiveAnchor + '\*')
                    FilteringIncludeFiles = Format-MultiPattern @('*.dll', '*.exe')
                    FilteringExcludeFiles = Format-MultiPattern @('*.tmp', '*.log')
                }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-glob.csv'))
        }

        # 6. Size filter (only files ≥ 1 MB). Touches the size-pruning branch.
        @{
            Name = 'ScanSizeFiltered'
            Description = "Scan $ScanRoot with size filter ≥ 1 MiB"
            Settings = @{
                $SectionGeneral = @{
                    FilteringSizeMinimum = '1'
                    FilteringSizeUnits   = '2'   # MiB (0=B, 1=KiB, 2=MiB, 3=GiB)
                }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-size.csv'))
        }

        # 7. Age filter (recent files only). Touches the FILETIME cutoff branch.
        @{
            Name = 'ScanAgeFiltered'
            Description = "Scan $ScanRoot with max-age 365 days"
            Settings = @{
                $SectionGeneral = @{ FilteringMaxAgeDays = '365' }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-age.csv'))
        }

        # 8. Hidden / protected / unknown inclusion. Maximises file count.
        @{
            Name = 'ScanIncludeHiddenAndUnknown'
            Description = "Scan including hidden/protected/unknown items"
            Settings = @{
                $SectionGeneral = @{
                    ExcludeHiddenDirectory    = '0'
                    ExcludeHiddenFile         = '0'
                    ExcludeProtectedDirectory = '0'
                    ExcludeProtectedFile      = '0'
                    ShowUnknown               = '1'
                    ShowFreeSpace             = '1'
                }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-hidden.csv'))
        }

        # 9. Single-threaded scan (covers the serial path / contention-free
        # branches).
        @{
            Name = 'ScanSingleThreaded'
            Description = "Accelerated scan with ScanningThreads=1"
            Settings = @{
                $SectionGeneral = @{ ScanningThreads = '1' }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-1t.csv'))
        }

        # 10. Maxed-out worker pool (exercises the BlockingQueue contention
        # paths).
        @{
            Name = 'ScanManyThreaded'
            Description = "Accelerated scan with ScanningThreads=8"
            Settings = @{
                $SectionGeneral = @{ ScanningThreads = '8' }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-8t.csv'))
        }

        # 11. Hardlink processing disabled (alternate dedupe path during scan).
        @{
            Name = 'ScanNoHardlinks'
            Description = "Scan with ProcessHardlinks=0 (skip hardlink dedupe)"
            Settings = @{
                $SectionGeneral = @{ ProcessHardlinks = '0' }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-nohl.csv'))
        }

        # 12. Backup/restore privileges off (slower file open path).
        @{
            Name = 'ScanNoBackupRestore'
            Description = "Scan without UseBackupRestore privilege"
            Settings = @{
                $SectionGeneral = @{ UseBackupRestore = '0' }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-nobr.csv'))
        }

        # 13. Reparse-following enabled (mount points / junctions). May add
        # extra subtrees on systems with mounted volumes.
        @{
            Name = 'ScanFollowReparse'
            Description = "Scan with mount-point/junction following enabled"
            Settings = @{
                $SectionGeneral = @{
                    ExcludeJunctions              = '0'
                    ExcludeSymbolicLinksDirectory = '0'
                    ExcludeVolumeMountPoints      = '0'
                    FollowVolumeMountPoints       = '1'
                }
            }
            Arguments = @($scanRootArg, '/saveto', (Join-Path $script:ArtifactDir 'scan-reparse.csv'))
        }

        # 14. Duplicate scan of C:\Windows → JSON (heavy hashing + dupe-tree
        # construction).
        @{
            Name = 'DupeScanWindowsJson'
            Description = "Duplicate scan of $DupeRoot (SHA-512) → JSON"
            Settings = @{
                $SectionGeneral = @{ FileHashAlgorithm = '3' }
                $SectionDupeTree = @{ ScanForDuplicates = '1' }
            }
            Arguments = @($dupeRootArg, '/savedupesto', $dupeJson)
        }

        # 15. Same dupe scan with the cheaper MD5 hash (different code path
        # in the hashing pipeline).
        @{
            Name = 'DupeScanWindowsMd5Csv'
            Description = "Duplicate scan of $DupeRoot (MD5) → CSV"
            Settings = @{
                $SectionGeneral = @{ FileHashAlgorithm = '0' }
                $SectionDupeTree = @{ ScanForDuplicates = '1' }
            }
            Arguments = @($dupeRootArg, '/savedupesto', $dupeCsv)
        }

        # 16. Reload the CSV produced earlier. /loadfrom does not auto-exit,
        # so the launcher kills the process after a short settle.
        @{
            Name = 'LoadFromCsv'
            Description = "Load CSV produced by ScanFastEngine (UI load path, killed after $LoadSettleSeconds s)"
            Settings = @{}
            Arguments = @('/loadfrom', $artifactCsv)
            KillAfterSettle = $true
            RequiresArtifact = $artifactCsv
        }

        # 17. Reload the JSON produced earlier (different parser path).
        @{
            Name = 'LoadFromJson'
            Description = "Load JSON produced by ScanFastEngineJson (UI load path, killed after $LoadSettleSeconds s)"
            Settings = @{}
            Arguments = @('/loadfrom', $artifactJson)
            KillAfterSettle = $true
            RequiresArtifact = $artifactJson
        }

        # 18. Reload the dupe JSON. Touches the dupe-tree deserializer.
        @{
            Name = 'LoadDupeJson'
            Description = "Load dupe JSON produced by DupeScanWindowsJson (killed after $LoadSettleSeconds s)"
            Settings = @{}
            Arguments = @('/loadfrom', $dupeJson)
            KillAfterSettle = $true
            RequiresArtifact = $dupeJson
        }
    )
}

# --- Main ------------------------------------------------------------------

function Invoke-Scenario {
    param([Parameter(Mandatory)] [hashtable] $Scenario)

    $name = $Scenario.Name
    $desc = $Scenario.Description

    Write-Section "Scenario: $name"
    Write-Host "  $desc" -ForegroundColor DarkGray

    if ($Scenario.ContainsKey('RequiresArtifact') -and -not (Test-Path -LiteralPath $Scenario.RequiresArtifact)) {
        Write-Warn "  Skipping: required artifact '$($Scenario.RequiresArtifact)' is missing (likely a prior scenario was skipped)."
        return
    }

    # Compose settings: baseline + scenario overrides.
    $base = New-BaselineSettings
    $merged = ($Scenario.Settings -and $Scenario.Settings.Count -gt 0) `
        ? (Merge-Settings -Base $base -Override $Scenario.Settings) `
        : $base
    Write-IniFile -Path $script:IniPath -Sections $merged

    $beforePgc = Get-PgcSnapshot
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    $kill = [bool] ($Scenario['KillAfterSettle'] ?? $false)

    $exit = $kill `
        ? (Invoke-WinDirStatScenario -Name $name -Arguments $Scenario.Arguments -KillAfterSettle) `
        : (Invoke-WinDirStatScenario -Name $name -Arguments $Scenario.Arguments)

    $sw.Stop()
    $newPgc = Get-NewPgcFiles -Before $beforePgc

    $pgcCount = @($newPgc).Count
    $status = $kill ? 'killed after settle' : "exit $exit"
    $msg = "  Done in {0:n1}s ({1}); produced {2} new .pgc file(s)." -f $sw.Elapsed.TotalSeconds, $status, $pgcCount
    if ($kill -or $exit -eq 0) { Write-Ok $msg } else { Write-Warn $msg }

    Invoke-PgoMerge -Label $name

    # Optional artifact cleanup.
    if (-not $KeepArtifacts -and -not $Scenario.ContainsKey('RequiresArtifact')) {
        # Only clean files that other scenarios don't read back.
        for ($i = 0; $i -lt $Scenario.Arguments.Count - 1; $i++) {
            if ($Scenario.Arguments[$i] -in @('/saveto', '/savedupesto')) {
                $artifact = $Scenario.Arguments[$i + 1]
                $needed = $false
                foreach ($s in $script:Scenarios) {
                    if ($s.ContainsKey('RequiresArtifact') -and $s.RequiresArtifact -eq $artifact) {
                        $needed = $true; break
                    }
                }
                if (-not $needed -and (Test-Path -LiteralPath $artifact)) {
                    Remove-Item -LiteralPath $artifact -Force -ErrorAction SilentlyContinue
                }
            }
        }
    }
}

function Select-Scenarios {
    param([Parameter(Mandatory)] [object[]] $All)

    $only = [System.Collections.Generic.HashSet[string]]::new(
        [string[]] @($OnlyScenarios), [System.StringComparer]::OrdinalIgnoreCase)
    $skip = [System.Collections.Generic.HashSet[string]]::new(
        [string[]] @($SkipScenarios), [System.StringComparer]::OrdinalIgnoreCase)

    return ,@($All | Where-Object {
        ($only.Count -eq 0 -or $only.Contains($_.Name)) -and -not $skip.Contains($_.Name)
    })
}

# --- Drive ----------------------------------------------------------------

Resolve-ExeContext

$script:Scenarios = New-ScenarioList

if ($List) {
    Write-Section 'PGO Training Scenarios'
    $i = 0
    foreach ($s in $script:Scenarios) {
        $i++
        Write-Host ("  {0,2}. {1,-30}  {2}" -f $i, $s.Name, $s.Description)
    }
    return
}

Resolve-PgoMgr

Write-Section 'PGO Training Run'
Write-Host "  Executable : $script:ExeFull" -ForegroundColor DarkGray
Write-Host "  Work dir   : $script:WorkDirFull" -ForegroundColor DarkGray
$pgdState = (Test-Path -LiteralPath $script:PgdPath) ? 'present' : 'MISSING'
Write-Host "  Seed PGD   : $script:PgdPath  [$pgdState]" -ForegroundColor DarkGray
Write-Host "  pgomgr     : $script:PgoMgrFull" -ForegroundColor DarkGray
Write-Host "  INI (auto) : $script:IniPath" -ForegroundColor DarkGray
Write-Host "  Scan root  : $ScanRoot" -ForegroundColor DarkGray
Write-Host "  Dupe root  : $DupeRoot" -ForegroundColor DarkGray
Write-Host "  Restrictive include root : $RestrictiveIncludeRoot" -ForegroundColor DarkGray

$selected = Select-Scenarios -All $script:Scenarios
if ($selected.Count -eq 0) {
    Write-Warn 'No scenarios selected after filters; nothing to do.'
    return
}

$wallStart = [DateTime]::UtcNow
foreach ($s in $selected) {
    try {
        Invoke-Scenario -Scenario $s
    } catch {
        Write-ErrorLine "Scenario '$($s.Name)' failed: $_"
        # Best-effort cleanup of any leftover process before next scenario.
        Get-Process -Name ($script:ExeBase) -ErrorAction SilentlyContinue |
            ForEach-Object { try { $_.Kill($true) } catch {} }
    }
}

Write-Section 'Done'
$elapsed = ([DateTime]::UtcNow - $wallStart)
Write-Ok ("Total wall time: {0:n1}m" -f $elapsed.TotalMinutes)

if (-not $NoMerge -and (Test-Path -LiteralPath $script:PgdPath)) {
    Write-Host "  Merged profile: $script:PgdPath" -ForegroundColor DarkGray
    Write-Host "  Re-link with /LTCG:PGOPTIMIZE (or VS Profile=true) to consume it." -ForegroundColor DarkGray
}

# Remove the portable-mode marker so subsequent (interactive) launches of
# the same .exe do not silently switch to portable mode.
if (Test-Path -LiteralPath $script:IniPath) {
    Remove-Item -LiteralPath $script:IniPath -Force -ErrorAction SilentlyContinue
}
