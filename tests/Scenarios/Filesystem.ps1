function Test-EnumerationContract {
    param($Context, $Case)
    $root = New-TestTree
    $deep = $root
    foreach ($level in 1..9) { $deep = Join-Path $deep ("level-$level-" + ('x' * 20)) }
    New-TestFile (Join-Path $deep 'deep-é.txt') 5003 31 | Out-Null
    $snapshot = Get-DiskSnapshot $root
    $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = $Case.Engine } }
    foreach ($spelling in @($root, ($root.ToUpperInvariant() + '\'), "\\?\$root", "$root\alpha\..")) {
        $name = 'alias-' + [guid]::NewGuid().ToString('N').Substring(0, 6)
        $rows = Invoke-ScanReport $runner @($spelling) $name
        Assert-ScanSnapshot $rows $snapshot $root -Physical
    }
}

function Test-OverlappingRoots {
    param($Context, $Case)
    $root = New-TestTree
    $similar = New-TestTree (Join-Path $Context.Root 'fixture extra;folder')
    $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = $Case.Engine } }
    $expected = @(@(Get-DiskSnapshot $root) + @(Get-DiskSnapshot $similar) | Where-Object { -not $_.Directory } |
        ForEach-Object Path | Sort-Object)
    $variants = @(
        @($root, (Join-Path $root 'alpha'), $similar, ($root.ToUpperInvariant() + '\')),
        @((Join-Path $root 'alpha'), $similar, "$root\alpha\..", $root),
        @("$root|$similar", "\\?\$root")
    )
    for ($index = 0; $index -lt $variants.Count; ++$index) {
        $rows = Invoke-ScanReport $runner $variants[$index] "overlap-$index"
        $actual = @(Get-ReportFiles $rows | ForEach-Object { Get-NormalPath $_.Name } | Sort-Object)
        Assert-Sequence $actual $expected `
            'Ancestor, alias, and similar-prefix roots neither omit nor double-count files'
        Assert-Equal ([long]$rows[0].Files) ([long]$expected.Count) `
            'The synthetic multi-root total counts each file once'
    }
}

function Test-ReparseBoundary {
    param($Context, $Case)
    $root = New-TestTree
    $outside = Join-Path $Context.Root 'outside'
    $sentinel = New-TestFile (Join-Path $outside 'external-sentinel.dat') 7109
    $link = Join-Path $root 'redirected'
    New-TestJunction $link $outside
    $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = $Case.Engine; ExcludeJunctions = 1 } }
    $rows = Invoke-ScanReport $runner @($root) 'excluded'
    Assert-True (@($rows | Where-Object Name -Like '*external-sentinel.dat').Count -eq 0) `
        'Excluded junctions hide target content'
    Assert-Equal @($rows | Where-Object Name -IEQ $link).Count 1 'The excluded junction remains visible as a directory'
    Set-TestSettings $runner @{ Options = @{ UseFastScanEngine = $Case.Engine; ExcludeJunctions = 0 } }
    $rows = Invoke-ScanReport $runner @($root) 'followed'
    $redirected = @($rows | Where-Object Name -IEQ (Join-Path $link 'external-sentinel.dat'))
    Assert-Equal $redirected.Count 1 'A followed junction enumerates through its selected path'
    Assert-Equal ([long]$redirected[0].'Logical Size') ([long](Get-Item -LiteralPath $sentinel).Length) `
        'A redirected file retains its logical size'
    $identityBefore = Get-FileIdentity $sentinel
    Remove-Item -LiteralPath $sentinel
    New-TestFile $sentinel 8209 73 | Out-Null
    $rows = Invoke-ScanReport $runner @($link) 'redirected-root'
    Assert-Equal ([long]$rows[0].'Logical Size') 8209L `
        'A junction used as the root observes the current target after replacement'
    Assert-True ((Get-FileIdentity $sentinel).Id -ne $identityBefore.Id) `
        'The fixture actually replaced the external file identity'
}

function Test-SymbolicLinks {
    param($Context, $Case)
    $target = New-TestFile (Join-Path $Context.Root 'outside\target.txt') 12347
    $directoryLink = Join-Path $Context.Fixture 'directory-link'
    $fileLink = Join-Path $Context.Fixture 'file-link.txt'
    try {
        New-Item -ItemType SymbolicLink -Path $directoryLink -Target ([IO.Path]::GetDirectoryName($target)) | Out-Null
        New-Item -ItemType SymbolicLink -Path $fileLink -Target $target | Out-Null
    } catch { Skip-Scenario 'Creating symbolic links requires Developer Mode or the symbolic-link privilege.' }
    $runner = New-TestRunner @{ Options = @{ ExcludeSymbolicLinksDirectory = 1; ExcludeSymbolicLinksFile = 1 } }
    $excluded = Invoke-ScanReport $runner @($Context.Fixture) 'excluded-links'
    Assert-True (@($excluded | Where-Object Name -Like '*directory-link\target.txt').Count -eq 0) `
        'A directory symlink is not traversed when excluded'
    Set-TestSettings $runner @{ Options = @{ ExcludeSymbolicLinksDirectory = 0; ExcludeSymbolicLinksFile = 0 } }
    $followed = Invoke-ScanReport $runner @($Context.Fixture) 'followed-links'
    $files = Get-ReportFiles $followed
    Assert-Sequence @($files.Name | Sort-Object) @($fileLink, (Join-Path $directoryLink 'target.txt') | Sort-Object) `
        'File and directory symlinks retain distinct visible paths to the same target'
    foreach ($file in $files) {
        Assert-Equal ([long]$file.'Logical Size') ([IO.FileInfo]::new($file.Name).Length) `
            'Symlink entries and files beneath linked directories retain their native enumeration sizes'
    }
}

function Test-AllocatedSizes {
    param($Context, $Case)
    $root = New-TestTree
    $utility = [pscustomobject]@{ Exe = Join-Path $env:windir 'System32\fsutil.exe'; Directory = $Context.Root }
    $sparse = New-TestFile (Join-Path $root 'sparse.bin') 0
    $probe = Invoke-TestProcess $utility @('sparse', 'setflag', $sparse)
    if ($probe.ExitCode -ne 0) { throw 'Could not create the sparse fixture; see the command output.' }
    $stream = [IO.File]::Open($sparse, 'Open', 'ReadWrite')
    try { $stream.SetLength(4MB) } finally { $stream.Dispose() }
    $utility.Exe = Join-Path $env:windir 'System32\compact.exe'
    $compressed = New-TestFile (Join-Path $root 'compressed.bin') 1MB -Zeros
    $probe = Invoke-TestProcess $utility @('/c', $compressed)
    if ($probe.ExitCode -ne 0) { throw 'Could not create the compressed fixture; see the command output.' }
    $wof = New-TestFile (Join-Path $root 'wof.bin') 2MB -Zeros
    $probe = Invoke-TestProcess $utility @('/c', '/exe:LZX', $wof)
    if ($probe.ExitCode -ne 0 -or [WdsNativeFs]::GetWofAlgorithm($wof) -ne 1) {
        throw 'Could not create a WOF-backed fixture; see the command output.'
    }
    $original = New-TestFile (Join-Path $root 'hardlink-original.bin') 12289
    New-Item -ItemType HardLink -Path (Join-Path $root 'hardlink-alias.bin') -Target $original | Out-Null
    [IO.File]::WriteAllText("${original}:Zone.Identifier", "[ZoneTransfer]`r`nZoneId=3")
    $snapshot = Get-DiskSnapshot $root
    Assert-True (($snapshot | Where-Object Relative -eq 'sparse.bin').Physical -lt 4MB) `
        'The sparse fixture has real holes'
    $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = $Case.Engine } }
    $rows = Invoke-ScanReport $runner @($root) 'allocations'
    Assert-ScanSnapshot $rows $snapshot $root -Physical
    Assert-Equal @($rows | Where-Object Name -Like '*Zone.Identifier*').Count 0 `
        'Alternate streams do not become standalone files'
    $before = @($rows | ForEach-Object { "$($_.Name)|$($_.'Logical Size')|$($_.'Physical Size')" } | Sort-Object)
    Set-TestSettings $runner @{ Options = @{ UseFastScanEngine = $Case.Engine; ProcessHardlinks = 1 } }
    $accounted = Invoke-ScanReport $runner @($root) 'hardlink-accounting'
    $after = @($accounted | ForEach-Object { "$($_.Name)|$($_.'Logical Size')|$($_.'Physical Size')" } | Sort-Object)
    Assert-Sequence $after $before 'Exported bytes remain comparable when UI hardlink accounting is enabled'
}

function Test-FilterTraversal {
    param($Context, $Case)
    $root = New-TestTree
    $hidden = New-TestFile (Join-Path $root 'alpha\nested\hidden.txt') 911
    [IO.File]::SetAttributes($hidden, [IO.FileAttributes]::Hidden)
    $runner = New-TestRunner @{
        Options = @{ UseFastScanEngine = $Case.Engine; ExcludeHiddenFile = 1 }
        DriveSelect = @{ FilteringIncludeDirs = Join-Path $root 'alpha\nested'; FilteringExcludeFiles = '*.dat' }
    }
    $rows = Invoke-ScanReport $runner @($root) 'filtered-empty'
    Assert-Equal @(Get-ReportFiles $rows).Count 0 'Traversal anchors do not bypass hidden-file and filename exclusions'
    $wanted = New-TestFile (Join-Path $root 'alpha\nested\retained.txt') 923
    $rows = Invoke-ScanReport $runner @($root) 'filtered-retained'
    Assert-Sequence @(Get-ReportFiles $rows | ForEach-Object Name) @($wanted) `
        'A nested include anchor reaches its matching file without admitting sibling branches'
    Assert-Equal ([long]$rows[0].'Logical Size') 923L 'Filtered recursive totals reflect only retained bytes'
}

function Test-RegexDirectoryFilters {
    param($Context, $Case)
    $root = $Context.Fixture
    $numeric = New-TestFile (Join-Path $root '123-abc\file.bin') 101
    $descendant = New-TestFile (Join-Path $root '123-abc\nested\file.bin') 103
    $repeated = New-TestFile (Join-Path $root 'abc-abc\file.bin') 107
    $spaced = New-TestFile (Join-Path $root 'abc xyz\file.bin') 109
    New-TestFile (Join-Path $root 'abc-123\file.bin') 113 | Out-Null
    New-TestFile (Join-Path $root 'abc-xyz\file.bin') 127 | Out-Null
    $patterns = @(
        @{ Regex = '\\(\d+)-\w+\b$'; Files = @($numeric, $descendant) },
        @{ Regex = '\\(\w+)-\1$'; Files = @($repeated) },
        @{ Regex = '\\\w+\s+\w+$'; Files = @($spaced) }
    )
    for ($index = 0; $index -lt $patterns.Count; ++$index) {
        $pattern = [regex]::Escape($root) + $patterns[$index].Regex
        $runner = New-TestRunner @{
            Options = @{ UseFastScanEngine = $Case.Engine; FilteringUseRegex = 1 }
            DriveSelect = @{ FilteringIncludeDirs = $pattern }
        }
        $rows = Invoke-ScanReport $runner @($root) "regex-$index"
        Assert-Sequence @(Get-ReportFiles $rows | ForEach-Object Name | Sort-Object) `
            @($patterns[$index].Files | Sort-Object) `
            'Directory regex escapes retain their meaning and include matching descendants'
    }
}

function Test-DeniedDirectory {
    param($Context, $Case)
    $visible = New-TestFile (Join-Path $Context.Fixture 'a-good\visible.txt') 317
    $blocked = Join-Path $Context.Fixture 'z-denied'
    New-TestFile (Join-Path $blocked 'private.txt') 331 | Out-Null
    $empty = Join-Path $Context.Fixture 'empty'
    [IO.Directory]::CreateDirectory($empty) | Out-Null
    $acl = Get-Acl -LiteralPath $blocked
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    try { $sid = $identity.User } finally { $identity.Dispose() }
    $denied = Get-Acl -LiteralPath $blocked
    $denied.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,
        [Security.AccessControl.FileSystemRights]::ListDirectory, [Security.AccessControl.AccessControlType]::Deny))
    Add-Cleanup 'Restore directory access' {
        param($Directory, $Security)
        Set-Acl -LiteralPath $Directory -AclObject $Security
    } @($blocked, $acl)
    Set-Acl -LiteralPath $blocked -AclObject $denied
    $runner = New-TestRunner
    $report = Join-Path $Context.Root "access-denied.$($Case.Format)"
    $result = Invoke-TestProcess $runner @('/saveto', $report, $Context.Fixture)
    Assert-Equal $result.ExitCode 1 'An unreadable branch makes the saved scan incomplete'
    $rows = Read-ScanReport $report
    Assert-Sequence @(Get-ReportFiles $rows | ForEach-Object Name) @($visible) `
        'An unreadable directory neither hangs enumeration nor hides readable siblings'
    Assert-Equal @($rows | Where-Object Name -IEQ $blocked).Count 1 'The inaccessible directory remains visible'
    $failed = @($rows | Where-Object Name -IEQ $blocked)[0]
    Assert-Equal $failed.ScanOutcome 'enumeration-error' 'The failed directory owns its enumeration outcome'
    Assert-True ([Uri]::UnescapeDataString($failed.ScanError).Contains($blocked)) `
        'The report preserves the failing filesystem path and system error'
    Assert-True (([Convert]::ToUInt32($failed.'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -ne 0) `
        'The unreadable branch carries the shared scan-error flag'
    Assert-True (([Convert]::ToUInt32($rows[0].'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -ne 0) `
        'The ancestor reflects incomplete descendants'
    Assert-Equal $rows[0].ScanOutcome 'complete' 'The parent listing has its own successful outcome'
    Assert-Equal @($rows | Where-Object Name -IEQ $empty)[0].ScanOutcome 'complete' `
        'An empty directory is a successful enumeration'

    $directReport = Join-Path $Context.Root "denied-root.$($Case.Format)"
    $result = Invoke-TestProcess $runner @('/saveto', $directReport, $blocked)
    Assert-Equal $result.ExitCode 1 'An inaccessible root still produces an incomplete report'
    Assert-Equal (Read-ScanReport $directReport)[0].ScanOutcome 'enumeration-error' `
        'Root enumeration failures retain their diagnostics'
    $filteredRunner = New-TestRunner @{ DriveSelect = @{ FilteringExcludeDirs = $blocked } }
    $filtered = Invoke-ScanReport $filteredRunner @($Context.Fixture) 'excluded-denied' -Format $Case.Format
    Assert-Equal @($filtered | Where-Object {
        ([Convert]::ToUInt32($_.'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -ne 0
    }).Count 0 'An intentional directory exclusion does not mark the scan incomplete'

    # Inspect and refresh the loaded model through targeted messages while its main window stays hidden.
    $process = Start-TestProcess $runner @('/loadfrom', $report) -Hidden
    $app = [pscustomobject]@{
        Process = $process; Runner = $runner; Window = [IntPtr]::Zero; Roots = @($Context.Fixture)
        ExpectExit = $false; TreeList = [IntPtr]::Zero; ExtensionList = [IntPtr]::Zero
    }
    $Context.Apps.Add($app)
    $app.Window = Wait-Until {
        @([TestDesktop]::Windows($process.Id) | Where-Object {
            [TestDesktop]::Class($_) -like 'WdsWindow.*' -and [TestDesktop]::Text($_) -like '*WinDirStat*'
        } | Select-Object -First 1)
    } 'The background process creates its main window'
    $list = Wait-Until {
        foreach ($control in [TestDesktop]::Children($app.Window)) {
            if ([TestDesktop]::Class($control) -ne 'SysListView32') { continue }
            if ('z-denied (Scan incomplete)' -in [NativeListViewHelper]::GetItemTexts($control)) { return $control }
        }
    } 'The loaded model preserves the failed local branch and its label'
    Assert-True (-not [TestDesktop]::IsWindowVisible($app.Window)) 'The validation does not activate a desktop window'
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]@((Find-AppRow $list 'z-denied (Scan incomplete)')))) `
        'The inaccessible branch remains selectable'
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Assert-True ('z-denied (Scan incomplete)' -in [NativeListViewHelper]::GetItemTexts($list)) `
        'Refreshing an inaccessible branch retains it with its error state'
    Assert-Equal @(Get-AppDialog $app).Count 0 'Local scan failures do not open a blocking error dialog'
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]@((Find-AppRow $list 'a-good')))) `
        'The healthy branch is selected in the loaded model'
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Assert-True ('z-denied (Scan incomplete)' -in [NativeListViewHelper]::GetItemTexts($list)) `
        'Refreshing a healthy sibling preserves the failed branch'
    Assert-True ([NativeListViewHelper]::GetItemTexts($list)[0].EndsWith(' (Scan incomplete)')) `
        'Refreshing a healthy sibling preserves ancestor incompleteness'

    Set-Acl -LiteralPath $blocked -AclObject $acl
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]@((Find-AppRow $list 'z-denied (Scan incomplete)')))) `
        'The repaired branch is selected without sending desktop keystrokes'
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Assert-True ('z-denied' -in [NativeListViewHelper]::GetItemTexts($list)) `
        'A successful branch refresh clears its error label'
    Assert-True (-not [NativeListViewHelper]::GetItemTexts($list)[0].EndsWith(' (Scan incomplete)')) `
        'A successful branch refresh clears ancestor incompleteness'
    Assert-Equal ([NativeListViewHelper]::GetItemTexts($list, 6)[(Find-AppRow $list 'z-denied')]) '1' `
        'The refreshed branch includes its previously unreadable file'
    Assert-True (-not [TestDesktop]::IsWindowVisible($app.Window)) 'The recovery check leaves the main window hidden'
}

foreach ($engine in 0, 1) {
    $engineName = if ($engine) { 'fast' } else { 'basic' }
    Register-Scenario "fs.paths.$engineName" Filesystem 'Long paths, Unicode, aliases, and exact enumeration' `
        Test-EnumerationContract -Tags Filesystem -Data @{ Engine = $engine }
    Register-Scenario "fs.roots.$engineName" Filesystem 'Overlapping and similarly named multi-root targets' `
        Test-OverlappingRoots -Tags Filesystem -Data @{ Engine = $engine }
    Register-Scenario "fs.junctions.$engineName" Filesystem 'Reparse boundaries and externally replaced targets' `
        Test-ReparseBoundary -Tags Filesystem,External -Requires Windows,Ntfs -Data @{ Engine = $engine }
    Register-Scenario "fs.allocation.$engineName" Filesystem 'Sparse, compressed, WOF, ADS, and hardlink accounting' `
        Test-AllocatedSizes -Tags Filesystem -Requires Windows,Ntfs,Compression -Data @{ Engine = $engine }
    Register-Scenario "fs.filters.$engineName" Filesystem 'Nested include anchors across exclusion boundaries' `
        Test-FilterTraversal -Tags Filesystem -Data @{ Engine = $engine }
    Register-Scenario "fs.regex.$engineName" Filesystem 'Regex escapes and backreferences in directory filters' `
        Test-RegexDirectoryFilters -Tags Filesystem -Data @{ Engine = $engine }
}
Register-Scenario fs.symlinks Filesystem 'File and directory symlinks share targets without losing their paths' `
    Test-SymbolicLinks -Tags Filesystem -Requires Windows,Ntfs
Register-Scenario fs.access-denied Filesystem 'Access-denied enumeration preserves visible siblings' `
    Test-DeniedDirectory -Tags Filesystem -Requires Windows,Ntfs -Data @{ Format = 'csv' }
Register-Scenario fs.access-denied.json Filesystem 'Local failure diagnostics survive JSON reload and scoped recovery' `
    Test-DeniedDirectory -Tags Filesystem -Requires Windows,Ntfs -Data @{ Format = 'json' }
