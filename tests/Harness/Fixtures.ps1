function New-TestFile {
    param([string] $Path, [int] $Size = 4096, [int] $Seed = 1, [switch] $Zeros)
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    $bytes = [byte[]]::new($Size)
    if (-not $Zeros) { [Random]::new($Seed).NextBytes($bytes) }
    [IO.File]::WriteAllBytes($Path, $bytes)
    [IO.File]::SetLastWriteTimeUtc($Path, [datetime]::new(2024, 2, 3, 12, 34, 10, [DateTimeKind]::Utc))
    return $Path
}

function New-TestTree {
    param([string] $Root = $script:Context.Fixture)
    $files = [ordered]@{
        'alpha\selected-a.bin' = 65537; 'alpha\selected-b.bin' = 16389
        'alpha\nested\deep.dat' = 8193; 'beta\untouched.txt' = 12289
        'beta\same-name.bin' = 4099; 'alpha\same-name.bin' = 4103
        'dupes\first.wdsdupe' = 32768; 'dupes\second.wdsdupe' = 32768
        'zero-length' = 0; 'with spaces;comma,brackets[1]\日本語-é-ф.txt' = 37
    }
    $index = 1
    foreach ($entry in $files.GetEnumerator()) {
        $seed = if ($entry.Key -like 'dupes\*') { 712 } else { $index++ }
        New-TestFile (Join-Path $Root $entry.Key) $entry.Value $seed | Out-Null
    }
    [IO.Directory]::CreateDirectory((Join-Path $Root 'empty\nested')) | Out-Null
    return $Root
}

function New-StressTree {
    param([int] $Count = $StressFileCount)
    $root = Join-Path $script:Context.Fixture 'stress'
    $payload = [byte[]]::new(65536)
    [Random]::new($Seed).NextBytes($payload)
    for ($index = 0; $index -lt $Count; ++$index) {
        $directory = Join-Path $root ('branch-{0:d3}' -f [int]($index % 64))
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        [BitConverter]::GetBytes($index).CopyTo($payload, 0)
        [IO.File]::WriteAllBytes((Join-Path $directory ('file-{0:d6}.bin' -f $index)), $payload)
    }
    Write-TestNote "Stress corpus: $Count files, $([Math]::Round($Count * $payload.Length / 1MB)) MiB; seed=$Seed"
    return $root
}

function Get-NormalPath {
    param([string] $Path)
    if ($Path.StartsWith('\\?\UNC\', [StringComparison]::OrdinalIgnoreCase)) { $Path = '\\' + $Path.Substring(8) }
    elseif ($Path.StartsWith('\\?\')) { $Path = $Path.Substring(4) }
    return [IO.Path]::GetFullPath($Path).TrimEnd('\').ToLowerInvariant()
}

function Get-FileIdentity {
    param([string] $Path)
    $links = [uint32]0
    $identity = [WdsNativeFs]::GetFileIdentity($Path, [ref]$links)
    if (-not $identity) { throw "Cannot read file identity: $Path" }
    return [pscustomobject]@{ Id = $identity; Links = $links }
}

function Get-DiskSnapshot {
    param([string] $Root)
    $items = [System.Collections.Generic.List[object]]::new()
    $pending = [System.Collections.Generic.Stack[IO.DirectoryInfo]]::new()
    $pending.Push([IO.DirectoryInfo]::new($Root))
    while ($pending.Count) {
        $directory = $pending.Pop()
        foreach ($item in $directory.EnumerateFileSystemInfos()) {
            $isDirectory = $item -is [IO.DirectoryInfo]
            $isLink = ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0
            $allocation = if ($isDirectory -or $isLink) { 0L } else { [WdsNativeFs]::GetAllocationSize($item.FullName) }
            if ($allocation -lt 0) { throw "Cannot inspect allocation: $($item.FullName)" }
            $items.Add([pscustomobject]@{
                Path = Get-NormalPath $item.FullName; Relative = [IO.Path]::GetRelativePath($Root, $item.FullName)
                Directory = $isDirectory; Link = $isLink; Attributes = [uint32]$item.Attributes
                Logical = $(if ($isDirectory -or $isLink) { 0L } else { [long]$item.Length }); Physical = $allocation
                LastChange = $item.LastWriteTimeUtc.ToString('yyyy-MM-ddTHH:mm:ssZ')
            })
            if ($isDirectory -and -not $isLink) { $pending.Push($item) }
        }
    }
    return @($items | Sort-Object Path)
}

function New-TestRunner {
    param([System.Collections.IDictionary] $Settings = @{}, [string[]] $LanguageLines = @())
    $folder = Join-Path $script:Context.Root ('app-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
    [IO.Directory]::CreateDirectory($folder) | Out-Null
    $exe = Join-Path $folder 'WinDirStat.exe'
    Copy-Item -LiteralPath $ExePath -Destination $exe
    $language = Join-Path ([IO.Path]::GetDirectoryName($ExePath)) 'lang_combined.bin'
    if (Test-Path -LiteralPath $language) { Copy-Item -LiteralPath $language -Destination $folder }
    if ($LanguageLines.Count) {
        [IO.File]::WriteAllLines((Join-Path $folder 'lang_en-US.txt'), $LanguageLines, [Text.UTF8Encoding]::new($false))
        if (-not $Settings.Contains('Options')) { $Settings['Options'] = @{} }
        $Settings['Options']['LanguageId'] = 1033
    }
    $runner = [pscustomobject]@{ Exe = $exe; Directory = $folder; Ini = [IO.Path]::ChangeExtension($exe, 'ini') }
    Set-TestSettings $runner $Settings
    return $runner
}

function Set-StorageTestCredential {
    param([string] $Target, [string] $User, [string] $Secret, [string] $Token = '')
    $endpoint = $Target
    if ($Target -match '^(s3://windirstat-[a-z-]+/\?region=us-east-1&|azure://windirstat/[^/?]+/\?)endpoint=') {
        $endpoint = [Uri]::UnescapeDataString(($Target -split 'endpoint=', 2)[1])
    }
    if ($endpoint -notmatch '^http://127\.0\.0\.1:\d+(?:/|$)' -or -not $User -or -not $Secret) {
        throw 'Only loopback fixture credentials can be saved.'
    }
    $name = 'WinDirStat/' + $Target
    Add-Cleanup 'Remove fixture-only storage credential' {
        param($name); & cmdkey.exe "/delete:$name" | Out-Null
    } @($name)
    if ($Token) { $Secret += "`n$Token" }
    [WdsNativeCredentials]::Save($name, $User, $Secret)
}

function Set-TestSettings {
    param([object] $Runner, [System.Collections.IDictionary] $Settings = @{})
    $sections = [ordered]@{
        Options = [ordered]@{
            LanguageId = 9; DarkMode = 0; UseFastScanEngine = 0; ScanningThreads = 4; UseBackupRestore = 0
            ShowElevationPrompt = 0; AutoElevate = 0; ShowFreeSpace = 0; ShowUnknown = 0; ProcessHardlinks = 0
            ShowDeletePermanentlyWarning = 0; ShowCreateHardlinkPrompt = 0; ShowRemoveMotwPrompt = 0
            ShowRemoveEmptyFoldersPrompt = 0; ShowSetDatesPrompt = 0; FontSizePercent = 100; ToolBarSizePercent = 100
            ShowFileTypes = 1; ShowVisualization = 1; GroupUnregisteredTypes = 0
            LayoutTopology = 0; LayoutPermutation = 0; UseSizeSuffixes = 0; UseWindowsLocale = 0
        }
        FileTreeView = [ordered]@{ ColumnVisibility = '1,1,1,1,1,1,1,1,1,1,0' }
        DupeView = [ordered]@{ ScanForDuplicates = 0 }
    }
    foreach ($section in $Settings.Keys) {
        if (-not $sections.Contains($section)) { $sections[$section] = [ordered]@{} }
        foreach ($key in $Settings[$section].Keys) { $sections[$section][$key] = $Settings[$section][$key] }
    }
    $lines = foreach ($section in $sections.GetEnumerator()) {
        "[$($section.Key)]"
        foreach ($entry in $section.Value.GetEnumerator()) { "$($entry.Key)=$($entry.Value)" }
        ''
    }
    [IO.File]::WriteAllText($Runner.Ini, ($lines -join "`r`n") + "`r`n", [Text.Encoding]::Unicode)
}

function Start-TestProcess {
    param([object] $Runner, [string[]] $Arguments = @(), [switch] $Redirect, [switch] $Hidden)
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Runner.Exe
    $info.WorkingDirectory = $Runner.Directory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    if ($Hidden) { $info.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden }
    $info.RedirectStandardOutput = $Redirect.IsPresent
    $info.RedirectStandardError = $Redirect.IsPresent
    if ($Runner.PSObject.Properties['Environment']) {
        foreach ($entry in $Runner.Environment.GetEnumerator()) { $info.Environment[$entry.Key] = $entry.Value }
    }
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($info)
    $script:Context.Commands.Add([pscustomobject]@{
        File = $info.FileName; Arguments = $Arguments; ProcessId = $process.Id
        ExitCode = $null; Stdout = ''; Stderr = ''; Seconds = $null
    })
    $script:Context.Processes.Add($process)
    return $process
}

function Invoke-TestProcess {
    param([object] $Runner, [string[]] $Arguments, [int] $Seconds = $TimeoutSeconds)
    $process = Start-TestProcess $Runner $Arguments -Redirect
    $output = $process.StandardOutput.ReadToEndAsync()
    $errors = $process.StandardError.ReadToEndAsync()
    $watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        while (-not $process.WaitForExit(100)) {
            if ($watch.Elapsed.TotalSeconds -ge $Seconds) {
                $process.Kill($true)
                [void] $process.WaitForExit(5000)
                throw "Process exceeded ${Seconds}s; arguments=$($Arguments -join ' ')"
            }
        }
        $result = [pscustomobject]@{
            ExitCode = $process.ExitCode; Stdout = $output.GetAwaiter().GetResult()
            Stderr = $errors.GetAwaiter().GetResult(); Seconds = $watch.Elapsed.TotalSeconds
        }
        return $result
    } finally {
        $watch.Stop()
        $command = $script:Context.Commands | Where-Object ProcessId -eq $process.Id | Select-Object -Last 1
        $command.Seconds = [Math]::Round($watch.Elapsed.TotalSeconds, 3)
        if ($process.HasExited) {
            $command.ExitCode = $process.ExitCode
            $command.Stdout = $output.GetAwaiter().GetResult()
            $command.Stderr = $errors.GetAwaiter().GetResult()
        }
    }
}

function Invoke-ScanReport {
    param(
        [object] $Runner, [string[]] $Roots, [string] $Name = 'scan',
        [ValidateSet('csv', 'json')] [string] $Format = 'csv',
        [ValidateSet('Scan', 'Duplicates', 'Permissions')] [string] $Kind = 'Scan'
    )
    $path = Join-Path $script:Context.Root "$Name.$Format"
    if (Test-Path -LiteralPath $path) { throw "Report names must be unique within a scenario: $path" }
    $flag = switch ($Kind) { Scan { '/saveto' }; Duplicates { '/savedupesto' }; Permissions { '/savepermsto' } }
    $probe = Invoke-TestProcess $Runner (@($flag, $path) + $Roots)
    Assert-Equal $probe.ExitCode 0 "The $Kind export terminates successfully; stderr=$($probe.Stderr)"
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) "The $Kind export creates $path"
    return Read-ScanReport $path
}

function Read-ScanReport {
    param([string] $Path)
    $rows = if ([IO.Path]::GetExtension($Path) -eq '.json') {
        [IO.File]::ReadAllText($Path) | ConvertFrom-Json
    } else { Import-Csv -LiteralPath $Path -Encoding utf8 }
    return ,@($rows)
}

function Get-ReportFiles {
    param([object[]] $Rows)
    return @($Rows | Where-Object { ([Convert]::ToUInt32($_.'WinDirStat Attributes', 16) -band 8) -ne 0 })
}

function Assert-ScanSnapshot {
    param([object[]] $Rows, [object[]] $Snapshot, [string] $Root, [switch] $Physical)
    Assert-True ($Rows.Count -gt 0) 'A scan report contains its root'
    $rootPath = Get-NormalPath $Root
    $realRows = @($Rows | Where-Object {
        ([Convert]::ToUInt32($_.'WinDirStat Attributes', 16) -band 15) -ne 0
    })
    $paths = @($realRows | ForEach-Object { Get-NormalPath $_.Name } | Sort-Object)
    $expectedPaths = @(@($rootPath) + @($Snapshot.Path) | Sort-Object)
    Assert-Sequence $paths $expectedPaths 'Every physical path is represented exactly once, including empty directories'
    foreach ($row in $realRows) {
        $path = Get-NormalPath $row.Name
        $disk = @($Snapshot | Where-Object Path -eq $path)
        if ($path -eq $rootPath -or ($disk.Count -and $disk[0].Directory)) {
            $descendants = @($Snapshot | Where-Object {
                $_.Path.StartsWith($path + '\', [StringComparison]::OrdinalIgnoreCase)
            })
            $files = @($descendants | Where-Object { -not $_.Directory })
            $folders = @($descendants | Where-Object Directory).Count
            $logical = [long]0
            $allocated = [long]0
            foreach ($file in $files) { $logical += $file.Logical; $allocated += $file.Physical }
            Assert-Equal ([long]$row.Files) ([long]$files.Count) "Recursive file count is correct for $path"
            Assert-Equal ([long]$row.Folders) ([long]$folders) "Recursive folder count is correct for $path"
            Assert-Equal ([long]$row.'Logical Size') $logical "Recursive logical size is correct for $path"
            if ($Physical) {
                Assert-Equal ([long]$row.'Physical Size') $allocated "Recursive allocated size is correct for $path"
            }
            continue
        }
        Assert-Equal ([long]$row.'Logical Size') $disk[0].Logical "Logical bytes match disk for $path"
        if ($Physical -and -not $disk[0].Link) {
            Assert-Equal ([long]$row.'Physical Size') $disk[0].Physical `
                "Allocated bytes match the file handle for $path"
        }
    }
}

function New-TestJunction {
    param([string] $Path, [string] $Target)
    New-Item -ItemType Junction -Path $Path -Target $Target | Out-Null
    Add-Cleanup "Remove junction $Path" {
        param($Link)
        if ([IO.Directory]::Exists($Link)) { [IO.Directory]::Delete($Link, $false) }
    } @($Path)
}

function New-TestShare {
    param([string] $Path)
    $name = 'WdsTest-' + [guid]::NewGuid().ToString('N').Substring(0, 12)
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    try { $account = $identity.Name } finally { $identity.Dispose() }
    New-SmbShare -Name $name -Path $Path -FullAccess $account -ErrorAction Stop | Out-Null
    Add-Cleanup "Remove share $name" { param($Share); Remove-SmbShare -Name $Share -Force -ErrorAction Stop } @($name)
    return "\\localhost\$name"
}

function Start-FixtureMutation {
    param([string] $Root)
    $scriptPath = Join-Path $script:Context.Root 'external-writer.ps1'
    $ready = Join-Path $script:Context.Root 'writer-ready'
    $stop = Join-Path $script:Context.Root 'writer-stop'
    $source = @'
param([string] $Root, [string] $Ready, [string] $Stop)
$ErrorActionPreference = 'Stop'
$folder = Join-Path $Root 'external-races'
[IO.Directory]::CreateDirectory($folder) | Out-Null
$deadline = [datetime]::UtcNow.AddSeconds(90)
$payload = [byte[]]::new(16384)
for ($index = 0; [datetime]::UtcNow -lt $deadline -and -not [IO.File]::Exists($Stop); ++$index) {
    $path = Join-Path $folder ("race-$($index % 32).bin")
    $renamed = Join-Path $folder ("renamed-$($index % 32).bin")
    [IO.File]::WriteAllBytes($path, $payload)
    [IO.File]::Move($path, $renamed, $true)
    if ($index % 2) { [IO.File]::Delete($renamed) }
    if ($index -eq 32) { [IO.File]::WriteAllText($Ready, 'writing') }
    Start-Sleep -Milliseconds 5
}
'@
    [IO.File]::WriteAllText($scriptPath, $source, [Text.UTF8Encoding]::new($false))
    $runner = [pscustomobject]@{ Exe = (Get-Process -Id $PID).Path; Directory = $script:Context.Root }
    $process = Start-TestProcess $runner @('-NoProfile', '-File', $scriptPath, $Root, $ready, $stop) -Redirect
    $worker = [pscustomobject]@{
        Process = $process; Ready = $ready; Stop = $stop
        Stdout = $process.StandardOutput.ReadToEndAsync(); Stderr = $process.StandardError.ReadToEndAsync()
    }
    Add-Cleanup 'Stop external writer' { param($StopPath); [IO.File]::WriteAllText($StopPath, 'stop') } @($stop)
    return $worker
}

function Stop-FixtureMutation {
    param([object] $Worker)
    [IO.File]::WriteAllText($Worker.Stop, 'stop')
    Wait-Until { $Worker.Process.HasExited } 'The external writer stops before filesystem reconciliation' -Seconds 10 |
        Out-Null
    Assert-Equal $Worker.Process.ExitCode 0 `
        "External mutation worker completes; stderr=$($Worker.Stderr.GetAwaiter().GetResult())"
}
