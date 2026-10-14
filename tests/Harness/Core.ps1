$script:Scenarios = [System.Collections.Generic.List[object]]::new()
$script:Results = [System.Collections.Generic.List[object]]::new()
$script:Context = $null

function Register-Scenario {
    param(
        [string] $Id, [string] $Suite, [string] $Name, [string] $Body,
        [string[]] $Tags = @(), [string[]] $Requires = @('Windows'), [hashtable] $Data = @{}
    )
    if (@($script:Scenarios | Where-Object Id -eq $Id).Count) { throw "Duplicate scenario ID: $Id" }
    $script:Scenarios.Add([pscustomobject]@{
        Id = $Id; Suite = $Suite; Name = $Name; Body = $Body; Tags = $Tags; Requires = $Requires; Data = $Data
    })
}

function Assert-True {
    param([bool] $Condition, [string] $Because)
    $script:Context.Checks.Add($Because)
    if (-not $Condition) { throw [System.InvalidOperationException]::new($Because) }
}

function Assert-Equal {
    param([AllowNull()] $Actual, [AllowNull()] $Expected, [string] $Because)
    Assert-True ($Actual -ceq $Expected) "$Because (expected '$Expected', observed '$Actual')"
}

function Assert-Sequence {
    param([AllowEmptyCollection()] [object[]] $Actual, [AllowEmptyCollection()] [object[]] $Expected, [string] $Because)
    $actualText = ConvertTo-Json -InputObject @($Actual) -Compress -Depth 8
    $expectedText = ConvertTo-Json -InputObject @($Expected) -Compress -Depth 8
    if ($actualText -ceq $expectedText) { Assert-True $true $Because; return }
    $actualText = $actualText.Substring(0, [Math]::Min(1000, $actualText.Length))
    $expectedText = $expectedText.Substring(0, [Math]::Min(1000, $expectedText.Length))
    Assert-True $false "$Because (expected $expectedText, observed $actualText)"
}

function Skip-Scenario {
    param([string] $Reason)
    if ($script:Context.Checks.Count) { throw "A scenario cannot skip after making assertions: $Reason" }
    throw [System.OperationCanceledException]::new($Reason)
}

function Add-Cleanup {
    param([string] $Name, [scriptblock] $Body, [object[]] $Arguments = @())
    $script:Context.Cleanup.Add([pscustomobject]@{ Name = $Name; Body = $Body; Arguments = $Arguments })
}

function Write-TestNote {
    param([string] $Message)
    $script:Context.Notes.Add($Message)
    if ($Details) { Write-Host "       $Message" -ForegroundColor DarkGray }
}

function Wait-Until {
    param(
        [scriptblock] $Probe, [string] $Because, [int] $Seconds = $TimeoutSeconds,
        [int] $StableMilliseconds = 0
    )
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    $readyAt = $null
    while ($watch.Elapsed.TotalSeconds -lt $Seconds) {
        foreach ($app in $script:Context.Apps) {
            if (-not $app.ExpectExit -and $app.Process.HasExited) {
                throw "Application exited while waiting for $Because; exit=$($app.Process.ExitCode)"
            }
        }
        $value = & $Probe
        if ($value) {
            if ($null -eq $readyAt) { $readyAt = $watch.ElapsedMilliseconds }
            if ($watch.ElapsedMilliseconds - $readyAt -ge $StableMilliseconds) { return $value }
        } else { $readyAt = $null }
        Start-Sleep -Milliseconds 75
    }
    throw [System.TimeoutException]::new("Timed out after ${Seconds}s: $Because.")
}

function Assert-OwnedPath {
    param([string] $Path)
    $absolute = [IO.Path]::GetFullPath($Path)
    $prefix = [IO.Path]::GetFullPath($RunRoot).TrimEnd('\') + '\'
    if (-not $absolute.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside this test run: $absolute"
    }
    return $absolute
}

function Remove-OwnedTree {
    param([string] $Path)
    $absolute = Assert-OwnedPath $Path
    if (-not [IO.Directory]::Exists($absolute)) { return }
    # Remove links themselves before recursion; fixtures can point outside their scan root.
    foreach ($entry in [IO.DirectoryInfo]::new($absolute).EnumerateFileSystemInfos()) {
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            $entry.Delete()
        } elseif ($entry -is [IO.DirectoryInfo]) {
            Remove-OwnedTree $entry.FullName
        } else {
            $entry.Attributes = [IO.FileAttributes]::Normal
            $entry.Delete()
        }
    }
    [IO.Directory]::Delete($absolute)
}

function Get-CapabilityReason {
    param([string] $Capability)
    switch ($Capability) {
        Windows { if (-not $IsWindows) { return 'Windows is required.' } }
        Desktop {
            if (-not [Environment]::UserInteractive -or -not [TestDesktop]::HasInputDesktop()) {
                return 'An unlocked interactive Windows desktop is required.'
            }
            if ([Threading.Thread]::CurrentThread.ApartmentState -ne [Threading.ApartmentState]::STA) {
                return 'Run through Run-Tests.cmd or start PowerShell with -STA for desktop scenarios.'
            }
            if ([Environment]::Is64BitProcess -ne $script:BinaryIs64Bit) {
                return 'PowerShell and WinDirStat must have matching bitness for native virtual-list access.'
            }
        }
        Ntfs {
            if ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($RunRoot)).DriveFormat -ne 'NTFS') {
                return 'The fixture volume must be NTFS.'
            }
        }
        Compression {
            if (-not ([WdsNativeFs]::GetVolumeFlags([IO.Path]::GetPathRoot($RunRoot)) -band 0x10)) {
                return 'The fixture volume does not support NTFS compression.'
            }
        }
        Elevated {
            $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
            try {
                $elevated = [Security.Principal.WindowsPrincipal]::new($identity).IsInRole(
                    [Security.Principal.WindowsBuiltInRole]::Administrator)
            }
            finally { $identity.Dispose() }
            if (-not $elevated) {
                return 'Run an elevated PowerShell session for disposable storage/share scenarios.'
            }
        }
        Mtp { if (-not $MtpDevice) { return 'Set -MtpDevice to the exact connected device label in Select Target.' } }
        S3 { if (-not $env:WDS_S3_TEST_ENDPOINT) { return 'Run Test-S3.ps1 for the disposable local S3 fixture.' } }
        Remote {
            if (-not $env:WDS_REMOTE_TEST_ENDPOINT) {
                return 'Run Test-Remote.ps1 for local WebDAV and Azure fixtures.'
            }
        }
        default { throw "Unknown capability: $Capability" }
    }
    return ''
}

function Save-FailureEvidence {
    param([object] $Context, [System.Management.Automation.ErrorRecord] $Failure)
    [IO.File]::WriteAllText((Join-Path $Context.Root 'failure.txt'), $Failure.ToString() + "`r`n" +
        $Failure.ScriptStackTrace + "`r`n" + $Failure.InvocationInfo.PositionMessage)
    foreach ($app in $Context.Apps) {
        if ($app.Process.HasExited -or $app.Window -eq [IntPtr]::Zero) { continue }
        try {
            $windows = foreach ($window in [TestDesktop]::Windows($app.Process.Id)) { [TestDesktop]::Describe($window) }
            $windows | ConvertTo-Json -Depth 8 |
                Set-Content -LiteralPath (Join-Path $Context.Root "windows-$($app.Process.Id).json") -Encoding utf8
            $lists = foreach ($list in [NativeListViewHelper]::GetVisibleListViewsIncludingEmpty($app.Window)) {
                [pscustomobject]@{
                    Handle = $list.ToInt64(); Count = [NativeListViewHelper]::GetItemCount($list)
                    Selected = [NativeListViewHelper]::GetSelectedCount($list)
                    Names = [NativeListViewHelper]::GetItemTexts($list, 0, 128)
                    Values = [NativeListViewHelper]::GetItemTexts($list, 2, 128)
                }
            }
            ConvertTo-Json -InputObject @($lists) -Depth 8 |
                Set-Content -LiteralPath (Join-Path $Context.Root "lists-$($app.Process.Id).json") -Encoding utf8
        } catch { $Context.Notes.Add("Window evidence unavailable: $($_.Exception.Message)") }
        foreach ($window in [TestDesktop]::Windows($app.Process.Id)) {
            if (-not [TestDesktop]::IsWindowVisible($window)) { continue }
            try {
                Save-Frame ([TestDesktop]::Capture($window)) (Join-Path $Context.Root "window-$($window.ToInt64()).png")
            } catch { $Context.Notes.Add("Screenshot unavailable for ${window}: $($_.Exception.Message)") }
        }
    }
}

function Invoke-TestScenario {
    param([object] $Scenario, [int] $Iteration)
    $root = Join-Path $RunRoot "$($Scenario.Id)-$Iteration"
    [IO.Directory]::CreateDirectory($root) | Out-Null
    $script:Context = [pscustomobject]@{
        Root = $root; Fixture = Join-Path $root 'fixture'; Scenario = $Scenario; Iteration = $Iteration
        Apps = [System.Collections.Generic.List[object]]::new()
        Processes = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
        Cleanup = [System.Collections.Generic.List[object]]::new()
        Checks = [System.Collections.Generic.List[string]]::new()
        Notes = [System.Collections.Generic.List[string]]::new()
        Commands = [System.Collections.Generic.List[object]]::new()
    }
    $context = $script:Context
    $status = 'PASS'
    $message = ''
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $uiMutex = $null
    $uiOwned = $false
    $previousDpi = [IntPtr]::Zero
    $previousForeground = [IntPtr]::Zero
    $previousCursor = $null
    Write-Host ("[RUN ] {0}  {1}" -f $Scenario.Id, $Scenario.Name) -ForegroundColor Cyan
    try {
        foreach ($requirement in $Scenario.Requires) {
            $reason = Get-CapabilityReason $requirement
            if ($reason) { Skip-Scenario $reason }
        }
        if ('Desktop' -in $Scenario.Requires) {
            $uiMutex = [Threading.Mutex]::new($false, 'Global\WinDirStat-E2E-UI-Automation')
            try { $uiOwned = $uiMutex.WaitOne(0) }
            catch [Threading.AbandonedMutexException] { $uiOwned = $true }
            if (-not $uiOwned) { throw 'Another WinDirStat test run owns the desktop. Run UI scenarios sequentially.' }
            $previousDpi = [TestDesktop]::DpiContext([IntPtr](-4))
            $previousForeground = [TestDesktop]::GetForegroundWindow()
            $previousCursor = [TestDesktop]::CursorPosition()
        }
        [IO.Directory]::CreateDirectory($context.Fixture) | Out-Null
        & $Scenario.Body $context $Scenario.Data
        if (-not $context.Checks.Count) { throw 'Scenario completed without asserting an observable outcome.' }
    } catch [System.OperationCanceledException] {
        $status = 'SKIP'
        $message = $_.Exception.Message
    } catch {
        $status = 'FAIL'
        $message = $_.Exception.Message
        try { Save-FailureEvidence $context $_ }
        catch { $context.Notes.Add("Failure evidence unavailable: $($_.Exception.Message)") }
    } finally {
        foreach ($app in $context.Apps) {
            try { Stop-TestApp $app }
            catch { $status = 'FAIL'; $message += " Cleanup: $($_.Exception.Message)" }
        }
        for ($index = $context.Cleanup.Count - 1; $index -ge 0; --$index) {
            try {
                $cleanup = $context.Cleanup[$index]
                $cleanupArguments = $cleanup.Arguments
                & $cleanup.Body @cleanupArguments
            }
            catch { $status = 'FAIL'; $message += " $($context.Cleanup[$index].Name): $($_.Exception.Message)" }
        }
        foreach ($process in $context.Processes) {
            try {
                if (-not $process.HasExited) { $process.Kill($true); [void] $process.WaitForExit(5000) }
                $command = $context.Commands | Where-Object ProcessId -eq $process.Id | Select-Object -Last 1
                if ($command -and $process.HasExited -and $null -eq $command.ExitCode) {
                    $command.ExitCode = $process.ExitCode
                    $command.Seconds = [Math]::Round(($process.ExitTime - $process.StartTime).TotalSeconds, 3)
                }
            } catch { $status = 'FAIL'; $message += " Process cleanup: $($_.Exception.Message)" }
            finally { $process.Dispose() }
        }
        if ($previousCursor) { [TestDesktop]::MoveCursor($previousCursor[0], $previousCursor[1]) }
        if ([TestDesktop]::IsWindow($previousForeground)) {
            [void] [TestDesktop]::SetForegroundWindow($previousForeground)
        }
        if ($previousDpi -ne [IntPtr]::Zero) { [void] [TestDesktop]::DpiContext($previousDpi) }
        if ($uiOwned) { $uiMutex.ReleaseMutex() }
        if ($uiMutex) { $uiMutex.Dispose() }
    }
    $watch.Stop()
    $result = [pscustomobject]@{
        Id = $Scenario.Id; Suite = $Scenario.Suite; Name = $Scenario.Name; Iteration = $Iteration
        Tags = $Scenario.Tags; Status = $status; Checks = $context.Checks.Count
        Seconds = [Math]::Round($watch.Elapsed.TotalSeconds, 3); Message = $message
        Artifacts = ''; Notes = @($context.Notes); Commands = @($context.Commands)
    }
    if ($status -ne 'FAIL' -and -not $KeepArtifacts) {
        try { Remove-OwnedTree $root }
        catch {
            $result.Status = 'FAIL'; $result.Message = "Fixture cleanup failed: $($_.Exception.Message)"
        }
    }
    if ($result.Status -eq 'FAIL' -or $KeepArtifacts) {
        $result.Artifacts = $root
        $evidence = @{ Result = $result; Assertions = @($context.Checks); Seed = $Seed }
        $evidence | ConvertTo-Json -Depth 12 |
            Set-Content -LiteralPath (Join-Path $root 'scenario.json') -Encoding utf8
    }
    $script:Results.Add($result)
    $color = switch ($result.Status) { PASS { 'Green' }; FAIL { 'Red' }; SKIP { 'Yellow' } }
    Write-Host ("[{0}] {1}  {2:n1}s  {3} checks" -f $result.Status, $result.Id, $result.Seconds,
        $result.Checks) -ForegroundColor $color
    if ($result.Message) { Write-Host "       $($result.Message)" -ForegroundColor $color }
    if ($result.Artifacts) { Write-Host "       Artifacts: $($result.Artifacts)" -ForegroundColor DarkGray }
    $script:Context = $null
}

function Write-TestReports {
    param([double] $Seconds)
    [IO.Directory]::CreateDirectory($OutputPath) | Out-Null
    $passed = @($script:Results | Where-Object Status -eq PASS).Count
    $failed = @($script:Results | Where-Object Status -eq FAIL).Count
    $skipped = @($script:Results | Where-Object Status -eq SKIP).Count
    $report = [ordered]@{
        SchemaVersion = 1; Executable = $ExePath; Sha256 = (Get-FileHash -LiteralPath $ExePath).Hash
        Build = $script:ExecutableInfo
        Profile = $Profile; Seed = $Seed; StartedUtc = $script:StartedUtc; Seconds = $Seconds
        Passed = $passed; Failed = $failed; Skipped = $skipped; Results = @($script:Results)
    }
    $report | ConvertTo-Json -Depth 15 |
        Set-Content -LiteralPath (Join-Path $OutputPath 'results.json') -Encoding utf8
    $settings = [Xml.XmlWriterSettings]::new()
    $settings.Indent = $true
    $writer = [Xml.XmlWriter]::Create((Join-Path $OutputPath 'junit.xml'), $settings)
    $invariant = [Globalization.CultureInfo]::InvariantCulture
    try {
        $writer.WriteStartDocument()
        $writer.WriteStartElement('testsuite')
        foreach ($pair in @{ name = 'WinDirStat'; tests = $script:Results.Count; failures = $failed;
            skipped = $skipped; time = $Seconds }.GetEnumerator()) {
            $writer.WriteAttributeString($pair.Key, [Convert]::ToString($pair.Value, $invariant))
        }
        foreach ($result in $script:Results) {
            $writer.WriteStartElement('testcase')
            $writer.WriteAttributeString('classname', $result.Suite)
            $writer.WriteAttributeString('name', "$($result.Id)[$($result.Iteration)]")
            $writer.WriteAttributeString('time', $result.Seconds.ToString($invariant))
            if ($result.Status -ne 'PASS') {
                $writer.WriteStartElement($(if ($result.Status -eq 'FAIL') { 'failure' } else { 'skipped' }))
                $writer.WriteAttributeString('message', $result.Message)
                $writer.WriteString($result.Artifacts)
                $writer.WriteEndElement()
            }
            $writer.WriteEndElement()
        }
        $writer.WriteEndElement()
        $writer.WriteEndDocument()
    } finally { $writer.Dispose() }
    Write-Host "`nPassed: $passed  Failed: $failed  Skipped: $skipped  Elapsed: $([Math]::Round($Seconds, 1))s"
    Write-Host "Reports: $OutputPath"
    if ($failed) { return 1 }
    if (-not $passed) { return 2 }
    if ($RequireAll -and $skipped) { return 3 }
    return 0
}
