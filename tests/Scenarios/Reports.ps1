function Get-ReportSignature {
    param([object[]] $Rows, [string[]] $Fields)
    $signatures = foreach ($row in $Rows) {
        $values = [ordered]@{}
        foreach ($field in $Fields) { $values[$field] = [string]$row.$field }
        ConvertTo-Json $values -Compress
    }
    return @($signatures | Sort-Object)
}

function Test-DuplicateContent {
    param($Context, $Case)
    $root = New-TestTree
    $first = New-TestFile (Join-Path $root 'full-hash\first.bin') 2MB 319
    $second = Join-Path $root 'full-hash\second.bin'
    $different = Join-Path $root 'full-hash\same-prefix-different-middle.bin'
    Copy-Item -LiteralPath $first -Destination $second
    Copy-Item -LiteralPath $first -Destination $different
    $stream = [IO.File]::Open($different, 'Open', 'ReadWrite')
    try { $stream.Position = 1MB + 211; $stream.WriteByte(0x17); $stream.WriteByte(0x25) }
    finally { $stream.Dispose() }
    $before = @(Get-ChildItem -LiteralPath $root -File -Recurse | Get-FileHash | Sort-Object Path)
    $groups = @($before | Group-Object Hash | Where-Object Count -GT 1)
    $expected = @($groups.Group.Path | ForEach-Object { Get-NormalPath $_ } | Sort-Object)
    $runner = New-TestRunner @{ Options = @{ FileHashAlgorithm = $Case.Algorithm; SampleLargeFiles = 0 } }
    $rows = Invoke-ScanReport $runner @($root) 'duplicates' -Kind Duplicates
    Assert-Sequence @($rows.Name | ForEach-Object { Get-NormalPath $_ } | Sort-Object) $expected `
        'Duplicate exports agree with independent whole-file hashes, including files sharing their prefix and suffix'
    $after = @(Get-ChildItem -LiteralPath $root -File -Recurse | Get-FileHash | Sort-Object Path)
    Assert-Sequence @($after.Hash) @($before.Hash) 'Duplicate scanning preserves every source payload'
}

function Test-ReportRoundTrip {
    param($Context, $Case)
    $root = New-TestTree
    $snapshot = Get-DiskSnapshot $root
    $runner = New-TestRunner
    $source = Invoke-ScanReport $runner @($root) 'saved' -Format $Case.Format
    Assert-ScanSnapshot $source $snapshot $root -Physical
    $sentinel = New-TestFile (Join-Path $root 'unscanned-after-export.txt') 7829
    [IO.File]::WriteAllBytes((Join-Path $root 'alpha\selected-a.bin'), [byte[]]::new(3))
    $app = Start-TestApp -Runner $runner -Arguments @('/loadfrom', (Join-Path $Context.Root "saved.$($Case.Format)"))
    $app.Roots = @($root)
    $loaded = Save-AppReport $app 'loaded' -Format $Case.Format
    $fields = @('Name', 'Files', 'Folders', 'Logical Size', 'Physical Size', 'Attributes', 'Last Change',
        'WinDirStat Attributes', 'Index')
    Assert-Sequence (Get-ReportSignature $loaded $fields) (Get-ReportSignature $source $fields) `
        'Loading and re-exporting uses the saved model even when live files have changed'
    Assert-Equal @($loaded | Where-Object Name -IEQ $sentinel).Count 0 `
        'Loading a report does not scan newly created files'
    Invoke-AppCommand $app 'ID_REFRESH_ALL'
    Wait-AppIdle $app
    $refreshed = Save-AppReport $app 'refreshed'
    Assert-ScanSnapshot $refreshed (Get-DiskSnapshot $root) $root -Physical
}

function Test-QuietFailure {
    param($Context, $Case)
    $root = New-TestTree
    $runner = New-TestRunner
    $missing = Join-Path $Context.Root 'nonexistent'
    $output = Join-Path $Context.Root 'must-not-exist.csv'
    $invalid = @(
        @('/saveto', $output, $missing),
        @('/saveto', $output, (Join-Path $root 'alpha\selected-a.bin')),
        @('/saveto', (Join-Path $missing 'report.csv'), $root),
        @('/saveto', $output, '/savedupesto', (Join-Path $Context.Root 'also-missing.csv'), $root)
    )
    foreach ($arguments in $invalid) {
        $probe = Invoke-TestProcess $runner $arguments -Seconds 10
        Assert-Equal $probe.ExitCode 1 `
            'An invalid quiet invocation terminates with a normal error rather than a hidden dialog or crash'
        Assert-True (-not (Test-Path -LiteralPath $output)) `
            'A rejected quiet invocation does not leave a misleading output report'
    }
    $rows = Invoke-ScanReport $runner @($root) 'after-rejection'
    Assert-ScanSnapshot $rows (Get-DiskSnapshot $root) $root
}

function Test-PermissionExport {
    param($Context, $Case)
    $root = New-TestTree
    $explicit = Join-Path $root 'explicit'
    $inherited = Join-Path $explicit 'inherited'
    [IO.Directory]::CreateDirectory($inherited) | Out-Null
    $acl = Get-Acl -LiteralPath $explicit
    $sid = [Security.Principal.SecurityIdentifier]::new('S-1-5-7')
    $rule = [Security.AccessControl.FileSystemAccessRule]::new($sid, [Security.AccessControl.FileSystemRights]::Read,
        [Security.AccessControl.InheritanceFlags]'ContainerInherit, ObjectInherit',
        [Security.AccessControl.PropagationFlags]::None, [Security.AccessControl.AccessControlType]::Allow)
    $acl.AddAccessRule($rule)
    Set-Acl -LiteralPath $explicit -AclObject $acl
    $runner = New-TestRunner
    $csv = Invoke-ScanReport $runner @($root) 'permissions-csv' -Kind Permissions
    $json = Invoke-ScanReport $runner @($root) 'permissions-json' -Kind Permissions -Format json
    $fields = @('Name', 'Account', 'Access', 'Rights', 'Applies To', 'Inherited', 'Access Mask')
    Assert-Sequence (Get-ReportSignature $csv $fields) (Get-ReportSignature $json $fields) `
        'CSV and JSON describe the same real ACLs and inheritance scopes'
    Assert-True (@($csv | Where-Object Name -IEQ $explicit).Count -gt 0) 'An explicit child ACL is exported'
    Assert-Equal @($csv | Where-Object Name -IEQ $inherited).Count 0 `
        'Inherited-only descendants do not duplicate parent ACL entries'
    $expectedAccount = $sid.Translate([Security.Principal.NTAccount]).Value
    Assert-True (@($csv | Where-Object { $_.Name -ieq $explicit -and $_.Account -ieq $expectedAccount }).Count -gt 0) `
        'The exported account resolves the SID actually applied to the fixture'
}

foreach ($algorithm in @(@{ Name = 'sha256'; Value = 2 }, @{ Name = 'xxhash'; Value = 5 })) {
    Register-Scenario "reports.duplicates.$($algorithm.Name)" `
        Reports 'Full-file duplicate verification across staged hashing' `
        Test-DuplicateContent -Tags Filesystem -Data @{ Algorithm = $algorithm.Value }
}
foreach ($format in 'csv', 'json') {
    Register-Scenario "reports.roundtrip.$format" Reports `
        'Saved models survive external changes and refresh to current disk state' `
        Test-ReportRoundTrip -Tags Desktop,Persistence,External -Requires Windows,Desktop -Data @{ Format = $format }
}
Register-Scenario reports.quiet-failure Reports `
    'Invalid external CLI requests terminate and leave the next scan usable' `
    Test-QuietFailure -Tags External
Register-Scenario reports.permissions Reports 'Real ACL inheritance agrees across CSV and JSON exports' `
    Test-PermissionExport -Tags Filesystem -Requires Windows,Ntfs
