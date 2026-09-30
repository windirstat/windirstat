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
Register-Scenario interaction.settings-transaction Interaction `
    'Cross-page validation applies display settings atomically and persists them' `
    Test-SettingsTransaction -Tags Desktop,Persistence -Requires Windows,Desktop
Register-Scenario interaction.search-recovery Interaction `
    'Invalid search criteria recover without destroying the previous results' `
    Test-SearchRecovery -Tags Desktop -Requires Windows,Desktop
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
