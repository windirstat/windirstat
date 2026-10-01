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
    $app = Start-TestApp @($root) -Settings @{ TreeMapView = @{ TreeMapHeightFactor = 38 } }
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

function Preserve-TestClipboard {
    $clipboard = [System.Windows.Forms.Clipboard]::GetDataObject()
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

Register-Scenario interaction.scoped-refresh Interaction `
    'Selected refresh reconciles mutations without rescanning sibling branches' `
    Test-ScopedRefresh -Tags Desktop,External -Requires Windows,Desktop
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
