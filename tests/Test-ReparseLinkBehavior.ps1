#Requires -Version 7.0
#Requires -RunAsAdministrator
param(
    [string] $DriveOne = 'E:',
    [string] $DriveTwo = 'F:',

    [string] $ExePath = (Join-Path $PSScriptRoot '..\publish\x64\WinDirStat.exe'),

    [int] $TimeoutSeconds = 120,

    [switch] $KeepArtifacts,

    [Alias('ShowPassedDetails')]
    [switch] $Details
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot       = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$workRoot       = Join-Path $repoRoot 'build\reparse-link-test'
$runRoot        = Join-Path $workRoot 'runner'
$driveOneLetter = ($DriveOne -replace ':.*', '').ToUpperInvariant()
$driveTwoLetter = ($DriveTwo -replace ':.*', '').ToUpperInvariant()
$scanRoot       = "${driveOneLetter}:\wds-reparse-test"
$symbolPass     = [string] [char] 0x2713
$symbolFail     = [string] [char] 0x2717
$symbolWarn     = '!'

# --- Display helpers ---

function Get-StatusColor {
    param([Parameter(Mandatory)] [string] $Status)
    switch ($Status) {
        'PASS'  { 'Green' }
        'FAIL'  { 'Red' }
        'WARN'  { 'Yellow' }
        default { 'Gray' }
    }
}

function Write-ColoredLine {
    param(
        [Parameter(Mandatory)] [string] $Message,
        [ConsoleColor] $Color = [ConsoleColor]::Gray
    )
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

function Write-SymbolCell {
    param(
        [Parameter(Mandatory)] [string] $Text,
        [Parameter(Mandatory)] [int] $Width,
        [ConsoleColor] $Color = [ConsoleColor]::Gray
    )
    Write-Host -NoNewline $Text.PadRight($Width) -ForegroundColor $Color
}

# --- INI helpers ---

function ConvertTo-IniText {
    param([Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Sections)

    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($section in $Sections.GetEnumerator()) {
        [void] $lines.Add("[$($section.Key)]")
        foreach ($entry in $section.Value.GetEnumerator()) {
            [void] $lines.Add("$($entry.Key)=$($entry.Value)")
        }
        [void] $lines.Add('')
    }
    return $lines -join "`r`n"
}

function Write-PortableIni {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Sections
    )
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [System.IO.File]::WriteAllText($Path, (ConvertTo-IniText -Sections $Sections), [System.Text.Encoding]::Unicode)
}

function New-BaseIniSections {
    param([switch] $BasicScanEngine)

    [ordered] @{
        Options     = [ordered] @{
            LanguageId                    = 9
            UseFastScanEngine             = if ($BasicScanEngine) { 0 } else { 1 }
            UseBackupRestore              = 0
            ShowElevationPrompt           = 0
            AutoElevate                   = 0
            ShowFreeSpace                 = 0
            ShowUnknown                   = 0
            ProcessHardlinks              = 0
            ExcludeJunctions              = 1
            ExcludeSymbolicLinksDirectory = 1
            ExcludeSymbolicLinksFile      = 1
            ExcludeVolumeMountPoints      = 1
            FollowVolumeMountPoints       = 0
        }
        DriveSelect = [ordered] @{}
        DupeView    = [ordered] @{ ScanForDuplicates = 0 }
    }
}

function Set-IniValue {
    param(
        [Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Sections,
        [Parameter(Mandatory)] [string] $Section,
        [Parameter(Mandatory)] [string] $Name,
        [AllowNull()] [object] $Value
    )
    if (!$Sections.Contains($Section)) {
        $Sections[$Section] = [ordered] @{}
    }
    $Sections[$Section][$Name] = $Value
}

# --- Filesystem helpers ---

function New-TestFile {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [int] $Size,
        [byte] $Seed = 31
    )
    $parent = Split-Path -Parent $Path
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $bytes = [byte[]]::new($Size)
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $bytes[$i] = [byte] ((($i + $Seed) % 251) + 1)
    }
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function Normalize-ComparePath {
    param([Parameter(Mandatory)] [string] $Path)
    return [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
}

# --- Drive setup ---

function Assert-ValidTestDrive {
    param([Parameter(Mandatory)] [string] $Letter)

    if ($Letter -notmatch '^[A-Za-z]$') {
        throw "Invalid drive letter '$Letter'. Expected a single letter (A-Z)."
    }
    if ($Letter.ToUpperInvariant() -eq 'C') {
        throw "Drive C: is protected and cannot be used as a test drive."
    }
    if (!(Test-Path "${Letter}:\")) {
        throw "Drive ${Letter}: is not accessible. Ensure the drive is inserted/mounted before running."
    }
}

function Format-TestDrive {
    param(
        [Parameter(Mandatory)] [string] $Letter,
        [Parameter(Mandatory)] [string] $Label
    )
    Write-ColoredLine "  Formatting ${Letter}: as NTFS (label: $Label) ..." DarkGray
    Format-Volume -DriveLetter $Letter -FileSystem NTFS -NewFileSystemLabel $Label -Force -Confirm:$false |
        Out-Null
    Write-ColoredLine "  Drive ${Letter}: formatted." DarkGray
}

function Get-VolumeGuidPath {
    param([Parameter(Mandatory)] [string] $Letter)

    $output = @(& mountvol "${Letter}:\" /L 2>&1)
    $guid   = ($output | Where-Object { $_ -match '\\\\\?\\Volume' } | Select-Object -First 1)
    if (!$guid) {
        throw "Could not retrieve volume GUID for drive ${Letter}: $($output -join ' ')"
    }
    $guid.Trim()
}

function Mount-VolumeAtPath {
    param(
        [Parameter(Mandatory)] [string] $Directory,
        [Parameter(Mandatory)] [string] $VolumeGuidPath
    )
    New-Item -ItemType Directory -Force -Path $Directory | Out-Null
    $result = @(& mountvol $Directory $VolumeGuidPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "mountvol failed (exit $LASTEXITCODE): $($result -join ' ')"
    }
}

function Dismount-VolumeAtPath {
    param([Parameter(Mandatory)] [string] $Directory)

    if (!(Test-Path -LiteralPath $Directory)) { return }
    $result = @(& mountvol $Directory /D 2>&1)
    if ($LASTEXITCODE -ne 0) {
        Write-ColoredLine "Warning: mountvol /D failed for $Directory : $($result -join ' ')" Yellow
    }
}

# --- WinDirStat runner ---

function Invoke-WinDirStatCsv {
    param(
        [Parameter(Mandatory)] [string] $Exe,
        [Parameter(Mandatory)] [string] $Csv,
        [Parameter(Mandatory)] [string] $Root
    )

    if (Test-Path -LiteralPath $Csv) {
        Remove-Item -LiteralPath $Csv -Force
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName         = $Exe
    $startInfo.WorkingDirectory = Split-Path -Parent $Exe
    $startInfo.UseShellExecute  = $false
    $startInfo.CreateNoWindow   = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError  = $true

    if ([System.Diagnostics.ProcessStartInfo].GetProperty('ArgumentList')) {
        [void] $startInfo.ArgumentList.Add('/saveto')
        [void] $startInfo.ArgumentList.Add($Csv)
        [void] $startInfo.ArgumentList.Add($Root)
    }
    else {
        $startInfo.Arguments = "/saveto `"$Csv`" `"$Root`""
    }

    $sw      = [System.Diagnostics.Stopwatch]::StartNew()
    $process = [System.Diagnostics.Process]::Start($startInfo)
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        try { $process.Kill($true) } catch {}
        throw "WinDirStat did not finish within $TimeoutSeconds seconds."
    }
    $stderr = $process.StandardError.ReadToEnd()
    $sw.Stop()

    if ($process.ExitCode -ne 0) {
        throw "WinDirStat exited with code $($process.ExitCode). Stderr: $stderr"
    }
    if (!(Test-Path -LiteralPath $Csv)) {
        throw "WinDirStat exited successfully but did not create CSV: $Csv"
    }

    [pscustomobject] @{
        CommandLine    = "`"$Exe`" /saveto `"$Csv`" `"$Root`""
        ExitCode       = $process.ExitCode
        ElapsedSeconds = [math]::Round($sw.Elapsed.TotalSeconds, 3)
    }
}

function Read-CsvPaths {
    param([Parameter(Mandatory)] [string] $Csv)

    $rows = @(Import-Csv -LiteralPath $Csv -Encoding UTF8)
    if (!$rows) {
        throw "CSV contained no rows: $Csv"
    }
    if (!($rows[0].PSObject.Properties.Name -contains 'Name')) {
        throw "CSV missing expected 'Name' column. Headers: $($rows[0].PSObject.Properties.Name -join ', ')"
    }
    @($rows | ForEach-Object { Normalize-ComparePath $_.Name } | Sort-Object)
}

# --- Test assertions ---

function New-CheckContext {
    [pscustomobject] @{
        Count    = 0
        Failures = [System.Collections.Generic.List[string]]::new()
        Warnings = [System.Collections.Generic.List[string]]::new()
    }
}

function Add-Failure {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Message
    )
    [void] $Context.Failures.Add($Message)
}

function Add-Warning {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Message
    )
    [void] $Context.Warnings.Add($Message)
}

function Assert-True {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Name,
        [bool] $Condition
    )
    $Context.Count++
    if (!$Condition) {
        Add-Failure -Context $Context -Message "$Name expected true but was false"
    }
}

function Assert-False {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Name,
        [bool] $Condition
    )
    $Context.Count++
    if ($Condition) {
        Add-Failure -Context $Context -Message "$Name expected false but was true"
    }
}

# --- Scenario runner ---

function Invoke-Scenario {
    param(
        [Parameter(Mandatory)] [string] $Name,
        [Parameter(Mandatory)] [string] $Behavior,
        [Parameter(Mandatory)] [scriptblock] $Body
    )

    $context     = New-CheckContext
    $commandLine = ''
    $elapsed     = $null
    $errorText   = $null

    try {
        $output = & $Body $context
        if ($output -and $output.PSObject.Properties.Name -contains 'CommandLine') {
            $commandLine = $output.CommandLine
        }
        if ($output -and $output.PSObject.Properties.Name -contains 'ElapsedSeconds') {
            $elapsed = $output.ElapsedSeconds
        }
    }
    catch {
        $errorText = $_.Exception.Message
        Add-Failure -Context $context -Message $errorText
    }

    $status = if ($context.Failures.Count -gt 0) { 'FAIL' }
              elseif ($context.Warnings.Count -gt 0) { 'WARN' }
              else { 'PASS' }

    [pscustomobject] @{
        Name           = $Name
        Status         = $status
        Behavior       = $Behavior
        Checks         = $context.Count
        Failures       = @($context.Failures)
        Warnings       = @($context.Warnings)
        CommandLine    = $commandLine
        ElapsedSeconds = $elapsed
        Error          = $errorText
    }
}

# --- Results display ---

function Write-SuiteResultsTable {
    param([Parameter(Mandatory)] [pscustomobject[]] $Results)

    $scenarioWidth = [Math]::Max(8, @($Results | ForEach-Object { $_.Name.Length } | Measure-Object -Maximum).Maximum)
    $scenarioWidth = [Math]::Min($scenarioWidth, 66)
    $statusWidth   = 6
    $checksWidth   = 8
    $elapsedWidth  = 8

    Write-ColoredLine 'Scenario results:' DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Status'   $statusWidth   DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Scenario' $scenarioWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Checks'   $checksWidth   DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Elapsed'  $elapsedWidth  DarkCyan
    Write-Host ''
    Write-Host -NoNewline '  '
    Write-ColoredLine ("".PadRight($statusWidth + $scenarioWidth + $checksWidth + $elapsedWidth + 6, '-')) DarkGray

    foreach ($result in $Results) {
        $statusColor = Get-StatusColor $result.Status
        $name        = $result.Name
        if ($name.Length -gt $scenarioWidth) { $name = $name.Substring(0, [Math]::Max(0, $scenarioWidth - 3)) + '...' }
        $elapsedText = if ($null -eq $result.ElapsedSeconds) { '-' } else { "$($result.ElapsedSeconds)s" }

        Write-Host -NoNewline '  '
        Write-SymbolCell $result.Status                  $statusWidth   $statusColor
        Write-Host -NoNewline '  '
        Write-SymbolCell $name                           $scenarioWidth Gray
        Write-Host -NoNewline '  '
        Write-SymbolCell ([string] $result.Checks)       $checksWidth   Gray
        Write-Host -NoNewline '  '
        Write-SymbolCell $elapsedText                    $elapsedWidth  Gray
        Write-Host ''
    }
}

function Write-ScenarioSummary {
    param([Parameter(Mandatory)] [pscustomobject] $Result)

    $statusColor = Get-StatusColor $Result.Status
    $symbol      = switch ($Result.Status) {
        'PASS'  { $symbolPass }
        'WARN'  { $symbolWarn }
        default { $symbolFail }
    }

    Write-Host ''
    Write-ColoredLine "=== $symbol $($Result.Name) ===" Cyan
    Write-LabelValue 'Result'            $Result.Status $statusColor
    Write-LabelValue 'Expected behavior' $Result.Behavior
    Write-LabelValue 'Checks'            $Result.Checks
    if ($Result.CommandLine)              { Write-LabelValue 'Command'         $Result.CommandLine }
    if ($null -ne $Result.ElapsedSeconds) { Write-LabelValue 'Elapsed seconds' $Result.ElapsedSeconds }
    if ($Result.Failures.Count -gt 0) {
        Write-ColoredLine 'Failures:' Red
        foreach ($failure in $Result.Failures) { Write-ColoredLine "  - $failure" Red }
    }
    if ($Result.Warnings.Count -gt 0) {
        Write-ColoredLine 'Warnings:' Yellow
        foreach ($warning in $Result.Warnings) { Write-ColoredLine "  - $warning" Yellow }
    }
}

# --- Fixture setup ---

function Prepare-ScanFixtures {
    # Build the real-dir/real-file baseline on DriveOne.
    New-Item -ItemType Directory -Force -Path $scanRoot | Out-Null

    $realDir   = Join-Path $scanRoot 'real-dir'
    $realChild = Join-Path $realDir  'real-child.dat'
    $realFile  = Join-Path $scanRoot 'real-file.dat'

    New-Item -ItemType Directory -Force -Path $realDir | Out-Null
    New-TestFile -Path $realChild -Size 1024 -Seed 7
    New-TestFile -Path $realFile  -Size 1024 -Seed 11

    $info = [ordered] @{
        RealDir    = $realDir
        RealChild  = $realChild
        RealFile   = $realFile

        Symlinks = [ordered] @{
            Created  = $false
            Error    = $null
            FileLink    = Join-Path $scanRoot 'symlink-file.dat'
            DirLink     = Join-Path $scanRoot 'symlink-dir'
            DirLinkChild = Join-Path $scanRoot 'symlink-dir\real-child.dat'
        }

        Junction = [ordered] @{
            Created       = $false
            Error         = $null
            JunctionDir   = Join-Path $scanRoot 'junction-dir'
            JunctionChild = Join-Path $scanRoot 'junction-dir\real-child.dat'
        }

        MountPoint = [ordered] @{
            Created     = $false
            Error       = $null
            MountDir    = Join-Path $scanRoot 'mountpoint-dir'
            MountedFile = Join-Path $scanRoot 'mountpoint-dir\mounted-file.dat'
        }
    }

    # Symbolic links (requires SeCreateSymbolicLinkPrivilege).
    try {
        New-Item -ItemType SymbolicLink `
            -Path   $info.Symlinks.FileLink `
            -Target $realFile `
            -ErrorAction Stop | Out-Null
        New-Item -ItemType SymbolicLink `
            -Path   $info.Symlinks.DirLink `
            -Target $realDir `
            -ErrorAction Stop | Out-Null
        $info.Symlinks.Created = $true
    }
    catch {
        $info.Symlinks.Created = $false
        $info.Symlinks.Error   = $_.Exception.Message
    }

    # On-volume junction point.
    try {
        New-Item -ItemType Junction `
            -Path   $info.Junction.JunctionDir `
            -Target $realDir `
            -ErrorAction Stop | Out-Null
        $info.Junction.Created = $true
    }
    catch {
        $info.Junction.Created = $false
        $info.Junction.Error   = $_.Exception.Message
    }

    # Off-volume mount point: create files on DriveTwo first, then mount.
    try {
        New-TestFile -Path "${driveTwoLetter}:\mounted-file.dat" -Size 1024 -Seed 17
        $volGuid = Get-VolumeGuidPath -Letter $driveTwoLetter
        Mount-VolumeAtPath -Directory $info.MountPoint.MountDir -VolumeGuidPath $volGuid
        $info.MountPoint.Created = $true
    }
    catch {
        $info.MountPoint.Created = $false
        $info.MountPoint.Error   = $_.Exception.Message
    }

    [pscustomobject] $info
}

# --- Scenario helper: write ini and scan ---

function Invoke-ScanWithIni {
    param(
        [Parameter(Mandatory)] [string] $Exe,
        [Parameter(Mandatory)] [string] $CsvName,
        [Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Sections
    )

    $csv = Join-Path $workRoot "${CsvName}.csv"
    Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $Sections
    $run   = Invoke-WinDirStatCsv -Exe $Exe -Csv $csv -Root $scanRoot
    $paths = Read-CsvPaths -Csv $csv
    [pscustomobject] @{ Run = $run; Paths = $paths }
}

# ============================================================
# Main
# ============================================================

$sourceExe       = [System.IO.Path]::GetFullPath($ExePath)
$suiteSucceeded  = $false
$fixtureInfo     = $null

try {
    # Validate parameters.
    Assert-ValidTestDrive -Letter $driveOneLetter
    Assert-ValidTestDrive -Letter $driveTwoLetter
    if ($driveOneLetter -eq $driveTwoLetter) {
        throw "DriveOne and DriveTwo must be different drives."
    }
    if (-not (Test-Path -LiteralPath $sourceExe)) {
        throw "WinDirStat executable not found: $sourceExe"
    }

    Write-ColoredLine "Reparse/link behavior suite" Cyan
    Write-LabelValue 'DriveOne'  "${driveOneLetter}:"
    Write-LabelValue 'DriveTwo'  "${driveTwoLetter}:"
    Write-LabelValue 'Scan root' $scanRoot
    Write-LabelValue 'Exe'       $sourceExe
    Write-Host ''
    Write-ColoredLine "WARNING: Drives ${driveOneLetter}: and ${driveTwoLetter}: will be FORMATTED. All data will be lost." Yellow
    Write-Host ''

    # Set up runner directory.
    if (Test-Path -LiteralPath $workRoot) {
        Remove-Item -LiteralPath $workRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $runRoot | Out-Null
    $testExe = Join-Path $runRoot 'WinDirStat.exe'
    Copy-Item -LiteralPath $sourceExe -Destination $testExe -Force

    # Format both drives for a clean slate.
    Write-ColoredLine 'Formatting test drives...' Cyan
    Format-TestDrive -Letter $driveOneLetter -Label 'WdsTestOne'
    Format-TestDrive -Letter $driveTwoLetter -Label 'WdsTestTwo'

    # Build the fixture tree.
    Write-ColoredLine 'Preparing scan fixtures...' Cyan
    $fixtureInfo = Prepare-ScanFixtures

    Write-LabelValue 'Symlinks'    $(if ($fixtureInfo.Symlinks.Created) { 'ready' } else { "unavailable: $($fixtureInfo.Symlinks.Error)" }) $(if ($fixtureInfo.Symlinks.Created) { 'DarkGray' } else { 'Yellow' })
    Write-LabelValue 'Junction'    $(if ($fixtureInfo.Junction.Created) { 'ready' } else { "unavailable: $($fixtureInfo.Junction.Error)" }) $(if ($fixtureInfo.Junction.Created) { 'DarkGray' } else { 'Yellow' })
    Write-LabelValue 'MountPoint'  $(if ($fixtureInfo.MountPoint.Created) { 'ready' } else { "unavailable: $($fixtureInfo.MountPoint.Error)" }) $(if ($fixtureInfo.MountPoint.Created) { 'DarkGray' } else { 'Yellow' })
    Write-Host ''

    $results = [System.Collections.Generic.List[pscustomobject]]::new()

    # ----------------------------------------------------------
    # Scenario 1: All excluded — default WinDirStat behavior.
    # Directory link nodes and junction/mount nodes appear in
    # the CSV, but none of their children are enumerated.
    # File symlinks are invisible entirely.
    # ----------------------------------------------------------
    [void] $results.Add((Invoke-Scenario -Name 'AllExcluded_Defaults' -Behavior 'With all reparse-point exclusions enabled (defaults), directory link nodes appear in the CSV but their children are not enumerated, and file symlinks are invisible.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        $scan = Invoke-ScanWithIni -Exe $testExe -CsvName 'all-excluded' -Sections $sections
        $paths = $scan.Paths

        # Real dir/file should always be present.
        Assert-True  $ctx 'Real dir in CSV'        ((Normalize-ComparePath $fixtureInfo.RealDir)   -in $paths)
        Assert-True  $ctx 'Real child in CSV'      ((Normalize-ComparePath $fixtureInfo.RealChild) -in $paths)
        Assert-True  $ctx 'Real file in CSV'       ((Normalize-ComparePath $fixtureInfo.RealFile)  -in $paths)

        if ($fixtureInfo.Symlinks.Created) {
            Assert-False $ctx 'File symlink absent (excluded)'      ((Normalize-ComparePath $fixtureInfo.Symlinks.FileLink)     -in $paths)
            Assert-True  $ctx 'Dir symlink node visible'            ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLink)      -in $paths)
            Assert-False $ctx 'Dir symlink child absent (excluded)' ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLinkChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Symlink fixtures unavailable: $($fixtureInfo.Symlinks.Error)"
        }

        if ($fixtureInfo.Junction.Created) {
            Assert-True  $ctx 'Junction node visible'             ((Normalize-ComparePath $fixtureInfo.Junction.JunctionDir)   -in $paths)
            Assert-False $ctx 'Junction child absent (excluded)'  ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True  $ctx 'Mount point node visible'             ((Normalize-ComparePath $fixtureInfo.MountPoint.MountDir)    -in $paths)
            Assert-False $ctx 'Mount point content absent (excluded)' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    # ----------------------------------------------------------
    # Scenario 2: All following enabled — all link types are
    # traversed and their contents appear in the CSV.
    # ----------------------------------------------------------
    [void] $results.Add((Invoke-Scenario -Name 'AllFollowed_AllEnabled' -Behavior 'With all reparse-point exclusions disabled, every link type is traversed: file symlinks appear, directory symlink children are enumerated, junction children are enumerated, and mount point contents appear.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 0
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile'      0
        Set-IniValue $sections 'Options' 'ExcludeJunctions'              0
        Set-IniValue $sections 'Options' 'ExcludeVolumeMountPoints'      0
        Set-IniValue $sections 'Options' 'FollowVolumeMountPoints'       1
        $scan  = Invoke-ScanWithIni -Exe $testExe -CsvName 'all-followed' -Sections $sections
        $paths = $scan.Paths

        Assert-True $ctx 'Real dir in CSV'   ((Normalize-ComparePath $fixtureInfo.RealDir)   -in $paths)
        Assert-True $ctx 'Real child in CSV' ((Normalize-ComparePath $fixtureInfo.RealChild) -in $paths)
        Assert-True $ctx 'Real file in CSV'  ((Normalize-ComparePath $fixtureInfo.RealFile)  -in $paths)

        if ($fixtureInfo.Symlinks.Created) {
            Assert-True $ctx 'File symlink visible (allowed)'       ((Normalize-ComparePath $fixtureInfo.Symlinks.FileLink)     -in $paths)
            Assert-True $ctx 'Dir symlink node visible'             ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLink)      -in $paths)
            Assert-True $ctx 'Dir symlink child visible (followed)' ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLinkChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Symlink fixtures unavailable: $($fixtureInfo.Symlinks.Error)"
        }

        if ($fixtureInfo.Junction.Created) {
            Assert-True $ctx 'Junction node visible'              ((Normalize-ComparePath $fixtureInfo.Junction.JunctionDir)   -in $paths)
            Assert-True $ctx 'Junction child visible (followed)'  ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True $ctx 'Mount point node visible'              ((Normalize-ComparePath $fixtureInfo.MountPoint.MountDir)    -in $paths)
            Assert-True $ctx 'Mount point content visible (followed)' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    # ----------------------------------------------------------
    # Scenario 3: Only junctions excluded. Symlinks and mount
    # points are followed; junctions are not.
    # ----------------------------------------------------------
    [void] $results.Add((Invoke-Scenario -Name 'JunctionsExcluded_OthersFollowed' -Behavior 'With only ExcludeJunctions enabled, junction children are not enumerated while directory symlinks and mount point contents are traversed normally.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 0
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile'      0
        Set-IniValue $sections 'Options' 'ExcludeJunctions'              1
        Set-IniValue $sections 'Options' 'ExcludeVolumeMountPoints'      0
        Set-IniValue $sections 'Options' 'FollowVolumeMountPoints'       1
        $scan  = Invoke-ScanWithIni -Exe $testExe -CsvName 'junctions-excluded' -Sections $sections
        $paths = $scan.Paths

        Assert-True $ctx 'Real dir in CSV'   ((Normalize-ComparePath $fixtureInfo.RealDir)   -in $paths)
        Assert-True $ctx 'Real child in CSV' ((Normalize-ComparePath $fixtureInfo.RealChild) -in $paths)

        if ($fixtureInfo.Symlinks.Created) {
            Assert-True $ctx 'File symlink visible'              ((Normalize-ComparePath $fixtureInfo.Symlinks.FileLink)     -in $paths)
            Assert-True $ctx 'Dir symlink child visible'         ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLinkChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Symlink fixtures unavailable: $($fixtureInfo.Symlinks.Error)"
        }

        if ($fixtureInfo.Junction.Created) {
            Assert-True  $ctx 'Junction node visible'             ((Normalize-ComparePath $fixtureInfo.Junction.JunctionDir)   -in $paths)
            Assert-False $ctx 'Junction child absent (excluded)'  ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True $ctx 'Mount point content visible' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    # ----------------------------------------------------------
    # Scenario 4: Directory symlinks excluded, file symlinks
    # visible. Junctions and mount points followed.
    # ----------------------------------------------------------
    [void] $results.Add((Invoke-Scenario -Name 'DirSymlinksExcluded_FileSymlinksVisible' -Behavior 'With ExcludeSymbolicLinksDirectory enabled and ExcludeSymbolicLinksFile disabled, directory symlink children are blocked but file symlinks appear in the CSV.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 1
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile'      0
        Set-IniValue $sections 'Options' 'ExcludeJunctions'              0
        Set-IniValue $sections 'Options' 'ExcludeVolumeMountPoints'      0
        Set-IniValue $sections 'Options' 'FollowVolumeMountPoints'       1
        $scan  = Invoke-ScanWithIni -Exe $testExe -CsvName 'dirsymlink-excluded-filesymlink-visible' -Sections $sections
        $paths = $scan.Paths

        if ($fixtureInfo.Symlinks.Created) {
            Assert-True  $ctx 'File symlink visible (allowed)'         ((Normalize-ComparePath $fixtureInfo.Symlinks.FileLink)     -in $paths)
            Assert-True  $ctx 'Dir symlink node visible'               ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLink)      -in $paths)
            Assert-False $ctx 'Dir symlink child absent (dir excluded)' ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLinkChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Symlink fixtures unavailable: $($fixtureInfo.Symlinks.Error)"
        }

        if ($fixtureInfo.Junction.Created) {
            Assert-True $ctx 'Junction child visible (followed)' ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True $ctx 'Mount point content visible' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    # ----------------------------------------------------------
    # Scenario 5: File symlinks excluded, directory symlinks
    # followed. Junctions and mount points followed.
    # ----------------------------------------------------------
    [void] $results.Add((Invoke-Scenario -Name 'FileSymlinksExcluded_DirSymlinksFollowed' -Behavior 'With ExcludeSymbolicLinksFile enabled and ExcludeSymbolicLinksDirectory disabled, file symlinks are invisible but directory symlink children are enumerated.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 0
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile'      1
        Set-IniValue $sections 'Options' 'ExcludeJunctions'              0
        Set-IniValue $sections 'Options' 'ExcludeVolumeMountPoints'      0
        Set-IniValue $sections 'Options' 'FollowVolumeMountPoints'       1
        $scan  = Invoke-ScanWithIni -Exe $testExe -CsvName 'filesymlink-excluded-dirsymlink-followed' -Sections $sections
        $paths = $scan.Paths

        if ($fixtureInfo.Symlinks.Created) {
            Assert-False $ctx 'File symlink absent (excluded)'      ((Normalize-ComparePath $fixtureInfo.Symlinks.FileLink)     -in $paths)
            Assert-True  $ctx 'Dir symlink node visible'            ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLink)      -in $paths)
            Assert-True  $ctx 'Dir symlink child visible (followed)' ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLinkChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Symlink fixtures unavailable: $($fixtureInfo.Symlinks.Error)"
        }

        if ($fixtureInfo.Junction.Created) {
            Assert-True $ctx 'Junction child visible (followed)' ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True $ctx 'Mount point content visible' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    # ----------------------------------------------------------
    # Scenario 6: Only mount points excluded. Symlinks and
    # junctions are fully followed.
    # ----------------------------------------------------------
    [void] $results.Add((Invoke-Scenario -Name 'MountPointsExcluded_OthersFollowed' -Behavior 'With only ExcludeVolumeMountPoints enabled, the off-volume mount point directory is visible but its contents are not enumerated, while symlinks and junctions are traversed normally.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 0
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile'      0
        Set-IniValue $sections 'Options' 'ExcludeJunctions'              0
        Set-IniValue $sections 'Options' 'ExcludeVolumeMountPoints'      1
        Set-IniValue $sections 'Options' 'FollowVolumeMountPoints'       0
        $scan  = Invoke-ScanWithIni -Exe $testExe -CsvName 'mountpoints-excluded' -Sections $sections
        $paths = $scan.Paths

        if ($fixtureInfo.Symlinks.Created) {
            Assert-True $ctx 'File symlink visible'        ((Normalize-ComparePath $fixtureInfo.Symlinks.FileLink)     -in $paths)
            Assert-True $ctx 'Dir symlink child visible'   ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLinkChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Symlink fixtures unavailable: $($fixtureInfo.Symlinks.Error)"
        }

        if ($fixtureInfo.Junction.Created) {
            Assert-True $ctx 'Junction child visible (followed)' ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True  $ctx 'Mount point node visible'              ((Normalize-ComparePath $fixtureInfo.MountPoint.MountDir)    -in $paths)
            Assert-False $ctx 'Mount point content absent (excluded)' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    # ----------------------------------------------------------
    # Scenario 7: All excluded — repeat with the basic (non-NTFS)
    # scan engine to verify the alternative code path also
    # enforces reparse-point exclusions correctly.
    # ----------------------------------------------------------
    [void] $results.Add((Invoke-Scenario -Name 'AllExcluded_BasicScanEngine' -Behavior 'With all exclusions enabled and UseFastScanEngine disabled, the basic NtQueryDirectoryFile code path should enforce the same reparse-point exclusions as the NTFS MFT path.' -Body {
        param($ctx)

        $sections = New-BaseIniSections -BasicScanEngine
        $scan  = Invoke-ScanWithIni -Exe $testExe -CsvName 'all-excluded-basic' -Sections $sections
        $paths = $scan.Paths

        Assert-True $ctx 'Real dir in CSV'   ((Normalize-ComparePath $fixtureInfo.RealDir)   -in $paths)
        Assert-True $ctx 'Real child in CSV' ((Normalize-ComparePath $fixtureInfo.RealChild) -in $paths)
        Assert-True $ctx 'Real file in CSV'  ((Normalize-ComparePath $fixtureInfo.RealFile)  -in $paths)

        if ($fixtureInfo.Symlinks.Created) {
            Assert-False $ctx 'File symlink absent (excluded)'      ((Normalize-ComparePath $fixtureInfo.Symlinks.FileLink)     -in $paths)
            Assert-True  $ctx 'Dir symlink node visible'            ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLink)      -in $paths)
            Assert-False $ctx 'Dir symlink child absent (excluded)' ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLinkChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Symlink fixtures unavailable: $($fixtureInfo.Symlinks.Error)"
        }

        if ($fixtureInfo.Junction.Created) {
            Assert-True  $ctx 'Junction node visible'            ((Normalize-ComparePath $fixtureInfo.Junction.JunctionDir)   -in $paths)
            Assert-False $ctx 'Junction child absent (excluded)' ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True  $ctx 'Mount point node visible'              ((Normalize-ComparePath $fixtureInfo.MountPoint.MountDir)    -in $paths)
            Assert-False $ctx 'Mount point content absent (excluded)' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    # ----------------------------------------------------------
    # Scenario 8: All followed — basic scan engine.
    # ----------------------------------------------------------
    [void] $results.Add((Invoke-Scenario -Name 'AllFollowed_BasicScanEngine' -Behavior 'With all exclusions disabled and UseFastScanEngine disabled, the basic scan engine should enumerate all link types including file symlinks, directory symlink children, junction children, and mount point contents.' -Body {
        param($ctx)

        $sections = New-BaseIniSections -BasicScanEngine
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 0
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile'      0
        Set-IniValue $sections 'Options' 'ExcludeJunctions'              0
        Set-IniValue $sections 'Options' 'ExcludeVolumeMountPoints'      0
        Set-IniValue $sections 'Options' 'FollowVolumeMountPoints'       1
        $scan  = Invoke-ScanWithIni -Exe $testExe -CsvName 'all-followed-basic' -Sections $sections
        $paths = $scan.Paths

        Assert-True $ctx 'Real dir in CSV'   ((Normalize-ComparePath $fixtureInfo.RealDir)   -in $paths)
        Assert-True $ctx 'Real child in CSV' ((Normalize-ComparePath $fixtureInfo.RealChild) -in $paths)
        Assert-True $ctx 'Real file in CSV'  ((Normalize-ComparePath $fixtureInfo.RealFile)  -in $paths)

        if ($fixtureInfo.Symlinks.Created) {
            Assert-True $ctx 'File symlink visible'              ((Normalize-ComparePath $fixtureInfo.Symlinks.FileLink)     -in $paths)
            Assert-True $ctx 'Dir symlink child visible'         ((Normalize-ComparePath $fixtureInfo.Symlinks.DirLinkChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Symlink fixtures unavailable: $($fixtureInfo.Symlinks.Error)"
        }

        if ($fixtureInfo.Junction.Created) {
            Assert-True $ctx 'Junction child visible'  ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True $ctx 'Mount point content visible' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    # ----------------------------------------------------------
    # Scenario 9: Junction and mount point distinguished.
    # Junctions excluded, mount points followed (and vice versa),
    # confirming WinDirStat separates them even though both use
    # IO_REPARSE_TAG_MOUNT_POINT at the filesystem level.
    # ----------------------------------------------------------
    [void] $results.Add((Invoke-Scenario -Name 'JunctionExcluded_MountPointFollowed' -Behavior 'With ExcludeJunctions enabled and ExcludeVolumeMountPoints disabled, on-volume junction children must be absent while off-volume mount point contents are enumerated, demonstrating that the two reparse-point subtypes are correctly distinguished.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 0
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile'      0
        Set-IniValue $sections 'Options' 'ExcludeJunctions'              1
        Set-IniValue $sections 'Options' 'ExcludeVolumeMountPoints'      0
        Set-IniValue $sections 'Options' 'FollowVolumeMountPoints'       1
        $scan  = Invoke-ScanWithIni -Exe $testExe -CsvName 'junction-excl-mount-followed' -Sections $sections
        $paths = $scan.Paths

        if ($fixtureInfo.Junction.Created) {
            Assert-True  $ctx 'Junction node visible'             ((Normalize-ComparePath $fixtureInfo.Junction.JunctionDir)   -in $paths)
            Assert-False $ctx 'Junction child absent (excluded)'  ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True $ctx 'Mount point content visible (followed)' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'MountPointExcluded_JunctionFollowed' -Behavior 'With ExcludeVolumeMountPoints enabled and ExcludeJunctions disabled, off-volume mount point contents must be absent while on-volume junction children are enumerated.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 0
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile'      0
        Set-IniValue $sections 'Options' 'ExcludeJunctions'              0
        Set-IniValue $sections 'Options' 'ExcludeVolumeMountPoints'      1
        Set-IniValue $sections 'Options' 'FollowVolumeMountPoints'       0
        $scan  = Invoke-ScanWithIni -Exe $testExe -CsvName 'mount-excl-junction-followed' -Sections $sections
        $paths = $scan.Paths

        if ($fixtureInfo.Junction.Created) {
            Assert-True $ctx 'Junction child visible (followed)' ((Normalize-ComparePath $fixtureInfo.Junction.JunctionChild) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Junction fixture unavailable: $($fixtureInfo.Junction.Error)"
        }

        if ($fixtureInfo.MountPoint.Created) {
            Assert-True  $ctx 'Mount point node visible'              ((Normalize-ComparePath $fixtureInfo.MountPoint.MountDir)    -in $paths)
            Assert-False $ctx 'Mount point content absent (excluded)' ((Normalize-ComparePath $fixtureInfo.MountPoint.MountedFile) -in $paths)
        }
        else {
            Add-Warning -Context $ctx -Message "Mount point fixture unavailable: $($fixtureInfo.MountPoint.Error)"
        }

        [pscustomobject] @{ CommandLine = $scan.Run.CommandLine; ElapsedSeconds = $scan.Run.ElapsedSeconds }
    }))

    # ----------------------------------------------------------
    # Summary
    # ----------------------------------------------------------
    $failed = @($results | Where-Object { $_.Status -eq 'FAIL' })
    $warned = @($results | Where-Object { $_.Status -eq 'WARN' })

    Write-Host ''
    Write-ColoredLine '=== Suite Summary ===' Cyan
    Write-LabelValue 'Scenarios run' $results.Count
    Write-LabelValue 'Passed'  ($results.Count - $failed.Count - $warned.Count) Green
    Write-LabelValue 'Warned'  $warned.Count $(if ($warned.Count -eq 0) { 'Green' } else { 'Yellow' })
    Write-LabelValue 'Failed'  $failed.Count $(if ($failed.Count -eq 0) { 'Green' } else { 'Red' })
    Write-SuiteResultsTable -Results @($results)

    if ($Details) {
        Write-ColoredLine 'Scenario details:' Cyan
        foreach ($result in $results) { Write-ScenarioSummary -Result $result }
    }
    elseif ($failed.Count -gt 0 -or $warned.Count -gt 0) {
        if ($failed.Count -gt 0) {
            Write-ColoredLine 'Failed scenario details:' Red
            foreach ($result in $failed) { Write-ScenarioSummary -Result $result }
        }
        if ($warned.Count -gt 0) {
            Write-ColoredLine 'Warning scenario details:' Yellow
            foreach ($result in $warned) { Write-ScenarioSummary -Result $result }
        }
    }

    if ($failed.Count -gt 0) {
        throw "Reparse/link suite failed in $($failed.Count) scenario(s): $(@($failed | ForEach-Object { $_.Name }) -join ', ')"
    }

    Write-ColoredLine 'Reparse/link behavior suite passed.' Green
    $suiteSucceeded = $true
}
finally {
    # Dismount the test volume before any cleanup so the directory can be removed.
    if ($fixtureInfo -and $fixtureInfo.MountPoint.Created) {
        Dismount-VolumeAtPath -Directory $fixtureInfo.MountPoint.MountDir
    }

    if (-not $KeepArtifacts) {
        if (Test-Path -LiteralPath $workRoot) {
            Remove-Item -LiteralPath $workRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
        if (Test-Path -LiteralPath $scanRoot) {
            Remove-Item -LiteralPath $scanRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    elseif (Test-Path -LiteralPath $workRoot) {
        Write-ColoredLine "Kept test artifacts in: $workRoot" Yellow
        Write-ColoredLine "Kept scan fixtures in:  $scanRoot" Yellow
    }

    if (-not $suiteSucceeded) {
        Write-ColoredLine 'Reparse/link behavior suite FAILED.' Red
    }
}
