#Requires -Version 7.6
<#
.SYNOPSIS
    WinDirStat behavioral integration and desktop regression suite.
.DESCRIPTION
    Runs isolated scenarios against the shipping executable. Shared harnesses own
    fixtures, processes, UI synchronization, filesystem observations, and reports.
    Standard runs broad local coverage. Extended adds prolonged stress/resource
    checks; All also selects hardware and privileged storage scenarios. Explicit
    -Only, -Scenario, or -Tag selection includes matching scenarios from any profile.
    Use -List to inspect scenarios without launching or building the application.
    The default executable is the newest local x64 build or published executable;
    -ExePath selects a specific build. Reports identify its version and SHA-256.

    Every scenario produces PASS, FAIL, or SKIP with elapsed time and check count.
    Failed scenarios retain settings, commands, fixtures, and available screenshots.
    -KeepArtifacts also retains successful and skipped scenarios. JSON and JUnit
    reports are written to -OutputPath, or the run's temporary reports directory.

    UI scenarios require an unlocked interactive desktop and matching executable /
    PowerShell bitness. Test windows stay above other apps and use keyboard input;
    leave the desktop idle during these scenarios. Run through Run-Tests.cmd or
    pwsh -STA. Storage tests use
    only test-created VHD files and temporary shares; MTP requires -MtpDevice with
    the exact device label in Select Target. No application build is performed.

    Exit codes: 0 = passed (possibly with skips), 1 = failed, 2 = invalid selection /
    prerequisites or no passing scenarios, 3 = skipped scenarios with -RequireAll.
.EXAMPLE
    .\Run-Tests.cmd -Tag Race -Repeat 5 -Shuffle -Seed 42
.EXAMPLE
    .\Run-Tests.cmd -Scenario render.* -KeepArtifacts
#>
param(
    [string] $ExePath,
    [ValidateSet('Standard', 'Extended', 'All')] [string] $Profile = 'Standard',
    [string] $Only, [string] $Skip, [string] $Scenario, [string] $Tag, [string] $ExcludeTag,
    [ValidateRange(1, 100)] [int] $Repeat = 1,
    [int] $Seed = 1729, [switch] $Shuffle,
    [ValidateRange(5, 1800)] [int] $TimeoutSeconds = 120,
    [ValidateRange(1000, 100000)] [int] $StressFileCount = 3000,
    [string] $MtpDevice,
    [string] $OutputPath,
    [switch] $KeepArtifacts, [switch] $Details, [switch] $RequireAll, [switch] $List
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$script:ExtendedStressFileCount = if ($PSBoundParameters.ContainsKey('StressFileCount')) { $StressFileCount } else { 12000 }
$RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ResourceHeaderPath = Join-Path $RepoRoot 'windirstat\resource.h'
. (Join-Path $PSScriptRoot 'ScriptSupport.ps1')
. (Join-Path $PSScriptRoot 'Harness\Core.ps1')
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'Scenarios') -Filter '*.ps1' | Sort-Object Name) {
    . $file.FullName
}

function Split-Selection {
    param([string] $Value)
    return ,@($Value -split '[,;\s]+' | Where-Object { $_ })
}

$onlyNames = Split-Selection $Only
$skipNames = Split-Selection $Skip
$scenarioPatterns = Split-Selection $Scenario
$tags = Split-Selection $Tag
$excludedTags = Split-Selection $ExcludeTag
$suites = @($script:Scenarios.Suite | Sort-Object -Unique)
$knownTags = @($script:Scenarios.Tags | Sort-Object -Unique)
$unknown = @($onlyNames + $skipNames | Where-Object { $_ -notin $suites })
$unknownTags = @($tags + $excludedTags | Where-Object { $_ -notin $knownTags })
if ($unknown.Count -or $unknownTags.Count) {
    Write-Host "Unknown suites: $($unknown -join ', '); unknown tags: $($unknownTags -join ', ')" -ForegroundColor Red
    Write-Host "Suites: $($suites -join ', '); tags: $($knownTags -join ', ')"
    exit 2
}
foreach ($pattern in $scenarioPatterns) {
    if (-not @($script:Scenarios | Where-Object Id -Like $pattern).Count) {
        Write-Host "Scenario pattern matches nothing: $pattern" -ForegroundColor Red
        exit 2
    }
}
$selected = @($script:Scenarios | Where-Object {
    $entry = $_
    $profileMatches = $Profile -eq 'All' -or
        ('Hardware' -notin $entry.Tags -and ($Profile -eq 'Extended' -or 'Extended' -notin $entry.Tags))
    $explicitSelection = $tags.Count -or $scenarioPatterns.Count -or $entry.Suite -in $onlyNames
    ($profileMatches -or $explicitSelection) -and
        (-not $onlyNames.Count -or $entry.Suite -in $onlyNames) -and $entry.Suite -notin $skipNames -and
        (-not $scenarioPatterns.Count -or @($scenarioPatterns | Where-Object { $entry.Id -like $_ }).Count) -and
        (-not $tags.Count -or @($entry.Tags | Where-Object { $_ -in $tags }).Count) -and
        -not @($entry.Tags | Where-Object { $_ -in $excludedTags }).Count
})
if (-not $selected.Count) { Write-Host 'Selection contains no scenarios.' -ForegroundColor Red; exit 2 }
if ($List) {
    foreach ($entry in $selected) {
        Write-Host "$($entry.Id) [$($entry.Suite)]  $($entry.Name)"
        Write-Host "    Tags: $($entry.Tags -join ', '); requires: $($entry.Requires -join ', ')"
    }
    Write-Host "Selected: $($selected.Count) scenarios"
    exit 0
}
if (-not $IsWindows) { Write-Host 'This suite requires Windows.' -ForegroundColor Red; exit 2 }
if (-not $ExePath) {
    $candidates = @('build\WinDirStat_x64.exe', 'publish\x64\WinDirStat.exe') |
        ForEach-Object { Get-Item -LiteralPath (Join-Path $RepoRoot $_) -ErrorAction SilentlyContinue } |
        Sort-Object LastWriteTimeUtc -Descending
    $ExePath = @($candidates | Select-Object -First 1 -ExpandProperty FullName) -join ''
    if (-not $ExePath) { Write-Host 'Build WinDirStat or supply -ExePath.' -ForegroundColor Red; exit 2 }
}
$ExePath = [IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    Write-Host "Executable not found: $ExePath" -ForegroundColor Red
    exit 2
}
$exeInfo = Get-Item -LiteralPath $ExePath
$newestSource = Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'windirstat') -Recurse -File |
    Where-Object { $_.Extension -in @('.cpp', '.h', '.rc', '.vcxproj', '.props', '.targets', '.txt') } |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
$script:ExecutableInfo = [pscustomobject]@{
    Version = $exeInfo.VersionInfo.FileVersion; BuiltUtc = $exeInfo.LastWriteTimeUtc.ToString('o')
    NewestSource = $newestSource.FullName; NewestSourceUtc = $newestSource.LastWriteTimeUtc.ToString('o')
    MayBeStale = $exeInfo.LastWriteTimeUtc -lt $newestSource.LastWriteTimeUtc
}
$binary = [IO.File]::ReadAllBytes($ExePath)
$peOffset = [BitConverter]::ToInt32($binary, 0x3C)
$script:BinaryIs64Bit = [BitConverter]::ToUInt16($binary, $peOffset + 4) -in @(0x8664, 0xAA64)
$script:ResourceIds = Read-CHeaderNumericDefines $ResourceHeaderPath
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type -Path (Join-Path $PSScriptRoot 'Harness\NativeInterop.cs')
Add-Type -Path (Join-Path $PSScriptRoot 'Harness\Desktop.cs')
. (Join-Path $PSScriptRoot 'Harness\Fixtures.ps1')
. (Join-Path $PSScriptRoot 'Harness\Ui.ps1')

$RunRoot = Join-Path ([IO.Path]::GetTempPath()) ("WinDirStatTests-{0}-{1}" -f $PID, [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($RunRoot) | Out-Null
if (-not $OutputPath) { $OutputPath = Join-Path $RunRoot 'reports' }
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
$script:StartedUtc = [datetime]::UtcNow.ToString('o')
$watch = [Diagnostics.Stopwatch]::StartNew()
$random = [Random]::new($Seed)
Write-Host "WinDirStat behavioral tests: $($selected.Count) scenarios, profile=$Profile, repeat=$Repeat, seed=$Seed"
Write-Host "Executable: $ExePath"
Write-Host "Version: $($script:ExecutableInfo.Version); built=$($script:ExecutableInfo.BuiltUtc)"
if ($script:ExecutableInfo.MayBeStale) {
    Write-Host "Executable may be stale: newer source $($script:ExecutableInfo.NewestSource)" -ForegroundColor Yellow
}
Write-Host "Artifacts: $RunRoot"
for ($iteration = 1; $iteration -le $Repeat; ++$iteration) {
    $order = @($selected)
    if ($Shuffle) {
        for ($index = $order.Count - 1; $index -gt 0; --$index) {
            $other = $random.Next($index + 1)
            $order[$index], $order[$other] = $order[$other], $order[$index]
        }
    }
    foreach ($entry in $order) { Invoke-TestScenario $entry $iteration }
}
$watch.Stop()
exit (Write-TestReports $watch.Elapsed.TotalSeconds)
