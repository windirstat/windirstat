function Start-ObservedWatcher {
    param([object] $App, [string] $Root)
    Get-AppList $App -AllFiles | Out-Null
    Invoke-AppCommand $App 'ID_TOOLS_WATCHER'
    $list = Get-AppList $App -TabName 'File Watcher'
    $probe = New-TestFile (Join-Path $Root 'watcher-ready.probe') 101
    Wait-WatcherEvent $list $probe 'Created'
    return $list
}

function Wait-WatcherEvent {
    param([IntPtr] $List, [string] $Path, [string] $Action)
    Wait-Until {
        $names = [NativeListViewHelper]::GetItemTexts($List)
        $actions = [NativeListViewHelper]::GetItemTexts($List, 2)
        for ($index = 0; $index -lt [Math]::Min($names.Length, $actions.Length); ++$index) {
            if ($names[$index] -ieq $Path -and $actions[$index] -eq $Action) { return $true }
        }
    } "Watcher records '$Action' for '$Path'" | Out-Null
}

function Test-WatcherTransitions {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root)
    $list = Start-ObservedWatcher $app $root
    $file = New-TestFile (Join-Path $root 'created-by-external-process.txt') 719
    Wait-WatcherEvent $list $file 'Created'
    [IO.File]::AppendAllText($file, 'updated')
    Wait-WatcherEvent $list $file 'Modified'
    $renamed = Join-Path $root 'renamed-externally.txt'
    [IO.File]::Move($file, $renamed)
    Wait-WatcherEvent $list $file 'Renamed (Old)'
    Wait-WatcherEvent $list $renamed 'Renamed (New)'
    [IO.File]::Delete($renamed)
    Wait-WatcherEvent $list $renamed 'Deleted'
    Assert-True $true 'Create, write, paired rename, and delete notifications reach the visible watcher'
    Invoke-AppCommand $app 'ID_WATCHER_CLEAR' -Unchecked
    Wait-Until { [NativeListViewHelper]::GetItemCount($list) -eq 0 } 'Clear removes the observed history' | Out-Null
    $next = New-TestFile (Join-Path $root 'after-clear.txt') 727
    Wait-WatcherEvent $list $next 'Created'
    Assert-True $true 'Clearing history does not stop monitoring or discard the next event'
}

function Test-WatcherPauseFilter {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root) -Width 1600
    $list = Start-ObservedWatcher $app $root
    Invoke-AppCommand $app 'ID_WATCHER_PAUSE' -Unchecked
    Wait-Until { (Get-AppCommandState $app 'ID_WATCHER_START').Enabled } `
        'The watcher stops its pending directory read' |
        Out-Null
    $paused = New-TestFile (Join-Path $root 'while-paused.txt') 811
    Invoke-AppCommand $app 'ID_WATCHER_START' -Unchecked
    Wait-Until { (Get-AppCommandState $app 'ID_WATCHER_PAUSE').Enabled } 'The watcher restarts' | Out-Null
    $first = New-TestFile (Join-Path $root 'include-this.txt') 821
    Wait-WatcherEvent $list $first 'Created'
    $second = New-TestFile (Join-Path $root 'hide-this.txt') 823
    Wait-WatcherEvent $list $second 'Created'
    Assert-True ($paused -notin [NativeListViewHelper]::GetItemTexts($list)) `
        'Restarting does not invent events that occurred while paused'
    $filter = Get-DialogControl $app.Window 'ID_WATCHER_FILTER'
    [TestDesktop]::SetText($filter, 'include-this')
    Wait-Until {
        $names = [NativeListViewHelper]::GetItemTexts($list)
        $first -in $names -and $second -notin $names
    } 'The quick filter selects the expected live history' | Out-Null
    $filtered = @([NativeListViewHelper]::GetItemTexts($list))
    [TestDesktop]::SetText($filter, '[')
    Assert-Sequence @([NativeListViewHelper]::GetItemTexts($list)) $filtered `
        'Invalid regex input preserves the last valid visible history'
    [TestDesktop]::SetText($filter, '')
    Wait-Until { $second -in [NativeListViewHelper]::GetItemTexts($list) } `
        'Clearing the filter restores retained history' |
        Out-Null
    Assert-True $true 'Filtering does not destroy the watcher history'
}

function Test-PauseResumeScan {
    param($Context, $Case)
    $root = New-StressTree
    $app = Start-TestApp @($root) -Settings @{
        Options = @{ UseFastScanEngine = 0; ScanningThreads = 1 }; DupeView = @{ ScanForDuplicates = 1 }
    } -DuringScan
    Wait-AppScanning $app
    Invoke-AppCommand $app 'ID_SCAN_SUSPEND'
    Wait-Until {
        (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled -and [TestDesktop]::Text($app.Window) -like '*(Suspended)*'
    } 'Suspension reaches the UI and worker queues' | Out-Null
    $list = Get-AppList $app -AllFiles
    $state = @{ Last = '' }
    Wait-Until {
        $counts = [NativeListViewHelper]::GetItemTexts($list, 6) -join '|'
        $same = $state.Last -eq $counts
        $state.Last = $counts
        return $same
    } 'In-flight scan updates settle while suspended' -StableMilliseconds 500 | Out-Null
    $before = $state.Last
    Wait-Until { ([NativeListViewHelper]::GetItemTexts($list, 6) -join '|') -eq $before } `
        'Suspended file counts remain unchanged' -StableMilliseconds 700 | Out-Null
    Assert-True $true 'Suspending prevents continued visible enumeration after in-flight work settles'
    Invoke-AppCommand $app 'ID_SCAN_RESUME'
    Wait-AppIdle $app
    $stopDisabled = $true
    try {
        Wait-Until { -not (Get-AppCommandState $app 'ID_SCAN_STOP').Enabled } `
            'Scan completion updates the visible Stop command without additional input' -Seconds 3 | Out-Null
    } catch [System.TimeoutException] {
        $stopDisabled = $false
        Save-Frame ([TestDesktop]::Capture($app.Window)) (Join-Path $Context.Root 'completed-stop-enabled.png')
    }
    $rows = Save-AppReport $app 'resumed'
    Assert-Equal @(Get-ReportFiles $rows).Count $StressFileCount 'Resume completes every file exactly once'
    Assert-Equal ([long]$rows[0].'Logical Size') ([long]$StressFileCount * 65536) `
        'Resume preserves recursive byte totals'
    Assert-GraphRendered $app | Out-Null
    Assert-True $stopDisabled 'Completing the resumed scan disables Stop without an additional user action'
}

function Test-StopAndRescan {
    param($Context, $Case)
    $root = New-StressTree
    $app = Start-TestApp @($root) -Settings @{ Options = @{ ScanningThreads = 1 }; DupeView = @{ ScanForDuplicates = 1 } } `
        -DuringScan
    Wait-AppScanning $app
    Invoke-AppCommand $app 'ID_SCAN_STOP'
    Wait-AppIdle $app
    Assert-True (-not (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled) `
        'A stopped scan cannot accidentally resume old workers'
    $new = New-TestFile (Join-Path $root 'after-stop.txt') 853
    Invoke-AppCommand $app 'ID_REFRESH_ALL'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'rescanned'
    Assert-Equal @(Get-ReportFiles $rows).Count ($StressFileCount + 1) `
        'Rescanning replaces partial data without duplicate rows'
    Assert-Equal @($rows | Where-Object Name -IEQ $new).Count 1 'The new scan observes the external sentinel'
    Assert-GraphRendered $app | Out-Null
}

function Test-ScanResizeOverlap {
    param($Context, $Case)
    $root = New-StressTree
    $app = Start-TestApp @($root) -Settings @{ Options = @{ ScanningThreads = 1 }; DupeView = @{ ScanForDuplicates = 1 } } `
        -DuringScan
    Wait-AppScanning $app
    [void] [TestDesktop]::Message($app.Window, 0x0231)
    Add-Cleanup 'Release resize suspension' {
        param($ResizedApp)
        if ([TestDesktop]::IsWindow($ResizedApp.Window)) { [void] [TestDesktop]::Message($ResizedApp.Window, 0x0232) }
    } @($app)
    $rect = [TestDesktop]::Rectangle($app.Window)
    [TestDesktop]::Position($app.Window, $rect[0], $rect[1], 900, 640)
    Invoke-AppCommand $app 'ID_SCAN_SUSPEND'
    Wait-Until { (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled } `
        'The scan suspends during a live resize' | Out-Null
    [void] [TestDesktop]::Message($app.Window, 0x0232)
    Invoke-AppCommand $app 'ID_SCAN_RESUME'
    Wait-AppIdle $app
    Assert-GraphRendered $app | Out-Null
    $rows = Save-AppReport $app 'resize-overlap'
    Assert-Equal @(Get-ReportFiles $rows).Count $StressFileCount `
        'Overlapping resize and scan suspension releases every worker'
    $baseline = Get-StableFrame (Get-GraphWindow $app)
    [TestDesktop]::Repaint((Get-GraphWindow $app))
    Assert-FrameEqual $baseline (Get-StableFrame (Get-GraphWindow $app)) `
        'Rendering resumes with the current complete model'
}

function Test-WatcherShutdown {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root)
    Start-ObservedWatcher $app $root | Out-Null
    $worker = Start-FixtureMutation $root
    Wait-Until { Test-Path -LiteralPath $worker.Ready } `
        'The external writer is actively mutating the watched root' | Out-Null
    Stop-TestApp $app
    Assert-Equal $app.Process.ExitCode 0 `
        'Shutdown cancels pending directory notifications and joins watcher threads cleanly'
    Stop-FixtureMutation $worker
    $moved = Join-Path $Context.Root 'moved-after-exit'
    [IO.Directory]::Move($root, $moved)
    Assert-True ([IO.Directory]::Exists($moved)) `
        'No surviving app or worker handle prevents the fixture root from moving'
}

function Test-ExternalMutationDuringScan {
    param($Context, $Case)
    $root = New-StressTree -Count $script:ExtendedStressFileCount
    $worker = Start-FixtureMutation $root
    Wait-Until { Test-Path -LiteralPath $worker.Ready } 'External mutations have begun' | Out-Null
    $app = Start-TestApp @($root) -Settings @{ Options = @{ ScanningThreads = 16 }; DupeView = @{ ScanForDuplicates = 1 } } `
        -DuringScan
    Wait-AppScanning $app
    Invoke-AppCommand $app 'ID_SCAN_SUSPEND'
    Wait-Until { (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled } `
        'The concurrently mutated scan suspends' | Out-Null
    Stop-FixtureMutation $worker
    Invoke-AppCommand $app 'ID_SCAN_RESUME'
    Wait-AppIdle $app
    Invoke-AppCommand $app 'ID_REFRESH_ALL'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'after-mutations'
    $expected = @([IO.Directory]::EnumerateFiles($root, '*', [IO.SearchOption]::AllDirectories) |
        ForEach-Object { Get-NormalPath $_ } | Sort-Object)
    Assert-Sequence @(Get-ReportFiles $rows | ForEach-Object { Get-NormalPath $_.Name } | Sort-Object) $expected `
        'After external create/rename/delete races, a settled refresh converges to the actual filesystem'
    Assert-GraphRendered $app | Out-Null
}

function Test-ExtensionGroupingDuringScan {
    param($Context, $Case)
    $root = New-StressTree
    $app = Start-TestApp @($root) -Settings @{
        Options = @{ UseFastScanEngine = 0; ScanningThreads = 1 }; DupeView = @{ ScanForDuplicates = 1 }
    } -DuringScan
    Wait-AppScanning $app
    Invoke-AppCommand $app 'ID_SCAN_SUSPEND'
    Wait-Until { (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled } 'The scan suspends' | Out-Null
    $grouping = Get-AppCommandState $app 'ID_VIEW_GROUP_TYPES'
    Assert-True (-not $grouping.Enabled) 'Extension grouping is unavailable until the scan settles'
    [void] [TestDesktop]::Message($app.Window, 0x0111, (Get-ResourceId 'ID_VIEW_GROUP_TYPES'))
    Assert-Equal (Get-AppCommandState $app 'ID_VIEW_GROUP_TYPES').Checked $grouping.Checked `
        'Direct command dispatch cannot rebuild extension data while workers are active'
    Invoke-AppCommand $app 'ID_SCAN_RESUME'
    Wait-AppIdle $app
    Wait-Until { (Get-AppCommandState $app 'ID_VIEW_GROUP_TYPES').Enabled } `
        'Extension grouping becomes available after worker completion' | Out-Null
    Invoke-AppCommand $app 'ID_VIEW_GROUP_TYPES'
    Assert-Equal (Get-AppCommandState $app 'ID_VIEW_GROUP_TYPES').Checked (-not $grouping.Checked) `
        'Extension grouping works after the scan settles'
    Assert-Equal @(Get-ReportFiles (Save-AppReport $app 'grouped')).Count $StressFileCount `
        'Regrouping preserves the completed scan model'
}

Register-Scenario lifecycle.extension-grouping Lifecycle 'Extension regrouping waits for scan worker completion' `
    Test-ExtensionGroupingDuringScan -Tags Desktop,Race -Requires Windows,Desktop
Register-Scenario lifecycle.watcher-events Lifecycle `
    'External create/write/rename/delete notifications and history clearing' `
    Test-WatcherTransitions -Tags Desktop,External,Race -Requires Windows,Desktop
Register-Scenario lifecycle.watcher-filter Lifecycle 'Watcher pause/restart and invalid live filters preserve history' `
    Test-WatcherPauseFilter -Tags Desktop,External,Race -Requires Windows,Desktop
Register-Scenario lifecycle.pause-resume Lifecycle `
    'Suspended enumeration stops updating and resumes without omissions' `
    Test-PauseResumeScan -Tags Desktop,Race -Requires Windows,Desktop
Register-Scenario lifecycle.stop-rescan Lifecycle 'Cancellation followed by a clean rescan replaces partial models' `
    Test-StopAndRescan -Tags Desktop,Race -Requires Windows,Desktop
Register-Scenario lifecycle.resize-overlap Lifecycle `
    'Live resize and scan suspension overlap without stranding rendering' `
    Test-ScanResizeOverlap -Tags Desktop,Race,Rendering -Requires Windows,Desktop
Register-Scenario lifecycle.watcher-shutdown Lifecycle `
    'Shutdown during notification traffic cancels I/O and releases handles' `
    Test-WatcherShutdown -Tags Desktop,External,Race -Requires Windows,Desktop
Register-Scenario lifecycle.mutation-stress Lifecycle `
    'Parallel enumeration and external create/rename/delete traffic converge' `
    Test-ExternalMutationDuringScan -Tags Desktop,External,Race,Extended -Requires Windows,Desktop
