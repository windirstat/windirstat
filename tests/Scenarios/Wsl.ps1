function New-WslFixture {
    # A disposable tree on the distribution's root filesystem with names Windows forbids, invalid UTF-8,
    # sparse and hard-linked files, a dangling symlink, and two mounts that a scan must not enter.
    param([string] $Distro)
    $linux = '/var/tmp/wds-test-' + [guid]::NewGuid().ToString('N').Substring(0, 12)
    Add-Cleanup 'Remove the WSL fixture and its mounts' {
        param($distro, $linux)
        Invoke-WslRoot $distro 'umount "$1/bound" "$1/tree/inner-mnt" "$1/mnt" 2>/dev/null; rm -rf "$1"' @($linux) |
            Out-Null
    } @($Distro, $linux)
    Invoke-WslRoot $Distro @'
set -e
r=$1
mkdir -p "$r/tree/deep/er" "$r/tree/empty-dir" "$r/tree/inner-mnt" "$r/mnt" "$r/bound"
head -c 5000 /dev/zero > "$r/tree/deep/er/five-thousand.bin"
printf abc > "$r/tree/colon:name"
printf q > "$r/tree/q?mark"
printf bs > "$r/tree/back\slash"
printf 1234 > "$r/tree/star*pipe|"
printf 'Grüße' > "$r/tree/Grüße.txt"
printf invalid > "$r/tree/$(printf 'bad\377name')"
printf invalid > "$r/tree/$(printf 'lead\365\200\200\342\202name')"
truncate -s 8M "$r/tree/sparse.bin"
head -c 12000 /dev/zero > "$r/tree/linked-a"
ln "$r/tree/linked-a" "$r/tree/linked-b"
ln -s missing-target "$r/tree/link"
mount -t tmpfs wds-test "$r/mnt"
printf hidden > "$r/mnt/inside"
mount -t tmpfs wds-test "$r/tree/inner-mnt"
printf hidden > "$r/tree/inner-mnt/inside"
mount --bind "$r/tree" "$r/bound"
'@ @($linux) | Out-Null
    return [pscustomobject]@{ Linux = $linux; Windows = "\\wsl.localhost\$Distro" + $linux.Replace('/', '\') }
}

function Get-WslRelativeRows {
    param([object[]] $Rows, [string] $Root)
    $map = @{}
    foreach ($row in $Rows) {
        $name = [string]$row.Name
        if ($name -ieq $Root) { $map[''] = $row; continue }
        if ($name.StartsWith($Root + '\', [StringComparison]::OrdinalIgnoreCase)) { $map[$name.Substring($Root.Length + 1)] = $row }
    }
    return $map
}

function Test-WslScan {
    param($Context, $Case)
    $distro = Get-WslTestDistro
    $fixture = New-WslFixture $distro
    $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = 1; ExcludeSymbolicLinksFile = 0 } }
    Invoke-WslRoot $distro 'mount -o remount,noexec "$1/mnt"' @($fixture.Linux) | Out-Null
    $previousTmp = $env:TMPDIR
    $previousWsl = $env:WSLENV
    try {
        $env:TMPDIR = $fixture.Linux + '/mnt'
        $env:WSLENV = 'TMPDIR/u'
        $rows = Get-WslRelativeRows (Invoke-ScanReport $runner @($fixture.Windows) 'helper') $fixture.Windows
        Invoke-WslRoot $distro 'test -z "$(find "$1/mnt" -name "wds-wsl-scan.*" -print)"' @($fixture.Linux) | Out-Null
    }
    finally { $env:TMPDIR = $previousTmp; $env:WSLENV = $previousWsl }

    # Forbidden characters appear in the private-use range and invalid UTF-8 as U+FFFD, as on the share: one
    # for each invalid byte and one for a truncated sequence
    $files = [ordered]@{
        'tree\deep\er\five-thousand.bin' = 5000; "tree\colon$([char]0xF03A)name" = 3; "tree\q$([char]0xF03F)mark" = 1
        "tree\back$([char]0xF05C)slash" = 2; "tree\star$([char]0xF02A)pipe$([char]0xF07C)" = 4
        'tree\Grüße.txt' = 7; "tree\bad$([char]0xFFFD)name" = 7; 'tree\sparse.bin' = 8MB
        "tree\lead$(([string][char]0xFFFD) * 4)name" = 7
        'tree\linked-a' = 12000; 'tree\linked-b' = 12000; 'tree\link' = 14
    }
    $folders = @('tree', 'tree\deep', 'tree\deep\er', 'tree\empty-dir', 'tree\inner-mnt', 'mnt', 'bound')
    Assert-Sequence @($rows.Keys | Where-Object { $_ } | Sort-Object) @(@($files.Keys) + $folders | Sort-Object) `
        'The helper retries noexec storage, lists every entry with share names, and does not enter mounts'
    foreach ($file in $files.GetEnumerator()) {
        Assert-Equal ([long]$rows[$file.Key].'Logical Size') ([long]$file.Value) "Logical size of $($file.Key)"
    }
    foreach ($mount in 'mnt', 'bound', 'tree\inner-mnt') {
        Assert-Equal "$($rows[$mount].Files)/$($rows[$mount].Folders)" '0/0' "The $mount mount point stays empty"
    }
    Assert-True ([long]$rows['tree\sparse.bin'].'Physical Size' -lt 8MB) 'A sparse file counts only allocated blocks'
    $linked = @([long]$rows['tree\linked-a'].'Physical Size', [long]$rows['tree\linked-b'].'Physical Size')
    Assert-True (($linked | Measure-Object -Minimum).Minimum -eq 0 -and ($linked | Measure-Object -Maximum).Maximum -ge 12000) `
        'Blocks shared by hard links are counted once'
    $allocated = ($files.Keys | ForEach-Object { [long]$rows[$_].'Physical Size' } | Measure-Object -Sum).Sum
    Assert-Equal ([long]$rows[''].'Physical Size') ([long]$allocated) 'The root totals the allocation of its files'
    Assert-Equal ([long]$rows[''].Files) ([long]$files.Count) 'The root counts every file once'
    Assert-Equal ([string]$rows[''].ScanOutcome) 'complete' 'The helper scan completes'

    # Inodes from different filesystems can coincide, including files mounted inside the scanned tree.
    Add-Cleanup 'Unmount the WSL file fixtures' {
        param($distro, $linux)
        Invoke-WslRoot $distro 'umount "$1/tree/mounted-a" "$1/tree/mounted-b"' @($linux) | Out-Null
    } @($distro, $fixture.Linux)
    Invoke-WslRoot $distro @'
set -e
touch "$1/tree/mounted-a" "$1/tree/mounted-b"
mount --bind "$1/mnt/inside" "$1/tree/mounted-a"
mount --bind "$1/tree/inner-mnt/inside" "$1/tree/mounted-b"
test "$(stat -c %i "$1/tree/mounted-a")" = "$(stat -c %i "$1/tree/mounted-b")"
'@ @($fixture.Linux) | Out-Null
    $mounted = Get-WslRelativeRows (Invoke-ScanReport $runner @($fixture.Windows) 'mounted-files') $fixture.Windows
    $extra = [long]$mounted['tree\mounted-a'].'Physical Size' + [long]$mounted['tree\mounted-b'].'Physical Size'
    Assert-True ($extra -ge 8192) 'Files on distinct mounts keep their separate allocations'
    Assert-Equal ([long]$mounted[''].'Physical Size') ([long]$rows[''].'Physical Size' + $extra) `
        'Equal inode numbers on distinct mounts are not counted as hardlinks'
}

function Test-WslRefresh {
    param($Context, $Case)
    $distro = Get-WslTestDistro
    $fixture = New-WslFixture $distro
    $app = Start-TestApp -Roots @($fixture.Windows) -Settings @{
        Options = @{ UseFastScanEngine = 1; ExcludeSymbolicLinksFile = 0 }
    } -Hidden
    $list = Wait-Until {
        foreach ($control in [TestDesktop]::Children($app.Window)) {
            if ([TestDesktop]::Class($control) -ne 'SysListView32') { continue }
            if ('tree' -in [NativeListViewHelper]::GetItemTexts($control)) { return $control }
        }
    } 'The scanned WSL tree is listed'
    Assert-Equal ([NativeListViewHelper]::GetItemTexts($list, 6)[(Find-AppRow $list 'tree')]) '12' `
        'The tree initially holds its twelve files'

    # A refreshed folder deep inside the distribution is listed again by the helper from that folder
    Invoke-WslRoot $distro 'head -c 777 /dev/zero > "$1/tree/added.bin"' @($fixture.Linux) | Out-Null
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]@((Find-AppRow $list 'tree')))) 'The tree folder is selected'
    Wait-Until { (Get-AppCommandState $app 'ID_REFRESH_SELECTED').Enabled } 'The folder can be refreshed' | Out-Null
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Assert-Equal ([NativeListViewHelper]::GetItemTexts($list, 6)[(Find-AppRow $list 'tree')]) '13' `
        'Refreshing the folder adds the new file but does not enter the tmpfs mounted inside it'

    # A mount point refreshed on its own stays unentered, as in the first listing
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]@((Find-AppRow $list 'mnt')))) 'The mount point is selected'
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Assert-Equal ([NativeListViewHelper]::GetItemTexts($list, 6)[(Find-AppRow $list 'mnt')]) '0' `
        'Refreshing a mount point keeps it unentered'
}

function Test-WslHardlinks {
    param($Context, $Case)
    $distro = Get-WslTestDistro
    $fixture = New-WslFixture $distro
    Invoke-WslRoot $distro @'
set -e
mkdir -p "$1/links/a" "$1/links/b"
head -c 65536 /dev/zero > "$1/links/a/a.bin"
ln "$1/links/a/a.bin" "$1/links/b/b.bin"
'@ @($fixture.Linux) | Out-Null
    $root = $fixture.Windows + '\links'
    foreach ($name in 'a.bin', 'b.bin') {
        $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = 1 }
            DriveSelect = @{ FilteringIncludeFiles = $name } }
        $rows = Invoke-ScanReport $runner @($root) $name
        Assert-Equal ([long]$rows[0].'Physical Size') 65536L 'A retained hardlink counts its blocks after filtering'
    }

    $app = Start-TestApp -Roots @($root) -Settings @{ Options = @{ UseFastScanEngine = 1 } }
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('a')
    [void][NativeListViewHelper]::PostRight($list)
    Wait-Until { 'a.bin' -in [NativeListViewHelper]::GetItemTexts($list) } 'The linked file is visible' | Out-Null
    Select-AppRows $app $list @('a.bin')
    Invoke-WslRoot $distro 'truncate -s 0 "$1/links/a/a.bin"' @($fixture.Linux) | Out-Null
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    $shrunk = Save-AppReport $app 'shrunk'
    Assert-Equal ([long]$shrunk[0].'Physical Size') 0L 'Refreshing one file replaces stale allocation on its other link'

    Invoke-WslRoot $distro 'head -c 8192 /dev/zero > "$1/links/b/b.bin"' @($fixture.Linux) | Out-Null
    Select-AppRows $app $list @('b')
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    $grown = Save-AppReport $app 'grown'
    Assert-Equal ([long]$grown[0].'Physical Size') 8192L 'A partial refresh counts shared blocks once'

    $counted = @(Get-ReportFiles $grown | Where-Object { [long]$_.'Physical Size' -gt 0 })[0]
    $branch = if ($counted.Name.EndsWith('\a.bin')) { 'a' } else { 'b' }
    Invoke-WslRoot $distro 'rm "$1/links/$2/$2.bin"' @($fixture.Linux, $branch) | Out-Null
    Select-AppRows $app $list @($branch)
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    $deleted = Save-AppReport $app 'deleted'
    Assert-Equal ([long]$deleted[0].'Physical Size') 8192L 'Removing the counted link transfers allocation to its survivor'
    Stop-TestApp $app
    $survivor = if ($branch -eq 'a') { 'b' } else { 'a' }
    Invoke-WslRoot $distro 'ln "$1/links/$2/$2.bin" "$1/links/$3/$3.bin"' @($fixture.Linux, $survivor, $branch) |
        Out-Null

    # Switching engines while refreshing a saved branch replaces all identities in its WSL tree.
    $snapshot = 'grown'
    foreach ($engine in 0, 1) {
        $switched = Start-TestApp -Settings @{ Options = @{ UseFastScanEngine = $engine } } `
            -Arguments @('/loadfrom', (Join-Path $Context.Root "$snapshot.csv"))
        $switched.Roots = @($root)
        $switchedList = Get-AppList $switched -AllFiles
        Select-AppRows $switched $switchedList @('a')
        Invoke-AppCommand $switched 'ID_REFRESH_SELECTED'
        Wait-AppIdle $switched
        $snapshot = "engine-$engine"
        $rows = Save-AppReport $switched $snapshot
        Assert-Equal @($rows | Where-Object {
            ([Convert]::ToUInt64($_.Index, 16) -ne 0) -ne [bool]$engine
        }).Count 0 'A partial refresh never mixes native and share identities'
        Assert-Equal ([long]$rows[0].'Physical Size') $(if ($engine) { 8192L } else { 16384L }) `
            'The complete tree uses the selected engine accounting'
        Stop-TestApp $switched
    }
}

function Test-WslInterruption {
    param($Context, $Case)
    $distro = Get-WslTestDistro
    $fixture = New-WslFixture $distro
    Invoke-WslRoot $distro @'
set -e
cat > "$1/helper" <<'SH'
#!/bin/sh
rm -f "$0"
echo ready > "$WDS_TEST_STATE/ready"
head -c 131072 /dev/zero >&2
while [ ! -e "$WDS_TEST_STATE/release" ]; do sleep 0.1; done
SH
cat > "$1/bad-helper" <<'SH'
#!/bin/sh
rm -f "$0"
exec /bin/cat "$WDS_TEST_STATE/stream"
SH
'@ @($fixture.Linux) | Out-Null

    # A directory moved under another during traversal can be listed twice and form a cycle in the snapshot.
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    $writer.Write([Convert]::FromHexString('57445357534C01000100000000000000'))
    foreach ($entry in @(@(1, 0, ''), @(2, 1, 'a'), @(1, 2, 'b'))) {
        $name = [Text.Encoding]::UTF8.GetBytes($entry[2])
        $writer.Write([byte[]]@(1, 1, 0, 0)); $writer.Write([uint16]$name.Length); $writer.Write([uint16]0)
        $writer.Write([uint32]1); $writer.Write([uint64]$entry[0]); $writer.Write([uint64]$entry[1])
        $writer.Write([uint64]0); $writer.Write([uint64]0); $writer.Write([int64]0)
        $writer.Write([uint32]0); $writer.Write([uint32]0); $writer.Write($name)
    }
    $writer.Write([byte[]]@(3, 0, 0, 0)); $writer.Write([uint32]0); $writer.Write([uint64]3)
    [IO.File]::WriteAllBytes(($fixture.Windows + '\stream'), $stream.ToArray())
    $writer.Dispose()
    $tools = Join-Path $Context.Root 'wsl-tools'
    [IO.Directory]::CreateDirectory($tools) | Out-Null
    [IO.File]::WriteAllText((Join-Path $tools 'cat'),
        "#!/bin/sh`nif [ -e `"`$WDS_TEST_STATE/native`" ]; then exec /bin/cat; fi`n" +
        "/bin/cat >/dev/null`n/bin/cat `"`$WDS_TEST_HELPER`"`n", [Text.UTF8Encoding]::new($false))
    foreach ($command in 'mktemp', 'chmod', 'cp', 'rm', 'head', 'sleep') {
        [IO.File]::WriteAllText((Join-Path $tools $command),
            "#!/bin/sh`nPATH=/usr/bin:/bin exec $command `"`$@`"`n", [Text.UTF8Encoding]::new($false))
    }
    $saved = @{}
    foreach ($name in 'PATH', 'WSLENV', 'WDS_TEST_HELPER', 'WDS_TEST_STATE') {
        $saved[$name] = [Environment]::GetEnvironmentVariable($name)
    }
    try {
        $env:PATH = $tools + ';' + $env:PATH
        $env:WSLENV = 'PATH/l:WDS_TEST_HELPER/u:WDS_TEST_STATE/u'
        $env:WDS_TEST_STATE = $fixture.Linux
        $env:WDS_TEST_HELPER = $fixture.Linux + '/bad-helper'
        $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = 1 } }
        $rows = Get-WslRelativeRows (Invoke-ScanReport $runner @($fixture.Windows) 'cyclic') $fixture.Windows
        Assert-True ($rows.ContainsKey('tree\deep\er\five-thousand.bin') -and -not $rows.ContainsKey('a')) `
            'A cyclic helper snapshot falls back to the share'

        # A distribution shown in place of its disk has no share to fall back on, since the share also lists
        # /proc and the distribution's disk again under /mnt/wslg
        $disk = Get-WslTestDisk $distro
        if ($disk -and (Test-Path -LiteralPath $disk -PathType Leaf)) {
            $diskRunner = New-TestRunner @{ Options = @{ UseFastScanEngine = 1; ScanWslDisks = 1 } }
            $report = Join-Path $Context.Root 'failed-disk.csv'
            $probe = Invoke-TestProcess $diskRunner @('/saveto', $report, [IO.Path]::GetDirectoryName($disk))
            $share = "\\wsl.localhost\$distro"
            $diskRows = Read-ScanReport $report
            $distroRow = $diskRows | Where-Object Name -eq $share
            Assert-True ($probe.ExitCode -ne 0 -and $distroRow -and $distroRow.ScanOutcome -eq 'enumeration-error') `
                'A virtual disk that the helper cannot list is reported as incomplete'
            Assert-Sequence @($diskRows | Where-Object { $_.Name.StartsWith($share + '\') } | ForEach-Object Name) `
                @("$share\<Unused Virtual Disk Space>") `
                'A virtual disk that the helper cannot list is not read through the share'
        }
        else { Write-TestNote "The virtual disk fallback was not checked: $distro has no disk file." }

        # A failed native branch refresh cannot mix share metadata with surviving native hardlinks.
        Invoke-WslRoot $distro @'
set -e
PATH=/usr/bin:/bin
mkdir -p "$1/links/a" "$1/links/b"
head -c 65536 /dev/zero > "$1/links/a/a.bin"
ln "$1/links/a/a.bin" "$1/links/b/b.bin"
touch "$1/native"
'@ @($fixture.Linux) | Out-Null
        $linked = Start-TestApp -Roots @($fixture.Windows + '\links') -Settings @{
            Options = @{ UseFastScanEngine = 1 }
        }
        $linkedList = Get-AppList $linked -AllFiles
        Invoke-WslRoot $distro 'PATH=/usr/bin:/bin rm "$1/native"' @($fixture.Linux) | Out-Null
        Select-AppRows $linked $linkedList @('a')
        Invoke-AppCommand $linked 'ID_REFRESH_SELECTED'
        Wait-AppIdle $linked
        $failed = Save-AppReport $linked 'failed-links'
        Assert-Equal ([long]$failed[0].'Physical Size') 65536L 'A failed refresh does not count shared blocks twice'
        Assert-True (([Convert]::ToUInt32($failed[0].'WinDirStat Attributes', 16) -band 0x4000) -ne 0) `
            'The failed refresh leaves the tree incomplete'
        Assert-Equal ([string]@($failed | Where-Object Name -EQ ($fixture.Windows + '\links\a'))[0].ScanOutcome) `
            'enumeration-error' 'The native branch reports its helper failure'

        Invoke-WslRoot $distro 'PATH=/usr/bin:/bin touch "$1/native"' @($fixture.Linux) | Out-Null
        Invoke-AppCommand $linked 'ID_REFRESH_SELECTED'
        Wait-AppIdle $linked
        $recovered = Save-AppReport $linked 'recovered-links'
        Assert-Equal ([long]$recovered[0].'Physical Size') 65536L 'Retrying restores the link without double counting'
        Assert-Equal ([long]$recovered[0].Files) 2L 'Retrying restores both hardlinks'
        Assert-True (([Convert]::ToUInt32($recovered[0].'WinDirStat Attributes', 16) -band 0x4000) -eq 0) `
            'Retrying clears the incomplete result'
        Stop-TestApp $linked
        Invoke-WslRoot $distro 'PATH=/usr/bin:/bin rm "$1/native"' @($fixture.Linux) | Out-Null

        $env:WDS_TEST_HELPER = $fixture.Linux + '/helper'
        $app = Start-TestApp -Roots @($fixture.Windows) -Settings @{
            Options = @{ UseFastScanEngine = 1 }
        } -Hidden -DuringScan
        Wait-Until { Test-Path -LiteralPath ($fixture.Windows + '\ready') } 'The stalled helper starts' | Out-Null
        Wait-AppScanning $app
        Invoke-AppCommand $app 'ID_SCAN_SUSPEND'
        Wait-Until { (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled } `
            'A silent helper with a full error pipe can be paused' -Seconds 5 | Out-Null
        Invoke-AppCommand $app 'ID_SCAN_RESUME'
        Invoke-AppCommand $app 'ID_SCAN_STOP'
        Wait-AppIdle $app -Seconds 8
        Assert-True (-not (Get-AppCommandState $app 'ID_SCAN_RESUME').Enabled) 'Stopping the stalled helper finishes'
    }
    finally {
        foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
        Invoke-WslRoot $distro 'touch "$1/release"' @($fixture.Linux) | Out-Null
    }
}

function Get-WslTestDisk {
    # The virtual disk file of a registered distribution, as its registration names it
    param([string] $Distro)
    $lxss = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Lxss'
    foreach ($key in Get-ChildItem -LiteralPath $lxss -ErrorAction Ignore) {
        $entry = Get-ItemProperty -LiteralPath $key.PSPath
        $names = @($entry.PSObject.Properties.Name)
        if ('DistributionName' -notin $names -or 'BasePath' -notin $names -or $entry.DistributionName -ine $Distro) {
            continue
        }
        $file = if ('VhdFileName' -in $names -and $entry.VhdFileName) { $entry.VhdFileName } else { 'ext4.vhdx' }
        return Join-Path ($entry.BasePath -replace '^\\\\\?\\', '') $file
    }
    return ''
}

function Test-WslDiskScan {
    param($Context, $Case)
    $distro = Get-WslTestDistro
    $disk = Get-WslTestDisk $distro
    if (-not $disk -or -not (Test-Path -LiteralPath $disk -PathType Leaf)) { Skip-Scenario "$distro has no disk file." }
    # Scans name folders by their long names, which can differ from a registration made through 8.3 aliases
    $disk = [WdsNativeFs]::GetLongPath($disk)
    $folder = [IO.Path]::GetDirectoryName($disk)
    $share = "\\wsl.localhost\$distro"
    $fixture = New-WslFixture $distro
    $searchName = 'wds-search-' + [guid]::NewGuid().ToString('N') + '.bin'
    Invoke-WslRoot $distro 'printf search > "$1/tree/$2"' @($fixture.Linux, $searchName) | Out-Null

    # Scanning the folder that holds a running distribution's disk lists the distribution's files in its place
    $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = 1; ScanWslDisks = 1 } }
    $before = [WdsNativeFs]::GetAllocationSize($disk)
    $rows = Invoke-ScanReport $runner @($folder) 'disk'
    $after = [WdsNativeFs]::GetAllocationSize($disk)
    $byName = @{}
    foreach ($row in $rows) { $byName[[string]$row.Name] = $row }
    Assert-True (-not $byName.ContainsKey($disk)) 'The virtual disk is not listed as a file'
    Assert-True ($byName.ContainsKey($share) -and $byName.ContainsKey("$share\<Unused Virtual Disk Space>")) `
        'The distribution and the disk space its files leave unused are listed in place of the disk'
    $total = [long]$byName[$share].'Physical Size'
    Assert-True ($total -ge [Math]::Min($before, $after) -and $total -le [Math]::Max($before, $after)) `
        "The distribution totals the allocation of its disk ($total bytes; disk $before to $after bytes)"

    # The report reopens with the distribution under its folder even where the disk now lives elsewhere
    $report = Join-Path $Context.Root 'moved.csv'
    $text = [IO.File]::ReadAllText((Join-Path $Context.Root 'disk.csv'))
    [IO.File]::WriteAllText($report, $text.Replace("`"$folder", '"C:\WdsMovedDisk'), [Text.UTF8Encoding]::new($false))
    $app = Start-TestApp -Runner $runner -Arguments @('/loadfrom', $report)
    $app.Roots = @('C:\WdsMovedDisk')
    $list = Get-AppList $app -AllFiles
    $row = Find-AppRow $list $share
    $files = [NativeListViewHelper]::GetItemTexts($list, 6)[$row]
    Assert-Equal ($files -replace '\D', '') ([string]$byName[$share].Files) 'The reopened distribution keeps its files'

    # Search and watcher snapshots must resolve share paths inside a virtual-disk branch after refresh.
    Wait-Until { (Get-AppCommandState $app 'ID_SEARCH').Enabled } 'The reopened report can be searched' | Out-Null
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_TERM'), $searchName)
    Set-DialogCheck $dialog 'IDC_SEARCH_FOLDERS' $false
    Close-AppDialog $dialog 1
    $searchPath = $fixture.Windows + '\tree\' + $searchName
    $list = Get-AppList $app -TabName 'Search Results'
    Wait-Until { $searchPath -in [NativeListViewHelper]::GetItemTexts($list) } `
        'The virtual-disk file is listed in search results' | Out-Null
    Select-AppRows $app $list @($searchPath)
    Invoke-WslRoot $distro 'printf refreshed > "$1/tree/$2"' @($fixture.Linux, $searchName) | Out-Null
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app

    # The result is restored once the refresh finishes, showing the file's new logical size
    Wait-Until {
        $row = [array]::IndexOf([NativeListViewHelper]::GetItemTexts($list), $searchPath)
        $row -ge 0 -and ([NativeListViewHelper]::GetItemTexts($list, 2)[$row] -replace '\D', '') -eq '9'
    } 'The refreshed search result shows the new file size' | Out-Null
}

function Test-WslRunnerChecks {
    param($Context, $Case)
    $distro = Get-WslTestDistro
    $arm = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture -eq 'Arm64'
    $native, $other = if ($arm) { 'aarch64', 'x86_64' } else { 'x86_64', 'aarch64' }
    $runners = @{}
    foreach ($machine in $native, $other) {
        $runners[$machine] = Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'Build\Temp\WSL') -Recurse -File `
            -Filter "FinderWSLRunner-$machine" -ErrorAction Ignore | Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
    }
    if (-not $runners[$native]) { Skip-Scenario 'Build WinDirStat to produce the WSL runners.' }

    # The runner for this processor runs directly; the other one runs under QEMU when the distribution has it
    $emulator = ([string](Invoke-WslRoot $distro "command -v qemu-$other-static || command -v qemu-$other || true")).Trim()
    foreach ($machine in $native, $other) {
        if ($machine -eq $other -and (-not $emulator -or -not $runners[$other])) {
            Write-TestNote "The $other runner was not run: its binary or qemu-$other is missing."
            continue
        }
        $arguments = @('-NoProfile', '-File', (Join-Path $RepoRoot 'tests\Test-WslRunner.ps1'),
            '-Runner', $runners[$machine].FullName, '-Distro', $distro)
        if ($machine -eq $other) { $arguments += @('-Emulator', $emulator) }
        $output = (& ([Environment]::ProcessPath) @arguments 2>&1 | Select-Object -Last 15) -join ' '
        Assert-Equal $LASTEXITCODE 0 "The $machine runner passes its checks: $output"
    }
}

Register-Scenario wsl.scan Wsl 'The WSL helper retries noexec storage and lists share names without entering mounts' `
    Test-WslScan -Tags Wsl,External -Requires Windows,Wsl
Register-Scenario wsl.refresh Wsl 'Refreshing a WSL folder lists it again through the helper' `
    Test-WslRefresh -Tags Wsl,Persistence -Requires Windows,Wsl
Register-Scenario wsl.hardlinks Wsl 'Filtering and partial refresh count WSL hardlinks once' `
    Test-WslHardlinks -Tags Wsl,Persistence -Requires Windows,Wsl
Register-Scenario wsl.interruption Wsl 'Silent WSL helpers and error output do not block pause or stop' `
    Test-WslInterruption -Tags Wsl,Race -Requires Windows,Wsl
Register-Scenario wsl.disk Wsl 'A running distribution''s disk is listed as its files and reopens from a report' `
    Test-WslDiskScan -Tags Wsl,Persistence,External -Requires Windows,Wsl
Register-Scenario wsl.runner Wsl 'The WSL runners preserve concurrent streams, deep paths, and cleanup' `
    Test-WslRunnerChecks -Tags Wsl,External -Requires Windows,Wsl
