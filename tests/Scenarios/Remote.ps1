function New-RemoteRunner {
    param([hashtable] $Settings = @{}, [string] $Protocol = 'webdav', [switch] $NoCredentials)
    $runner = New-TestRunner $Settings
    $credentials = Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__settings"
    $runner | Add-Member Environment @{}
    if (-not $NoCredentials) {
        $user = if ($Protocol -eq 'azure') { 'windirstat' } else { 'fixture' }
        $secret = if ($Protocol -eq 'azure') { $credentials.key } else { 'windirstat-test-password' }
        foreach ($mode in '', 'denied', 'malformed', 'missing-metadata', 'entities', 'loop', 'partial',
            'retry', 'encoded', 'slow', 'foreign', 'escape', 'oversize', 'oversize-queued',
            'oversize-escaped', 'empty-date', 'delete-partial', 'parallel') {
            Set-StorageTestCredential (Get-RemoteTarget $Protocol -Mode $mode) $user $secret
        }
        if ($Protocol -eq 'webdav') {
            Set-StorageTestCredential (Get-RemoteTarget webdav 'unicode/') $user $secret
        }
    }
    return $runner
}

function Get-RemoteTarget {
    param([string] $Protocol, [string] $Prefix = '', [string] $Mode)
    if ($Protocol -eq 'webdav') {
        if ($Mode) { return "$env:WDS_REMOTE_TEST_ENDPOINT/$Mode/" }
        return $env:WDS_REMOTE_DAV_ENDPOINT + [Uri]::EscapeDataString($Prefix).Replace('%2F', '/')
    }
    $endpoint = if ($Mode) { $env:WDS_REMOTE_TEST_ENDPOINT } else { $env:WDS_REMOTE_AZURE_ENDPOINT }
    $container = if ($Mode) { $Mode } else { 'objects' }
    return "azure://windirstat/$container/" + [Uri]::EscapeDataString($Prefix).Replace('%2F', '/') +
        '?endpoint=' + [Uri]::EscapeDataString($endpoint)
}

function Assert-RemoteObjects {
    param([object[]] $Rows, [string] $Protocol, [string] $Prefix = '')
    $manifest = (Invoke-WebRequest "$env:WDS_REMOTE_TEST_ENDPOINT/__manifest").Content | ConvertFrom-Json -AsHashtable
    $expected = $manifest[$Protocol]
    $root = (Get-RemoteTarget $Protocol -Prefix $Prefix) -split '\?', 2 | Select-Object -First 1
    $base = (Get-RemoteTarget $Protocol) -split '\?', 2 | Select-Object -First 1
    $files = @(Get-ReportFiles $Rows | Where-Object { $_.Name.StartsWith($root, [StringComparison]::Ordinal) })
    $names = @($expected.Keys | Where-Object { $_.StartsWith($Prefix, [StringComparison]::Ordinal) })
    Assert-Equal $files.Count $names.Count 'Every remote file is enumerated exactly once'
    $actual = [Collections.Generic.Dictionary[string,long]]::new([StringComparer]::Ordinal)
    foreach ($file in $files) {
        $name = [Uri]::UnescapeDataString(($file.Name -split '\?', 2)[0].Substring($base.Length))
        Assert-True (-not $actual.ContainsKey($name)) "Distinct remote identity: $name"
        $actual.Add($name, [long]$file.'Logical Size')
    }
    foreach ($name in $names) {
        Assert-True $actual.ContainsKey($name) "Exact name is preserved: $name"
        Assert-Equal $actual[$name] ([long]$expected[$name]) "Metadata size matches the server: $name"
    }
    Assert-True (-not ($Rows | Where-Object Name -Match 'windirstat-test-password|AccountKey=|[?&]sig=')) `
        'Reports contain connection identities without credentials'
}

function Test-RemoteEnumeration {
    param($Context, $Case)
    $protocol = $Case.Protocol
    $runner = New-RemoteRunner @{ DupeView = @{ ScanForDuplicates = 1 } } -Protocol $protocol
    $uri = Get-RemoteTarget $protocol
    $rows = Invoke-ScanReport $runner @($uri) 'full'
    Assert-RemoteObjects $rows $protocol
    $rows = Invoke-ScanReport $runner @((New-TestTree), $uri, (Get-RemoteTarget $protocol 'unicode/')) 'mixed'
    Assert-RemoteObjects $rows $protocol
    Assert-Equal @((Get-ReportFiles $rows) | Where-Object Name -NotMatch '^(azure|https?)://').Count 10 `
        'Mixed scans retain local files and deduplicate overlapping remote roots'
    if ($protocol -eq 'azure') {
        Set-StorageTestCredential (Get-RemoteTarget azure) 'windirstat' `
            (Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__settings").sas
        Assert-RemoteObjects (Invoke-ScanReport $runner @((Get-RemoteTarget $protocol 'unicode/')) 'sas') `
            $protocol 'unicode/'
        Assert-RemoteObjects (Invoke-ScanReport $runner @((Get-RemoteTarget $protocol 'prefix-only')) 'prefix') `
            $protocol 'prefix-only'
    }
}

function Test-RemoteFiltering {
    param($Context, $Case)
    $protocol = $Case.Protocol
    foreach ($regex in 0, 1) {
        $runner = New-RemoteRunner -Protocol $protocol
        $path = (Get-RemoteTarget $protocol 'unicode/') -split '\?', 2 | Select-Object -First 1
        $pattern = if ($regex) { [regex]::Escape($path) + '$' } else { $path }
        Set-TestSettings $runner @{ Options = @{ FilteringUseRegex = $regex }
            DriveSelect = @{ FilteringIncludeDirs = $pattern } }
        Assert-RemoteObjects (Invoke-ScanReport $runner @((Get-RemoteTarget $protocol)) "filtered-$regex") `
            $protocol 'unicode/'
        Set-TestSettings $runner @{ Options = @{ FilteringUseRegex = $regex }
            DriveSelect = @{ FilteringIncludeDirs = ''; FilteringExcludeDirs = $pattern } }
        $files = @(Get-ReportFiles (Invoke-ScanReport $runner @((Get-RemoteTarget $protocol)) "excluded-$regex"))
        Assert-Equal @($files | Where-Object Name -Like '*unicode/*').Count 0 `
            'Excluded directory descendants are absent'
        Assert-True ($files.Count -gt 1000) 'Unrelated remote directories are retained'
    }
    if ($protocol -eq 'azure') {
        $runner = New-RemoteRunner @{ DriveSelect = @{
            FilteringIncludeDirs = 'azure://windirstat/objects/prefix-only/'
        } } -Protocol $protocol
        $rows = Invoke-ScanReport $runner @((Get-RemoteTarget $protocol 'prefix-on')) 'partial-prefix-filter'
        $files = @(Get-ReportFiles $rows)
        Assert-Equal $files.Count 1 'Directory filters traverse roots ending within an object-key component'
        Assert-Equal ([long]$files[0].'Logical Size') 13 'Only the included descendant is retained'
    }
}

function Test-RemoteFailures {
    param($Context, $Case)
    $protocol = $Case.Protocol
    $runner = New-RemoteRunner -Protocol $protocol
    $modes = @('denied', 'malformed', 'missing-metadata', 'entities')
    $modes += if ($protocol -eq 'azure') { @('loop', 'partial') } else {
        @('foreign', 'escape', 'oversize', 'oversize-queued', 'oversize-escaped')
    }
    foreach ($mode in $modes) {
        $report = Join-Path $Context.Root "$mode.csv"
        $result = Invoke-TestProcess $runner @('/saveto', $report, (Get-RemoteTarget $protocol -Mode $mode))
        Assert-Equal $result.ExitCode 1 "$mode cannot produce a successful scan"
        $rows = Read-ScanReport $report
        Assert-True (([Convert]::ToUInt32($rows[0].'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -ne 0) `
            'Incomplete results retain their error state'
        if ($mode -eq 'partial') {
            Assert-True (@(Get-ReportFiles $rows).Count -gt 0) 'Earlier pages survive a later failure'
        }
    }
    $retry = Invoke-ScanReport $runner @((Get-RemoteTarget $protocol -Mode 'retry')) 'retried'
    Assert-Equal @(Get-ReportFiles $retry).Count 1 'Transient failures retry without duplicate entries'
    if ($protocol -eq 'azure') {
        $encoded = Invoke-ScanReport $runner @((Get-RemoteTarget $protocol -Mode 'encoded')) 'encoded'
        $name = @(Get-ReportFiles $encoded)[0].Name
        Assert-True ($name.Contains('special') -and $name.Contains('%252F.txt')) `
            'Encoded XML names are decoded exactly once'
        Set-StorageTestCredential (Get-RemoteTarget azure) 'windirstat' ([Convert]::ToBase64String([byte[]]::new(32)))
    } else { Set-StorageTestCredential (Get-RemoteTarget webdav) 'fixture' 'incorrect' }
    $result = Invoke-TestProcess $runner @('/saveto', (Join-Path $Context.Root 'unauthorized.csv'),
        (Get-RemoteTarget $protocol))
    Assert-Equal $result.ExitCode 1 'Real servers reject incorrect credentials'
    & cmdkey.exe ('/delete:WinDirStat/' + (Get-RemoteTarget $protocol)) | Out-Null
    if ($protocol -eq 'azure') {
        $credentials = Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__settings"
        $runner.Environment.AZURE_STORAGE_ACCOUNT = 'windirstat'
        $runner.Environment.AZURE_STORAGE_KEY = $credentials.key
        $runner.Environment.AZURE_STORAGE_SAS_TOKEN = $credentials.sas
    } else {
        $runner.Environment.WEBDAV_USERNAME = 'fixture'
        $runner.Environment.WEBDAV_PASSWORD = 'windirstat-test-password'
    }
    $result = Invoke-TestProcess $runner @('/saveto', (Join-Path $Context.Root 'environment.csv'),
        (Get-RemoteTarget $protocol))
    Assert-Equal $result.ExitCode 1 'Environment credentials cannot supply a missing connection credential'
    Assert-Equal @(Get-ReportFiles (Read-ScanReport (Join-Path $Context.Root 'environment.csv'))).Count 0 `
        'A scan without supplied credentials cannot return anonymous results'
}

function Test-WebDavCollectionMetadata {
    param($Context, $Case)
    $rows = Invoke-ScanReport (New-RemoteRunner) @((Get-RemoteTarget webdav -Mode empty-date)) 'empty-date'
    Assert-Equal ([long]$rows[0].Files) 0 'An empty collection remains empty'
    Assert-Equal $rows[0].'Last Change' '2025-01-01T00:00:00Z' 'The collection retains its own modification time'
}

function Test-RemoteRoundTrip {
    param($Context, $Case)
    $protocol = $Case.Protocol
    $runner = New-RemoteRunner -Protocol $protocol
    $uri = Get-RemoteTarget $protocol
    $source = Invoke-ScanReport $runner @($uri) 'source' -Format $Case.Format
    $app = Start-TestApp -Runner $runner -Arguments @('/loadfrom', (Join-Path $Context.Root "source.$($Case.Format)"))
    $app.Roots = @($uri)
    $loaded = Save-AppReport $app 'loaded' -Format $Case.Format
    $fields = @('Name', 'Files', 'Folders', 'Logical Size', 'Physical Size', 'Last Change', 'WinDirStat Attributes')
    Assert-Sequence (Get-ReportSignature $loaded $fields) (Get-ReportSignature $source $fields) `
        'Remote identities, metadata, empty directories, and marker objects survive save/reload'
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('unicode')
    Assert-True (Get-AppCommandState $app 'ID_CLEANUP_DELETE').Enabled 'Remote children support permanent deletion'
    foreach ($command in 'ID_CLEANUP_DELETE_BIN', 'ID_CLEANUP_MOVE_TO', 'ID_CLEANUP_OPEN_SELECTED', 'ID_COMPUTE_HASH') {
        Assert-True (-not (Get-AppCommandState $app $command).Enabled) `
            'Filesystem actions are unavailable for remote items'
    }
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Assert-RemoteObjects (Save-AppReport $app 'refreshed') $protocol
    Save-Frame ([TestDesktop]::Capture($app.Window)) (Join-Path $Context.Root 'remote-results.png')
}

function Test-RemoteConnection {
    param($Context, $Case)
    $protocol = $Case.Protocol
    $runner = New-RemoteRunner @{ Options = @{ FontSizePercent = $Case.Font } } -NoCredentials
    $credentials = Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__settings"
    $secret = if ($Case.Sas) { $credentials.sas }
        elseif ($protocol -eq 'azure') { $credentials.key }
        else { 'windirstat-test-password' }
    if ($protocol -eq 'azure') {
        $runner.Environment.AZURE_STORAGE_ACCOUNT = 'windirstat'
        $runner.Environment.AZURE_STORAGE_KEY = $credentials.key
        $runner.Environment.AZURE_STORAGE_SAS_TOKEN = $credentials.sas
    }
    $runner.Environment.WEBDAV_PASSWORD = 'windirstat-test-password'
    $runner.Environment.WEBDAV_USERNAME = 'fixture'
    $app = Start-TestApp -Runner $runner -Roots @((New-TestTree))
    $select = Open-AppDialog $app 'ID_FILE_SELECT' 'IDC_CLOUD_STORAGE'
    $button = Get-DialogControl $select 'IDC_CLOUD_STORAGE'
    [void][TestDesktop]::PostMessage($button, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    $menu = Wait-Until {
        [TestDesktop]::Windows($app.Process.Id) | Where-Object { [TestDesktop]::Class($_) -eq '#32768' } |
            Select-Object -First 1
    } 'The cloud storage menu opens' -Seconds 10
    $key = if ($protocol -eq 'azure') { 65 } else { 87 }
    [void][TestDesktop]::PostMessage($menu, 0x0100, [IntPtr]$key, [IntPtr]::Zero)
    $dialog = Wait-Until { Get-AppDialog $app 'IDC_REMOTE_SECRET' } 'The connection editor opens' -Seconds 10
    Assert-True ([TestDesktop]::Message($dialog, 0x007F).ToInt64() -ne 0) `
        'The connection editor displays a title-bar icon'
    Assert-Equal ([TestDesktop]::Message($dialog, 0x007F)) ([TestDesktop]::Message($select, 0x007F)) `
        'The connection editor uses the WinDirStat title-bar icon'
    $target = Get-RemoteTarget $protocol 'unicode/'
    $values = @{ IDC_REMOTE_SECRET = $secret }
    if ($protocol -eq 'azure') {
        $values.IDC_REMOTE_ACCOUNT = 'windirstat'; $values.IDC_REMOTE_CONTAINER = 'objects'
        $values.IDC_REMOTE_PREFIX = 'unicode/'; $values.IDC_REMOTE_ENDPOINT = $env:WDS_REMOTE_AZURE_ENDPOINT
    } else {
        $values.IDC_REMOTE_ACCOUNT = 'fixture'; $values.IDC_REMOTE_ENDPOINT = $target
    }
    foreach ($entry in $values.GetEnumerator()) {
        [TestDesktop]::SetText((Get-DialogControl $dialog $entry.Key), $entry.Value)
    }
    foreach ($control in 'IDC_REMOTE_ENDPOINT', 'IDC_REMOTE_ACCOUNT', 'IDC_REMOTE_SECRET') {
        $field = Get-DialogControl $dialog $control
        [TestDesktop]::SetText($field, '')
        Assert-True (-not [TestDesktop]::IsWindowEnabled([TestDesktop]::GetDlgItem($dialog, 1))) `
            'Connections require an explicit endpoint and credentials even with valid environment defaults'
        [TestDesktop]::SetText($field, $values[$control])
    }
    Assert-True ([TestDesktop]::IsWindowEnabled([TestDesktop]::GetDlgItem($dialog, 1))) `
        'Connections accept supplied settings and credentials'
    if ($Case.Font -eq 100) {
        Set-DialogCheck $dialog 'IDC_REMOTE_REMEMBER' $true
        $credential = 'WinDirStat/' + $(if ($protocol -eq 'azure') { Get-RemoteTarget $protocol } else { $target })
        Add-Cleanup 'Remove fixture-only saved credential' {
            param($name); & cmdkey.exe "/delete:$name" | Out-Null
        } @($credential)
    }
    [TestDesktop]::Expose($dialog)
    Save-Frame ([TestDesktop]::Capture($dialog)) (Join-Path $Context.Root 'connection.png')
    Close-AppDialog $dialog 1
    foreach ($control in 'IDC_FAST_SCAN_CHECKBOX', 'IDC_SCAN_DUPLICATES') {
        Assert-True (-not [TestDesktop]::IsWindowVisible(
            [TestDesktop]::GetDlgItem($select, (Get-ResourceId $control)))) `
            'Filesystem-only scan options are hidden for remote connections'
    }
    Close-AppDialog $select 1
    Wait-AppIdle $app
    $app.Roots = @($target)
    Assert-RemoteObjects (Save-AppReport $app 'connected') $protocol 'unicode/'
    if ($Case.Font -eq 100) {
        Assert-RemoteObjects (Invoke-ScanReport $runner @($target) 'remembered') $protocol 'unicode/'
    }
    Assert-True (-not [IO.File]::ReadAllText($runner.Ini).Contains($secret)) `
        'Credentials are absent from settings and history'
}

function Test-RemoteCancellation {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__control?slow=1" | Out-Null
    Add-Cleanup 'Reset delayed remote response' {
        Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__control?slow=0" | Out-Null
    }
    $before = (Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__stats").slow
    $app = Start-TestApp -Runner (New-RemoteRunner -Protocol $Case.Protocol) `
        -Roots @((Get-RemoteTarget $Case.Protocol -Mode 'slow')) -DuringScan
    Wait-Until { (Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__stats").slow -gt $before } `
        'A listing is delayed' | Out-Null
    Invoke-AppCommand $app 'ID_SCAN_SUSPEND'
    Wait-Until { (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled } `
        'Pause interrupts remote I/O' -Seconds 5 | Out-Null
    Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__control?slow=0" | Out-Null
    Invoke-AppCommand $app 'ID_SCAN_RESUME'
    Wait-AppIdle $app
    Assert-Equal @(Get-ReportFiles (Save-AppReport $app 'resumed')).Count 1 `
        'Resume retries the interrupted directory once'
}

function Get-StorageDeletionTarget {
    param([string] $Protocol)
    if ($Protocol -eq 's3') { return Get-S3Target -Bucket 'windirstat-delete' }
    if ($Protocol -eq 'azure') { return (Get-RemoteTarget azure).Replace('/objects/', '/deleteobjects/') }
    return $env:WDS_REMOTE_DAV_ENDPOINT -replace '/dav/$', '/delete/'
}

function Get-StorageDeletionNames {
    param([object[]] $Rows, [string] $Target)
    $base = ($Target -split '\?', 2)[0]
    return @(Get-ReportFiles $Rows | Where-Object { $_.Name.StartsWith($base, [StringComparison]::Ordinal) } |
        ForEach-Object { [Uri]::UnescapeDataString(($_.Name -split '\?', 2)[0].Substring($base.Length)) } |
        Sort-Object -CaseSensitive)
}

function Get-StorageDeletionError {
    param([object] $App, [switch] $AnyError)
    foreach ($dialog in [TestDesktop]::Windows($App.Process.Id)) {
        if ([TestDesktop]::Class($dialog) -ne '#32770' -or -not [TestDesktop]::IsWindowVisible($dialog)) { continue }
        $children = [TestDesktop]::Children($dialog)
        $message = ($children | ForEach-Object { [TestDesktop]::Text($_) }) -join "`n"
        $hasOk = @($children | Where-Object {
            [TestDesktop]::Class($_) -eq 'Button' -and [TestDesktop]::Text($_).Replace('&', '').Trim() -eq 'OK'
        }).Count -gt 0
        if ($message -match 'HTTP \d+' -or $AnyError -and $hasOk) {
            return [pscustomobject]@{ Dialog = $dialog; Message = $message }
        }
    }
}

function Test-StorageDeletion {
    param($Context, $Case)
    $protocol = $Case.Protocol
    $endpoint = if ($protocol -eq 's3') { $env:WDS_S3_TEST_ENDPOINT } else { $env:WDS_REMOTE_TEST_ENDPOINT }
    Invoke-RestMethod "$endpoint/__control?seed=delete&delete=" | Out-Null
    $settings = @{ Options = @{ ShowMicrosoftProgress = [int]$Case.Microsoft }
        DriveSelect = @{ FilteringExcludeFiles = 'filtered.bin' } }
    $runner = if ($protocol -eq 's3') { New-S3Runner $settings } else {
        New-RemoteRunner $settings -Protocol $protocol
    }
    $target = Get-StorageDeletionTarget $protocol
    if ($protocol -eq 's3') {
        Set-StorageTestCredential $target 'windirstat-test' 'windirstat-test-secret' 'temporary-test-token'
    } elseif ($protocol -eq 'azure') {
        $credentials = Invoke-RestMethod "$endpoint/__settings"
        $secret = if ($Case.Sas) { $credentials.deleteSas } else { $credentials.key }
        Set-StorageTestCredential $target 'windirstat' $secret
    } else { Set-StorageTestCredential $target 'fixture' 'windirstat-test-password' }
    $local = $Context.Fixture
    $localDelete = New-TestFile (Join-Path $local 'local-delete.txt') 73
    $localKeep = New-TestFile (Join-Path $local 'local-keep.txt') 79
    $localEmpty = Join-Path $local 'local-empty'
    New-TestFile (Join-Path $localEmpty 'member.txt') 83 | Out-Null
    $app = Start-TestApp -Runner $runner -Roots @($target, $local)
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @($target)
    foreach ($command in 'ID_CLEANUP_DELETE', 'ID_CLEANUP_EMPTY_FOLDER', 'ID_CLEANUP_DELETE_BIN') {
        Assert-True (-not (Get-AppCommandState $app $command).Enabled) 'Remote scan roots cannot be deleted or emptied'
    }
    Expand-AppDirectory $app $list $target 'bulk'
    Expand-AppDirectory $app $list $local 'local-delete.txt'

    # Select exact case without the harness's Windows filename normalization, together with a local file.
    $texts = [NativeListViewHelper]::GetItemTexts($list)
    $indices = @(for ($i = 0; $i -lt $texts.Length; ++$i) {
        if ($texts[$i] -ceq 'Case.txt' -or $texts[$i] -ceq 'local-delete.txt') { $i }
    })
    Assert-Equal $indices.Count 2 'An exact remote object and a local file form a mixed selection'
    [void][NativeListViewHelper]::FocusListView($list)
    Assert-True ([NativeListViewHelper]::SelectItems($list, [int[]]$indices)) 'The mixed deletion is selected'
    Assert-True (-not (Get-AppCommandState $app 'ID_CLEANUP_DELETE_BIN').Enabled) 'Remote selections have no recycle action'
    Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
    Wait-AppIdle $app
    Assert-True (-not [IO.File]::Exists($localDelete) -and [IO.File]::Exists($localKeep)) `
        'Mixed deletion removes only the selected local file'
    $names = Get-StorageDeletionNames (Save-AppReport $app 'case') $target
    Assert-True ('Case.txt' -cnotin $names -and 'case.txt' -cin $names) 'Remote deletion preserves case-sensitive siblings'

    Expand-AppDirectory $app $list 'unicode' 'café 雪.txt'
    Expand-AppDirectory $app $list 'odd' '%23 &+.txt'
    Select-AppRows $app $list @('café 雪.txt', '%23 &+.txt')
    if ($protocol -eq 's3') { $before = Invoke-RestMethod "$endpoint/__stats" }
    Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
    Wait-AppIdle $app
    if ($protocol -eq 's3') {
        $after = Invoke-RestMethod "$endpoint/__stats"
        Assert-Equal ($after.batches - $before.batches) 1 'Selected files from separate folders share one batch'
        Assert-Equal ($after.deletions - $before.deletions) 1 'Selected exact identities require one destructive request'
    }
    $names = Get-StorageDeletionNames (Save-AppReport $app 'encoded') $target
    Assert-True ('unicode/café 雪.txt' -cnotin $names -and 'odd/%23 &+.txt' -cnotin $names) `
        'Unicode, percent escapes, spaces, ampersands, and plus signs retain exact deletion identities'
    if ($protocol -eq 's3') {
        Select-AppRows $app $list @('a\b|c"d&%? #.txt', 'dots')
        Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
        Wait-AppIdle $app
        $names = Get-StorageDeletionNames (Save-AppReport $app 'path-syntax') $target
        Assert-True ('odd/a\b|c"d&%? #.txt' -cnotin $names -and 'dots/../file' -cnotin $names) `
            'Object deletion retains literal backslashes and dot path components'
    }

    Select-AppRows $app $list @('empty', 'local-empty')
    Invoke-AppCommand $app 'ID_CLEANUP_EMPTY_FOLDER'
    Wait-AppIdle $app
    $names = Get-StorageDeletionNames (Save-AppReport $app 'emptied') $target
    Assert-True ('empty/member.txt' -cnotin $names -and (Find-AppRow $list 'empty' -Optional) -ge 0) `
        'Empty folder preserves the selected collection'
    Assert-True ([IO.Directory]::Exists($localEmpty) -and [IO.Directory]::GetFileSystemEntries($localEmpty).Length -eq 0) `
        'Mixed empty folder also preserves the local folder while removing its contents'
    if ($protocol -ne 'webdav') { Assert-True ('empty/' -cin $names) 'Empty folder preserves its own marker object' }

    # Distinguish a directory from a blob with the same display name using their available actions.
    $texts = [NativeListViewHelper]::GetItemTexts($list)
    $directory = @(for ($i = 0; $i -lt $texts.Length; ++$i) {
        if ($texts[$i] -cne 'same') { continue }
        [void][NativeListViewHelper]::SelectItems($list, [int[]]@($i))
        if ((Get-AppCommandState $app 'ID_CLEANUP_EMPTY_FOLDER').Enabled) { $i }
    })
    Assert-Equal $directory.Count 1 'The directory sharing a blob name is independently selectable'
    [void][NativeListViewHelper]::SelectItems($list, [int[]]$directory)
    Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
    Wait-AppIdle $app
    $names = Get-StorageDeletionNames (Save-AppReport $app 'same-name') $target
    Assert-True (-not @($names | Where-Object { $_.StartsWith('same/', [StringComparison]::Ordinal) }).Count) `
        'Deleting a directory removes its descendants and marker'
    if ($protocol -ne 'webdav') { Assert-True ('same' -cin $names) 'The similarly named sibling blob remains' }

    Invoke-RestMethod "$endpoint/__control?seed=late" | Out-Null
    foreach ($folder in 'folder', 'bulk') {
        $before = if ($protocol -eq 's3') { (Invoke-RestMethod "$endpoint/__stats").deleted } else { 0 }
        if ($folder -eq 'folder') {
            Expand-AppDirectory $app $list 'folder' 'visible.txt'
            Select-AppRows $app $list @('folder')
            $indices = [int[]]@((Find-AppRow $list 'folder'), (Find-AppRow $list 'visible.txt'))
            [void][NativeListViewHelper]::SelectItems($list, $indices)
            Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) 1 `
                'Selecting a folder and its child retains only the folder selection'
            Assert-True (Get-AppCommandState $app 'ID_CLEANUP_EMPTY_FOLDER').Enabled `
                'The retained selection is the folder'
        } else { Select-AppRows $app $list @($folder) }
        Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
        Wait-AppIdle $app
        if ($protocol -eq 's3' -and $folder -eq 'folder') {
            Assert-Equal ((Invoke-RestMethod "$endpoint/__stats").deleted - $before) 4 `
                'Overlapping folder and file selections delete each object only once'
        }
        if ($protocol -eq 's3' -and $folder -eq 'bulk') {
            Assert-Equal ((Invoke-RestMethod "$endpoint/__stats").deleted - $before) 1005 `
                'The complete paginated deletion plan is processed exactly once'
        }
    }
    $rows = Save-AppReport $app 'deleted'
    $names = Get-StorageDeletionNames $rows $target
    Assert-True (-not @($names | Where-Object { $_ -clike 'folder/*' -or $_ -clike 'bulk/*' }).Count) `
        'Folder deletion removes paginated, filtered, nested, and newly added objects'
    Assert-True ('folderSibling.txt' -cin $names -and 'keep.txt' -cin $names) 'Unselected sibling objects remain'
    Assert-Equal @($rows | Where-Object { $_.Name.StartsWith((($target -split '\?', 2)[0] + 'folder/'),
        [StringComparison]::Ordinal) }).Count 0 'The results remove the vanished folder'
    Set-TestSettings $runner @{}
    $fresh = Invoke-ScanReport $runner @($target) 'unfiltered'
    Assert-Sequence (Get-StorageDeletionNames $fresh $target) $names `
        'Fresh unfiltered enumeration confirms the UI deletion results against the service'
}

function Test-RemoteDeletionFailure {
    param($Context, $Case)
    $target = Get-RemoteTarget webdav -Mode 'delete-partial'
    $app = Start-TestApp -Runner (New-RemoteRunner) -Roots @($target)
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('locked')
    Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
    $errorDialog = Wait-Until { Get-StorageDeletionError $app } 'A WebDAV member failure is displayed'
    Assert-True ($errorDialog.Message.Contains('423')) `
        'A 207 multistatus exposes the failed member status'
    Close-AppDialog $errorDialog.Dialog -ButtonName 'OK'
    Wait-AppIdle $app
    Assert-True ((Find-AppRow $list 'locked' -Optional) -ge 0) 'A collection with undeleted members remains visible'
}

function Test-AzureDeletionPermission {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__control?seed=delete" | Out-Null
    $target = Get-StorageDeletionTarget azure
    $settings = Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__settings"
    $runner = New-RemoteRunner -Protocol azure
    Set-StorageTestCredential $target 'windirstat' $settings.sas
    $app = Start-TestApp -Runner $runner -Roots @($target)
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('keep.txt')
    Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
    $errorDialog = Wait-Until { Get-StorageDeletionError $app } 'Read-only SAS deletion is rejected'
    Assert-True ($errorDialog.Message.Contains('403')) `
        'The service reports missing SAS deletion permission'
    Close-AppDialog $errorDialog.Dialog -ButtonName 'OK'
    Wait-AppIdle $app
    Assert-True ('keep.txt' -cin (Get-StorageDeletionNames (Save-AppReport $app 'denied') $target)) `
        'Permission failure retains the object in the results'
}

Register-Scenario remote.webdav.collection-metadata Remote 'Empty WebDAV collection metadata' `
    Test-WebDavCollectionMetadata -Tags External -Requires Windows,Remote
Register-Scenario remote.webdav.deletion-failure Remote 'WebDAV partial deletion failures preserve surviving collections' `
    Test-RemoteDeletionFailure -Tags External -Requires Windows,Desktop,Remote
Register-Scenario remote.azure.deletion-permission Remote 'Read-only SAS cannot delete blobs' `
    Test-AzureDeletionPermission -Tags External -Requires Windows,Desktop,Remote

foreach ($protocol in 'webdav', 'azure') {
    Register-Scenario "remote.$protocol.deletion" Remote 'Remote deletion, empty folder, exact identities, and refresh' `
        Test-StorageDeletion -Tags External -Requires Windows,Desktop,Remote `
        -Data @{ Protocol = $protocol; Microsoft = $true; Sas = $false }
    if ($protocol -eq 'azure') {
        Register-Scenario remote.azure.deletion-sas Remote 'Blob deletion using SAS credentials' `
            Test-StorageDeletion -Tags External -Requires Windows,Desktop,Remote `
            -Data @{ Protocol = $protocol; Microsoft = $false; Sas = $true }
    }
    Register-Scenario "remote.$protocol.enumeration" Remote 'Native enumeration against Apache WebDAV or Azurite' `
        Test-RemoteEnumeration -Tags External -Requires Windows,Remote -Data @{ Protocol = $protocol }
    Register-Scenario "remote.$protocol.filtering" Remote 'Remote directory filters preserve traversal and case' `
        Test-RemoteFiltering -Tags External -Requires Windows,Remote -Data @{ Protocol = $protocol }
    Register-Scenario "remote.$protocol.failures" Remote `
        'Authentication, malformed metadata, partial results, and retries' `
        Test-RemoteFailures -Tags External -Requires Windows,Remote -Data @{ Protocol = $protocol }
    foreach ($format in 'csv', 'json') {
        Register-Scenario "remote.$protocol.roundtrip.$format" Remote `
            'Save, reload, scoped refresh, and action availability' `
            Test-RemoteRoundTrip -Tags External -Requires Windows,Desktop,Remote `
            -Data @{ Protocol = $protocol; Format = $format }
    }
    Register-Scenario "remote.$protocol.cancellation" Remote 'Pause and resume interrupted network requests' `
        Test-RemoteCancellation -Tags External -Requires Windows,Desktop,Remote -Data @{ Protocol = $protocol }
    foreach ($font in 100, 200) {
        Register-Scenario "remote.$protocol.connection.$font" Remote `
            'Connection editor, saved credentials, and text scaling' `
            Test-RemoteConnection -Tags External -Requires Windows,Desktop,Remote `
            -Data @{ Protocol = $protocol; Font = $font; Sas = $false }
        if ($protocol -eq 'azure') {
            Register-Scenario "remote.azure.connection.sas.$font" Remote `
                'SAS connection editor, saved credentials, and text scaling' `
                Test-RemoteConnection -Tags External -Requires Windows,Desktop,Remote `
                -Data @{ Protocol = $protocol; Font = $font; Sas = $true }
        }
    }
}

function Test-WebDavBoundedTraversal {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__control?parallel=reset" | Out-Null
    $runner = New-RemoteRunner @{ Options = @{ ScanningThreads = 16 } }
    $rows = Invoke-ScanReport $runner @((Get-RemoteTarget webdav -Mode parallel)) 'parallel'
    Assert-Equal @(Get-ReportFiles $rows).Count 12 'Every collection completes under concurrent traversal'
    $maximum = (Invoke-RestMethod "$env:WDS_REMOTE_TEST_ENDPOINT/__stats").maxActiveDav
    Assert-True ($maximum -ge 2 -and $maximum -le 4) 'WebDAV overlaps requests within a four-worker bound'
}

Register-Scenario remote.webdav.bounded-traversal Remote 'WebDAV collection requests overlap with bounded concurrency' `
    Test-WebDavBoundedTraversal -Tags External -Requires Windows,Remote
