function Test-ScopedRefresh {
    param($Context, $Case)
    $root = New-TestTree
    $before = @(Get-DiskSnapshot $root)
    $app = Start-TestApp @($root)
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('alpha')
    Remove-Item -LiteralPath (Join-Path $root 'alpha\selected-a.bin')
    New-TestFile (Join-Path $root 'alpha\external-selected.wdsnew') 9001 | Out-Null
    $outside = New-TestFile (Join-Path $root 'beta\external-unselected.wdsnew') 9011
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'selected-refresh'
    $prefix = (Get-NormalPath (Join-Path $root 'alpha')) + '\'
    $snapshot = @(@($before | Where-Object { -not $_.Path.StartsWith($prefix) }) +
        @(Get-DiskSnapshot $root | Where-Object { $_.Path.StartsWith($prefix) }))
    Assert-ScanSnapshot $rows $snapshot $root
    Assert-Equal @($rows | Where-Object Name -IEQ $outside).Count 0 `
        'Refresh Selected preserves the unselected branch snapshot'
    Invoke-AppCommand $app 'ID_REFRESH_ALL'
    Wait-AppIdle $app
    Assert-ScanSnapshot (Save-AppReport $app 'whole-refresh') (Get-DiskSnapshot $root) $root -Physical
}

function Test-RefreshTimestamps {
    param($Context, $Case)
    $folder = Join-Path $Context.Fixture 'changed'
    $paths = @(foreach ($index in 0..3) {
        $path = New-TestFile (Join-Path $folder "file-$index.bin") (16384 -shr $index)
        [IO.File]::SetLastWriteTimeUtc($path, [datetime]::new(2030 - $index, 1, 1, 0, 0, 0, [DateTimeKind]::Utc))
        $path
    })
    $app = Start-TestApp @($Context.Fixture)
    $list = Get-AppList $app -AllFiles
    Expand-AppDirectory $app $list 'changed' 'file-0.bin'
    Select-AppRows $app $list @($paths[0], $paths[1], $paths[2])
    [IO.File]::Delete($paths[0])
    [IO.File]::Delete($paths[1])
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'refreshed-timestamps'
    Assert-Sequence @((Get-ReportFiles $rows).Name | Sort-Object) @($paths[2], $paths[3] | Sort-Object) `
        'Refreshing deleted siblings preserves only the surviving files'
    foreach ($path in $Context.Fixture, $folder) {
        Assert-Equal @($rows | Where-Object Name -eq $path)[0].'Last Change' '2028-01-01T00:00:00Z' `
            'Removing consecutive newest children updates the folder and ancestor timestamps'
    }
}

function Test-RemovedRootRefresh {
    param($Context, $Case)
    $root = New-TestTree
    $second = New-TestTree (Join-Path $Context.Root 'second-root')
    $app = Start-TestApp @($root, $second)
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @($second)
    Remove-OwnedTree $second
    $sentinel = New-TestFile (Join-Path $root 'after-second-root-disappeared.txt') 10103
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'removed-root'
    Assert-Equal @($rows | Where-Object { $_.Name -like "$second*" }).Count 0 `
        'A missing direct root is removed without dereferencing stale selections'
    Assert-Equal @($rows | Where-Object Name -IEQ $sentinel).Count 0 `
        'Refreshing the removed root does not silently rescan surviving roots'
    Invoke-AppCommand $app 'ID_REFRESH_ALL'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'surviving-root'
    Assert-Equal @($rows | Where-Object Name -IEQ $sentinel).Count 1 'Refresh All still reaches the surviving root'
    Assert-GraphRendered $app | Out-Null
}

function Get-ValidationDialog {
    param([object] $App, [IntPtr] $Origin)
    return Wait-Until {
        @([TestDesktop]::Windows($App.Process.Id) | Where-Object {
            $_ -ne $Origin -and [TestDesktop]::Class($_) -eq '#32770' -and [TestDesktop]::IsWindowVisible($_)
        } | Select-Object -First 1)
    } 'Invalid input produces an owned validation dialog' -Seconds 10
}

function Test-SettingsRoundTrip {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root) -Settings @{
        TreeMapView = @{ TreeMapHeightFactor = 38 }
        Options = @{ ScanningThreads = 16; FileHashAlgorithm = 0 }
    }
    $settings = Open-AppDialog $app 'ID_CONFIGURE'
    Select-SettingsPage $settings 'Graphs'
    $height = Get-DialogControl $settings 'IDC_HEIGHT'
    $label = Get-DialogControl $settings 'IDC_STATICHEIGHT'
    Assert-Equal ([TestDesktop]::Text($label)) '38' 'The height label matches the active treemap factor'
    [void] [TestDesktop]::Message($height, 0x0405, 1, 159)
    [void] [NativeListViewHelper]::FocusWindow($height)
    Invoke-AppKeys $app '{RIGHT}' -Window $settings
    Wait-Until { [TestDesktop]::Text($label) -eq '160' } 'The slider label follows its actual value' | Out-Null
    Select-SettingsPage $settings 'Filtering'
    $filter = "first`r`n`r`nthird"
    [TestDesktop]::SetText((Get-DialogControl $settings 'IDC_FILTERING_EXCLUDE_FILES'), $filter)
    Select-SettingsPage $settings 'Advanced'
    $threads = Get-DialogControl $settings 'IDC_COMBO_THREADS'
    $hash = Get-DialogControl $settings 'IDC_HASH_ALGORITHM'
    Assert-Equal ([TestDesktop]::Message($threads, 0x0146).ToInt32()) 16 'All scanning thread choices are populated'
    Assert-Equal ([TestDesktop]::Message($hash, 0x0146).ToInt32()) 6 'All hash algorithms are populated'
    Assert-Equal ([TestDesktop]::ControlText($threads)) '16' 'The upper thread limit loads its matching choice'
    Assert-Equal ([TestDesktop]::ControlText($hash)) 'MD5' 'The persisted hash index loads its matching algorithm'
    $algorithms = @('MD5', 'SHA1', 'SHA256', 'SHA384', 'SHA512', 'xxHash')
    for ($index = 0; $index -lt $algorithms.Count; ++$index) {
        [void][TestDesktop]::Message($hash, 0x014E, $index)
        Assert-Equal ([TestDesktop]::ControlText($hash)) $algorithms[$index] 'Hash choices retain their setting indices'
    }
    [void][NativeListViewHelper]::FocusWindow($threads)
    Invoke-AppKeys $app '{HOME}' -Window $settings
    Close-AppDialog $settings 1
    Stop-TestApp $app
    $app = Start-TestApp @($root) -Runner $app.Runner
    $settings = Open-AppDialog $app 'ID_CONFIGURE'
    Select-SettingsPage $settings 'Graphs'
    Assert-Equal ([TestDesktop]::Text((Get-DialogControl $settings 'IDC_STATICHEIGHT'))) '160' `
        'Height values above 100 survive a fresh process with matching display values'
    Assert-Equal ([TestDesktop]::Message((Get-DialogControl $settings 'IDC_HEIGHT'), 0x0400).ToInt32()) 160 `
        'The loaded slider position matches the applied height factor'
    Select-SettingsPage $settings 'Filtering'
    Assert-Equal ([TestDesktop]::ControlText((Get-DialogControl $settings 'IDC_FILTERING_EXCLUDE_FILES'))) $filter `
        'Saving and restarting preserves blank lines in filter text'
    Select-SettingsPage $settings 'Advanced'
    Assert-Equal ([TestDesktop]::ControlText((Get-DialogControl $settings 'IDC_COMBO_THREADS'))) '1' `
        'The first scanning thread choice persists as one thread'
    Assert-Equal ([TestDesktop]::ControlText((Get-DialogControl $settings 'IDC_HASH_ALGORITHM'))) 'xxHash' `
        'The final hash algorithm survives a fresh process'
    Close-AppDialog $settings
}

function Test-SettingsTransaction {
    param($Context, $Case)
    $root = $Context.Fixture
    $first = New-TestFile (Join-Path $root 'timestamp-first.txt') 103
    $second = New-TestFile (Join-Path $root 'timestamp-second.txt') 107
    [IO.File]::SetLastWriteTimeUtc($second, [IO.File]::GetLastWriteTimeUtc($first).AddSeconds(25))
    $app = Start-TestApp @($root) -Settings @{ Options = @{ ShowTimeSeconds = 0 } }
    $list = Get-AppList $app -AllFiles
    $indexOne = Find-AppRow $list 'timestamp-first.txt'
    $indexTwo = Find-AppRow $list 'timestamp-second.txt'
    $times = [NativeListViewHelper]::GetItemTexts($list, 8)
    Assert-Equal $times[$indexOne] $times[$indexTwo] 'The initial timestamp format omits seconds'
    $unscanned = New-TestFile (Join-Path $root 'must-not-be-rescanned.txt') 109
    $settings = Open-AppDialog $app 'ID_CONFIGURE'
    Select-SettingsPage $settings 'General'
    Set-DialogCheck $settings 'IDC_SHOW_TIME_SECONDS' $true
    Select-SettingsPage $settings 'Advanced'
    [TestDesktop]::SetText((Get-DialogControl $settings 'IDC_LARGEST_FILE_COUNT'), '10001')
    [void] [TestDesktop]::PostMessage([TestDesktop]::GetDlgItem($settings, 1), 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    $errorDialog = Get-ValidationDialog $app $settings
    $unchanged = [NativeListViewHelper]::GetItemTexts($list, 8)
    Assert-Equal $unchanged[$indexOne] $times[$indexOne] `
        'Validation on another page does not partially apply display changes'
    Assert-True ([IO.File]::ReadAllText($app.Runner.Ini) -match '(?m)^ShowTimeSeconds=0\r?$') `
        'Failed settings validation leaves portable settings unchanged'
    Close-AppDialog $errorDialog -ButtonName 'OK'
    [TestDesktop]::SetText((Get-DialogControl $settings 'IDC_LARGEST_FILE_COUNT'), '50')
    Close-AppDialog $settings 1
    Wait-AppIdle $app
    $withSeconds = [NativeListViewHelper]::GetItemTexts($list, 8)
    Assert-True ($withSeconds[$indexOne] -ne $withSeconds[$indexTwo]) `
        'A valid transaction updates the existing timestamp rows'
    Assert-Equal (Find-AppRow $list 'must-not-be-rescanned.txt' -Optional) `
        -1 'A display-only change does not rescan the filesystem'
    Assert-True ([IO.File]::ReadAllText($app.Runner.Ini) -match '(?m)^ShowTimeSeconds=1\r?$') `
        'A valid transaction persists its display choice'
    Stop-TestApp $app
    $again = Start-TestApp @($root) -Runner $app.Runner
    $dialog = Open-AppDialog $again 'ID_CONFIGURE'
    Select-SettingsPage $dialog 'General'
    Assert-Equal ([TestDesktop]::Message((Get-DialogControl $dialog 'IDC_SHOW_TIME_SECONDS'), 0x00F0).ToInt32()) 1 `
        'The choice survives a normal app shutdown and relaunch'
    Close-AppDialog $dialog
}

function Test-SearchRecovery {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root)
    Get-AppList $app -AllFiles | Out-Null
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_TERM'), 'selected-[ab]\.bin$')
    Set-DialogCheck $dialog 'IDC_SEARCH_REGEX' $true
    Set-DialogCheck $dialog 'IDC_SEARCH_FOLDERS' $false
    Close-AppDialog $dialog 1
    $list = Get-AppList $app -TabName 'Search Results'
    $expected = @((Join-Path $root 'alpha\selected-a.bin'), (Join-Path $root 'alpha\selected-b.bin') | Sort-Object)
    Wait-Until { @($expected | Where-Object { $_ -notin [NativeListViewHelper]::GetItemTexts($list) }).Count -eq 0 } `
        'The search exposes both matching files beneath its summary row' | Out-Null
    $valid = @([NativeListViewHelper]::GetItemTexts($list))
    Assert-Sequence @($valid | Where-Object { [IO.Path]::IsPathFullyQualified($_) } | Sort-Object) $expected `
        'Regex search observes exactly the matching files in the existing scanned model'
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [void] [NativeListViewHelper]::FocusWindow((Get-DialogControl $dialog 'IDC_SEARCH_TERM'))
    Invoke-AppKeys $app '^a[' -Window $dialog
    Wait-Until { -not [TestDesktop]::IsWindowEnabled([TestDesktop]::GetDlgItem($dialog, 1)) } `
        'Invalid regex input prevents submission' | Out-Null
    Assert-Sequence @([NativeListViewHelper]::GetItemTexts($list)) $valid `
        'Typing invalid criteria preserves the visible results'
    Close-AppDialog $dialog
    Assert-Sequence @([NativeListViewHelper]::GetItemTexts($list)) $valid `
        'Invalid regex input preserves valid results after cancelling'
    Assert-True ([NativeListViewHelper]::FocusListView($list)) `
        'Search results remain keyboard-focusable after validation recovery'
}

function Test-WatcherRefresh {
    param($Context, $Case)
    $root = New-TestTree
    $roots = @($root)
    if ($Case.MultiRoot) {
        $root = New-TestTree (Join-Path $Context.Root 'fixture-sibling')
        $roots += $root
    }
    $app = Start-TestApp $roots
    $list = Start-ObservedWatcher $app $root
    $sentinel = New-TestFile (Join-Path $root 'beta\unrefreshed.txt') 313
    $deleted = Join-Path $root 'alpha\selected-a.bin'
    [IO.File]::Delete($deleted)
    Wait-WatcherEvent $list $deleted 'Deleted'
    Select-AppRows $app $list @($deleted)
    Invoke-AppKeys $app '{F5}'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'watcher-deleted'
    Assert-Equal @($rows | Where-Object Name -IEQ $deleted).Count 0 'Watcher refresh removes a deleted scanned item'
    Assert-Equal @($rows | Where-Object Name -IEQ $sentinel).Count 0 'Watcher refresh preserves unrelated branches'
    Assert-True ($deleted -in [NativeListViewHelper]::GetItemTexts($list)) 'Refreshing preserves watcher history'

    # A repeated deleted entry and a new directory must resolve to an existing scanned ancestor.
    $created = New-TestFile (Join-Path $root 'alpha\new-directory\created.txt') 317
    Wait-WatcherEvent $list $created 'Created'
    Select-AppRows $app $list @($deleted)
    Invoke-AppKeys $app '{F5}'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'watcher-missing-ancestor'
    Assert-Equal @($rows | Where-Object Name -IEQ $created).Count 1 `
        'Missing watcher paths refresh their scanned ancestor'
    Assert-Equal @($rows | Where-Object Name -IEQ $sentinel).Count 0 'Ancestor refresh stays within its scanned branch'

    $modified = Join-Path $root 'alpha\selected-b.bin'
    New-TestFile $modified 32003 | Out-Null
    Wait-WatcherEvent $list $modified 'Modified'
    $names = [NativeListViewHelper]::GetItemTexts($list)
    $actions = [NativeListViewHelper]::GetItemTexts($list, 2)
    $index = @(for ($i = 0; $i -lt $names.Length; ++$i) {
        if ($names[$i] -ieq $modified -and $actions[$i] -eq 'Modified') { $i }
    })[0]
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]@($index))) 'A modified watcher snapshot is selected'
    Invoke-AppKeys $app '{F5}'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'watcher-modified'
    Assert-Equal ([long]@($rows | Where-Object Name -IEQ $modified)[0].'Logical Size') 32003 `
        'Refreshing a watcher snapshot updates the scanned item size'
    Assert-Equal @($rows | Where-Object Name -IEQ $sentinel).Count 0 `
        'Modified watcher refresh preserves sibling branches'
}

function Test-SearchRefresh {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root)
    Get-AppList $app -AllFiles | Out-Null
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    $term = 'selected-[ab]\.bin$|^alpha$'
    if ($Case.Root) { $term += '|fixture$' }
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_TERM'), $term)
    Set-DialogCheck $dialog 'IDC_SEARCH_REGEX' $true
    Set-DialogCheck $dialog 'IDC_SEARCH_FOLDERS' $Case.Folders
    if ($Case.Minimum) {
        [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_SIZE_MIN'), '1000')
        [void] [TestDesktop]::Message((Get-DialogControl $dialog 'IDC_SEARCH_SIZE_UNITS'), 0x014E, 0)
    }
    Close-AppDialog $dialog 1
    $list = Get-AppList $app -TabName 'Search Results'
    $first = Join-Path $root 'alpha\selected-a.bin'
    $second = Join-Path $root 'alpha\selected-b.bin'
    $folder = Join-Path $root 'alpha'
    $expected = @($first, $second)
    if ($Case.Folders) { $expected += $folder }
    if ($Case.Root) { $expected += $root }
    foreach ($selection in @(@($first), @($first, $second), @($folder), @($root))) {
        if ($selection[0] -eq $folder -and -not $Case.Folders) { continue }
        if ($selection[0] -eq $root -and -not $Case.Root) { continue }
        New-TestFile $first 32771 | Out-Null
        New-TestFile $second 8197 | Out-Null
        Select-AppRows $app $list $selection
        Invoke-AppKeys $app '{F5}'
        Wait-AppIdle $app
        $paths = @([NativeListViewHelper]::GetItemTexts($list) | Where-Object { [IO.Path]::IsPathFullyQualified($_) })
        Assert-Sequence @($paths | Sort-Object) @($expected | Sort-Object) `
            'Single, multiple, and folder refreshes preserve surviving search results'
        Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) $selection.Count `
            'Refreshed search rows retain their selection'
        $sizes = [NativeListViewHelper]::GetItemTexts($list, 2)
        Assert-Equal ([long]($sizes[(Find-AppRow $list $first)] -replace '\D', '')) 32771 `
            'The restored search row displays refreshed metadata'
        Assert-True ([NativeListViewHelper]::GetItemTexts($list)[0] -eq "Search Results ($($expected.Count)+)") `
            'Refresh reports the surviving count and retains the existing incomplete-results marker'
    }

    if ($Case.Minimum) {
        New-TestFile $first 17 | Out-Null
        Select-AppRows $app $list @($first)
        Invoke-AppKeys $app '{F5}'
        Wait-AppIdle $app
        $expected = @($expected | Where-Object { $_ -ne $first })
        Assert-Equal (Find-AppRow $list $first -Optional) -1 'Refresh reapplies the original search size filter'
    }
    [IO.File]::Delete($second)
    Select-AppRows $app $list @($second)
    Invoke-AppKeys $app '{F5}'
    Wait-AppIdle $app
    $expected = @($expected | Where-Object { $_ -ne $second })
    $paths = @([NativeListViewHelper]::GetItemTexts($list) | Where-Object { [IO.Path]::IsPathFullyQualified($_) })
    Assert-Sequence @($paths | Sort-Object) @($expected | Sort-Object) `
        'Refreshing a deleted search result removes only results that no longer match'

    Select-AppRows $app $list @([NativeListViewHelper]::GetItemTexts($list)[0])
    [void] [TestDesktop]::Message($list, 0x0100, 0x25)
    $tree = Get-AppList $app -AllFiles
    Select-AppRows $app $tree @($folder)
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Invoke-AppCommand $app 'ID_VIEW_SEARCH_RESULTS'
    Assert-Equal ([NativeListViewHelper]::GetItemCount($list)) 1 'Refreshing keeps the search root collapsed'
    [void] [NativeListViewHelper]::PostRight($list)
    Wait-Until { [NativeListViewHelper]::GetItemCount($list) -gt 1 } 'The refreshed search expands' | Out-Null
    $paths = @([NativeListViewHelper]::GetItemTexts($list) | Where-Object { [IO.Path]::IsPathFullyQualified($_) })
    Assert-Sequence @($paths | Sort-Object) @($expected | Sort-Object) `
        'Collapsed search results are restored without stale rows'
}

function Test-SearchRefreshCaseSensitive {
    param($Context, $Case)
    $root = $Context.Fixture
    $folder = Join-Path $root 'alpha'
    [IO.Directory]::CreateDirectory($folder) | Out-Null
    $utility = [pscustomobject]@{ Exe = Join-Path $env:windir 'System32\fsutil.exe'; Directory = $Context.Root }
    $probe = Invoke-TestProcess $utility @('file', 'setCaseSensitiveInfo', $folder, 'enable')
    if ($probe.ExitCode -ne 0) { Skip-Scenario 'Case-sensitive directories are unavailable for this fixture.' }
    $first = New-TestFile (Join-Path $folder 'selected-a.bin') 65537
    $second = New-TestFile (Join-Path $folder 'SELECTED-A.bin') 173
    if ([IO.Directory]::GetFiles($folder).Count -ne 2) {
        Skip-Scenario 'The filesystem did not preserve two filenames differing only in case.'
    }
    $app = Start-TestApp @($root)
    Get-AppList $app -AllFiles | Out-Null
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_TERM'), '^selected-a\.bin$|^alpha$')
    Set-DialogCheck $dialog 'IDC_SEARCH_REGEX' $true
    Set-DialogCheck $dialog 'IDC_SEARCH_FOLDERS' $true
    Close-AppDialog $dialog 1
    $list = Get-AppList $app -TabName 'Search Results'
    Assert-Equal ([NativeListViewHelper]::GetItemCount($list)) 4 `
        'The search finds both case-distinct files and their folder'
    Select-AppRows $app $list @($folder)
    Invoke-AppKeys $app '{F5}'
    Wait-AppIdle $app
    $names = [NativeListViewHelper]::GetItemTexts($list)
    $paths = @($names | Where-Object { [IO.Path]::IsPathFullyQualified($_) })
    Assert-Sequence @($paths | Sort-Object -CaseSensitive) @($first, $second, $folder | Sort-Object -CaseSensitive) `
        'Refresh preserves distinct results whose filenames differ only in case'
    $sizes = [NativeListViewHelper]::GetItemTexts($list, 2)
    Assert-Equal ([long]($sizes[[Array]::IndexOf($names, $second)] -replace '\D', '')) 173 `
        'The restored result references the file with the exact original name'
}

function Test-SearchRefreshLarge {
    param($Context, $Case)
    $root = $Context.Fixture
    $folder = Join-Path $root 'big'
    $count = [Math]::Max(50000, $script:ExtendedStressFileCount)
    $expected = @(for ($index = 0; $index -lt $count; ++$index) {
        $path = Join-Path $folder "d$index"
        [IO.Directory]::CreateDirectory($path) | Out-Null
        $path
    })
    $untouched = Join-Path $root 'd-untouched'
    [IO.Directory]::CreateDirectory($untouched) | Out-Null
    $app = Start-TestApp @($root) -Settings @{ SearchView = @{ SearchMaxResults = 1000000 } }
    Get-AppList $app -AllFiles | Out-Null
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_TERM'), '^d')
    Set-DialogCheck $dialog 'IDC_SEARCH_REGEX' $true
    Set-DialogCheck $dialog 'IDC_SEARCH_FOLDERS' $true
    Close-AppDialog $dialog 1
    $list = Get-AppList $app -TabName 'Search Results'
    Assert-Equal ([NativeListViewHelper]::GetItemCount($list)) ($count + 2) 'Every folder appears in the search'
    $names = [NativeListViewHelper]::GetItemTexts($list)
    $indices = [int[]]@([Array]::IndexOf($names, $expected[1]), [Array]::IndexOf($names, $untouched))
    Assert-True ([NativeListViewHelper]::SelectItems($list, $indices)) `
        'Select results inside and outside the refreshed folder'
    [IO.Directory]::Delete($expected[0])
    $expected = @($expected | Select-Object -Skip 1) + @($untouched)
    [IO.Directory]::CreateDirectory((Join-Path $folder 'd-new')) | Out-Null
    $tree = Get-AppList $app -AllFiles
    Select-AppRows $app $tree @($folder)

    $timing = @{ LastResponse = 0L; Longest = 0L }
    $watch = [Diagnostics.Stopwatch]::StartNew()
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-Until {
        try { [void] [TestDesktop]::Message($app.Window, 0) }
        catch [System.TimeoutException] { return $false }
        $now = $watch.ElapsedMilliseconds
        $timing.Longest = [Math]::Max($timing.Longest, $now - $timing.LastResponse)
        $timing.LastResponse = $now
        [NativeListViewHelper]::GetItemCount($list) -eq ($expected.Count + 1) -and
            [TestDesktop]::Text($app.Window) -notmatch '(?:^| - )(Scanning|\d+([.,]\d+)?%)(?: |$)'
    } 'Large folder refresh finishes' -StableMilliseconds 500 | Out-Null
    $watch.Stop()
    Write-TestNote ("Refresh $count results: {0} ms total; {1} ms longest response" -f
        $watch.ElapsedMilliseconds, $timing.Longest)

    Invoke-AppCommand $app 'ID_VIEW_SEARCH_RESULTS'
    $names = [NativeListViewHelper]::GetItemTexts($list)
    $paths = @($names | Where-Object { [IO.Path]::IsPathFullyQualified($_) })
    $expectedSet = [Collections.Generic.HashSet[string]]::new([string[]]$expected, [StringComparer]::Ordinal)
    Assert-True ($paths.Count -eq $expected.Count -and $expectedSet.SetEquals([string[]]$paths)) `
        'Bulk refresh restores surviving results without adding new matches'
    Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) 2 `
        'Refreshed and untouched search results retain their selection'
    foreach ($path in $expected[0], $untouched) {
        Assert-Equal ([TestDesktop]::Message($list, 0x102C, [Array]::IndexOf($names, $path), 2).ToInt32()) 2 `
            'Selection remains attached to the same search result after compaction'
    }
    Assert-True ($timing.Longest -lt 3000) 'Refreshing a large search does not block the UI for three seconds'

    $selected = @($expected | Select-Object -First 1000)
    $indices = [int[]]@($selected | ForEach-Object { [Array]::IndexOf($names, $_) })
    Assert-True ([NativeListViewHelper]::SelectItems($list, $indices)) 'Select a large batch of search results'
    [IO.Directory]::Delete($selected[0])
    $expected = @($expected | Select-Object -Skip 1)
    $timing = @{ LastResponse = 0L; Longest = 0L }
    $watch.Restart()
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-Until {
        try { [void] [TestDesktop]::Message($app.Window, 0) }
        catch [System.TimeoutException] { return $false }
        $now = $watch.ElapsedMilliseconds
        $timing.Longest = [Math]::Max($timing.Longest, $now - $timing.LastResponse)
        $timing.LastResponse = $now
        [NativeListViewHelper]::GetItemCount($list) -eq ($expected.Count + 1) -and
            [TestDesktop]::Text($app.Window) -notmatch '(?:^| - )(Scanning|\d+([.,]\d+)?%)(?: |$)'
    } 'Refreshing a large selection drops the deleted result' -StableMilliseconds 500 | Out-Null
    $watch.Stop()
    Write-TestNote ("Refresh 1000 selected results: {0} ms total; {1} ms longest response" -f
        $watch.ElapsedMilliseconds, $timing.Longest)
    $paths = @([NativeListViewHelper]::GetItemTexts($list) | Where-Object { [IO.Path]::IsPathFullyQualified($_) })
    [void]$expectedSet.Remove($selected[0])
    Assert-True ($paths.Count -eq $expected.Count -and $expectedSet.SetEquals([string[]]$paths)) `
        'Batch selection refresh preserves exact paths'
    Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) 999 'Only surviving refreshed results stay selected'
    Assert-True ($timing.Longest -lt 3000) 'Refreshing many selected results does not block the UI for three seconds'
}

function Preserve-TestClipboard {
    $previous = [System.Windows.Forms.Clipboard]::GetDataObject()
    $clipboard = [System.Windows.Forms.DataObject]::new()
    if ($previous) {
        foreach ($format in $previous.GetFormats($false)) {
            $value = $previous.GetData($format, $false)
            if ($null -ne $value) { $clipboard.SetData($format, $false, $value) }
        }
    }
    Add-Cleanup 'Restore clipboard' {
        param($Previous)
        if ($Previous) { [System.Windows.Forms.Clipboard]::SetDataObject($Previous, $true) }
        else { [System.Windows.Forms.Clipboard]::Clear() }
    } @($clipboard)
}

function Test-ContextSelection {
    param($Context, $Case)
    Preserve-TestClipboard
    $root = New-TestTree
    $app = Start-TestApp @($root)
    $list = Get-AppList $app -AllFiles
    Expand-AppDirectory $app $list 'alpha' -Child 'selected-a.bin'
    Select-AppRows $app $list @('selected-a.bin', 'selected-b.bin')
    [void] [NativeListViewHelper]::PostContextMenu($list)
    Wait-Until {
        @([TestDesktop]::Windows($app.Process.Id) | Where-Object { [TestDesktop]::Class($_) -eq '#32768' }).Count -gt 0
    } 'The keyboard context menu opens for the current selection' | Out-Null
    Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) 2 `
        'Opening a keyboard context menu preserves the multi-selection'
    Invoke-AppKeys $app '{ESC}'
    Wait-Until {
        @([TestDesktop]::Windows($app.Process.Id) | Where-Object { [TestDesktop]::Class($_) -eq '#32768' }).Count -eq 0
    } 'Cancelling the context menu releases its menu loop' | Out-Null
    Invoke-AppCommand $app 'ID_EDIT_COPY_CLIPBOARD'
    $expected = @((Join-Path $root 'alpha\selected-a.bin'), (Join-Path $root 'alpha\selected-b.bin') | Sort-Object)
    Wait-Until {
        $text = [System.Windows.Forms.Clipboard]::GetText()
        $text.Contains('selected-a.bin') -and $text.Contains('selected-b.bin')
    } 'Copy Path receives the preserved context selection' | Out-Null
    Assert-Sequence @([System.Windows.Forms.Clipboard]::GetText() -split '\r?\n' | Sort-Object) $expected `
        'Context selection routes both exact paths to the clipboard'
    Assert-Equal ([TestDesktop]::CaptureOwner($app.Window)) ([IntPtr]::Zero) `
        'Cancelling the popup does not retain mouse capture'
}

function Test-SameNameNavigation {
    param($Context, $Case)
    Preserve-TestClipboard
    $root = New-TestTree
    $app = Start-TestApp @($root)
    $list = Get-AppList $app -AllFiles
    foreach ($branch in 'alpha', 'beta') {
        Expand-AppDirectory $app $list $branch -Child 'same-name.bin'
        Select-AppRows $app $list @('same-name.bin')
        [System.Windows.Forms.Clipboard]::SetText('pending')
        Invoke-AppCommand $app 'ID_EDIT_COPY_CLIPBOARD'
        $expected = Join-Path $root "$branch\same-name.bin"
        Wait-Until { [System.Windows.Forms.Clipboard]::GetText() -eq $expected } `
            'Copy Path resolves the selected branch identity' |
            Out-Null
        Assert-Equal ([System.Windows.Forms.Clipboard]::GetText()) $expected `
            'Identical display names retain different filesystem identities'
        Select-AppRows $app $list @($branch)
        [void] [TestDesktop]::PostMessage($list, 0x0100, [IntPtr]0x25, [IntPtr]::Zero)
        [void] [TestDesktop]::PostMessage($list, 0x0101, [IntPtr]0x25, [IntPtr]::Zero)
        Wait-Until { (Find-AppRow $list 'same-name.bin' -Optional) -eq -1 } `
            'Collapsing a branch removes only its visible child rows' |
            Out-Null
    }
}

function Test-VerifiedFileOperation {
    param($Context, $Case)
    $root = $Context.Fixture
    $paths = @(foreach ($name in 'single.bin', 'multiple-a.bin', 'multiple-b.bin', 'untouched.bin') {
        New-TestFile (Join-Path $root $name) 2MB -Zeros
    })
    if ($Case.Kind -eq 'Motw') {
        foreach ($path in $paths) { [IO.File]::WriteAllText("${path}:Zone.Identifier", "[ZoneTransfer]`r`nZoneId=3") }
    }
    $hashes = @($paths | Get-FileHash | ForEach-Object Hash)
    $app = Start-TestApp @($root)
    $list = Get-AppList $app -AllFiles
    foreach ($names in @(@('single.bin'), @('multiple-a.bin', 'multiple-b.bin'))) {
        Select-AppRows $app $list $names
        Invoke-AppCommand $app $Case.Command
        Wait-AppIdle $app
        foreach ($name in $names) {
            $path = Join-Path $root $name
            switch ($Case.Kind) {
                Compress {
                    Assert-True (([IO.File]::GetAttributes($path) -band [IO.FileAttributes]::Compressed) -ne 0) `
                        'Every selected file becomes NTFS-compressed'
                }
                Sparse {
                    Assert-True (([IO.File]::GetAttributes($path) -band [IO.FileAttributes]::SparseFile) -ne 0) `
                        'Every selected file acquires sparse-file metadata'
                    Assert-True ([WdsNativeFs]::GetAllocationSize($path) -lt 2MB) `
                        'Sparsification frees allocated zero ranges'
                }
                Motw {
                    $streams = @(Get-Item -LiteralPath $path -Stream Zone.Identifier -ErrorAction SilentlyContinue)
                    Assert-Equal $streams.Count 0 `
                        'Every selected file loses its real Mark-of-the-Web stream'
                }
            }
        }
    }
    Assert-Sequence @($paths | Get-FileHash | ForEach-Object Hash) $hashes `
        'Single and multiple file operations preserve source contents'
    $untouched = Join-Path $root 'untouched.bin'
    if ($Case.Kind -eq 'Motw') {
        Assert-Equal @(Get-Item -LiteralPath $untouched -Stream Zone.Identifier).Count 1 `
            'An unselected file retains its Mark-of-the-Web stream'
    } else {
        Assert-Equal ([IO.File]::GetAttributes($untouched) -band
            ([IO.FileAttributes]::Compressed -bor [IO.FileAttributes]::SparseFile)) ([IO.FileAttributes]0) `
            'The operation does not alter unselected file attributes'
    }
    Assert-ScanSnapshot (Save-AppReport $app 'after-operation') (Get-DiskSnapshot $root) $root -Physical
}

function Test-DeduplicateGroups {
    param($Context, $Case)
    $root = $Context.Fixture
    $paths = @(foreach ($group in 1, 2) {
        foreach ($member in 'a', 'b') { New-TestFile (Join-Path $root "group-$group-$member.bin") 32768 $group }
    })
    $hashes = @($paths | Get-FileHash | ForEach-Object Hash)
    $app = Start-TestApp @($root) -Settings @{ DupeView = @{ ScanForDuplicates = 1 } }
    Get-AppList $app -AllFiles | Out-Null
    Invoke-AppCommand $app 'ID_VIEW_DUPLICATE_FILES'
    $list = Get-AppList $app
    Expand-AppGroups $app $list $paths
    Select-AppRows $app $list @($paths[0])
    Assert-True (-not (Get-AppCommandState $app 'ID_CLEANUP_CREATE_HARDLINK').Enabled) `
        'A single duplicate cannot trigger hardlink replacement'
    Select-AppRows $app $list $paths
    Invoke-AppCommand $app 'ID_CLEANUP_CREATE_HARDLINK'
    Wait-AppIdle $app
    $identities = @($paths | ForEach-Object { Get-FileIdentity $_ })
    Assert-Equal $identities[0].Id $identities[1].Id 'The first hash group shares a physical file after deduplication'
    Assert-Equal $identities[2].Id $identities[3].Id 'The second hash group shares a separate physical file'
    Assert-True ($identities[0].Id -ne $identities[2].Id) `
        'Selecting multiple hash groups does not cross-link different contents'
    Assert-Sequence @($paths | Get-FileHash | ForEach-Object Hash) $hashes `
        'Hardlink deduplication preserves each group payload'
    Assert-ScanSnapshot (Save-AppReport $app 'after-dedup') (Get-DiskSnapshot $root) $root -Physical
}

function Test-DuplicateBatchUpdates {
    param($Context, $Case)
    $root = $Context.Fixture
    $incoming = Join-Path $root 'incoming'
    [IO.Directory]::CreateDirectory($incoming) | Out-Null
    $stable = @(foreach ($group in 1, 2) {
        foreach ($member in 'a', 'b') {
            New-TestFile (Join-Path $root "stable\group-$group-$member.bin") 32768 $group
        }
    })
    $app = Start-TestApp @($root) -Settings @{ DupeView = @{ ScanForDuplicates = 1 } }
    Get-AppList $app -AllFiles | Out-Null
    Invoke-AppCommand $app 'ID_VIEW_DUPLICATE_FILES'
    $list = Get-AppList $app
    $texts = [NativeListViewHelper]::GetItemTexts($list)
    Assert-Equal $texts.Count 3 'The initial duplicate tree exposes two collapsed hash groups'
    Select-AppRows $app $list @($texts[1])
    [void] [NativeListViewHelper]::PostRight($list)
    Wait-Until { [NativeListViewHelper]::GetItemCount($list) -eq 5 } 'Only the selected hash group expands' | Out-Null
    $texts = [NativeListViewHelper]::GetItemTexts($list)
    $expanded = @($stable | Where-Object { $_ -in $texts })
    $hidden = @($stable | Where-Object { $_ -notin $texts })
    Select-AppRows $app $list $expanded
    if ($Case.Collapsed) {
        Select-AppRows $app $list @($texts[0])
        [void] [TestDesktop]::Message($list, 0x0100, 0x25)
        Assert-Equal ([NativeListViewHelper]::GetItemCount($list)) 1 'The duplicate root collapses before refresh'
    }

    $added = @(foreach ($group in 1, 2, 3) {
        foreach ($member in 'c', 'd') {
            New-TestFile (Join-Path $incoming "group-$group-$member.bin") 32768 $group
        }
    })
    $tree = Get-AppList $app -AllFiles
    Select-AppRows $app $tree @($incoming)
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Invoke-AppCommand $app 'ID_VIEW_DUPLICATE_FILES'
    $texts = [NativeListViewHelper]::GetItemTexts($list)
    if ($Case.Collapsed) {
        Assert-Equal $texts.Count 1 'New groups and files leave the duplicate root collapsed'
        Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) 1 'The collapsed root retains its selection'
    } else {
        Assert-Equal $texts.Count 8 'The expanded group receives new files while the other groups stay collapsed'
        Assert-Equal @($hidden | Where-Object { $_ -in $texts }).Count 0 'Existing collapsed groups remain collapsed'
        Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) $expanded.Count `
            'Adding new groups and files preserves the existing multi-selection'
        foreach ($file in $expanded) {
            Assert-Equal ([TestDesktop]::Message($list, 0x102C, (Find-AppRow $list $file), 2).ToInt32()) 2 `
                'The same file remains selected after row insertion and sorting'
        }
        $counts = [NativeListViewHelper]::GetItemTexts($list, 1)
        $groupCounts = @(for ($index = 1; $index -lt $texts.Count; ++$index) {
            if (-not [IO.Path]::IsPathFullyQualified($texts[$index])) { $counts[$index] }
        })
        Assert-Sequence @($groupCounts | Sort-Object) @('2', '4', '4') 'Every hash group displays its updated count'
    }
    $expected = @($stable + $added | Sort-Object)
    $rows = Save-AppReport $app 'batched-duplicates' -Command 'ID_SAVE_DUPLICATES'
    Assert-Sequence @($rows.Name | Sort-Object) $expected `
        'Duplicate export includes every old and new matching file once'
    Assert-Equal @($rows | Where-Object { [long]$_.'Logical Size' -ne 32768 }).Count 0 `
        'Batched duplicate additions retain each file size'
    Expand-AppGroups $app $list $expected
    Assert-Equal ([NativeListViewHelper]::GetItemCount($list)) 14 `
        'Expanding all groups reveals every duplicate exactly once'

    Select-AppRows $app $list @($stable + @($added[-1]))
    if ($Case.Collapsed) {
        Select-AppRows $app $list @([NativeListViewHelper]::GetItemTexts($list)[0])
        [void] [TestDesktop]::Message($list, 0x0100, 0x25)
    }
    foreach ($path in $added | Select-Object -SkipLast 1) { [IO.File]::Delete($path) }
    $tree = Get-AppList $app -AllFiles
    Select-AppRows $app $tree @($incoming)
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Invoke-AppCommand $app 'ID_VIEW_DUPLICATE_FILES'
    Assert-Equal ([NativeListViewHelper]::GetItemCount($list)) $(if ($Case.Collapsed) { 1 } else { 7 }) `
        'Removing duplicate children and singleton groups preserves expansion'
    Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) $(if ($Case.Collapsed) { 1 } else { 4 }) `
        'Batch removal preserves selection on surviving duplicate rows'
    Assert-True ([TestDesktop]::Message($list, 0x100C, -1, 1).ToInt32() -ge 0) `
        'Removing the focused duplicate group leaves a surviving row focused'
    $rows = Save-AppReport $app 'pruned-duplicates' -Command 'ID_SAVE_DUPLICATES'
    Assert-Sequence @($rows.Name | Sort-Object) @($stable | Sort-Object) `
        'Pruned duplicate exports retain only the surviving pairs'
    Expand-AppGroups $app $list $stable
    $names = [NativeListViewHelper]::GetItemTexts($list)
    $sizes = [NativeListViewHelper]::GetItemTexts($list, 3)
    for ($index = 1; $index -lt $names.Count; ++$index) {
        if ([IO.Path]::IsPathFullyQualified($names[$index])) { continue }
        Assert-Equal ([long]($sizes[$index] -replace '\D', '')) 65536 `
            'Surviving duplicate groups subtract the removed file sizes'
    }
}

function Test-LargestRefresh {
    param($Context, $Case)
    $root = $Context.Fixture
    $first = New-TestFile (Join-Path $root 'changed\first.bin') 65536
    $second = New-TestFile (Join-Path $root 'changed\second.bin') 32768
    $stable = New-TestFile (Join-Path $root 'stable.bin') 49152
    $replacement = New-TestFile (Join-Path $root 'replacement.bin') 16384
    $app = Start-TestApp @($root) -Settings @{ Options = @{ LargeFileCount = 3 } }
    Get-AppList $app -AllFiles | Out-Null
    Invoke-AppCommand $app 'ID_VIEW_LARGEST_FILES'
    $list = Get-AppList $app -TabName 'Largest Files'
    Select-AppRows $app $list @($stable)
    [IO.File]::Delete($first)
    [IO.File]::Delete($second)
    $tree = Get-AppList $app -AllFiles
    Select-AppRows $app $tree @((Join-Path $root 'changed'))
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Invoke-AppCommand $app 'ID_VIEW_LARGEST_FILES'
    $paths = @([NativeListViewHelper]::GetItemTexts($list) | Where-Object { [IO.Path]::IsPathFullyQualified($_) })
    Assert-Sequence @($paths | Sort-Object) @($stable, $replacement | Sort-Object) `
        'Largest files replaces removed rows with the next surviving candidate'
    Assert-Equal ([TestDesktop]::Message($list, 0x102C, (Find-AppRow $list $stable), 2).ToInt32()) 2 `
        'The surviving largest file keeps its selection across removal and insertion'
}

function Test-CleanupReparseReplacement {
    param($Context, $Case)
    $root = New-TestTree
    $stale = Join-Path $root 'ordinary-before-scan'
    [IO.Directory]::CreateDirectory($stale) | Out-Null
    $target = Join-Path $Context.Root 'external-target\empty-sentinel'
    [IO.Directory]::CreateDirectory($target) | Out-Null
    $app = Start-TestApp @($root)
    $list = Get-AppList $app -AllFiles
    [IO.Directory]::Delete($stale)
    New-TestJunction $stale ([IO.Path]::GetDirectoryName($target))
    Select-AppRows $app $list @($root)
    Invoke-AppCommand $app 'ID_CLEANUP_REMOVE_EMPTY'
    Wait-AppIdle $app
    Assert-True (-not [IO.Directory]::Exists((Join-Path $root 'empty'))) `
        'Ordinary empty directories are actually removed'
    Assert-True ([IO.Directory]::Exists($target)) `
        'Cleanup does not traverse an ordinary directory replaced with a junction after scanning'
    Assert-True (([IO.File]::GetAttributes($stale) -band [IO.FileAttributes]::ReparsePoint) -ne 0) `
        'The replacement junction is preserved'
}

function Test-SelectedDeletion {
    param($Context, $Case)
    $root = New-TestTree
    $removed = Join-Path $root 'alpha'
    $single = Join-Path $root 'beta\untouched.txt'
    $before = Get-DiskSnapshot $root
    $hashes = @($before | Where-Object { -not $_.Directory -and
        -not $_.Path.StartsWith((Get-NormalPath $removed) + '\') -and $_.Path -ne (Get-NormalPath $single) } |
        ForEach-Object Path | Get-FileHash | Sort-Object Path)
    $app = Start-TestApp @($root) -Settings @{ Options = @{ ShowMicrosoftProgress = $Case.Microsoft } }
    $list = Get-AppList $app -AllFiles
    Expand-AppDirectory $app $list 'beta' 'untouched.txt'
    Select-AppRows $app $list @('alpha', 'untouched.txt')
    New-TestFile (Join-Path $removed 'created-after-scan.txt') 353 | Out-Null
    Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
    Wait-AppIdle $app
    Assert-True (-not [IO.Directory]::Exists($removed) -and -not [IO.File]::Exists($single)) `
        'Permanent deletion removes the selected folder, new descendants, and separately selected file'
    $after = @($hashes.Path | Get-FileHash | Sort-Object Path)
    Assert-Sequence @($after.Hash) @($hashes.Hash) 'Permanent deletion preserves every unselected payload'
    Assert-ScanSnapshot (Save-AppReport $app 'deleted-local') (Get-DiskSnapshot $root) $root -Physical
}

function Test-CloudSelectionDialog {
    param($Context, $Case)
    $root = New-TestTree
    $savedRect = [byte[]](@(100, 100, 433, 331) | ForEach-Object { [BitConverter]::GetBytes([int]$_) })
    $app = Start-TestApp @($root) -Settings @{
        Options = @{ DarkMode = $Case.Dark; FontSizePercent = $Case.Font }
        DriveSelect = @{
            CloudConnectionsRect = [Convert]::ToHexString($savedRect)
            SelectDrivesRadio = 2; SelectDrivesFolder = $root
        }
    }
    $dialog = Open-AppDialog $app 'ID_FILE_SELECT' 'IDC_TARGET_DRIVES_LIST'
    $bounds = [TestDesktop]::Rectangle($dialog)
    [TestDesktop]::Position($dialog, $bounds[0], $bounds[1],
        $bounds[2] - $bounds[0] + 80, $bounds[3] - $bounds[1] + 30)
    $localSize = [TestDesktop]::ClientSize($dialog)
    Close-AppDialog $dialog 1
    Wait-AppIdle $app
    Assert-ScanSnapshot (Save-AppReport $app 'selected-local') (Get-DiskSnapshot $root) $root

    $picker = Open-AppDialog $app 'ID_FILE_SELECT_CLOUD' 'IDC_REMOTE_RECENT'
    foreach ($control in 'IDOK', 'IDC_REMOTE_EDIT', 'IDC_REMOTE_REMOVE') {
        $id = if ($control -eq 'IDOK') { 1 } else { Get-ResourceId $control }
        Assert-True (-not [TestDesktop]::IsWindowEnabled([TestDesktop]::GetDlgItem($picker, $id))) `
            'The empty list disables actions that require a connection'
    }
    $bounds = [TestDesktop]::Rectangle($picker)
    [TestDesktop]::Position($picker, $bounds[0], $bounds[1],
        $bounds[2] - $bounds[0] + 80, $bounds[3] - $bounds[1] + 30)
    $pickerSize = [TestDesktop]::ClientSize($picker)
    $dialog = Open-CloudConnection $app $picker
    Assert-True (-not [TestDesktop]::IsWindowEnabled($picker)) 'The editor owns input until OK or Cancel'
    $heights = @{}
    foreach ($provider in 'S3', 'Azure Blob', 'WebDAV') {
        Select-CloudProvider $dialog $provider
        Assert-True (-not [TestDesktop]::IsWindowEnabled([TestDesktop]::GetDlgItem($dialog, 1))) `
            'Incomplete settings cannot be saved'
        [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_REMOTE_ENDPOINT'), "https://$provider.example.invalid/")
        $heights[$provider] = [TestDesktop]::ClientSize($dialog)[1]
    }
    Assert-True ($heights.WebDAV -lt $heights.S3) 'The WebDAV form does not inherit the S3 form height'
    Select-CloudProvider $dialog 'S3'
    $field = Get-DialogControl $dialog 'IDC_REMOTE_ENDPOINT'
    Assert-Equal ([TestDesktop]::ControlText($field)) 'https://S3.example.invalid/' `
        'Each provider preserves unfinished input'
    $remember = [TestDesktop]::Rectangle((Get-DialogControl $dialog 'IDC_REMOTE_REMEMBER'))
    $confirm = [TestDesktop]::Rectangle([TestDesktop]::GetDlgItem($dialog, 1))
    Assert-True ($remember[3] -lt $confirm[1]) 'The form fits above OK at the current text size'
    $height = [TestDesktop]::ClientSize($dialog)[1]
    Set-DialogCheck $dialog 'IDC_S3_TOKEN_TOGGLE' $true
    Assert-True ([TestDesktop]::ClientSize($dialog)[1] -gt $height) 'Optional session-token input expands the form'
    [void](Get-DialogControl $dialog 'IDC_S3_TOKEN')
    $bounds = [TestDesktop]::Rectangle($dialog)
    $width = [TestDesktop]::ClientSize($field)[0]
    [TestDesktop]::Position($dialog, $bounds[0], $bounds[1],
        $bounds[2] - $bounds[0] + 160, $bounds[3] - $bounds[1] + 60)
    Assert-Equal ([TestDesktop]::ClientSize($field)[0]) ($width + 160) 'Fields grow with the editor'
    $bucket = [TestDesktop]::Rectangle((Get-DialogControl $dialog 'IDC_S3_BUCKET'))
    $region = [TestDesktop]::Rectangle((Get-DialogControl $dialog 'IDC_S3_REGION'))
    Assert-True ($bucket[2] -lt $region[0]) 'Paired fields stay separated when resizing'
    [TestDesktop]::Expose($dialog)
    Save-Frame ([TestDesktop]::Capture($dialog)) (Join-Path $Context.Root 'cloud-editor.png')
    Close-AppDialog $dialog
    Wait-Until { [TestDesktop]::IsWindowEnabled($picker) } 'Cancel returns control to the connection list' | Out-Null
    Assert-Equal ([NativeListViewHelper]::GetItemTexts((Get-DialogControl $picker 'IDC_REMOTE_RECENT')).Count) 0 `
        'Cancel discards the draft without creating a connection'
    Assert-Sequence ([TestDesktop]::ClientSize($picker)) $pickerSize 'Resizing the editor leaves the picker size intact'
    $dialog = Open-CloudConnection $app $picker
    Select-CloudProvider $dialog 'S3'
    Assert-Equal ([TestDesktop]::ControlText((Get-DialogControl $dialog 'IDC_REMOTE_ENDPOINT'))) '' `
        'New Connection clears canceled drafts'
    $values = @{
        IDC_S3_BUCKET = 'preview-bucket'; IDC_S3_REGION = 'us-east-1'; IDC_REMOTE_PREFIX = 'reports/'
        IDC_REMOTE_ENDPOINT = 'https://storage.example.invalid'; IDC_S3_ACCESS = 'preview-user'
        IDC_REMOTE_SECRET = 'preview-secret'
    }
    foreach ($entry in $values.GetEnumerator()) {
        [TestDesktop]::SetText((Get-DialogControl $dialog $entry.Key), $entry.Value)
    }
    Close-AppDialog $dialog 1
    Wait-Until { [TestDesktop]::IsWindowEnabled($picker) } 'OK returns control to the connection list' | Out-Null
    Assert-Sequence ([NativeListViewHelper]::GetItemTexts((Get-DialogControl $picker 'IDC_REMOTE_RECENT'))) `
        @('preview-bucket/reports/') 'OK adds the connection to the picker without starting a scan'
    Assert-True ([TestDesktop]::IsWindowEnabled([TestDesktop]::GetDlgItem($picker, 1))) `
        'The new connection is selected and ready to scan'
    $dialog = Open-CloudConnection $app $picker
    [void][TestDesktop]::PostMessage($dialog, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    Wait-Until { -not [TestDesktop]::IsWindow($dialog) -and [TestDesktop]::IsWindowEnabled($picker) } `
        'The editor close button returns to the connection list' | Out-Null
    Close-AppDialog $picker
    Assert-ScanSnapshot (Save-AppReport $app 'connection-saved') (Get-DiskSnapshot $root) $root

    $dialog = Open-AppDialog $app 'ID_FILE_SELECT' 'IDC_TARGET_DRIVES_LIST'
    Assert-Sequence ([TestDesktop]::ClientSize($dialog)) $localSize 'The local picker retains its own window size'
    Assert-Equal ([TestDesktop]::ControlText((Get-DialogControl $dialog 'IDC_BROWSE_FOLDER'))) $root `
        'Cloud editing leaves local selection intact'
    Close-AppDialog $dialog
    $dialog = Open-AppDialog $app 'ID_FILE_SELECT_CLOUD' 'IDC_REMOTE_RECENT'
    Assert-Sequence ([TestDesktop]::ClientSize($dialog)) $pickerSize 'The recent picker retains its own window size'
    [void][TestDesktop]::PostMessage($dialog, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    Wait-Until { -not [TestDesktop]::IsWindow($dialog) } 'The window close button cancels the entire selection' | Out-Null
}

function Test-CloudSelectionKeyboard {
    param($Context, $Case)
    $app = Start-TestApp @((New-TestTree))
    $picker = Open-AppDialog $app 'ID_FILE_SELECT_CLOUD' 'IDC_REMOTE_RECENT'
    $dialog = Open-CloudConnection $app $picker
    $provider = Get-DialogControl $dialog 'IDC_REMOTE_PROVIDER'
    [TestDesktop]::Activate($dialog)
    [void][NativeListViewHelper]::FocusWindow($provider)
    Invoke-AppKeys $app '{TAB}' -Window $dialog
    Assert-Equal ([TestDesktop]::Focus($dialog)) (Get-DialogControl $dialog 'IDC_S3_BUCKET') `
        'Tab enters the active provider form'
    [void][NativeListViewHelper]::FocusWindow($provider)
    Invoke-AppKeys $app '{DOWN}' -Window $dialog
    Assert-Equal ([TestDesktop]::Message($provider, 0x0147).ToInt32()) 1 'Arrow keys select another provider'
    Invoke-AppKeys $app '{TAB}' -Window $dialog
    Assert-Equal ([TestDesktop]::Focus($dialog)) (Get-DialogControl $dialog 'IDC_REMOTE_ACCOUNT') `
        'Hidden provider fields are skipped in keyboard navigation'
    Invoke-AppKeys $app '{ESC}' -Window $dialog
    Wait-Until { -not [TestDesktop]::IsWindow($dialog) -and [TestDesktop]::IsWindowEnabled($picker) } `
        'Escape closes the editor and restores the connection list' | Out-Null
    Invoke-AppKeys $app '{ESC}' -Window $picker
    Wait-Until { -not [TestDesktop]::IsWindow($picker) } 'Escape closes the connection list' | Out-Null
}

foreach ($appearance in @(@{ Dark = 0; Font = 100 }, @{ Dark = 1; Font = 200 })) {
    Register-Scenario "interaction.cloud-selection.$($appearance.Font)" Interaction `
        'Separate local and cloud selection preserve input, scans, and resizable layouts' `
        Test-CloudSelectionDialog -Tags Desktop -Requires Windows,Desktop -Data $appearance
}
Register-Scenario interaction.cloud-keyboard Interaction `
    'Cloud connection editing supports keyboard traversal and cancellation' `
    Test-CloudSelectionKeyboard -Tags Desktop -Requires Windows,Desktop

Register-Scenario interaction.scoped-refresh Interaction `
    'Selected refresh reconciles mutations without rescanning sibling branches' `
    Test-ScopedRefresh -Tags Desktop,External -Requires Windows,Desktop
Register-Scenario interaction.refresh-timestamps Interaction `
    'Batch refresh updates ancestor timestamps after deleting the newest siblings' `
    Test-RefreshTimestamps -Tags Desktop,External -Requires Windows,Desktop
Register-Scenario interaction.removed-root Interaction `
    'Refreshing a vanished multi-root selection preserves surviving roots' `
    Test-RemovedRootRefresh -Tags Desktop,External,Race -Requires Windows,Desktop
Register-Scenario interaction.settings-roundtrip Interaction `
    'Height factors and blank lines survive settings persistence' `
    Test-SettingsRoundTrip -Tags Desktop,Persistence -Requires Windows,Desktop
Register-Scenario interaction.settings-transaction Interaction `
    'Cross-page validation applies display settings atomically and persists them' `
    Test-SettingsTransaction -Tags Desktop,Persistence -Requires Windows,Desktop
Register-Scenario interaction.search-recovery Interaction `
    'Invalid search criteria recover without destroying the previous results' `
    Test-SearchRecovery -Tags Desktop -Requires Windows,Desktop
foreach ($multiRoot in $false, $true) {
    $scope = if ($multiRoot) { 'multiple' } else { 'single' }
    Register-Scenario "interaction.watcher-refresh.$scope" Interaction `
        'Watcher snapshot refresh reconciles deleted and modified paths in the current scan' `
        Test-WatcherRefresh -Tags Desktop,External -Requires Windows,Desktop -Data @{ MultiRoot = $multiRoot }
}
foreach ($search in @(
    @{ Id = 'files'; Folders = $false; Minimum = $false; Root = $false },
    @{ Id = 'folders'; Folders = $true; Minimum = $false; Root = $false },
    @{ Id = 'size'; Folders = $true; Minimum = $true; Root = $false },
    @{ Id = 'root'; Folders = $true; Minimum = $false; Root = $true }
)) {
    Register-Scenario "interaction.search-refresh.$($search.Id)" Interaction `
        'Search refresh preserves matching rows and selections while updating metadata and filters' `
        Test-SearchRefresh -Tags Desktop,External -Requires Windows,Desktop -Data $search
}
Register-Scenario interaction.search-refresh.case-sensitive Interaction `
    'Search refresh distinguishes separate files whose names differ only in case' `
    Test-SearchRefreshCaseSensitive -Tags Desktop,External,Filesystem -Requires Windows,Desktop,Ntfs
Register-Scenario interaction.search-refresh.large Interaction `
    'Large search refresh remains responsive and preserves surviving results and selections' `
    Test-SearchRefreshLarge -Tags Desktop,External,Extended -Requires Windows,Desktop
Register-Scenario interaction.context-selection Interaction `
    'Keyboard popup cancellation preserves selection and command targeting' `
    Test-ContextSelection -Tags Desktop -Requires Windows,Desktop
Register-Scenario interaction.same-name Interaction `
    'Tree navigation distinguishes identical filenames across branches' `
    Test-SameNameNavigation -Tags Desktop -Requires Windows,Desktop
foreach ($operation in @(
    @{ Id = 'compress'; Kind = 'Compress'; Command = 'ID_COMPRESS_LZNT1'; Requires = @('Windows', 'Desktop', 'Ntfs', 'Compression') },
    @{ Id = 'sparsify'; Kind = 'Sparse'; Command = 'ID_CLEANUP_SPARSIFY_FILE'; Requires = @('Windows', 'Desktop', 'Ntfs') },
    @{ Id = 'motw'; Kind = 'Motw'; Command = 'ID_CLEANUP_REMOVE_MOTW'; Requires = @('Windows', 'Desktop', 'Ntfs') }
)) {
    Register-Scenario "interaction.$($operation.Id)" Interaction `
        'Single and multiple file actions update disk and UI, preserving unselected files' `
        Test-VerifiedFileOperation -Tags Desktop,Filesystem -Requires $operation.Requires -Data $operation
}
Register-Scenario interaction.dedup-groups Interaction `
    'Hardlink deduplication retains independent sources within multiple hash groups' `
    Test-DeduplicateGroups -Tags Desktop,Filesystem -Requires Windows,Desktop,Ntfs
Register-Scenario interaction.cleanup-reparse Interaction `
    'Empty-folder cleanup rechecks reparse boundaries after external replacement' `
    Test-CleanupReparseReplacement -Tags Desktop,Filesystem,External -Requires Windows,Desktop,Ntfs
foreach ($collapsed in $false, $true) {
    $state = if ($collapsed) { 'collapsed' } else { 'expanded' }
    Register-Scenario "interaction.duplicate-batch.$state" Interaction `
        'Batched duplicate additions preserve tree expansion, selection, counts, and export contents' `
        Test-DuplicateBatchUpdates -Tags Desktop,External -Requires Windows,Desktop -Data @{ Collapsed = $collapsed }
}
Register-Scenario interaction.largest-refresh Interaction `
    'Largest-file refresh refills removed candidates and preserves surviving selection' `
    Test-LargestRefresh -Tags Desktop,External -Requires Windows,Desktop
foreach ($microsoft in 0, 1) {
    Register-Scenario "interaction.delete.$microsoft" Interaction `
        'Permanent deletion reconciles selected folders and files without changing surviving payloads' `
        Test-SelectedDeletion -Tags Desktop,Filesystem,External -Requires Windows,Desktop `
        -Data @{ Microsoft = $microsoft }
}
