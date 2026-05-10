param(
    [ValidateSet('Release', 'Debug')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string] $Platform = 'x64',

    [string] $ExePath,

    [switch] $SkipBuild,

    [int] $TimeoutSeconds = 120,

    [switch] $KeepArtifacts,

    [switch] $ShowBuildOutput,

    [Alias('ShowPassedDetails')]
    [switch] $Details
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')
$repoRoot = $repoRoot.ProviderPath
$buildRoot = Join-Path $repoRoot 'build'
$workRoot = Join-Path $buildRoot 'filter-regex-csv-test'
$runRoot = Join-Path $workRoot 'runner'
$scanRoot = Join-Path $workRoot 'scan-root'
$platformShortName = switch ($Platform) {
    'Win32' { 'x86' }
    'x64' { 'x64' }
    'ARM64' { 'arm64' }
}
$targetName = "WinDirStat_$($platformShortName)_filtertest"
$symbolIn = [string] [char] 0x25cf
$symbolOut = [string] [char] 0x25cb
$symbolPass = [string] [char] 0x2713
$symbolFail = [string] [char] 0x2717

function Get-StatusColor {
    param([Parameter(Mandatory)] [string] $Status)

    switch ($Status) {
        'PASS' { 'Green' }
        'FAIL' { 'Red' }
        default { 'Yellow' }
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

function Find-MSBuild {
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio',
        'C:\Program Files (x86)\Microsoft Visual Studio'
    )

    foreach ($root in $candidates) {
        if (!(Test-Path -LiteralPath $root)) { continue }
        $match = Get-ChildItem -LiteralPath $root -Recurse -Filter MSBuild.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -like '*\MSBuild\Current\Bin\MSBuild.exe' } |
            Select-Object -First 1
        if ($match) { return $match.FullName }
    }

    throw 'MSBuild.exe was not found. Pass -SkipBuild -ExePath <path-to-WinDirStat.exe> to test an existing build.'
}

function Remove-TestBuildArtifacts {
    $paths = @(
        (Join-Path $buildRoot "$targetName.exe"),
        (Join-Path $buildRoot "$targetName.pdb"),
        (Join-Path $buildRoot "$targetName.lib"),
        (Join-Path $repoRoot "intermediate\$($Platform)_$($Configuration)\windirstat\$targetName.exe.recipe"),
        (Join-Path $repoRoot "intermediate\$($Platform)_$($Configuration)\windirstat\$targetName.iobj"),
        (Join-Path $repoRoot "intermediate\$($Platform)_$($Configuration)\windirstat\$targetName.ipdb"),
        (Join-Path $repoRoot "intermediate\$($Platform)_$($Configuration)\windirstat\$targetName.pch")
    )

    foreach ($path in $paths) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Force
        }
    }
}

function New-TestFile {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [int] $Size
    )

    $parent = Split-Path -Parent $Path
    New-Item -ItemType Directory -Force -Path $parent | Out-Null

    $bytes = [byte[]]::new($Size)
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $bytes[$i] = [byte] (($i % 251) + 1)
    }
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function ConvertTo-CxxRegexLiteralPath {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [switch] $DoubleBackslash
    )

    $full = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $specials = '.+*?^$()[]{}|'
    $builder = [System.Text.StringBuilder]::new()

    foreach ($ch in $full.ToCharArray()) {
        if ($ch -eq '\') {
            if ($DoubleBackslash) {
                [void] $builder.Append('\\')
            }
            else {
                [void] $builder.Append('\')
            }
        }
        elseif ($specials.Contains($ch)) {
            [void] $builder.Append('\')
            [void] $builder.Append($ch)
        }
        else {
            [void] $builder.Append($ch)
        }
    }

    return $builder.ToString()
}

function Normalize-ComparePath {
    param([Parameter(Mandatory)] [string] $Path)
    return [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
}

function Test-PathUnder {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Root
    )

    $candidate = Normalize-ComparePath $Path
    $prefix = Normalize-ComparePath $Root
    return $candidate.Equals($prefix, [System.StringComparison]::OrdinalIgnoreCase) -or
        $candidate.StartsWith("$prefix\", [System.StringComparison]::OrdinalIgnoreCase)
}

function Get-SetDifference {
    param(
        [Parameter(Mandatory)] [string[]] $Left,
        [Parameter(Mandatory)] [string[]] $Right
    )

    $rightSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($item in $Right) { [void] $rightSet.Add($item) }

    @($Left | Where-Object { !$rightSet.Contains($_) } | Sort-Object)
}

function New-StringSet {
    param([AllowNull()] [string[]] $Items)

    $set = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($item in @($Items)) {
        if (![string]::IsNullOrWhiteSpace($item)) {
            [void] $set.Add((Normalize-ComparePath $item))
        }
    }
    return ,$set
}

function Get-RelativeTestPath {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Root
    )

    $candidate = Normalize-ComparePath $Path
    $prefix = Normalize-ComparePath $Root
    if ($candidate.Equals($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        return '.'
    }
    if ($candidate.StartsWith("$prefix\", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $candidate.Substring($prefix.Length + 1)
    }
    return $candidate
}

function Format-ListBlock {
    param(
        [Parameter(Mandatory)] [string] $Title,
        [AllowNull()] [string[]] $Items,
        [int] $Limit = 200
    )

    $itemsSafe = @($Items)
    Write-ColoredLine "$Title ($($itemsSafe.Count)):" DarkCyan
    if ($itemsSafe.Count -eq 0) {
        Write-ColoredLine '  (none)' DarkGray
        return
    }

    foreach ($item in ($itemsSafe | Select-Object -First $Limit)) {
        Write-Host "  - $item"
    }
    if ($itemsSafe.Count -gt $Limit) {
        Write-ColoredLine "  ... $($itemsSafe.Count - $Limit) more" DarkGray
    }
}

function New-Scenario {
    param(
        [Parameter(Mandatory)] [string] $Name,
        [Parameter(Mandatory)] [bool] $UseRegex,
        [Parameter(Mandatory)] [string] $ExpectedBehavior,
        [string[]] $IncludeDirs = @(),
        [string[]] $ExcludeDirs = @(),
        [string[]] $IncludeFiles = @(),
        [string[]] $ExcludeFiles = @(),
        [int] $SizeMinimum = 0,
        [int] $SizeUnits = 0,
        [string[]] $ExpectedRows = @()
    )

    [pscustomobject] @{
        Name = $Name
        UseRegex = $UseRegex
        ExpectedBehavior = $ExpectedBehavior
        IncludeDirs = @($IncludeDirs)
        ExcludeDirs = @($ExcludeDirs)
        IncludeFiles = @($IncludeFiles)
        ExcludeFiles = @($ExcludeFiles)
        SizeMinimum = $SizeMinimum
        SizeUnits = $SizeUnits
        ExpectedRows = @($ExpectedRows | ForEach-Object { Normalize-ComparePath $_ } | Sort-Object)
        AllRows = @()
    }
}

function Write-PortableIni {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [pscustomobject] $Scenario
    )

    $recordSeparator = [string] [char] 0x1e
    $useRegexInt = if ($Scenario.UseRegex) { 1 } else { 0 }
    $ini = @(
        '[Options]',
        'LanguageId=9',
        "FilteringUseRegex=$useRegexInt",
        "FilteringSizeMinimum=$($Scenario.SizeMinimum)",
        "FilteringSizeUnits=$($Scenario.SizeUnits)",
        'UseFastScanEngine=1',
        'UseBackupRestore=0',
        'ShowElevationPrompt=0',
        'AutoElevate=0',
        'ShowFreeSpace=0',
        'ShowUnknown=0',
        'ProcessHardlinks=0',
        '',
        '[DriveSelect]',
        "FilteringIncludeDirs=$(@($Scenario.IncludeDirs) -join $recordSeparator)",
        "FilteringExcludeDirs=$(@($Scenario.ExcludeDirs) -join $recordSeparator)",
        "FilteringIncludeFiles=$(@($Scenario.IncludeFiles) -join $recordSeparator)",
        "FilteringExcludeFiles=$(@($Scenario.ExcludeFiles) -join $recordSeparator)",
        '',
        '[DupeView]',
        'ScanForDuplicates=0',
        ''
    ) -join "`r`n"

    # Persisted string settings store newlines as ASCII 0x1e. Keep that exact
    # separator in the INI while using CRLF for ordinary INI line endings.
    [System.IO.File]::WriteAllText($Path, $ini, [System.Text.Encoding]::Unicode)
}

function Invoke-WinDirStatCsvScan {
    param(
        [Parameter(Mandatory)] [string] $Exe,
        [Parameter(Mandatory)] [string] $Csv,
        [Parameter(Mandatory)] [string] $Root
    )

    $arguments = "/savetocsv `"$Csv`" `"$Root`""
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Exe
    $startInfo.Arguments = $arguments
    $startInfo.WorkingDirectory = Split-Path -Parent $Exe
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $process = [System.Diagnostics.Process]::Start($startInfo)
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        try { $process.Kill() } catch {}
        throw "WinDirStat did not finish within $TimeoutSeconds seconds."
    }
    $sw.Stop()

    if ($process.ExitCode -ne 0) {
        throw "WinDirStat exited with code $($process.ExitCode)."
    }

    if (!(Test-Path -LiteralPath $Csv)) {
        throw "WinDirStat exited successfully but did not create CSV: $Csv"
    }

    [pscustomobject] @{
        CommandLine = "`"$Exe`" $arguments"
        ExitCode = $process.ExitCode
        ElapsedSeconds = [math]::Round($sw.Elapsed.TotalSeconds, 3)
    }
}

function Read-CsvPaths {
    param([Parameter(Mandatory)] [string] $Csv)

    $rows = @(Import-Csv -LiteralPath $Csv -Encoding UTF8)
    if (!$rows) {
        throw 'CSV did not contain any rows.'
    }
    if (!($rows[0].PSObject.Properties.Name -contains 'Name')) {
        throw "CSV did not contain the expected English 'Name' column. Headers: $($rows[0].PSObject.Properties.Name -join ', ')"
    }

    [pscustomobject] @{
        Rows = $rows
        Paths = @($rows | ForEach-Object { Normalize-ComparePath $_.Name } | Sort-Object)
    }
}

function Write-SymbolCell {
    param(
        [Parameter(Mandatory)] [string] $Text,
        [Parameter(Mandatory)] [int] $Width,
        [ConsoleColor] $Color = [ConsoleColor]::Gray
    )

    Write-Host -NoNewline $Text.PadRight($Width) -ForegroundColor $Color
}

function Write-PathVerificationTable {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Result,
        [int] $Limit = 250
    )

    $expectedSet = New-StringSet $Result.ExpectedRows
    $actualSet = New-StringSet $Result.ActualRows
    $allSet = New-StringSet $Result.AllRows
    foreach ($item in @($Result.ExpectedRows + $Result.ActualRows)) {
        if (![string]::IsNullOrWhiteSpace($item)) {
            [void] $allSet.Add((Normalize-ComparePath $item))
        }
    }

    $rows = @($allSet | ForEach-Object {
        $expected = $expectedSet.Contains($_)
        $actual = $actualSet.Contains($_)
        [pscustomobject] @{
            Path = $_
            RelativePath = Get-RelativeTestPath -Path $_ -Root $Result.ScanRoot
            Expected = $expected
            Actual = $actual
            Verified = $expected -eq $actual
        }
    } | Sort-Object RelativePath)

    Write-ColoredLine "Path verification table ($($rows.Count) path(s)):" DarkCyan
    Write-ColoredLine "  Should/In CSV: $symbolIn included or present, $symbolOut excluded or absent; Verified: $symbolPass matched, $symbolFail mismatch" DarkGray

    if ($rows.Count -eq 0) {
        Write-ColoredLine '  (none)' DarkGray
        return
    }

    $visibleRows = @($rows | Select-Object -First $Limit)
    $pathWidth = [Math]::Max(
        13,
        @($visibleRows | ForEach-Object { $_.RelativePath.Length } | Measure-Object -Maximum).Maximum
    )
    $pathWidth = [Math]::Min($pathWidth, 110)
    $expectedWidth = 8
    $actualWidth = 8
    $verifiedWidth = 8

    Write-Host -NoNewline '  '
    Write-SymbolCell 'Path' $pathWidth Cyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Should' $expectedWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'In CSV' $actualWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Verified' $verifiedWidth DarkCyan
    Write-Host ''

    Write-Host -NoNewline '  '
    Write-ColoredLine ("".PadRight($pathWidth + $expectedWidth + $actualWidth + $verifiedWidth + 6, '-')) DarkGray

    foreach ($row in $visibleRows) {
        $expectedSymbol = if ($row.Expected) { $symbolIn } else { $symbolOut }
        $actualSymbol = if ($row.Actual) { $symbolIn } else { $symbolOut }
        $verifiedSymbol = if ($row.Verified) { $symbolPass } else { $symbolFail }
        $rowColor = if (!$row.Verified) { 'Red' } elseif ($row.Expected) { 'Green' } else { 'DarkGray' }
        $actualColor = if ($row.Actual -eq $row.Expected) { $rowColor } else { 'Red' }
        $displayPath = $row.RelativePath
        if ($displayPath.Length -gt $pathWidth) {
            $displayPath = $displayPath.Substring(0, [Math]::Max(0, $pathWidth - 3)) + '...'
        }

        Write-Host -NoNewline '  '
        Write-SymbolCell $displayPath $pathWidth $rowColor
        Write-Host -NoNewline '  '
        Write-SymbolCell $expectedSymbol $expectedWidth $(if ($row.Expected) { 'Green' } else { 'DarkGray' })
        Write-Host -NoNewline '  '
        Write-SymbolCell $actualSymbol $actualWidth $actualColor
        Write-Host -NoNewline '  '
        Write-SymbolCell $verifiedSymbol $verifiedWidth $(if ($row.Verified) { 'Green' } else { 'Red' })
        Write-Host ''
    }

    if ($rows.Count -gt $Limit) {
        Write-ColoredLine "  ... $($rows.Count - $Limit) more path(s)" DarkGray
    }
}

function Write-SuiteResultsTable {
    param([Parameter(Mandatory)] [pscustomobject[]] $Results)

    $scenarioWidth = [Math]::Max(
        8,
        @($Results | ForEach-Object { $_.Name.Length } | Measure-Object -Maximum).Maximum
    )
    $scenarioWidth = [Math]::Min($scenarioWidth, 62)
    $statusWidth = 6
    $modeWidth = 13
    $rowsWidth = 13
    $elapsedWidth = 8

    Write-ColoredLine 'Scenario results:' DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Status' $statusWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Scenario' $scenarioWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Mode' $modeWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Rows' $rowsWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Elapsed' $elapsedWidth DarkCyan
    Write-Host ''

    Write-Host -NoNewline '  '
    Write-ColoredLine ("".PadRight($statusWidth + $scenarioWidth + $modeWidth + $rowsWidth + $elapsedWidth + 8, '-')) DarkGray

    foreach ($result in $Results) {
        $statusColor = Get-StatusColor $result.Status
        $name = $result.Name
        if ($name.Length -gt $scenarioWidth) {
            $name = $name.Substring(0, [Math]::Max(0, $scenarioWidth - 3)) + '...'
        }
        $mode = if ($result.UseRegex) { 'regex' } else { 'glob' }
        $rowsText = "$(@($result.ActualRows).Count)/$(@($result.ExpectedRows).Count)"
        $elapsedText = if ($null -eq $result.ElapsedSeconds) { '-' } else { "$($result.ElapsedSeconds)s" }

        Write-Host -NoNewline '  '
        Write-SymbolCell $result.Status $statusWidth $statusColor
        Write-Host -NoNewline '  '
        Write-SymbolCell $name $scenarioWidth Gray
        Write-Host -NoNewline '  '
        Write-SymbolCell $mode $modeWidth Gray
        Write-Host -NoNewline '  '
        Write-SymbolCell $rowsText $rowsWidth Gray
        Write-Host -NoNewline '  '
        Write-SymbolCell $elapsedText $elapsedWidth Gray
        Write-Host ''
    }
}

function Write-ScenarioSummary {
    param([Parameter(Mandatory)] [pscustomobject] $Result)

    $statusColor = Get-StatusColor $Result.Status
    $actualBehavior = if ($Result.Error) {
        "Scan failed before validation completed: $($Result.Error)"
    }
    elseif (@($Result.MissingRows).Count -eq 0 -and @($Result.UnexpectedRows).Count -eq 0) {
        'CSV output matched the expected paths exactly.'
    }
    else {
        "CSV output differed from expectation: $(@($Result.MissingRows).Count) missing path(s), $(@($Result.UnexpectedRows).Count) unexpected path(s)."
    }

    Write-Host ''
    Write-ColoredLine "=== $($Result.Name) ===" Cyan
    Write-LabelValue 'Result' $Result.Status $statusColor
    Write-LabelValue 'Command' $Result.CommandLine
    Write-LabelValue 'Test base' $Result.ScanRoot
    Write-LabelValue 'Mode' $(if ($Result.UseRegex) { 'regex' } else { 'glob/non-regex' })
    Write-LabelValue 'Size minimum' "$($Result.SizeMinimum) unit index $($Result.SizeUnits)"
    Format-ListBlock 'Input include directories' $Result.IncludeDirs
    Format-ListBlock 'Input exclude directories' $Result.ExcludeDirs
    Format-ListBlock 'Input include files' $Result.IncludeFiles
    Format-ListBlock 'Input exclude files' $Result.ExcludeFiles
    Write-LabelValue 'Expected behavior' $Result.ExpectedBehavior
    Write-LabelValue 'Actual behavior' $actualBehavior $statusColor
    Write-PathVerificationTable $Result
    if (@($Result.MissingRows).Count -gt 0) {
        Format-ListBlock 'Missing expected paths' $Result.MissingRows
    }
    if (@($Result.UnexpectedRows).Count -gt 0) {
        Format-ListBlock 'Unexpected actual paths' $Result.UnexpectedRows
    }
    if ($Result.Error) {
        Write-LabelValue 'Error' $Result.Error Red
    }
    else {
        Write-LabelValue 'Exit code' $Result.ExitCode
        Write-LabelValue 'Elapsed seconds' $Result.ElapsedSeconds
    }
}

function Invoke-Scenario {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Scenario,
        [Parameter(Mandatory)] [string] $Exe,
        [Parameter(Mandatory)] [string] $Root
    )

    $safeName = $Scenario.Name -replace '[^A-Za-z0-9_.-]', '_'
    $scenarioRoot = Join-Path $workRoot $safeName
    $scenarioIni = Join-Path $scenarioRoot 'WinDirStat.ini'
    $runnerIni = Join-Path $runRoot 'WinDirStat.ini'
    $scenarioCsv = Join-Path $scenarioRoot 'results.csv'
    New-Item -ItemType Directory -Force -Path $scenarioRoot | Out-Null

    $actualRows = @()
    $missingRows = @()
    $unexpectedRows = @()
    $commandLine = "`"$Exe`" /savetocsv `"$scenarioCsv`" `"$Root`""
    $exitCode = $null
    $elapsedSeconds = $null
    $errorText = $null

    try {
        Write-PortableIni -Path $scenarioIni -Scenario $Scenario
        Copy-Item -LiteralPath $scenarioIni -Destination $runnerIni -Force
        if (Test-Path -LiteralPath $scenarioCsv) {
            Remove-Item -LiteralPath $scenarioCsv -Force
        }

        $scan = Invoke-WinDirStatCsvScan -Exe $Exe -Csv $scenarioCsv -Root $Root
        $commandLine = $scan.CommandLine
        $exitCode = $scan.ExitCode
        $elapsedSeconds = $scan.ElapsedSeconds

        $csv = Read-CsvPaths $scenarioCsv
        $actualRows = @($csv.Paths)
        $missingRows = Get-SetDifference -Left $Scenario.ExpectedRows -Right $actualRows
        $unexpectedRows = Get-SetDifference -Left $actualRows -Right $Scenario.ExpectedRows
    }
    catch {
        $errorText = $_.Exception.Message
        $missingRows = @($Scenario.ExpectedRows)
    }

    $status = if (!$errorText -and @($missingRows).Count -eq 0 -and @($unexpectedRows).Count -eq 0) { 'PASS' } else { 'FAIL' }

    [pscustomobject] @{
        Name = $Scenario.Name
        Status = $status
        CommandLine = $commandLine
        UseRegex = $Scenario.UseRegex
        SizeMinimum = $Scenario.SizeMinimum
        SizeUnits = $Scenario.SizeUnits
        IncludeDirs = @($Scenario.IncludeDirs)
        ExcludeDirs = @($Scenario.ExcludeDirs)
        IncludeFiles = @($Scenario.IncludeFiles)
        ExcludeFiles = @($Scenario.ExcludeFiles)
        ExpectedBehavior = $Scenario.ExpectedBehavior
        ExpectedRows = @($Scenario.ExpectedRows)
        AllRows = @($Scenario.AllRows)
        ActualRows = @($actualRows)
        MissingRows = @($missingRows)
        UnexpectedRows = @($unexpectedRows)
        ExitCode = $exitCode
        ElapsedSeconds = $elapsedSeconds
        Error = $errorText
        ScanRoot = $Root
        CsvPath = $scenarioCsv
        IniPath = $scenarioIni
    }
}

function Get-ExpectedRows {
    param(
        [string[]] $AllDirs,
        [string[]] $AllFiles,
        [hashtable] $FileSizes,
        [string[]] $IncludeDirRoots = @(),
        [string[]] $ExcludeDirRoots = @(),
        [switch] $IncludeFilePatterns,
        [switch] $ExcludeFilePatterns,
        [string[]] $IncludeFileNames = @(),
        [string[]] $ExcludeFileNames = @(),
        [int] $SizeMinimum = 0
    )

    $includeRoots = @($IncludeDirRoots)
    $excludeRoots = @($ExcludeDirRoots)
    $includeNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $excludeNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($name in @($IncludeFileNames)) { [void] $includeNames.Add($name) }
    foreach ($name in @($ExcludeFileNames)) { [void] $excludeNames.Add($name) }

    function IsExcludedPath([string] $Path) {
        foreach ($root in $excludeRoots) {
            if (Test-PathUnder -Path $Path -Root $root) { return $true }
        }
        $false
    }

    function IsIncludedDirectory([string] $Dir) {
        if ($includeRoots.Count -eq 0) { return $true }
        foreach ($root in $includeRoots) {
            if ((Test-PathUnder -Path $Dir -Root $root) -or (Test-PathUnder -Path $root -Root $Dir)) { return $true }
        }
        $false
    }

    function IsIncludedFileDirectory([string] $File) {
        if ($includeRoots.Count -eq 0) { return $true }
        foreach ($root in $includeRoots) {
            if (Test-PathUnder -Path $File -Root $root) { return $true }
        }
        $false
    }

    $candidateDirs = @($AllDirs | Where-Object { (IsIncludedDirectory $_) -and !(IsExcludedPath $_) })
    $candidateFiles = @($AllFiles | Where-Object {
        $name = [System.IO.Path]::GetFileName($_)
        (IsIncludedFileDirectory $_) -and
            !(IsExcludedPath $_) -and
            (!$IncludeFilePatterns -or $name -like 'include-*.keep' -or $name -eq 'anchor-pass.dat') -and
            ($includeNames.Count -eq 0 -or $includeNames.Contains($name)) -and
            (!$ExcludeFilePatterns -or !($name -eq 'include-blocked.keep' -or $name -like '*.skip')) -and
            ($excludeNames.Count -eq 0 -or !$excludeNames.Contains($name)) -and
            ($SizeMinimum -le 0 -or $FileSizes[$_] -ge $SizeMinimum)
    })

    @(@($candidateDirs) + @($candidateFiles) | ForEach-Object { Normalize-ComparePath $_ } | Sort-Object)
}

$sourceExe = $ExePath
$builtForTest = $false
$suiteSucceeded = $false

try {
    if (!$SkipBuild) {
        $msbuild = Find-MSBuild
        $solution = Join-Path $repoRoot 'windirstat.sln'
        $buildArgs = @(
            $solution,
            '/m:1',
            '/v:minimal',
            "/p:Configuration=$Configuration",
            "/p:Platform=$Platform",
            "/p:TargetName=$targetName"
        )

        Write-ColoredLine "Building $targetName..." Cyan
        $buildOutput = @(& $msbuild @buildArgs 2>&1)
        $buildExitCode = $LASTEXITCODE
        if ($ShowBuildOutput -or $buildExitCode -ne 0) {
            $buildOutput | ForEach-Object {
                $color = if ($buildExitCode -eq 0) { 'DarkGray' } else { 'Yellow' }
                Write-Host $_ -ForegroundColor $color
            }
        }
        if ($buildExitCode -ne 0) {
            throw "MSBuild failed with exit code $buildExitCode."
        }

        $sourceExe = Join-Path $buildRoot "$targetName.exe"
        $builtForTest = $true
        Write-ColoredLine "Build complete: $sourceExe" Green
    }
    elseif ([string]::IsNullOrWhiteSpace($sourceExe)) {
        $sourceExe = Join-Path $buildRoot "WinDirStat_$platformShortName.exe"
    }

    if (!(Test-Path -LiteralPath $sourceExe)) {
        throw "WinDirStat executable was not found: $sourceExe"
    }

    if (Test-Path -LiteralPath $workRoot) {
        Remove-Item -LiteralPath $workRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $runRoot, $scanRoot | Out-Null

    $testExe = Join-Path $runRoot 'WinDirStat.exe'
    Copy-Item -LiteralPath $sourceExe -Destination $testExe -Force

    function UnderTest([string] $RelativePath) {
        Normalize-ComparePath (Join-Path $scanRoot $RelativePath)
    }

    function RegexPath([string] $Path, [switch] $DoubleBackslash, [switch] $TrailingSlash) {
        '^' + (ConvertTo-CxxRegexLiteralPath $Path -DoubleBackslash:$DoubleBackslash) + ($TrailingSlash ? '\' : '$')
    }

    function Expected([hashtable] $Spec = @{}) {
        Get-ExpectedRows -AllDirs $allDirs -AllFiles $allFiles -FileSizes $fileSizes @Spec
    }

    function Scenario([hashtable] $Spec) {
        $expectedSpec = $Spec.ContainsKey('Expected') ? $Spec.Expected : @{}
        $args = @{
            Name = $Spec.Name
            UseRegex = $Spec.Regex
            ExpectedBehavior = $Spec.Behavior
            ExpectedRows = Expected $expectedSpec
        }
        foreach ($entry in @{
            IncludeDirs = 'IncludeDirs'; ExcludeDirs = 'ExcludeDirs'
            IncludeFiles = 'IncludeFiles'; ExcludeFiles = 'ExcludeFiles'
            SizeMinimum = 'SizeMinimum'; SizeUnits = 'SizeUnits'
        }.GetEnumerator()) {
            if ($Spec.ContainsKey($entry.Value)) { $args[$entry.Key] = $Spec[$entry.Value] }
        }
        $scenario = New-Scenario @args
        $scenario.AllRows = @($allExpected)
        $scenario
    }

    $dirRel = [ordered] @{
        Root = '.'
        Alpha = 'IncludedAlpha'
        AlphaNested = 'IncludedAlpha\Nested'
        AlphaExcluded = 'IncludedAlpha\ExcludedByDir'
        AlphaExcludedDeep = 'IncludedAlpha\Nested\ExcludedDeep'
        Beta = 'EscapedBeta'
        BetaExcluded = 'EscapedBeta\BetaExcluded'
        Conflict = 'IncludeConflict'
        AlphaSibling = 'IncludedAlphaSibling'
        AlphaSiblingDeep = 'IncludedAlphaSibling\Deep'
        Special = 'Special.Dir[One]'
        Outside = 'OutsideSibling'
        OutsideDeep1 = 'OutsideSibling\Deep1'
        OutsideDeep2 = 'OutsideSibling\Deep1\Deep2'
        OutsideDeep3 = 'OutsideSibling\Deep1\Deep2\Deep3'
    }
    $D = @{}
    foreach ($dir in $dirRel.GetEnumerator()) {
        $D[$dir.Key] = UnderTest $dir.Value
        New-Item -ItemType Directory -Force -Path $D[$dir.Key] | Out-Null
    }
    $allDirs = @($dirRel.Keys | ForEach-Object { $D[$_] })

    $fileRel = [ordered] @{
        RootKeep = @('include-root.keep', 64)
        RootTxt = @('root.txt', 64)
        AlphaKeep = @('IncludedAlpha\include-alpha.keep', 64)
        AlphaBlocked = @('IncludedAlpha\include-blocked.keep', 64)
        AlphaSkip = @('IncludedAlpha\alpha.skip', 64)
        AlphaTxt = @('IncludedAlpha\alpha.txt', 64)
        AlphaSmall = @('IncludedAlpha\include-too-small.keep', 1)
        NestedKeep = @('IncludedAlpha\Nested\include-nested.keep', 64)
        NestedTxt = @('IncludedAlpha\Nested\nested.txt', 64)
        ExcludedKeep = @('IncludedAlpha\ExcludedByDir\include-excluded.keep', 64)
        ExcludedDeepKeep = @('IncludedAlpha\Nested\ExcludedDeep\include-excluded-deep.keep', 64)
        BetaKeep = @('EscapedBeta\include-beta.keep', 64)
        BetaAnchor = @('EscapedBeta\anchor-pass.dat', 64)
        BetaTxt = @('EscapedBeta\beta.txt', 64)
        BetaExcludedKeep = @('EscapedBeta\BetaExcluded\include-beta-excluded.keep', 64)
        ConflictKeep = @('IncludeConflict\include-conflict.keep', 64)
        SiblingKeep = @('IncludedAlphaSibling\Deep\include-prefix-sibling.keep', 64)
        SiblingTxt = @('IncludedAlphaSibling\Deep\prefix-sibling.txt', 64)
        SpecialKeep = @('Special.Dir[One]\literal-special.keep', 64)
        SpecialTmp = @('Special.Dir[One]\literal-special.tmp', 64)
        OutsideKeep = @('OutsideSibling\include-outside.keep', 64)
        OutsideTxt = @('OutsideSibling\outside.txt', 64)
        OutsideDeepKeep = @('OutsideSibling\Deep1\Deep2\Deep3\include-outside-deep.keep', 64)
    }
    $F = @{}
    $fileSizes = @{}
    foreach ($file in $fileRel.GetEnumerator()) {
        $path = UnderTest $file.Value[0]
        $F[$file.Key] = $path
        New-TestFile -Path $path -Size $file.Value[1]
        $fileSizes[$path] = $file.Value[1]
    }
    $allFiles = @($fileRel.Keys | ForEach-Object { $F[$_] })

    $roots = @{
        Top = @($D.Alpha, $D.Beta)
        GlobTop = @($D.Alpha, $D.AlphaSibling, $D.Beta)
        Alpha = @($D.Alpha)
        Nested = @($D.AlphaNested)
        TopAndConflict = @($D.Alpha, $D.Beta, $D.Conflict)
        GlobTopAndConflict = @($D.Alpha, $D.AlphaSibling, $D.Beta, $D.Conflict)
        Conflict = @($D.Conflict)
        Special = @($D.Special)
        Root = @($D.Root)
        Excluded = @($D.AlphaExcluded, $D.AlphaExcludedDeep, $D.BetaExcluded, $D.Conflict)
        AlphaExcluded = @($D.AlphaExcluded)
    }

    $rx = @{
        IncludeTop = @((RegexPath $D.Alpha), (RegexPath $D.Beta -DoubleBackslash))
        IncludeAlpha = @(RegexPath $D.Alpha)
        IncludeNested = @(RegexPath $D.AlphaNested)
        IncludeAlphaTrailing = @(RegexPath $D.Alpha -TrailingSlash)
        IncludeAlphaLower = @(RegexPath ($D.Alpha.ToLowerInvariant()))
        IncludeRootTrailing = @(RegexPath $D.Root -TrailingSlash)
        IncludeSpecial = @(RegexPath $D.Special)
        IncludeConflict = @(RegexPath $D.Conflict)
        IncludeFiles = @('^include-.*\.keep$', '^anchor-pass\.dat$')
        IncludeBlocked = @('^include-blocked\.keep$')
        IncludeAlphaUpper = @('^INCLUDE-ALPHA\.KEEP$')
        IncludeSpecialFile = @('^literal-special\.keep$')
        ExcludeDirs = @((RegexPath $D.AlphaExcluded), (RegexPath $D.AlphaExcludedDeep), (RegexPath $D.BetaExcluded -DoubleBackslash), (RegexPath $D.Conflict))
        ExcludeAlphaTrailing = @(RegexPath $D.AlphaExcluded -TrailingSlash)
        ExcludeConflict = @(RegexPath $D.Conflict)
        ExcludeFiles = @('^include-blocked\.keep$', '^.*\.skip$')
        ExcludeBlocked = @('^include-blocked\.keep$')
    }
    $rx.IncludeTopAndConflict = @($rx.IncludeTop + (RegexPath $D.Conflict))

    $glob = @{
        IncludeTop = @((Join-Path $scanRoot 'Included*'), $D.Beta)
        IncludeAlpha = @($D.Alpha)
        IncludeNested = @(Join-Path $D.Alpha 'Nes*')
        IncludeAlphaTrailing = @("$($D.Alpha)\")
        IncludeAlphaLower = @(($D.Alpha.ToLowerInvariant()))
        IncludeRootTrailing = @("$($D.Root)\")
        IncludeSpecial = @($D.Special)
        IncludeConflict = @($D.Conflict)
        IncludeFiles = @('include-*.keep', 'anchor-pass.dat')
        IncludeBlocked = @('include-blocked.keep')
        IncludeAlphaUpper = @('INCLUDE-ALPHA.KEEP')
        IncludeSpecialFile = @('literal-special.keep')
        ExcludeDirs = @((Join-Path $D.Alpha 'Excluded*'), (Join-Path $D.AlphaNested 'Excluded*'), $D.BetaExcluded, $D.Conflict)
        ExcludeAlphaTrailing = @("$($D.AlphaExcluded)\")
        ExcludeConflict = @($D.Conflict)
        ExcludeFiles = @('include-blocked.keep', '*.skip')
        ExcludeBlocked = @('include-blocked.keep')
    }
    $glob.IncludeTopAndConflict = @($glob.IncludeTop + $D.Conflict)

    $allExpected = Expected
    $scenarioSpecs = @(
        @{ Name = 'Baseline_NoFilters'; Regex = $false; Behavior = 'With no filters, every directory and file in the generated scan tree should be exported.' }
        @{ Name = 'Regex_SizeOnly'; Regex = $true; SizeMinimum = 2; Expected = @{ SizeMinimum = 2 }; Behavior = 'Regex mode with only a size minimum should keep every directory but omit files smaller than two bytes.' }
        @{ Name = 'Regex_IncludeDirs_TopLevel'; Regex = $true; IncludeDirs = $rx.IncludeTop; Expected = @{ IncludeDirRoots = $roots.Top }; Behavior = 'Only IncludedAlpha and EscapedBeta, plus their descendants, should be scanned; root files and unrelated siblings should be absent.' }
        @{ Name = 'Regex_IncludeDirs_ExactPrefixCollision'; Regex = $true; IncludeDirs = $rx.IncludeAlpha; Expected = @{ IncludeDirRoots = $roots.Alpha }; Behavior = 'An exact regex include for IncludedAlpha should not include the same-prefix sibling IncludedAlphaSibling.' }
        @{ Name = 'Regex_IncludeDirs_RootTrailingSlash'; Regex = $true; IncludeDirs = $rx.IncludeRootTrailing; Expected = @{ IncludeDirRoots = $roots.Root }; Behavior = 'A regex include for the scan root with a trailing slash should include the whole scan tree.' }
        @{ Name = 'Regex_IncludeDirs_NestedOnly'; Regex = $true; IncludeDirs = $rx.IncludeNested; Expected = @{ IncludeDirRoots = $roots.Nested }; Behavior = 'The scanner should descend through IncludedAlpha to reach Nested, but should not scan sibling files directly under IncludedAlpha.' }
        @{ Name = 'Regex_TrailingSlash_IncludeExcludeDirs'; Regex = $true; IncludeDirs = $rx.IncludeAlphaTrailing; ExcludeDirs = $rx.ExcludeAlphaTrailing; Expected = @{ IncludeDirRoots = $roots.Alpha; ExcludeDirRoots = $roots.AlphaExcluded }; Behavior = 'Trailing slashes in regex directory filters should be tolerated, and the excluded child branch should override the included parent.' }
        @{ Name = 'Regex_ExcludeDirs'; Regex = $true; ExcludeDirs = $rx.ExcludeDirs; Expected = @{ ExcludeDirRoots = $roots.Excluded }; Behavior = 'All branches should be scanned except directory exclusions and everything below them.' }
        @{ Name = 'Regex_IncludeFiles'; Regex = $true; IncludeFiles = $rx.IncludeFiles; Expected = @{ IncludeFilePatterns = $true }; Behavior = 'All directories should be present, but only files matching include-*.keep or anchor-pass.dat should be exported.' }
        @{ Name = 'Regex_ExcludeFiles'; Regex = $true; ExcludeFiles = $rx.ExcludeFiles; Expected = @{ ExcludeFilePatterns = $true }; Behavior = 'All directories should be present, but include-blocked.keep and *.skip files should be absent.' }
        @{ Name = 'Regex_IncludeDirsAndFiles'; Regex = $true; IncludeDirs = $rx.IncludeTop; IncludeFiles = $rx.IncludeFiles; Expected = @{ IncludeDirRoots = $roots.Top; IncludeFilePatterns = $true }; Behavior = 'Only included directory branches should be scanned, and inside them only include-*.keep or anchor-pass.dat files should remain.' }
        @{ Name = 'Regex_ExcludeDirsAndFiles'; Regex = $true; ExcludeDirs = $rx.ExcludeDirs; ExcludeFiles = $rx.ExcludeFiles; Expected = @{ ExcludeDirRoots = $roots.Excluded; ExcludeFilePatterns = $true }; Behavior = 'Excluded directory branches should be absent, and file excludes should also remove blocked or *.skip files from remaining branches.' }
        @{ Name = 'Regex_AllFilters_PrecedenceAndSize'; Regex = $true; IncludeDirs = $rx.IncludeTopAndConflict; ExcludeDirs = $rx.ExcludeDirs; IncludeFiles = $rx.IncludeFiles; ExcludeFiles = $rx.ExcludeFiles; SizeMinimum = 2; Expected = @{ IncludeDirRoots = $roots.TopAndConflict; ExcludeDirRoots = $roots.Excluded; IncludeFilePatterns = $true; ExcludeFilePatterns = $true; SizeMinimum = 2 }; Behavior = 'Includes admit Alpha, Beta, and Conflict, but directory excludes win over includes; file excludes win over file includes; size minimum removes the one-byte keep file.' }
        @{ Name = 'Regex_IncludeExcludeSameDirectory'; Regex = $true; IncludeDirs = $rx.IncludeConflict; ExcludeDirs = $rx.ExcludeConflict; Expected = @{ IncludeDirRoots = $roots.Conflict; ExcludeDirRoots = $roots.Conflict }; Behavior = 'When the same directory is both included and excluded, the exclude should win and only the scan root ancestor should remain.' }
        @{ Name = 'Regex_FileExcludeOverridesSameInclude'; Regex = $true; IncludeFiles = $rx.IncludeBlocked; ExcludeFiles = $rx.ExcludeBlocked; Expected = @{ IncludeFileNames = @('include-blocked.keep'); ExcludeFileNames = @('include-blocked.keep') }; Behavior = 'When the same file name is both included and excluded, the exclude should win and no files should be exported.' }
        @{ Name = 'Regex_CaseInsensitive_DirAndFile'; Regex = $true; IncludeDirs = $rx.IncludeAlphaLower; IncludeFiles = $rx.IncludeAlphaUpper; Expected = @{ IncludeDirRoots = $roots.Alpha; IncludeFileNames = @('include-alpha.keep') }; Behavior = 'Regex matching should be case-insensitive for both directory paths and file names.' }
        @{ Name = 'Regex_SpecialCharacterPathAndFile'; Regex = $true; IncludeDirs = $rx.IncludeSpecial; IncludeFiles = $rx.IncludeSpecialFile; Expected = @{ IncludeDirRoots = $roots.Special; IncludeFileNames = @('literal-special.keep') }; Behavior = 'Regex directory paths and file names containing regex metacharacters should match when escaped as literals.' }
        @{ Name = 'Glob_IncludeDirs_TopLevel'; Regex = $false; IncludeDirs = $glob.IncludeTop; Expected = @{ IncludeDirRoots = $roots.GlobTop }; Behavior = 'Glob directory includes should scan Included* and EscapedBeta branches only, including the same-prefix sibling matched by the wildcard.' }
        @{ Name = 'Glob_IncludeDirs_ExactPrefixCollision'; Regex = $false; IncludeDirs = $glob.IncludeAlpha; Expected = @{ IncludeDirRoots = $roots.Alpha }; Behavior = 'An exact glob include for IncludedAlpha should not include the same-prefix sibling IncludedAlphaSibling.' }
        @{ Name = 'Glob_IncludeDirs_RootTrailingSlash'; Regex = $false; IncludeDirs = $glob.IncludeRootTrailing; Expected = @{ IncludeDirRoots = $roots.Root }; Behavior = 'A glob include for the scan root with a trailing slash should include the whole scan tree.' }
        @{ Name = 'Glob_IncludeDirs_NestedOnly'; Regex = $false; IncludeDirs = $glob.IncludeNested; Expected = @{ IncludeDirRoots = $roots.Nested }; Behavior = 'Glob directory include should descend through IncludedAlpha to reach Nes* while skipping unrelated sibling branches and files.' }
        @{ Name = 'Glob_TrailingSlash_IncludeExcludeDirs'; Regex = $false; IncludeDirs = $glob.IncludeAlphaTrailing; ExcludeDirs = $glob.ExcludeAlphaTrailing; Expected = @{ IncludeDirRoots = $roots.Alpha; ExcludeDirRoots = $roots.AlphaExcluded }; Behavior = 'Trailing slashes in glob directory filters should be tolerated, and the excluded child branch should override the included parent.' }
        @{ Name = 'Glob_ExcludeDirs'; Regex = $false; ExcludeDirs = $glob.ExcludeDirs; Expected = @{ ExcludeDirRoots = $roots.Excluded }; Behavior = 'Glob directory excludes should remove matching branches and descendants while leaving all other branches intact.' }
        @{ Name = 'Glob_IncludeFiles'; Regex = $false; IncludeFiles = $glob.IncludeFiles; Expected = @{ IncludeFilePatterns = $true }; Behavior = 'Glob file includes should keep all directories but export only include-*.keep and anchor-pass.dat files.' }
        @{ Name = 'Glob_ExcludeFiles'; Regex = $false; ExcludeFiles = $glob.ExcludeFiles; Expected = @{ ExcludeFilePatterns = $true }; Behavior = 'Glob file excludes should remove include-blocked.keep and *.skip files while preserving all directories and other files.' }
        @{ Name = 'Glob_IncludeDirsAndFiles'; Regex = $false; IncludeDirs = $glob.IncludeTop; IncludeFiles = $glob.IncludeFiles; Expected = @{ IncludeDirRoots = $roots.GlobTop; IncludeFilePatterns = $true }; Behavior = 'Glob directory includes and file includes should combine so only selected branches and selected file names appear.' }
        @{ Name = 'Glob_ExcludeDirsAndFiles'; Regex = $false; ExcludeDirs = $glob.ExcludeDirs; ExcludeFiles = $glob.ExcludeFiles; Expected = @{ ExcludeDirRoots = $roots.Excluded; ExcludeFilePatterns = $true }; Behavior = 'Glob directory excludes and file excludes should both apply, with directory excludes removing whole branches first.' }
        @{ Name = 'Glob_AllFilters_PrecedenceAndSize'; Regex = $false; IncludeDirs = $glob.IncludeTopAndConflict; ExcludeDirs = $glob.ExcludeDirs; IncludeFiles = $glob.IncludeFiles; ExcludeFiles = $glob.ExcludeFiles; SizeMinimum = 2; Expected = @{ IncludeDirRoots = $roots.GlobTopAndConflict; ExcludeDirRoots = $roots.Excluded; IncludeFilePatterns = $true; ExcludeFilePatterns = $true; SizeMinimum = 2 }; Behavior = 'Glob mode should match regex-mode all-filter behavior: excludes override includes, and the size minimum removes small files.' }
        @{ Name = 'Glob_IncludeExcludeSameDirectory'; Regex = $false; IncludeDirs = $glob.IncludeConflict; ExcludeDirs = $glob.ExcludeConflict; Expected = @{ IncludeDirRoots = $roots.Conflict; ExcludeDirRoots = $roots.Conflict }; Behavior = 'When the same directory is both included and excluded, the exclude should win and only the scan root ancestor should remain.' }
        @{ Name = 'Glob_FileExcludeOverridesSameInclude'; Regex = $false; IncludeFiles = $glob.IncludeBlocked; ExcludeFiles = $glob.ExcludeBlocked; Expected = @{ IncludeFileNames = @('include-blocked.keep'); ExcludeFileNames = @('include-blocked.keep') }; Behavior = 'When the same file name is both included and excluded, the exclude should win and no files should be exported.' }
        @{ Name = 'Glob_CaseInsensitive_DirAndFile'; Regex = $false; IncludeDirs = $glob.IncludeAlphaLower; IncludeFiles = $glob.IncludeAlphaUpper; Expected = @{ IncludeDirRoots = $roots.Alpha; IncludeFileNames = @('include-alpha.keep') }; Behavior = 'Glob matching should be case-insensitive for both directory paths and file names.' }
        @{ Name = 'Glob_SpecialCharacterPathAndFile'; Regex = $false; IncludeDirs = $glob.IncludeSpecial; IncludeFiles = $glob.IncludeSpecialFile; Expected = @{ IncludeDirRoots = $roots.Special; IncludeFileNames = @('literal-special.keep') }; Behavior = 'Glob directory paths and file names containing glob/regex metacharacters should match as literals when no wildcard is used.' }
    )
    $scenarios = @($scenarioSpecs | ForEach-Object { Scenario $_ })

    Write-ColoredLine "Prepared $($scenarios.Count) filtering scenarios against: $scanRoot" Cyan
    $results = @()
    foreach ($scenario in $scenarios) {
        $result = Invoke-Scenario -Scenario $scenario -Exe $testExe -Root $scanRoot
        $results += $result
    }

    $failed = @($results | Where-Object { $_.Status -ne 'PASS' })
    Write-Host ''
    Write-ColoredLine '=== Suite Summary ===' Cyan
    Write-LabelValue 'Scenarios run' $results.Count
    Write-LabelValue 'Passed' ($results.Count - $failed.Count) Green
    Write-LabelValue 'Failed' $failed.Count $(if ($failed.Count -eq 0) { 'Green' } else { 'Red' })
    Write-SuiteResultsTable $results
    if ($Details) {
        Write-ColoredLine 'Scenario details:' Cyan
        foreach ($result in $results) {
            Write-ScenarioSummary $result
        }
    }
    elseif ($failed.Count -gt 0) {
        Write-ColoredLine 'Failed scenario details:' Red
        foreach ($result in $failed) {
            Write-ScenarioSummary $result
        }
    }

    if ($failed.Count -gt 0) {
        throw "Filtering CSV test failed in $($failed.Count) scenario(s): $(@($failed | ForEach-Object { $_.Name }) -join ', ')"
    }

    Write-ColoredLine 'Filtering CSV suite passed.' Green
    $suiteSucceeded = $true
}
finally {
    if (!$KeepArtifacts) {
        if (Test-Path -LiteralPath $workRoot) {
            Remove-Item -LiteralPath $workRoot -Recurse -Force
        }
        if ($builtForTest) {
            Remove-TestBuildArtifacts
        }
    }
    elseif (Test-Path -LiteralPath $workRoot) {
        Write-ColoredLine "Kept test artifacts in: $workRoot" Yellow
    }

    if (!$suiteSucceeded) {
        Write-ColoredLine 'Filtering CSV suite failed.' Red
    }
}
