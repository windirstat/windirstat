function Invoke-TestDiskPart {
    param([string[]] $Lines, [string] $Name)
    $path = Join-Path $script:Context.Root "$Name.diskpart"
    [IO.File]::WriteAllLines($path, $Lines, [Text.Encoding]::Unicode)
    $runner = [pscustomobject]@{ Exe = Join-Path $env:windir 'System32\diskpart.exe'; Directory = $script:Context.Root }
    $result = Invoke-TestProcess $runner @('/s', $path)
    [IO.File]::WriteAllText((Join-Path $script:Context.Root "$Name.log"), $result.Stdout + $result.Stderr)
    return $result
}

function Test-DisposableFileSystem {
    param($Context, $Case)
    $image = Assert-OwnedPath (Join-Path $Context.Root 'scratch.vhd')
    $used = @([IO.Directory]::GetLogicalDrives() | ForEach-Object { $_.Substring(0, 1) })
    $letter = 90..68 | ForEach-Object { [string][char]$_ } | Where-Object { $_ -notin $used } | Select-Object -First 1
    if (-not $letter) { Skip-Scenario 'No unused drive letter is available for the disposable volume.' }
    $volumeRoot = "${letter}:\"
    Add-Cleanup 'Detach owned virtual disk' {
        param($ImageFile)
        $owned = Assert-OwnedPath $ImageFile
        if (-not [IO.File]::Exists($owned)) { return }
        $result = Invoke-TestDiskPart @("SELECT VDISK FILE=`"$owned`"", 'DETACH VDISK') 'detach'
        $imageState = Get-DiskImage -ImagePath $owned -ErrorAction SilentlyContinue
        if ($result.ExitCode -ne 0 -or ($imageState -and $imageState.Attached)) {
            throw 'Could not detach the test-created virtual disk; see detach.log.'
        }
    } @($image)
    Invoke-TestDiskPart @(
        "CREATE VDISK FILE=`"$image`" MAXIMUM=2048 TYPE=EXPANDABLE",
        "SELECT VDISK FILE=`"$image`"", 'ATTACH VDISK', 'CREATE PARTITION PRIMARY',
        "FORMAT FS=$($Case.FileSystem) QUICK LABEL=WdsTest", "ASSIGN LETTER=$letter"
    ) 'create' | Out-Null
    $imageState = Get-DiskImage -ImagePath $image -ErrorAction SilentlyContinue
    $partitions = @($imageState | Get-Disk -ErrorAction SilentlyContinue | Get-Partition -ErrorAction SilentlyContinue)
    $volume = @($partitions | Get-Volume -ErrorAction SilentlyContinue | Where-Object FileSystem -eq $Case.FileSystem)
    if (-not $imageState -or -not $imageState.Attached -or -not $volume.Count -or
        $volumeRoot -notin @($partitions | ForEach-Object AccessPaths)) {
        if ($Case.FileSystem -ne 'ReFS') { throw 'Could not create the disposable volume; see create.log.' }
        Skip-Scenario `
            "$($Case.FileSystem) could not be created on this Windows installation; see create.log with -KeepArtifacts."
    }
    $root = New-TestTree (Join-Path $volumeRoot 'fixture')
    if ($Case.FileSystem -eq 'NTFS') {
        New-Item -ItemType HardLink -Path (Join-Path $root 'hardlink-alias.bin') `
            -Target (Join-Path $root 'alpha\selected-a.bin') | Out-Null
        $blocked = Join-Path $root 'denied'
        New-TestFile (Join-Path $blocked 'private.bin') 347 | Out-Null
        $deep = $root
        foreach ($level in 1..9) { $deep = Join-Path $deep ("level-$level-" + ('x' * 20)) }
        New-TestFile (Join-Path $deep 'deep-é.txt') 5003 31 | Out-Null
        $sparse = New-TestFile (Join-Path $root 'wide-sparse.bin') 0
        $utility = [pscustomobject]@{ Exe = Join-Path $env:windir 'System32\fsutil.exe'; Directory = $Context.Root }
        $probe = Invoke-TestProcess $utility @('sparse', 'setflag', $sparse)
        if ($probe.ExitCode -ne 0) { throw 'Could not create the sparse fixture; see the command output.' }
        $stream = [IO.File]::Open($sparse, 'Open', 'ReadWrite')
        try { $stream.SetLength(4GB + 17L); $stream.WriteByte(0x71); $stream.Flush($true) }
        finally { $stream.Dispose() }
    }
    $snapshot = @([pscustomobject]@{ Path = Get-NormalPath $root; Directory = $true; Link = $false }) +
        @(Get-DiskSnapshot $root)
    foreach ($engine in 0, 1) {
        $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = $engine }
            DriveSelect = @{ FilteringIncludeDirs = $root } }
        $rows = Invoke-ScanReport $runner @($volumeRoot) "volume-$engine"
        Assert-Equal ([Convert]::ToUInt32($rows[0].'WinDirStat Attributes', 16) -band 15) 2 `
            'A drive root reaches the fast-engine selection path'
        Assert-ScanSnapshot $rows $snapshot $volumeRoot -Physical
    }
    if ($Case.FileSystem -eq 'NTFS') {
        $acl = Get-Acl -LiteralPath $blocked
        $denied = Get-Acl -LiteralPath $blocked
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        try { $sid = $identity.User } finally { $identity.Dispose() }
        $denied.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,
            [Security.AccessControl.FileSystemRights]::ListDirectory, [Security.AccessControl.AccessControlType]::Deny))
        Add-Cleanup 'Restore disposable directory access' {
            param($Directory, $Security); Set-Acl -LiteralPath $Directory -AclObject $Security
        } @($blocked, $acl)
        Set-Acl -LiteralPath $blocked -AclObject $denied
        foreach ($engine in 0, 1) {
            $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = $engine }
                DriveSelect = @{ FilteringIncludeDirs = $root } }
            $report = Join-Path $Context.Root "denied-volume-$engine.csv"
            $result = Invoke-TestProcess $runner @('/saveto', $report, $volumeRoot)
            Assert-Equal $result.ExitCode $(if ($engine) { 0 } else { 1 }) `
                'Raw MFT traversal sees denied descendants while basic enumeration reports the denied branch'
            $rows = Read-ScanReport $report
            if ($engine) { Assert-ScanSnapshot $rows $snapshot $volumeRoot -Physical }
            else {
                Assert-Equal @($rows | Where-Object Name -IEQ (Join-Path $blocked 'private.bin')).Count 0 `
                    'The basic engine does not bypass the fixture directory ACL'
            }
        }
    }
}

function Test-UncShare {
    param($Context, $Case)
    $root = New-TestTree
    $unc = New-TestShare $root
    $snapshot = @(Get-DiskSnapshot $root)
    $rootNormal = Get-NormalPath $root
    $uncNormal = Get-NormalPath $unc
    foreach ($entry in $snapshot) { $entry.Path = $uncNormal + $entry.Path.Substring($rootNormal.Length) }
    $runner = New-TestRunner
    foreach ($spelling in @($unc, ($unc + '\'), ('\\?\UNC\' + $unc.TrimStart('\')))) {
        $name = 'unc-' + [guid]::NewGuid().ToString('N').Substring(0, 6)
        Assert-ScanSnapshot (Invoke-ScanReport $runner @($spelling) $name) $snapshot $unc
    }
}

function Test-MtpRoundTrip {
    param($Context, $Case)
    $app = Start-TestApp
    $dialog = Wait-Until { Get-AppDialog $app 'IDC_BROWSE_FOLDER' } 'The initial target picker opens'
    $list = @([NativeListViewHelper]::GetVisibleListViewsIncludingEmpty($dialog) | Select-Object -First 1)
    if (-not $list.Count) { throw 'The target picker does not expose its native device list.' }
    $list = [IntPtr]$list[0]
    Wait-Until { [NativeListViewHelper]::GetItemCount($list) -gt 0 } 'Device enumeration populates the picker' `
        -StableMilliseconds 500 | Out-Null
    $index = Find-AppRow $list $MtpDevice -Optional
    if ($index -lt 0) { Skip-Scenario "The configured device '$MtpDevice' is not connected." }
    [void] [TestDesktop]::Message((Get-DialogControl $dialog 'IDC_RADIO_TARGET_DRIVES_SUBSET'), 0x00F5)
    Assert-True ([NativeListViewHelper]::SelectSingleItem($list, $index)) `
        'The explicitly configured MTP device is selected'
    Close-AppDialog $dialog 1
    Wait-AppIdle $app
    $rows = Save-AppReport $app 'mtp-saved'
    Assert-True ($rows.Count -gt 1) 'The device scan exports its contents'
    foreach ($row in $rows) {
        Assert-True (([Convert]::ToUInt32($row.'WinDirStat Attributes', 16) -band 0x80000000L) -ne 0) `
            'MTP exports retain their virtual-item flags'
        Assert-Equal ([Convert]::ToUInt64($row.Index, 16)) 0UL `
            'Process-local MTP registrations are not serialized as disk identities'
    }
    Stop-TestApp $app
    $loaded = Start-TestApp -Arguments @('/loadfrom', (Join-Path $Context.Root 'mtp-saved.csv'))
    $roundTrip = Save-AppReport $loaded 'mtp-loaded'
    $fields = @('Name', 'Files', 'Folders', 'Logical Size', 'Physical Size', 'WinDirStat Attributes', 'Index')
    Assert-Sequence (Get-ReportSignature $roundTrip $fields) (Get-ReportSignature $rows $fields) `
        'Device reports round trip through a fresh process without relying on stale shell registrations'
}

function Test-MonitorDpiTransition {
    param($Context, $Case)
    $screens = @([System.Windows.Forms.Screen]::AllScreens)
    if ($screens.Count -lt 2) { Skip-Scenario 'Two attached monitors with different DPI settings are required.' }
    $root = New-TestTree
    $app = Start-TestApp @($root)
    $origin = [TestDesktop]::Rectangle($app.Window)
    $firstDpi = [TestDesktop]::GetDpiForWindow($app.Window)
    $target = $screens | Where-Object { -not $_.Primary } | Select-Object -First 1
    [TestDesktop]::Position($app.Window, $target.WorkingArea.Left + 20, $target.WorkingArea.Top + 20, 800, 600)
    $secondDpi = [TestDesktop]::GetDpiForWindow($app.Window)
    if ($firstDpi -eq $secondDpi) { Skip-Scenario `
        'The first two monitors use the same DPI; no DPI transition was exercised.' }
    Assert-GraphRendered $app | Out-Null
    [TestDesktop]::Position($app.Window, $origin[0], $origin[1], $origin[2] - $origin[0], $origin[3] - $origin[1])
    $baseline = Assert-GraphRendered $app
    [TestDesktop]::Position($app.Window, $target.WorkingArea.Left + 20, $target.WorkingArea.Top + 20, 800, 600)
    Assert-GraphRendered $app | Out-Null
    [TestDesktop]::Position($app.Window, $origin[0], $origin[1], $origin[2] - $origin[0], $origin[3] - $origin[1])
    Assert-FrameEqual $baseline (Get-StableFrame (Get-GraphWindow $app)) `
        'A real monitor DPI round trip restores equivalent layout and cached painting' 'dpi'
}

foreach ($fileSystem in 'NTFS', 'FAT32', 'exFAT', 'ReFS') {
    Register-Scenario "devices.volume.$($fileSystem.ToLowerInvariant())" `
        Devices 'Drive-root scans exercise MFT traversal or filesystem fallback on a disposable image' `
        Test-DisposableFileSystem -Tags Hardware,Filesystem -Requires Windows,Elevated -Data @{ FileSystem = $fileSystem }
}
Register-Scenario devices.unc Devices `
    'Share roots and extended UNC aliases enumerate without a drive-letter assumption' `
    Test-UncShare -Tags Hardware,Filesystem,External -Requires Windows,Elevated
Register-Scenario devices.mtp Devices `
    'An explicitly selected device scan exports and reloads its virtual item metadata' `
    Test-MtpRoundTrip -Tags Hardware,Desktop,Persistence -Requires Windows,Desktop,Mtp
Register-Scenario devices.monitor-dpi Devices `
    'Moving between real DPI configurations invalidates and rebuilds graphics' `
    Test-MonitorDpiTransition -Tags Hardware,Desktop,Rendering -Requires Windows,Desktop
