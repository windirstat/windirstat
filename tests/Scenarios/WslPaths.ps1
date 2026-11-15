function Test-WslCaseSensitivePaths {
    param($Context, $Case)
    $distro = Get-WslTestDistro
    $fixture = New-WslFixture $distro
    $upper = $fixture.Windows + '\paths\A'
    $lower = $fixture.Windows + '\paths\a'
    $share = "\\wsl.localhost\$distro"
    $alias = "\\WSL`$\$($distro.ToUpperInvariant())"
    $longShare = "\\?\UNC\WSL.LOCALHOST\$($distro.ToUpperInvariant())"
    $roots = @($upper, $lower, $upper.Replace($share, $alias), $lower.Replace($share, $longShare))
    $expected = @(($upper + '\same.bin'), ($lower + '\same.bin'))
    $runner = New-TestRunner @{ Options = @{ UseFastScanEngine = 1 } }

    Invoke-WslRoot $distro @'
set -e
mkdir -p "$1/paths/A" "$1/paths/a"
head -c 17 /dev/zero > "$1/paths/A/same.bin"
head -c 31 /dev/zero > "$1/paths/a/same.bin"
'@ @($fixture.Linux) | Out-Null
    $source = Invoke-ScanReport $runner $roots 'case'
    $files = @(Get-ReportFiles $source)
    Assert-Equal $files.Count 2 'Case-only Linux roots both survive while UNC aliases are deduplicated'
    foreach ($path in $expected) {
        Assert-Equal @($files | Where-Object Name -CEQ $path).Count 1 'Each exact Linux path is present once'
    }

    # Loading and searching must retain both roots before paths are rebound after a refresh.
    $app = Start-TestApp -Runner $runner -Arguments @('/loadfrom', (Join-Path $Context.Root 'case.csv'))
    $app.Roots = @($upper, $lower)
    $loaded = Save-AppReport $app 'loaded'
    $fields = @('Name', 'Files', 'Folders', 'Logical Size', 'Physical Size', 'Index')
    Assert-Sequence (Get-ReportSignature $loaded $fields) (Get-ReportSignature $source $fields) `
        'Report round trips retain the exact case and ancestry of both roots'
    Get-AppList $app -AllFiles | Out-Null
    $dialog = Open-AppDialog $app 'ID_SEARCH' 'IDC_SEARCH_TERM'
    [TestDesktop]::SetText((Get-DialogControl $dialog 'IDC_SEARCH_TERM'), 'same.bin')
    Set-DialogCheck $dialog 'IDC_SEARCH_FOLDERS' $false
    Close-AppDialog $dialog 1
    $list = Get-AppList $app -TabName 'Search Results'
    Wait-Until {
        $names = [NativeListViewHelper]::GetItemTexts($list)
        @($expected | Where-Object { $_ -cnotin $names }).Count -eq 0
    } 'Search lists both exact paths' | Out-Null

    Invoke-WslRoot $distro `
        'head -c 19 /dev/zero > "$1/paths/A/same.bin"; head -c 37 /dev/zero > "$1/paths/a/same.bin"' `
        @($fixture.Linux) | Out-Null
    $names = [NativeListViewHelper]::GetItemTexts($list)
    $indices = @(for ($index = 0; $index -lt $names.Length; ++$index) {
        if ($names[$index] -cin $expected) { $index }
    })
    Assert-True ([NativeListViewHelper]::FocusListView($list) -and
        [NativeListViewHelper]::SelectItems($list, [int[]]$indices)) 'Both exact search paths are selected'
    Invoke-AppCommand $app 'ID_REFRESH_SELECTED'
    Wait-AppIdle $app
    Wait-Until {
        $names = [NativeListViewHelper]::GetItemTexts($list)
        @($expected | Where-Object { $_ -cnotin $names }).Count -eq 0
    } 'Refreshing rebinds each search result to its own case-sensitive root' | Out-Null
    $refreshed = Save-AppReport $app 'refreshed'
    Assert-Equal ([long]@($refreshed | Where-Object Name -CEQ $expected[0])[0].'Logical Size') 19L `
        'Refreshing updates the upper-case branch'
    Assert-Equal ([long]@($refreshed | Where-Object Name -CEQ $expected[1])[0].'Logical Size') 37L `
        'Refreshing updates the lower-case branch'
    Stop-TestApp $app

    # Extended UNC roots must still use native metadata, regardless of the retained share spelling.
    $longRoots = @($upper.Replace($share, $longShare), $lower.Replace($share, $alias), $upper, $lower)
    $longRows = Invoke-ScanReport $runner $longRoots 'extended-roots'
    $longFiles = @(Get-ReportFiles $longRows)
    Assert-Equal $longFiles.Count 2 'Extended and ordinary UNC spellings identify the same Linux roots'
    Assert-True (@($longFiles | Where-Object { [Convert]::ToUInt64($_.Index, 16) -eq 0 }).Count -eq 0) `
        'Extended UNC roots retain native Linux inode identities'
}

Register-Scenario wsl.paths Wsl 'Case-only Linux roots survive selection, report loading, and search refresh' `
    Test-WslCaseSensitivePaths -Tags Wsl,Desktop,Persistence -Requires Windows,Desktop,Wsl
