function New-S3Runner {
    param([hashtable] $Settings = @{}, [switch] $NoCredentials)
    $runner = New-TestRunner $Settings
    $empty = Join-Path $runner.Directory 'empty-credentials'
    [IO.File]::WriteAllText($empty, '')
    $runner | Add-Member Environment @{
        AWS_ACCESS_KEY_ID = 'ignored-access'; AWS_SECRET_ACCESS_KEY = 'ignored-secret'
        AWS_SESSION_TOKEN = 'ignored-token'; AWS_PROFILE = 'ignored-profile'; AWS_REGION = 'ignored-region'
        AWS_CONFIG_FILE = $empty; AWS_SHARED_CREDENTIALS_FILE = $empty
        AWS_ENDPOINT_URL = ''; AWS_ENDPOINT_URL_S3 = ''
    }
    if (-not $NoCredentials) {
        foreach ($bucket in 'test', 'other', 'denied', 'malformed', 'missing-metadata', 'loop',
            'partial', 'retry', 'slow', 'token', 'wide', 'branch') {
            Set-StorageTestCredential (Get-S3Target -Bucket "windirstat-$bucket") 'windirstat-test' `
                'windirstat-test-secret' 'temporary-test-token'
        }
    }
    return $runner
}

function Get-S3Target {
    param([string] $Bucket = 'windirstat-test', [string] $Prefix = '')
    return "s3://$Bucket/" + [Uri]::EscapeDataString($Prefix).Replace('%2F', '/') +
        '?region=us-east-1&endpoint=' + [Uri]::EscapeDataString($env:WDS_S3_TEST_ENDPOINT)
}

function Assert-S3Objects {
    param([object[]] $Rows, [string] $Prefix = '', [string] $Bucket = 'windirstat-test')
    $manifest = (Invoke-WebRequest "$env:WDS_S3_TEST_ENDPOINT/__manifest").Content | ConvertFrom-Json -AsHashtable
    $expected = @($manifest.Keys | Where-Object { $_.StartsWith($Prefix, [StringComparison]::Ordinal) } |
        ForEach-Object { [pscustomobject]@{ Name = $_; Value = $manifest[$_] } })
    $objects = @(Get-ReportFiles $Rows | Where-Object Name -Like 's3://*')
    Assert-Equal $objects.Count $expected.Count 'Every object is represented once, including folder marker objects'
    $actual = [Collections.Generic.Dictionary[string,long]]::new([StringComparer]::Ordinal)
    foreach ($row in $objects) {
        $key = [Uri]::UnescapeDataString(($row.Name -split '\?', 2)[0].Substring("s3://$Bucket/".Length))
        Assert-True (-not $actual.ContainsKey($key)) "Object identity remains unique: $key"
        $actual.Add($key, [long]$row.'Logical Size')
    }
    foreach ($entry in $expected) {
        Assert-True $actual.ContainsKey($entry.Name) "Exact object key survives enumeration: $($entry.Name)"
        Assert-Equal $actual[$entry.Name] ([long]$entry.Value) "Object byte count matches the server: $($entry.Name)"
    }
    $total = [long]($expected | Measure-Object Value -Sum).Sum
    if ($Rows[0].Name -like 's3://*') {
        Assert-Equal ([long]$Rows[0].'Logical Size') $total 'Prefix aggregation preserves the total object size'
        Assert-Equal ([long]$Rows[0].Files) ([long]$expected.Count) 'Prefix aggregation preserves the object count'
    }
    $stats = Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats"
    Assert-True ($stats.signed -gt 0) 'The fixture independently validates AWS Signature Version 4'
    Assert-Equal $stats.downloads 0 'Scanning and automatic duplicate detection issue no object downloads'
}

function Test-S3Enumeration {
    param($Context, $Case)
    $runner = New-S3Runner @{ DupeView = @{ ScanForDuplicates = 1 } }
    $rows = Invoke-ScanReport $runner @((Get-S3Target)) 'objects'
    Assert-S3Objects $rows
    $prefix = 'prefix-only'
    $rows = Invoke-ScanReport $runner @((Get-S3Target -Prefix $prefix)) 'prefix'
    Assert-S3Objects $rows $prefix
    $local = New-TestTree
    $rows = Invoke-ScanReport $runner @($local, (Get-S3Target), (Get-S3Target -Prefix 'unicode/')) 'mixed'
    Assert-S3Objects $rows
    Assert-Equal @((Get-ReportFiles $rows) | Where-Object Name -NotLike 's3://*').Count 10 `
        'Mixed scans preserve local files and remove overlapping remote prefixes'
}

function Test-S3Credentials {
    param($Context, $Case)
    $runner = New-S3Runner
    $uri = Get-S3Target -Prefix 'unicode/'
    Set-StorageTestCredential (Get-S3Target) 'windirstat-test' 'windirstat-test-secret'
    Assert-S3Objects (Invoke-ScanReport $runner @($uri) 'supplied') 'unicode/'
    $temporary = Get-S3Target -Bucket 'windirstat-token' -Prefix 'unicode/'
    Assert-S3Objects (Invoke-ScanReport $runner @($temporary) 'temporary') 'unicode/' 'windirstat-token'
    Set-StorageTestCredential (Get-S3Target -Bucket 'windirstat-token') 'windirstat-test' 'windirstat-test-secret'
    $result = Invoke-TestProcess $runner @('/saveto', (Join-Path $Context.Root 'missing-token.csv'), $temporary)
    Assert-Equal $result.ExitCode 1 'Temporary credentials require the supplied session token'
    Set-StorageTestCredential (Get-S3Target) 'windirstat-test' 'wrong-secret'
    $out = Join-Path $Context.Root 'bad-signature.csv'
    $result = Invoke-TestProcess $runner @('/saveto', $out, $uri)
    Assert-Equal $result.ExitCode 1 'A rejected signature exits unsuccessfully instead of reporting an empty success'
    $rows = Read-ScanReport $out
    Assert-True (([Convert]::ToUInt32($rows[0].'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -ne 0) `
        'The saved root records an incomplete scan after authentication failure'

    & cmdkey.exe ('/delete:WinDirStat/' + (Get-S3Target)) | Out-Null
    $profile = Join-Path $runner.Directory 'profiles'
    [IO.File]::WriteAllText($profile, "[default]`naws_access_key_id=windirstat-test`n" +
        "aws_secret_access_key=windirstat-test-secret`naws_session_token=temporary-test-token`n" +
        "region=us-east-1`nendpoint_url=$env:WDS_S3_TEST_ENDPOINT`n")
    $runner.Environment.AWS_SHARED_CREDENTIALS_FILE = $profile
    $runner.Environment.AWS_CONFIG_FILE = $profile
    $runner.Environment.AWS_PROFILE = 'default'
    $runner.Environment.AWS_ACCESS_KEY_ID = 'windirstat-test'
    $runner.Environment.AWS_SECRET_ACCESS_KEY = 'windirstat-test-secret'
    $runner.Environment.AWS_SESSION_TOKEN = 'temporary-test-token'
    $runner.Environment.AWS_REGION = 'us-east-1'
    $runner.Environment.AWS_DEFAULT_REGION = 'us-east-1'
    $runner.Environment.AWS_ENDPOINT_URL = $env:WDS_S3_TEST_ENDPOINT
    $runner.Environment.AWS_ENDPOINT_URL_S3 = $env:WDS_S3_TEST_ENDPOINT
    $out = Join-Path $Context.Root 'environment.csv'
    $result = Invoke-TestProcess $runner @('/saveto', $out, $uri)
    Assert-Equal $result.ExitCode 1 'Environment and profile credentials cannot supply missing S3 credentials'
    Assert-Equal @(Get-ReportFiles (Read-ScanReport $out)).Count 0 `
        'A scan without supplied credentials cannot return anonymous results'
    foreach ($target in @('s3://windirstat-test/?region=us-east-1', ('s3://windirstat-test/?endpoint=' +
        [Uri]::EscapeDataString($env:WDS_S3_TEST_ENDPOINT)))) {
        $result = Invoke-TestProcess $runner @('/saveto', (Join-Path $Context.Root 'incomplete.csv'), $target)
        Assert-Equal $result.ExitCode 1 'Environment and profile settings cannot supply a missing endpoint or region'
    }
}

function Test-S3Filtering {
    param($Context, $Case)
    foreach ($regex in 0, 1) {
        $patterns = @('s3://windirstat-test/unicode', 's3://windirstat-test/unicode/')
        $uri = Get-S3Target -Prefix 'unicode/'
        if ($regex) { $patterns += @('.*/unicode$', '.*/unicode/$', ([regex]::Escape($uri) + '$')) }
        else { $patterns += @('s3://*/unicode', 's3://*/unicode/', $uri) }
        $runner = New-S3Runner
        for ($index = 0; $index -lt $patterns.Count; ++$index) {
            Set-TestSettings $runner @{
                Options = @{ FilteringUseRegex = $regex }
                DriveSelect = @{ FilteringIncludeDirs = $patterns[$index] }
            }
            Assert-S3Objects (Invoke-ScanReport $runner @((Get-S3Target)) "include-$regex-$index") 'unicode/'
            Set-TestSettings $runner @{
                Options = @{ FilteringUseRegex = $regex }
                DriveSelect = @{ FilteringExcludeDirs = $patterns[$index] }
            }
            $files = @(Get-ReportFiles (Invoke-ScanReport $runner @((Get-S3Target)) "exclude-$regex-$index"))
            Assert-Equal $files.Count 1019 'Directory exclusions preserve objects in unrelated prefixes'
            Assert-Equal @($files | Where-Object Name -Like 's3://windirstat-test/unicode/*').Count 0 `
                'Directory exclusions remove all descendants of the matching prefix'
        }
        Set-TestSettings $runner @{
            Options = @{ FilteringUseRegex = $regex }
            DriveSelect = @{ FilteringIncludeDirs = 's3://windirstat-test/UNICODE' }
        }
        $rows = Invoke-ScanReport $runner @((Get-S3Target)) "case-$regex"
        Assert-Equal @(Get-ReportFiles $rows).Count 0 'S3 directory filters preserve case-sensitive key identity'
    }
    $runner = New-S3Runner @{ DriveSelect = @{
        FilteringIncludeDirs = 's3://windirstat-test/unicode'; FilteringExcludeFiles = '*.txt'
    } }
    Assert-Equal @(Get-ReportFiles (Invoke-ScanReport $runner @((Get-S3Target)) 'exclude')).Count 0 `
        'Filename exclusions still apply inside included S3 prefixes'
    $runner = New-S3Runner @{ DriveSelect = @{ FilteringIncludeDirs = 's3://windirstat-test/same/' } }
    $rows = Invoke-ScanReport $runner @((Get-S3Target -Prefix 'sa')) 'partial-prefix-filter'
    Assert-S3Objects $rows 'same/'
}

function Test-S3SearchRefresh {
    param($Context, $Case)
    $uri = Get-S3Target -Prefix 'same/'
    $app = Start-TestApp -Runner (New-S3Runner) -Roots @($uri, (Get-S3Target -Prefix 'unicode/'))
    Get-AppList $app -AllFiles | Out-Null
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_TERM'), '')
    Set-DialogCheck $dialog 'IDC_SEARCH_FILES' $true
    Set-DialogCheck $dialog 'IDC_SEARCH_FOLDERS' $Case.Folders
    Close-AppDialog $dialog 1
    $list = Get-AppList $app -TabName 'Search Results'
    $names = [NativeListViewHelper]::GetItemTexts($list)
    Assert-Equal ($names.Count - 1) $(if ($Case.Folders) { 5 } else { 3 }) `
        'The search includes the marker object and optionally its synthetic directory'
    $indices = @(for ($index = 1; $index -lt $names.Count; ++$index) {
        if ($names[$index] -ceq $uri) { $index }
    })
    Assert-Equal $indices.Count $(if ($Case.Folders) { 2 } else { 1 }) `
        'Objects and directories with identical URIs remain separate search results'
    $sizes = [NativeListViewHelper]::GetItemTexts($list, 2)
    $marker = @($indices | Where-Object { [long]($sizes[$_] -replace '\D', '') -eq 11 })
    [void][NativeListViewHelper]::FocusListView($list)
    Assert-True ([NativeListViewHelper]::SelectItems($list, [int[]]$marker)) `
        'The marker object can be selected independently of its synthetic directory'

    $tree = Get-AppList $app -AllFiles
    Select-AppRows $app $tree @($uri)
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Invoke-AppCommand $app 'ID_VIEW_SEARCH_RESULTS'
    $list = Get-AppList $app -TabName 'Search Results'
    $restored = [NativeListViewHelper]::GetItemTexts($list)
    Assert-Sequence @($restored | Select-Object -Skip 1 | Sort-Object -CaseSensitive) `
        @($names | Select-Object -Skip 1 | Sort-Object -CaseSensitive) `
        'Scoped refresh preserves every surviving object and directory search result'
    Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) 1 `
        'Refresh preserves selection for the marker object sharing its directory URI'
    $sizes = [NativeListViewHelper]::GetItemTexts($list, 2)
    $markerSizes = @(for ($index = 1; $index -lt $restored.Count; ++$index) {
        if ($restored[$index] -ceq $uri) { [long]($sizes[$index] -replace '\D', '') }
    })
    Assert-Sequence @($markerSizes | Sort-Object) $(if ($Case.Folders) { @(11L, 24L) } else { @(11L) }) `
        'Restored results retain the separate object and aggregate directory sizes'
}

function Test-S3SearchRefreshAncestors {
    param($Context, $Case)
    $root = Get-S3Target
    $folder = Get-S3Target -Prefix 'unicode/'
    $other = Get-S3Target -Bucket 'windirstat-other' -Prefix 'unicode/'
    $app = Start-TestApp -Runner (New-S3Runner) -Roots @($root, $other)
    Get-AppList $app -AllFiles | Out-Null
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_TERM'), '')
    Set-DialogCheck $dialog 'IDC_SEARCH_FILES' $false
    Set-DialogCheck $dialog 'IDC_SEARCH_FOLDERS' $true
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_SIZE_MIN'), '1')
    [void][TestDesktop]::Message((Get-DialogControl $dialog 'IDC_SEARCH_SIZE_UNITS'), 0x014E, 0)
    Close-AppDialog $dialog 1
    $list = Get-AppList $app -TabName 'Search Results'
    $expected = @([NativeListViewHelper]::GetItemTexts($list) | Select-Object -Skip 1)
    $sizes = [NativeListViewHelper]::GetItemTexts($list, 2)
    $rootSize = [long]($sizes[(Find-AppRow $list $root)] -replace '\D', '')
    Select-AppRows $app $list @($root)

    foreach ($command in 'ID_REFRESH_SELECTED', 'ID_FILTER_EXCLUDE_ITEM') {
        $tree = Get-AppList $app -AllFiles
        Expand-AppDirectory $app $tree $root 'unicode'
        Select-AppRows $app $tree @('unicode')
        Invoke-AppCommand $app $command
        Wait-AppIdle $app
        Invoke-AppCommand $app 'ID_VIEW_SEARCH_RESULTS'
        $list = Get-AppList $app -TabName 'Search Results'
        if ($command -eq 'ID_FILTER_EXCLUDE_ITEM') {
            $expected = @($expected | Where-Object { $_ -cne $folder })
            $rootSize -= 19
        }
        $names = @([NativeListViewHelper]::GetItemTexts($list) | Select-Object -Skip 1)
        Assert-Sequence @($names | Sort-Object -CaseSensitive) @($expected | Sort-Object -CaseSensitive) `
            'Scoped refresh and filtering preserve ancestor and unrelated connection results'
        Assert-Equal ([NativeListViewHelper]::GetSelectedCount($list)) 1 `
            'An untouched ancestor retains its search selection while its child changes'
        $sizes = [NativeListViewHelper]::GetItemTexts($list, 2)
        Assert-Equal ([long]($sizes[(Find-AppRow $list $root)] -replace '\D', '')) $rootSize `
            'The restored ancestor displays the updated aggregate size'
    }
}

function Test-S3ConnectionDialog {
    param($Context, $Case)
    $runner = New-S3Runner @{ Options = @{ FontSizePercent = $Case.Font } } -NoCredentials
    $runner.Environment.AWS_ACCESS_KEY_ID = 'windirstat-test'
    $runner.Environment.AWS_SECRET_ACCESS_KEY = 'windirstat-test-secret'
    $runner.Environment.AWS_SESSION_TOKEN = 'temporary-test-token'
    $runner.Environment.AWS_REGION = 'us-east-1'
    $runner.Environment.AWS_ENDPOINT_URL_S3 = $env:WDS_S3_TEST_ENDPOINT
    $app = Start-TestApp -Runner $runner -Roots @((New-TestTree))
    $select = Open-AppDialog $app 'ID_FILE_SELECT' 'IDC_CLOUD_STORAGE'
    $button = Get-DialogControl $select 'IDC_CLOUD_STORAGE'
    [void][TestDesktop]::PostMessage($button, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    $menu = Wait-Until {
        [TestDesktop]::Windows($app.Process.Id) | Where-Object { [TestDesktop]::Class($_) -eq '#32768' } |
            Select-Object -First 1
    } 'The cloud storage menu opens' -Seconds 10
    [void][TestDesktop]::PostMessage($menu, 0x0100, [IntPtr]83, [IntPtr]::Zero)
    $dialog = Wait-Until { Get-AppDialog $app 'IDC_S3_BUCKET' } 'The S3 connection editor opens'
    Assert-True ([TestDesktop]::Message($dialog, 0x007F).ToInt64() -ne 0) `
        'The S3 connection editor displays a title-bar icon'
    Assert-Equal ([TestDesktop]::Message($dialog, 0x007F)) ([TestDesktop]::Message($select, 0x007F)) `
        'The S3 connection editor uses the WinDirStat title-bar icon'
    $values = @{
        IDC_S3_BUCKET = 'windirstat-token'; IDC_REMOTE_PREFIX = 'unicode/'; IDC_REMOTE_ENDPOINT = $env:WDS_S3_TEST_ENDPOINT
        IDC_S3_REGION = 'us-east-1'; IDC_S3_ACCESS = 'windirstat-test'; IDC_REMOTE_SECRET = 'windirstat-test-secret'
        IDC_S3_TOKEN = 'temporary-test-token'
    }
    foreach ($entry in $values.GetEnumerator()) {
        [TestDesktop]::SetText((Get-DialogControl $dialog $entry.Key), $entry.Value)
    }
    foreach ($control in 'IDC_REMOTE_ENDPOINT', 'IDC_S3_REGION', 'IDC_S3_ACCESS', 'IDC_REMOTE_SECRET') {
        $field = Get-DialogControl $dialog $control
        [TestDesktop]::SetText($field, '')
        Assert-True (-not [TestDesktop]::IsWindowEnabled([TestDesktop]::GetDlgItem($dialog, 1))) `
            'S3 requires explicit settings and credentials even with valid environment defaults'
        [TestDesktop]::SetText($field, $values[$control])
    }
    Assert-True ([TestDesktop]::IsWindowEnabled([TestDesktop]::GetDlgItem($dialog, 1))) `
        'S3 accepts supplied settings and credentials'
    if ($Case.Font -eq 100) {
        Set-DialogCheck $dialog 'IDC_REMOTE_REMEMBER' $true
        Add-Cleanup 'Remove fixture-only saved S3 credential' {
            param($name); & cmdkey.exe "/delete:$name" | Out-Null
        } @('WinDirStat/' + (Get-S3Target -Bucket 'windirstat-token'))
    }
    [TestDesktop]::Expose($dialog)
    Save-Frame ([TestDesktop]::Capture($dialog)) (Join-Path $Context.Root 's3-connection.png')
    Close-AppDialog $dialog 1
    foreach ($control in 'IDC_FAST_SCAN_CHECKBOX', 'IDC_SCAN_DUPLICATES') {
        Assert-True (-not [TestDesktop]::IsWindowVisible(
            [TestDesktop]::GetDlgItem($select, (Get-ResourceId $control)))) `
            'Filesystem scan options are hidden for an S3-only selection'
    }
    Close-AppDialog $select 1
    Wait-AppIdle $app
    $target = Get-S3Target -Bucket 'windirstat-token' -Prefix 'unicode/'
    $app.Roots = @($target)
    Assert-S3Objects (Save-AppReport $app 'connected') 'unicode/' 'windirstat-token'
    if ($Case.Font -eq 100) {
        Assert-S3Objects (Invoke-ScanReport $runner @($target) 'remembered') 'unicode/' 'windirstat-token'
    }
    Assert-True (-not [IO.File]::ReadAllText($runner.Ini).Contains('windirstat-test-secret') -and
        -not [IO.File]::ReadAllText($runner.Ini).Contains('temporary-test-token')) `
        'S3 secrets and session tokens are absent from settings and history'
}

function Test-S3Failures {
    param($Context, $Case)
    $runner = New-S3Runner
    foreach ($mode in @('denied', 'malformed', 'missing-metadata', 'loop', 'partial')) {
        $path = Join-Path $Context.Root "$mode.csv"
        $result = Invoke-TestProcess $runner @('/saveto', $path, (Get-S3Target -Bucket "windirstat-$mode"))
        Assert-Equal $result.ExitCode 1 "$mode responses cannot be mistaken for a successful scan"
        $rows = Read-ScanReport $path
        Assert-True (([Convert]::ToUInt32($rows[0].'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -ne 0) `
            "$mode results retain their incomplete state"
        if ($mode -eq 'partial') {
            Assert-True (@(Get-ReportFiles $rows).Count -gt 0) `
                'A later-page failure preserves already enumerated objects'
        }
    }
    $rows = Invoke-ScanReport $runner @((Get-S3Target -Bucket 'windirstat-retry' -Prefix 'unicode/')) 'retry'
    Assert-S3Objects $rows 'unicode/' 'windirstat-retry'
    Assert-True ((Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats").retries -gt 0) `
        'A throttled listing is retried without duplicating objects'
}

function Test-S3RoundTrip {
    param($Context, $Case)
    $runner = New-S3Runner
    $uri = Get-S3Target
    $source = Invoke-ScanReport $runner @($uri) 'source' -Format $Case.Format
    $app = Start-TestApp -Runner $runner -Arguments @('/loadfrom', (Join-Path $Context.Root "source.$($Case.Format)"))
    $app.Roots = @($uri)
    $loaded = Save-AppReport $app 'loaded' -Format $Case.Format
    $fields = @('Name', 'Files', 'Folders', 'Logical Size', 'Physical Size', 'Last Change', 'WinDirStat Attributes')
    Assert-Sequence (Get-ReportSignature $loaded $fields) (Get-ReportSignature $source $fields) `
        'S3 keys, synthetic prefixes, metadata, and counts survive save/reload exactly'
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('unicode')
    Assert-True (Get-AppCommandState $app 'ID_CLEANUP_DELETE').Enabled 'Remote prefixes support permanent deletion'
    foreach ($command in 'ID_CLEANUP_DELETE_BIN', 'ID_CLEANUP_MOVE_TO', 'ID_CLEANUP_OPEN_SELECTED',
        'ID_CLEANUP_OPEN_IN_CONSOLE', 'ID_COMPUTE_HASH') {
        Assert-True (-not (Get-AppCommandState $app $command).Enabled) "$command is unavailable for remote prefixes"
    }
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Assert-S3Objects (Save-AppReport $app 'refreshed')
    Assert-GraphRendered $app | Out-Null
    Save-Frame ([TestDesktop]::Capture($app.Window)) (Join-Path $Context.Root 's3-results.png')
}

function Test-S3Cancellation {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?slow=1" | Out-Null
    Add-Cleanup 'Reset slow S3 fixture' { Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?slow=0" | Out-Null }
    $before = (Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats").slow
    $uri = Get-S3Target -Bucket 'windirstat-slow' -Prefix 'unicode/'
    $app = Start-TestApp -Runner (New-S3Runner) -Roots @($uri) -DuringScan
    Wait-Until { (Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats").slow -gt $before } `
        'The S3 request is awaiting a deliberately delayed response' | Out-Null
    $watch = [Diagnostics.Stopwatch]::StartNew()
    Invoke-AppCommand $app 'ID_SCAN_SUSPEND'
    Wait-Until { (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled } `
        'Pause cancels outstanding S3 I/O' -Seconds 5 | Out-Null
    Assert-True ($watch.Elapsed.TotalSeconds -lt 5) 'Pause responds before the delayed server response arrives'
    Invoke-AppCommand $app 'ID_SCAN_STOP'
    Wait-AppIdle $app
    $stopped = Save-AppReport $app 'stopped'
    Assert-True (([Convert]::ToUInt32($stopped[0].'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -ne 0) `
        'Stopping remote enumeration preserves its incomplete state in saved scans'
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?slow=0" | Out-Null
    Invoke-AppCommand $app 'ID_REFRESH_ALL'
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'restarted'
    Assert-S3Objects $rows 'unicode/' 'windirstat-slow'
    Assert-True (([Convert]::ToUInt32($rows[0].'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -eq 0) `
        'A successful replacement scan clears the incomplete state'
}

function Test-S3Resume {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?slow=1" | Out-Null
    Add-Cleanup 'Reset slow S3 fixture' { Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?slow=0" | Out-Null }
    $before = (Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats").slow
    $app = Start-TestApp -Runner (New-S3Runner) -Roots @((Get-S3Target -Bucket 'windirstat-slow')) -DuringScan
    Wait-Until { (Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats").slow -gt $before } `
        'The paginated listing is awaiting a delayed response' | Out-Null
    Invoke-AppCommand $app 'ID_SCAN_SUSPEND'
    Wait-Until { (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled } 'The remote scan pauses' -Seconds 5 | Out-Null
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?slow=0" | Out-Null
    Invoke-AppCommand $app 'ID_SCAN_RESUME'
    Wait-AppIdle $app
    Assert-S3Objects (Save-AppReport $app 'resumed') '' 'windirstat-slow'
}

function Test-S3DeletionFailure {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?seed=delete&delete=" | Out-Null
    $target = Get-StorageDeletionTarget s3
    $runner = New-S3Runner
    Set-StorageTestCredential $target 'windirstat-test' 'windirstat-test-secret' 'temporary-test-token'
    $app = Start-TestApp -Runner $runner -Roots @($target)
    $list = Get-AppList $app -AllFiles
    $selection = if ($Case.Mode -eq 'denied') { @('keep.txt') } else { @('bulk', 'folder', 'folderSibling.txt') }
    Select-AppRows $app $list $selection
    $before = (Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats").deleted
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?delete=$($Case.Mode)" | Out-Null
    Add-Cleanup 'Reset S3 deletion fault' { Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?delete=" | Out-Null }
    Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
    $dialog = Wait-Until { Get-StorageDeletionError $app } 'The deletion failure is displayed'
    Assert-True ($dialog.Message.Contains('403')) `
        'A failed delete or listing exposes the service error'
    Close-AppDialog $dialog.Dialog -ButtonName 'OK'
    Wait-AppIdle $app
    Assert-Equal (Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats").deleted $before `
        'Discovery failure cannot delete any selected object or prefix'
    foreach ($name in $selection) {
        Assert-True ((Find-AppRow $list $name -Optional) -ge 0) 'Every undeleted selection remains visible'
    }
}

function Test-S3DeletionCancellation {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?seed=delete&delete=" | Out-Null
    $target = Get-StorageDeletionTarget s3
    $runner = New-S3Runner
    Set-StorageTestCredential $target 'windirstat-test' 'windirstat-test-secret' 'temporary-test-token'
    $app = Start-TestApp -Runner $runner -Roots @($target)
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('keep.txt', 'folderSibling.txt')
    $before = Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats"
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?delete=slow" | Out-Null
    Add-Cleanup 'Reset delayed S3 deletion' { Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?delete=" | Out-Null }
    Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
    Wait-Until { (Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats").deleteSlow -gt $before.deleteSlow } `
        'A destructive response is delayed' | Out-Null
    $dialog = Wait-Until { Get-AppDialog $app 'IDC_PROGRESS_BAR' } 'Network deletion displays cancellable progress'
    $watch = [Diagnostics.Stopwatch]::StartNew()
    Close-AppDialog $dialog 2
    Assert-True ($watch.Elapsed.TotalSeconds -lt 5) 'Cancel interrupts the pending deletion request promptly'
    Wait-AppIdle $app
    Assert-Equal (Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats").deletions ($before.deletions + 1) `
        'Cancellation stops further destructive requests'
    Assert-True ('keep.txt' -cin (Get-StorageDeletionNames (Save-AppReport $app 'cancelled') $target)) `
        'Refreshing after cancellation retains surviving objects'
}

Register-Scenario s3.enumeration S3 'Signed, paginated S3 enumeration and mixed filesystem roots' `
    Test-S3Enumeration -Tags External -Requires @('Windows', 'S3')
Register-Scenario s3.credentials S3 'Explicit credentials, session tokens, and rejection of implicit settings' `
    Test-S3Credentials -Tags External -Requires @('Windows', 'S3')
Register-Scenario s3.filtering S3 'S3 path traversal, case-sensitive prefixes, and filename exclusions' `
    Test-S3Filtering -Tags External -Requires @('Windows', 'S3')
foreach ($folders in $false, $true) {
    $mode = if ($folders) { 'folders' } else { 'files' }
    Register-Scenario "s3.search-refresh.$mode" S3 'Scoped refresh retains marker objects and same-URI directories' `
        Test-S3SearchRefresh -Tags External -Requires Windows,Desktop,S3 -Data @{ Folders = $folders }
}
Register-Scenario s3.search-refresh.ancestors S3 'Scoped refresh and filtering retain size-filtered ancestors' `
    Test-S3SearchRefreshAncestors -Tags External -Requires Windows,Desktop,S3
Register-Scenario s3.failures S3 'Access denial, malformed XML, partial pagination, and throttling' `
    Test-S3Failures -Tags External -Requires @('Windows', 'S3')
foreach ($format in @('csv', 'json')) {
    Register-Scenario "s3.roundtrip.$format" S3 'S3 save/reload, actions, rendering, and scoped refresh' `
        Test-S3RoundTrip -Tags External -Requires @('Windows', 'Desktop', 'S3') -Data @{ Format = $format }
}
Register-Scenario s3.cancellation S3 'Pause, stop, and rescan while an S3 response is delayed' `
    Test-S3Cancellation -Tags External -Requires @('Windows', 'Desktop', 'S3')
foreach ($microsoft in $false, $true) {
    Register-Scenario "s3.deletion.$microsoft" S3 'S3 deletion, empty folder, exact identities, and refresh' `
        Test-StorageDeletion -Tags External -Requires Windows,Desktop,S3 `
        -Data @{ Protocol = 's3'; Microsoft = $microsoft; Sas = $false }
}
foreach ($mode in 'denied', 'list-failure') {
    Register-Scenario "s3.deletion.$mode" S3 'Deletion rejects permission and incomplete listing failures' `
        Test-S3DeletionFailure -Tags External -Requires Windows,Desktop,S3 -Data @{ Mode = $mode }
}
Register-Scenario s3.deletion.cancellation S3 'Cancellation interrupts remote deletion and preserves remaining selections' `
    Test-S3DeletionCancellation -Tags External -Requires Windows,Desktop,S3
Register-Scenario s3.resume S3 'Resuming an interrupted paginated S3 listing preserves every object exactly once' `
    Test-S3Resume -Tags External -Requires @('Windows', 'Desktop', 'S3')
foreach ($font in 100, 200) {
    Register-Scenario "s3.connection.$font" S3 'Native connection editor and scanning at alternate text scales' `
        Test-S3ConnectionDialog -Tags External -Requires @('Windows', 'Desktop', 'S3') -Data @{ Font = $font }
}

function Test-S3FlatPlan {
    param($Context, $Case)
    $runner = New-S3Runner
    $before = Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats"
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $rows = Invoke-ScanReport $runner @((Get-S3Target -Bucket 'windirstat-wide')) 'wide'
    $elapsed = $watch.Elapsed.TotalSeconds
    $after = Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats"
    Assert-Equal @(Get-ReportFiles $rows).Count 1000 'A wide tree retains every object'
    Assert-Equal ($after.listings - $before.listings) 1 'A broad scan reads one flat page instead of 1001 prefixes'
    Assert-Equal ($after.storageConnections - $before.storageConnections) 1 'The scan shares a persistent connection'
    Assert-True ([string]::IsNullOrEmpty($rows[0].'Physical Size')) 'Remote physical allocation remains unknown'
    Write-TestNote ('Wide scan: {0:N3}s, {1} listing' -f $elapsed, ($after.listings - $before.listings))
}

function Test-S3BranchRecovery {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?branch=denied" | Out-Null
    Add-Cleanup 'Restore branch fault' { Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?branch=denied" | Out-Null }
    $target = Get-S3Target -Bucket 'windirstat-branch'
    $runner = New-S3Runner @{ DriveSelect = @{ FilteringExcludeDirs = '*unused*' } }
    $report = Join-Path $Context.Root "branch.$($Case.Format)"
    $result = Invoke-TestProcess $runner @('/saveto', $report, $target)
    Assert-Equal $result.ExitCode 1 'A failed branch makes the scan incomplete'
    $rows = Read-ScanReport $report
    Assert-Equal @(Get-ReportFiles $rows).Count 1 'A healthy sibling continues after a branch failure'
    $denied = @($rows | Where-Object { $_.Name.Contains('/z-denied/') })[0]
    Assert-Equal $denied.ScanOutcome 'connection-error' 'The error belongs to the requested branch'
    Assert-True ($denied.ScanError.Contains('AccessDenied')) 'Reports retain the provider error code'
    $good = @($rows | Where-Object { $_.Name.Contains('/a-good/') -and $_.Folders -eq 0 -and $_.Files -eq 1 })[0]
    Assert-Equal $good.ScanOutcome 'complete' 'The healthy branch has an independent outcome'
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?branch=healthy" | Out-Null
    $app = Start-TestApp -Runner $runner -Arguments @('/loadfrom', $report) -DuringScan
    $app.Roots = @($target)
    $diagnostic = Wait-Until { Get-StorageDeletionError $app } 'The loaded incomplete branch exposes its diagnostic'
    Close-AppDialog $diagnostic.Dialog -ButtonName 'OK'
    Wait-AppIdle $app
    $app.Roots = @($target + ' (Scan incomplete)')
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('a-good')
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    $partial = Save-AppReport $app 'still-partial' -Format $Case.Format
    Assert-True (([Convert]::ToUInt32($partial[0].'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -ne 0) `
        'Refreshing a healthy sibling retains the failed branch and aggregate incompleteness'
    Select-AppRows $app $list @('z-denied (Scan incomplete)')
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    $recovered = Save-AppReport $app 'recovered' -Format $Case.Format
    Assert-Equal @(Get-ReportFiles $recovered).Count 2 'Refreshing the failed branch restores its objects'
    Assert-True (([Convert]::ToUInt32($recovered[0].'WinDirStat Attributes'.Substring(2), 16) -band 0x4000) -eq 0) `
        'Successful branch refresh clears aggregate ancestor incompleteness'
}

Register-Scenario s3.flat-plan S3 'Broad object scans use flat pages and persistent connections' `
    Test-S3FlatPlan -Tags External -Requires Windows,S3
foreach ($format in 'csv', 'json') {
    Register-Scenario "s3.branch-recovery.$format" S3 'Branch outcomes survive reload and recover on scoped refresh' `
        Test-S3BranchRecovery -Tags External -Requires Windows,Desktop,S3 -Data @{ Format = $format }
}

function Test-S3BatchOutcome {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?seed=delete&delete=" | Out-Null
    Add-Cleanup 'Reset S3 batch fault' { Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?delete=" | Out-Null }
    $runner = New-S3Runner
    $target = Get-StorageDeletionTarget s3
    Set-StorageTestCredential $target 'windirstat-test' 'windirstat-test-secret' 'temporary-test-token'
    $app = Start-TestApp -Runner $runner -Roots @($target)
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('bulk')
    $before = Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats"
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?delete=$($Case.Mode)" | Out-Null
    Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
    if ($Case.Mode -ne 'unsupported') {
        $errorDialog = Wait-Until { Get-StorageDeletionError $app -AnyError } 'The batch outcome is reported'
        if ($Case.Mode -eq 'batch-partial') {
            Assert-True ($errorDialog.Message.Contains('AccessDenied') -and $errorDialog.Message.Contains('bulk/')) `
                'Per-object failures expose the exact key and service code'
        }
        Close-AppDialog $errorDialog.Dialog -ButtonName 'OK'
    }
    Wait-AppIdle $app
    $after = Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats"
    Assert-Equal ($after.batches - $before.batches) 1 'A destructive batch is never retried'
    $names = Get-StorageDeletionNames (Save-AppReport $app 'batch') $target
    $remaining = @($names | Where-Object { $_.StartsWith('bulk/', [StringComparison]::Ordinal) }).Count
    Assert-Equal $remaining $(switch ($Case.Mode) { 'batch-partial' { 6 }; 'batch-incomplete' { 5 }; default { 0 } }) `
        'Refresh reflects service outcomes and preserves objects beyond the failed batch'
    if ($Case.Mode -eq 'unsupported') {
        Assert-Equal ($after.deleted - $before.deleted) 1005 'Unsupported batching falls back to exact individual deletes'
    }
}

function Test-S3DeleteXmlKeys {
    param($Context, $Case)
    Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__control?seed=delete&delete=" | Out-Null
    $runner = New-S3Runner
    $target = Get-StorageDeletionTarget s3
    Set-StorageTestCredential $target 'windirstat-test' 'windirstat-test-secret' 'temporary-test-token'
    $app = Start-TestApp -Runner $runner -Roots @($target)
    $list = Get-AppList $app -AllFiles
    foreach ($folder in 'invalid', 'xml') {
        $before = Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats"
        Select-AppRows $app $list @($folder)
        Invoke-AppCommand $app 'ID_CLEANUP_DELETE'
        Wait-AppIdle $app
        $after = Invoke-RestMethod "$env:WDS_S3_TEST_ENDPOINT/__stats"
        Assert-Equal ($after.deleted - $before.deleted) 2 'XML control and escaped keys retain exact delete identities'
        Assert-Equal ($after.batches - $before.batches) $(if ($folder -eq 'invalid') { 0 } else { 1 }) `
            'Invalid XML keys use single deletes while XML-safe keys batch'
    }
}

foreach ($mode in 'batch-partial', 'batch-incomplete', 'unsupported') {
    Register-Scenario "s3.batch.$mode" S3 'Batch response accounting, partial failures, and supported fallback' `
        Test-S3BatchOutcome -Tags External -Requires Windows,Desktop,S3 -Data @{ Mode = $mode }
}
Register-Scenario s3.batch.xml-keys S3 'Batch and individual deletion preserve unusual XML object names' `
    Test-S3DeleteXmlKeys -Tags External -Requires Windows,Desktop,S3

function Test-S3UnknownPhysical {
    param($Context, $Case)
    $runner = New-S3Runner
    $target = Get-S3Target -Prefix 'unicode/'
    $rows = Invoke-ScanReport $runner @((New-TestTree), $target) 'mixed' -Format $Case.Format
    Assert-True ([string]::IsNullOrEmpty($rows[0].'Physical Size')) 'Mixed physical totals are unknown'
    Assert-True (@($rows | Where-Object { $_.Name -notlike 's3://*' -and [long]$_.'Physical Size' -gt 0 }).Count -gt 0) `
        'Known local allocations remain available beneath a mixed root'
    $app = Start-TestApp -Runner $runner -Arguments @('/loadfrom', (Join-Path $Context.Root "mixed.$($Case.Format)"))
    $loaded = Save-AppReport $app 'loaded' -Format $Case.Format
    $fields = @('Name', 'Files', 'Folders', 'Logical Size', 'Physical Size')
    Assert-Sequence (Get-ReportSignature $loaded $fields) (Get-ReportSignature $rows $fields) `
        'Mixed reports preserve known local bytes and unknown physical totals on reload'
    $app.Roots = @($target)
    Get-AppList $app -AllFiles | Out-Null
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_TERM'), '')
    Close-AppDialog $dialog 1
    $list = Get-AppList $app -TabName 'Search Results'
    Assert-Equal ([NativeListViewHelper]::GetItemTexts($list, 1)[0]) '' `
        'Search totals preserve unknown physical allocation'
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_PHYSICAL_MIN'), '1')
    [void][TestDesktop]::Message((Get-DialogControl $dialog 'IDC_SEARCH_PHYSICAL_UNITS'), 0x014E, 0)
    Close-AppDialog $dialog 1
    $list = Get-AppList $app -TabName 'Search Results'
    $names = @([NativeListViewHelper]::GetItemTexts($list) | Select-Object -Skip 1)
    Assert-True ($names.Count -gt 0 -and -not @($names | Where-Object { $_ -like 's3://*' }).Count) `
        'Physical size filters match only items with known allocation'
}

foreach ($format in 'csv', 'json') {
    Register-Scenario "s3.unknown-physical.$format" S3 'Mixed reports and searches preserve unknown physical size' `
        Test-S3UnknownPhysical -Tags External -Requires Windows,Desktop,S3 -Data @{ Format = $format }
}
