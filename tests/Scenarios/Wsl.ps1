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
    $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = 1 } }
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $rows = Get-WslRelativeRows (Invoke-ScanReport $runner @($fixture.Windows) 'helper') $fixture.Windows
    $helperTime = $watch.Elapsed

    # Forbidden characters appear in the private-use range and invalid bytes as U+FFFD, as on the share
    $files = [ordered]@{
        'tree\deep\er\five-thousand.bin' = 5000; "tree\colon$([char]0xF03A)name" = 3; "tree\q$([char]0xF03F)mark" = 1
        "tree\back$([char]0xF05C)slash" = 2; "tree\star$([char]0xF02A)pipe$([char]0xF07C)" = 4
        'tree\Grüße.txt' = 7; "tree\bad$([char]0xFFFD)name" = 7; 'tree\sparse.bin' = 8MB
        'tree\linked-a' = 12000; 'tree\linked-b' = 12000; 'tree\link' = 14
    }
    $folders = @('tree', 'tree\deep', 'tree\deep\er', 'tree\empty-dir', 'tree\inner-mnt', 'mnt', 'bound')
    Assert-Sequence @($rows.Keys | Where-Object { $_ } | Sort-Object) @(@($files.Keys) + $folders | Sort-Object) `
        'The helper lists every entry with Windows share names and does not enter mounts'
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

    # The Plan 9 share must agree on every name and size outside the mounts it does enter
    $basicRunner = New-TestRunner @{ Options = @{ UseFastScanEngine = 0 } }
    $watch.Restart()
    $basic = Get-WslRelativeRows (Invoke-ScanReport $basicRunner @($fixture.Windows) 'share') $fixture.Windows
    Write-TestNote "Helper scan $([int]$helperTime.TotalMilliseconds) ms; share scan $([int]$watch.Elapsed.TotalMilliseconds) ms"
    $outsideMounts = { param($key) $key -and $key -notlike 'mnt\*' -and $key -notlike 'bound\*' -and
        $key -notlike 'tree\inner-mnt\*' }
    Assert-Sequence @($basic.Keys | Where-Object { & $outsideMounts $_ } | Sort-Object) `
        @($rows.Keys | Where-Object { & $outsideMounts $_ } | Sort-Object) 'Helper and share names agree'
    foreach ($file in $files.Keys | Where-Object { $_ -ne 'tree\link' }) {
        Assert-Equal ([long]$basic[$file].'Logical Size') ([long]$rows[$file].'Logical Size') "Share and helper agree on $file"
    }
}

function Test-WslRefresh {
    param($Context, $Case)
    $distro = Get-WslTestDistro
    $fixture = New-WslFixture $distro
    $app = Start-TestApp -Roots @($fixture.Windows) -Settings @{ Options = @{ UseFastScanEngine = 1 } } -Hidden
    $list = Wait-Until {
        foreach ($control in [TestDesktop]::Children($app.Window)) {
            if ([TestDesktop]::Class($control) -ne 'SysListView32') { continue }
            if ('tree' -in [NativeListViewHelper]::GetItemTexts($control)) { return $control }
        }
    } 'The scanned WSL tree is listed'
    Assert-Equal ([NativeListViewHelper]::GetItemTexts($list, 6)[(Find-AppRow $list 'tree')]) '11' `
        'The tree initially holds its eleven files'

    # A refreshed folder deep inside the distribution is listed again by the helper from that folder
    Invoke-WslRoot $distro 'head -c 777 /dev/zero > "$1/tree/added.bin"' @($fixture.Linux) | Out-Null
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]@((Find-AppRow $list 'tree')))) 'The tree folder is selected'
    Wait-Until { (Get-AppCommandState $app 'ID_REFRESH_SELECTED').Enabled } 'The folder can be refreshed' | Out-Null
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Assert-Equal ([NativeListViewHelper]::GetItemTexts($list, 6)[(Find-AppRow $list 'tree')]) '12' `
        'Refreshing the folder adds the new file but does not enter the tmpfs mounted inside it'

    # A mount point refreshed on its own stays unentered, as in the first listing
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]@((Find-AppRow $list 'mnt')))) 'The mount point is selected'
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Assert-Equal ([NativeListViewHelper]::GetItemTexts($list, 6)[(Find-AppRow $list 'mnt')]) '0' `
        'Refreshing a mount point keeps it unentered'
}

function Test-WslDiskImport {
    param($Context, $Case)
    # Reports list a WSL tree under the folder of the virtual disk it replaced; without a registered
    # distribution the tree belongs to the folder read just before it.
    $root = 'C:\WdsWslImport'
    $distro = 'WdsMissing' + [guid]::NewGuid().ToString('N').Substring(0, 8)
    $share = "\\wsl.localhost\$distro"
    $header = '"Name","Files","Folders","Logical Size","Physical Size","Attributes","Last Change","WinDirStat Attributes","Index","ScanOutcome","ScanError"'
    $lines = @(
        $header
        "`"$root`",2,3,7000,12288,`"`",2026-01-01T00:00:00Z,0x30000004,0x0,`"complete`",`"`""
        "`"$root\disk`",2,2,7000,12288,`"`",2026-01-01T00:00:00Z,0x20000004,0x0,`"complete`",`"`""
        "`"$share`",2,1,7000,12288,`"`",2026-01-01T00:00:00Z,0x20002004,0x2,`"complete`",`"`""
        "`"$share\etc`",1,0,5000,8192,`"`",2026-01-01T00:00:00Z,0x20002004,0xA,`"complete`",`"`""
        "`"$share\etc\hosts`",0,0,5000,8192,`"`",2026-01-01T00:00:00Z,0x20002008,0xB,`"complete`",`"`""
        "`"$share\readme`",0,0,2000,4096,`"`",2026-01-01T00:00:00Z,0x20002008,0xC,`"complete`",`"`""
        "`"$share\<Unused Virtual Disk Space>`",0,0,0,0,`"`",1601-01-01T00:00:00Z,0x20002020,0x0,`"complete`",`"`""
    )
    $path = Join-Path $Context.Root 'wsl-disk.csv'
    [IO.File]::WriteAllLines($path, $lines, [Text.UTF8Encoding]::new($false))
    $app = Start-TestApp -Arguments @('/loadfrom', $path) -Hidden
    $list = Wait-Until {
        foreach ($control in [TestDesktop]::Children($app.Window)) {
            if ([TestDesktop]::Class($control) -ne 'SysListView32') { continue }
            if ('disk' -in [NativeListViewHelper]::GetItemTexts($control)) { return $control }
        }
    } 'The imported report is listed'
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]@((Find-AppRow $list 'disk')))) 'The disk folder is selected'
    [void] [NativeListViewHelper]::PostRight($list)
    Wait-Until { $share -in [NativeListViewHelper]::GetItemTexts($list) } `
        'The WSL tree is imported inside the folder that held its disk' | Out-Null
    $row = [array]::IndexOf([NativeListViewHelper]::GetItemTexts($list), $share)
    Assert-Equal ([NativeListViewHelper]::GetItemTexts($list, 6)[$row]) '2' 'The imported WSL tree keeps its files'
}

Register-Scenario wsl.scan Wsl 'The WSL helper lists a distribution like its share, without entering mounts' `
    Test-WslScan -Tags Wsl,External -Requires Windows,Wsl
Register-Scenario wsl.refresh Wsl 'Refreshing a WSL folder lists it again through the helper' `
    Test-WslRefresh -Tags Wsl,Persistence -Requires Windows,Wsl
Register-Scenario wsl.disk-import Wsl 'Reports restore WSL trees shown in place of their virtual disks' `
    Test-WslDiskImport -Tags Wsl,Persistence,External
