function Test-RepaintContract {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root) -Settings @{ Options = @{ DarkMode = $Case.Dark } }
    $list = Get-AppList $app -AllFiles
    Expand-AppDirectory $app $list 'alpha' -Child 'selected-a.bin'
    Invoke-AppCommand $app $Case.Command
    Select-AppRows $app $list @('selected-a.bin')
    [TestDesktop]::ParkCursor($app.Window)
    $graph = Get-GraphWindow $app
    $baseline = Assert-GraphRendered $app
    Select-AppRows $app $list @('selected-b.bin')
    $otherSelection = Get-StableFrame $graph
    Assert-True ([TestDesktop]::Difference($baseline, $otherSelection) -gt 0.0001) `
        'Selecting a different tree item changes the rendered selection overlay'
    Select-AppRows $app $list @('selected-a.bin')
    Assert-FrameEqual $baseline (Get-StableFrame $graph) 'Reselecting restores the original overlay'
    [TestDesktop]::Repaint($graph, $true)
    Assert-FrameEqual $baseline (Get-StableFrame $graph) `
        'A partial invalidation preserves both data and selection overlays' 'partial'
    $cover = [TestDesktop]::Cover($graph)
    try {
        $covered = Get-StableFrame $graph -AllowOcclusion
        Assert-True ([TestDesktop]::Difference($baseline, $covered) -gt 0.05) `
            'The external window actually covers the displayed pixels'
    } finally { [void] [TestDesktop]::DestroyWindow($cover) }
    Assert-FrameEqual $baseline (Get-StableFrame $graph) `
        'Removing an occluding window repaints the same graph without forced invalidation' 'occlusion'
    [void] [TestDesktop]::ShowWindow($app.Window, 6)
    [void] [TestDesktop]::ShowWindow($app.Window, 9)
    [void] [TestDesktop]::SetForegroundWindow($app.Window)
    Assert-FrameEqual $baseline (Get-StableFrame $graph) `
        'Minimize and restore preserve the visible graph and its overlays' 'restore'
    $rect = [TestDesktop]::Rectangle($app.Window)
    [TestDesktop]::Position($app.Window, $rect[0], $rect[1], 900, 640)
    Assert-GraphRendered $app | Out-Null
    [TestDesktop]::Position($app.Window, $rect[0], $rect[1], $rect[2] - $rect[0], $rect[3] - $rect[1])
    Assert-FrameEqual $baseline (Get-StableFrame $graph) `
        'A resize round trip rebuilds an equivalent graph instead of retaining stale geometry' 'resize'
}

function Test-PaneRecovery {
    param($Context, $Case)
    $root = New-TestTree
    $position = [Convert]::ToHexString([BitConverter]::GetBytes([double]$Case.Position))
    $app = Start-TestApp @($root) -Settings @{ Options = @{
        WindowLayout = $Case.Layout; SubSplitterPos = $position; ShowFileTypes = $Case.Visible
    } }
    if (-not $Case.Visible) { Invoke-AppCommand $app 'ID_VIEW_SHOWFILETYPES' }
    Get-AppList $app -AllFiles | Out-Null
    $rect = [TestDesktop]::Rectangle($app.ExtensionList)
    Assert-True ($rect[2] - $rect[0] -ge 64 -and $rect[3] - $rect[1] -ge 48) `
        'A visible File Types pane recovers from a saved collapsed splitter position'
    $baseline = Get-StableFrame $app.ExtensionList
    Assert-True ([TestDesktop]::ColorCount($baseline) -gt 16) 'Recovery paints the extension data and controls'
    Invoke-AppCommand $app 'ID_VIEW_SHOWFILETYPES'
    Wait-Until {
        $size = [TestDesktop]::ClientSize($app.ExtensionList)
        -not (Get-AppCommandState $app 'ID_VIEW_SHOWFILETYPES').Checked -and ($size[0] -eq 0 -or $size[1] -eq 0)
    } 'The File Types pane is removed from the visible layout' | Out-Null
    Invoke-AppCommand $app 'ID_VIEW_SHOWFILETYPES'
    Get-AppList $app -AllFiles | Out-Null
    Assert-FrameEqual $baseline (Get-StableFrame $app.ExtensionList) `
        'Hiding and showing restores the recovered pane consistently' 'pane'
    $header = [TestDesktop]::Message($app.TreeList, 0x101F)
    $columns = [TestDesktop]::Message($header, 0x1200).ToInt32()
    $hidden = 0
    for ($column = 0; $column -lt $columns; ++$column) {
        if ([TestDesktop]::Message($app.TreeList, 0x101D, $column).ToInt32() -ne 0) { continue }
        ++$hidden
        [void] [NativeListViewHelper]::SetHeaderWidth($app.TreeList, $column, 80)
        Assert-Equal ([TestDesktop]::Message($app.TreeList, 0x101D, $column).ToInt32()) 0 `
            'Native header resize notifications cannot resurrect a hidden column'
    }
    Assert-True ($hidden -gt 0) 'The hidden-column regression exercised an actually hidden column'
}

function Test-SplitterCapture {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root) -Settings @{ Options = @{ WindowLayout = $Case.Layout } }
    $outer = [TestDesktop]::GetDlgItem($app.Window, 0xE900)
    $inner = [TestDesktop]::GetDlgItem($outer, 0xE900)
    $graph = Get-GraphWindow $app
    [TestDesktop]::ParkCursor($app.Window)
    foreach ($splitter in $outer, $inner) {
        $first = [TestDesktop]::GetDlgItem($splitter, 0xE900)
        $second = [TestDesktop]::GetDlgItem($splitter, 0xE901)
        Assert-True ($first -ne [IntPtr]::Zero -and $second -ne [IntPtr]::Zero) `
            'The configured topology exposes both real splitter panes'
        $firstRect = [TestDesktop]::Rectangle($first)
        $secondRect = [TestDesktop]::Rectangle($second)
        $axis = if ($secondRect[0] -ge $firstRect[2]) { 0 } else { 1 }
        foreach ($offset in 0, 3, 6) {
            [TestDesktop]::Activate($app.Window)
            $rect = [TestDesktop]::Rectangle($splitter)
            $before = [TestDesktop]::Rectangle($first)
            $point = @(20, 20)
            $point[$axis] = $before[$axis + 2] - $rect[$axis] + $offset - $(if ($first -eq $inner) { 2 } else { 0 })
            $packed = ($point[1] -shl 16) -bor ($point[0] -band 0xFFFF)
            # Capture and window moves make Windows report the real cursor, so keep it on the synthetic point
            [TestDesktop]::MoveCursor($rect[0] + $point[0], $rect[1] + $point[1])
            $graphFrame = if ($offset -eq 0) { [TestDesktop]::Repaint($graph); Get-StableFrame $graph 'graph' }
            [void] [TestDesktop]::Message($splitter, 0x0201, 1, $packed)
            Assert-Equal ([TestDesktop]::CaptureOwner($app.Window)) $splitter `
                'Dragging acquires capture on the intended splitter'
            if ($graphFrame) {
                [TestDesktop]::Repaint($graph)
                Assert-FrameEqual $graphFrame (Get-StableFrame $graph 'graph') `
                    'Pressing a splitter without dragging keeps the graph rendered' 'splitter-press'
            }
            [void] [TestDesktop]::Message($splitter, 0x0200, 1, $packed)
            Assert-Sequence ([TestDesktop]::Rectangle($first)) $before `
                'The first move retains the exact grab offset without jumping'
            $point[$axis] += 40
            $packed = ($point[1] -shl 16) -bor ($point[0] -band 0xFFFF)
            [TestDesktop]::MoveCursor($rect[0] + $point[0], $rect[1] + $point[1])
            [void] [TestDesktop]::Message($splitter, 0x0200, 1, $packed)
            $during = [TestDesktop]::Rectangle($first)
            Assert-Equal ($during[$axis + 2] - $before[$axis + 2]) 40 'Pane geometry follows the drag before mouse-up'
            if ($graphFrame -and -not [TestDesktop]::IsChild($splitter, $graph)) {
                [TestDesktop]::Repaint($graph)
                Assert-FrameEqual $graphFrame (Get-StableFrame $graph 'graph') `
                    'Dragging a splitter beside the graph keeps the graph rendered' 'splitter-drag'
            }
            if ($offset -eq 3) {
                [void] [TestDesktop]::Message($splitter, 0x0202, 0, $packed)
                Assert-Sequence ([TestDesktop]::Rectangle($first)) $during `
                    'Mouse-up accepts the currently painted pane geometry'
            } else {
                if ($offset -eq 0) { [void] [TestDesktop]::Message($splitter, 0x001F) }
                else {
                    $external = [TestDesktop]::Cover($app.Window)
                    try {
                        Assert-Equal ([TestDesktop]::TransferCapture($splitter, $external)) $splitter `
                            'Native capture transfers from the dragging splitter to the external window'
                        Assert-Equal ([TestDesktop]::CaptureOwner($external)) $external `
                            'The external window acquires actual mouse capture'
                        Wait-Until { [TestDesktop]::CaptureOwner($app.Window) -eq [IntPtr]::Zero } `
                            'The application observes external capture loss' | Out-Null
                    }
                    finally {
                        [void] [TestDesktop]::ReleaseCapture()
                        [void] [TestDesktop]::DestroyWindow($external)
                    }
                }
                [void] [TestDesktop]::Message($splitter, 0x0202, 0, $packed)
                Assert-Sequence ([TestDesktop]::Rectangle($first)) $before `
                    'Cancellation or capture loss restores the original splitter geometry'
            }
            Assert-Equal ([TestDesktop]::CaptureOwner($app.Window)) ([IntPtr]::Zero) `
                'The finished or cancelled drag releases capture'
        }
    }
    Assert-GraphRendered $app | Out-Null
}

function Test-LayoutRoundTrip {
    param($Context, $Case)
    $app = Start-TestApp @((New-TestTree)) -Settings @{ Options = @{ DarkMode = $Case.Dark } }
    $tree = Get-AppList $app -AllFiles
    Expand-AppDirectory $app $tree 'alpha' -Child 'selected-a.bin'
    Select-AppRows $app $tree @('selected-a.bin')
    $graph = Get-GraphWindow $app
    $baseline = Assert-GraphRendered $app
    $layouts = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($index in @(1..11) + @(0)) {
        Invoke-AppCommand $app 'ID_VIEW_WINDOW_LAYOUT'
        $popup = Wait-Until {
            @([TestDesktop]::Windows($app.Process.Id) | Where-Object {
                [TestDesktop]::Class($_) -eq 'WdsLayoutPopup' -and [TestDesktop]::IsWindowVisible($_)
            } | Select-Object -First 1)
        } 'The layout picker opens'
        $size = [TestDesktop]::ClientSize($popup)
        $x = [int](($index % 3 + 0.5) * $size[0] / 3)
        $y = [int](([Math]::Floor($index / 3) + 0.5) * $size[1] / 4)
        $point = ($y -shl 16) -bor $x
        [void][TestDesktop]::Message($popup, 0x0201, 1, $point)
        [void][TestDesktop]::Message($popup, 0x0202, 0, $point)
        Wait-Until { -not [TestDesktop]::IsWindowVisible($popup) } 'Choosing a layout closes the picker' | Out-Null
        $geometry = @(foreach ($pane in $tree, $app.ExtensionList, $graph) {
            Assert-True ([TestDesktop]::IsWindow($pane) -and [TestDesktop]::IsChild($app.Window, $pane)) `
                'Layout changes retain the existing view windows'
            $rect = [TestDesktop]::Rectangle($pane)
            Assert-True ($rect[2] - $rect[0] -gt 40 -and $rect[3] - $rect[1] -gt 40) `
                'Every pane remains visible with usable dimensions'
            $rect -join ','
        })
        [void]$layouts.Add($geometry -join ';')
        Assert-Equal ([NativeListViewHelper]::GetSelectedCount($tree)) 1 'Reparenting retains the file selection'
        Assert-GraphRendered $app | Out-Null
        foreach ($command in 'ID_VIEW_SHOWFILETYPES', 'ID_VIEW_SHOWVISUALIZATION',
            'ID_VIEW_SHOWFILETYPES', 'ID_VIEW_SHOWVISUALIZATION') {
            Invoke-AppCommand $app $command
            $pane = if ($command -eq 'ID_VIEW_SHOWFILETYPES') { $app.ExtensionList } else { $graph }
            $shown = (Get-AppCommandState $app $command).Checked
            Wait-Until {
                $size = [TestDesktop]::ClientSize($pane)
                if ($shown) { $size[0] -gt 40 -and $size[1] -gt 40 }
                else { $size[0] -eq 0 -or $size[1] -eq 0 }
            } 'Pane visibility follows its owning splitter in every layout' | Out-Null
        }
        $restored = @(foreach ($pane in $tree, $app.ExtensionList, $graph) {
            [TestDesktop]::Rectangle($pane) -join ','
        })
        Assert-Sequence $restored $geometry 'Hiding and showing panes restores the configured layout'
    }
    Assert-Equal $layouts.Count 12 'Every layout choice produces its own arrangement'
    Assert-FrameEqual $baseline (Get-StableFrame $graph) 'Returning to the default layout restores the graph' 'layout'
    Save-Frame (Get-StableFrame $app.Window) (Join-Path $Context.Root 'layout.png')
}

function Test-ListPainting {
    param($Context, $Case)
    foreach ($index in 0..149) {
        New-TestFile (Join-Path $Context.Fixture ('row-{0:d3}.bin' -f $index)) (4096 + $index) | Out-Null
    }
    $app = Start-TestApp @($Context.Fixture) -Settings @{
        Options = @{ AutomaticallyResizeColumns = 0 }
        FileTreeView = @{
            ColumnOrder = '0,4,3,2,1,9,8,7,6,5,10'; ColumnVisibility = '1,1,1,1,1,0,1,1,1,1,0'
            ColumnWidths = '220,100,110,120,130,80,90,100,140,90,180'
        }
    }
    $list = Get-AppList $app -AllFiles
    Select-AppRows $app $list @('row-050.bin')
    $size = [TestDesktop]::ClientSize($list)
    Assert-True ([TestDesktop]::Message($list, 0x101D, 0).ToInt32() -lt $size[0] / 2) `
        'Several columns are visible during the paint checks'
    [TestDesktop]::ParkCursor($app.Window)
    $baseline = Get-StableFrame $list
    Save-Frame $baseline (Join-Path $Context.Root 'list.png')
    [TestDesktop]::Repaint($list, $true)
    Assert-FrameEqual $baseline (Get-StableFrame $list) 'Partial list painting preserves reordered and hidden columns'

    [void] [TestDesktop]::Message($list, 0x1014, 192)
    $scrolled = Get-StableFrame $list
    Assert-True ([TestDesktop]::Difference($baseline, $scrolled) -gt 0.01) `
        'Horizontal scrolling moves the rendered columns'
    [TestDesktop]::Repaint($list)
    Assert-FrameEqual $scrolled (Get-StableFrame $list) 'A full repaint preserves horizontally scrolled column geometry'
    [void] [TestDesktop]::Message($list, 0x1014, -192)
    Assert-FrameEqual $baseline (Get-StableFrame $list) 'Scrolling back restores the original list painting'

    $width = [TestDesktop]::Message($list, 0x101D, 2).ToInt32()
    Assert-True ([NativeListViewHelper]::SetHeaderWidth($list, 2, $width + 40)) 'Resize a reordered column'
    Get-StableFrame $list | Out-Null
    Assert-True ([NativeListViewHelper]::SetHeaderWidth($list, 2, $width)) 'Restore the reordered column width'
    Assert-FrameEqual $baseline (Get-StableFrame $list) 'Resizing a column round trip refreshes the painted geometry'

    $gdi = [TestDesktop]::GdiObjects($app.Process.Handle)
    $app.Process.Refresh()
    $cpu = $app.Process.TotalProcessorTime
    $watch = [Diagnostics.Stopwatch]::StartNew()
    foreach ($iteration in 1..400) { [TestDesktop]::Repaint($list) }
    $watch.Stop()
    $app.Process.Refresh()
    Write-TestNote ('400 list redraws: {0:n0} ms elapsed; {1:n0} ms application CPU' -f
        $watch.Elapsed.TotalMilliseconds, ($app.Process.TotalProcessorTime - $cpu).TotalMilliseconds)
    Assert-True ([TestDesktop]::GdiObjects($app.Process.Handle) -le $gdi + 4) `
        'Repeated list painting releases temporary GDI resources'
    Assert-FrameEqual $baseline (Get-StableFrame $list) 'Repeated redraws retain row contents and selection'
}

function Test-ToolbarScaling {
    param($Context, $Case)
    $app = Start-TestApp @((New-TestTree)) -Settings @{ Options = @{ DarkMode = $Case.Dark; ToolBarSizePercent = 100 } }
    $toolbar = [TestDesktop]::Children($app.Window) | Where-Object {
        [TestDesktop]::Class($_) -eq 'ToolbarWindow32'
    } | Select-Object -First 1
    Invoke-AppKeys $app '{ESC}'
    Wait-Until { [TestDesktop]::Message($toolbar, 0x0409, (Get-ResourceId 'ID_REFRESH_ALL')).ToInt32() -ne 0 } `
        'Toolbar command states settle after scanning' | Out-Null
    [TestDesktop]::ParkCursor($app.Window)
    $baseline = Get-StableFrame $toolbar
    foreach ($percent in 100, 125, 150, 175, 200, 100) {
        Invoke-AppCommand $app "ID_VIEW_TOOLBAR_SIZE_$percent"
        foreach ($command in 'ID_FILE_SELECT', 'ID_SCAN_STOP', 'ID_CONFIGURE', 'ID_WATCHER_AUTOSCROLL') {
            Assert-True ([TestDesktop]::Message($toolbar, 0x042C, (Get-ResourceId $command)).ToInt32() -ge 0) `
                "$command retains its toolbar image at $percent percent"
        }
        Assert-Equal ([TestDesktop]::Message($toolbar, 0x0409, (Get-ResourceId 'ID_SCAN_STOP')).ToInt32()) 0 `
            'The idle scan action stays disabled after rebuilding toolbar images'
        $frame = Get-StableFrame $toolbar
        Assert-True ([TestDesktop]::ColorCount($frame) -gt 16) 'The toolbar paints its icons at every supported scale'
        if ($percent -eq 200) {
            Assert-True ($frame.Height -gt $baseline.Height) 'Increasing toolbar scale enlarges the buttons'
            Save-Frame $frame (Join-Path $Context.Root 'toolbar-200.png')
        }
    }
    Assert-FrameEqual $baseline $frame 'Restoring toolbar scale restores the original icons' 'toolbar'
    Save-Frame $frame (Join-Path $Context.Root 'toolbar-100.png')
}

function Test-ScaledLocalization {
    param($Context, $Case)
    $root = New-TestTree
    $languageLines = @('IDS_ANALYTICS_THRESHOLD=Seuil : {0}', 'IDS_ANALYTICS_COST=Coût : {0}',
        'IDS_GENERIC_TIME_SUFFIXES=jour-test,mois-test,an-test')
    $app = Start-TestApp @($root) -Settings @{ Options = @{ FontSizePercent = $Case.Font; DarkMode = $Case.Dark } } `
        -LanguageLines $languageLines
    Invoke-AppCommand $app 'ID_TOOLS_STORAGE_ANALYTICS'
    Wait-AppTab $app 'Storage Analytics'
    $labels = @([TestDesktop]::Children($app.Window) | Where-Object {
        [TestDesktop]::Class($_) -eq 'Static' -and [TestDesktop]::IsWindowVisible($_)
    } | ForEach-Object { [TestDesktop]::ControlText($_) })
    Assert-True (@($labels | Where-Object { $_ -like 'Seuil : *' }).Count -ge 3) `
        'Formatted threshold labels use the supplied localized prefix'
    Assert-True (@($labels | Where-Object { $_ -like 'Coût : *mois-test*' }).Count -eq 4) `
        'Cost labels use localized prefixes and time suffixes at the configured font scale'
    $dialog = Open-AppDialog $app 'ID_CONFIGURE'
    Select-SettingsPage $dialog 'General'
    $checkbox = Get-DialogControl $dialog 'IDC_SHOW_TIME_SECONDS'
    $rect = [TestDesktop]::Rectangle($checkbox)
    Assert-True ($rect[2] - $rect[0] -gt 100 -and $rect[3] - $rect[1] -gt 10) `
        'The scaled settings control retains usable geometry'
    Set-DialogCheck $dialog 'IDC_SHOW_TIME_SECONDS' $true
    Assert-Equal ([TestDesktop]::Message($checkbox, 0x00F0).ToInt32()) 1 'A scaled localized checkbox remains operable'
    Save-Frame (Get-StableFrame $checkbox) (Join-Path $Context.Root 'checkbox.png')
    Close-AppDialog $dialog
    Invoke-AppCommand $app 'ID_VIEW_ALL_FILES'
    Assert-GraphRendered $app | Out-Null
}

function Test-FontCacheRoundTrip {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root)
    [TestDesktop]::ParkCursor($app.Window)
    $graph = Get-GraphWindow $app
    $baseline = Assert-GraphRendered $app
    Invoke-AppCommand $app 'ID_VIEW_FONT_SIZE_200'
    Assert-GraphRendered $app | Out-Null
    Invoke-AppCommand $app 'ID_VIEW_FONT_SIZE_100'
    Assert-FrameEqual $baseline (Get-StableFrame $graph) `
        'Changing text scale and restoring it invalidates caches and restores equivalent painting' 'font'
}

function Test-GraphResourceCycles {
    param($Context, $Case)
    $root = New-TestTree
    $app = Start-TestApp @($root)
    $commands = @('ID_VIEW_TREEMAP_ROWS', 'ID_VIEW_TREEMAP_SQUARIFIED', 'ID_VIEW_TREEMAP_HILBERT',
        'ID_VIEW_TREEMAP_MOORE', 'ID_VIEW_FLAMEGRAPH', 'ID_VIEW_SUNBURST')
    foreach ($command in $commands) {
        Invoke-AppCommand $app $command
        Get-StableFrame (Get-GraphWindow $app) | Out-Null
    }
    $baseline = [TestDesktop]::GdiObjects($app.Process.Handle)
    foreach ($cycle in 1..12) {
        foreach ($command in $commands) {
            Invoke-AppCommand $app $command
            Assert-GraphRendered $app | Out-Null
        }
    }
    $after = [TestDesktop]::GdiObjects($app.Process.Handle)
    Assert-True ($after -le $baseline + 24) `
        "Repeated graph cache replacement keeps GDI resources bounded (before=$baseline, after=$after)"
}

function Test-EmptyGraphTransitions {
    param($Context, $Case)
    $root = $Context.Fixture
    $app = Start-TestApp @($root) -Settings @{ TreeMapView = @{ TreeMapUseLogicalSize = 1 } }
    [TestDesktop]::ParkCursor($app.Window)
    $commands = @('ID_VIEW_TREEMAP_ROWS', 'ID_VIEW_TREEMAP_SQUARIFIED', 'ID_VIEW_TREEMAP_HILBERT',
        'ID_VIEW_TREEMAP_MOORE', 'ID_VIEW_FLAMEGRAPH', 'ID_VIEW_SUNBURST')
    $empty = @{}
    foreach ($command in $commands) {
        Invoke-AppCommand $app $command
        $empty[$command] = Get-StableFrame (Get-GraphWindow $app)
    }
    $zero = New-TestFile (Join-Path $root 'zero.bin') 0
    Invoke-AppCommand $app 'ID_REFRESH_ALL'
    Wait-AppIdle $app
    foreach ($command in $commands) {
        Invoke-AppCommand $app $command
        Get-StableFrame (Get-GraphWindow $app) | Out-Null
    }
    Assert-ScanSnapshot (Save-AppReport $app 'zero-only') (Get-DiskSnapshot $root) $root -Physical
    $filled = New-TestFile (Join-Path $root 'filled.bin') 65537
    Invoke-AppCommand $app 'ID_REFRESH_ALL'
    Wait-AppIdle $app
    foreach ($command in $commands) {
        Invoke-AppCommand $app $command
        $frame = Get-StableFrame (Get-GraphWindow $app)
        Assert-True ([TestDesktop]::Difference($empty[$command], $frame) -gt 0.01) `
            "$command renders new data after an empty scan"
    }
    [IO.File]::Delete($filled)
    [IO.File]::Delete($zero)
    Invoke-AppCommand $app 'ID_REFRESH_ALL'
    Wait-AppIdle $app
    foreach ($command in $commands) {
        Invoke-AppCommand $app $command
        Assert-FrameEqual $empty[$command] (Get-StableFrame (Get-GraphWindow $app)) `
            "$command clears stale pixels when a refresh removes every file" $command
    }
    Assert-ScanSnapshot (Save-AppReport $app 'emptied') @() $root -Physical
}

foreach ($graph in @(
    @{ Id = 'rows'; Command = 'ID_VIEW_TREEMAP_ROWS' }, @{ Id = 'squarified'; Command = 'ID_VIEW_TREEMAP_SQUARIFIED' },
    @{ Id = 'hilbert'; Command = 'ID_VIEW_TREEMAP_HILBERT' }, @{ Id = 'moore'; Command = 'ID_VIEW_TREEMAP_MOORE' },
    @{ Id = 'flame'; Command = 'ID_VIEW_FLAMEGRAPH' }, @{ Id = 'sunburst'; Command = 'ID_VIEW_SUNBURST' }
)) {
    foreach ($dark in 0, 1) {
        $theme = if ($dark) { 'dark' } else { 'light' }
        Register-Scenario "render.$($graph.Id).$theme" Rendering `
            'Selection overlays, partial paint, occlusion, restore, and resize' `
            Test-RepaintContract -Tags Desktop,Rendering -Requires Windows,Desktop -Data @{ Command = $graph.Command; Dark = $dark }
    }
}
foreach ($layout in @(@{ Id = 'vertical'; Layout = 0; Position = 0.999 },
    @{ Id = 'horizontal'; Layout = 10; Position = 0.001 })) {
    foreach ($visible in 0, 1) {
        $data = $layout.Clone()
        $data.Visible = $visible
        Register-Scenario "render.pane.$($layout.Id).$visible" Rendering `
            'Collapsed pane recovery and hidden-column notification handling' `
            Test-PaneRecovery -Tags Desktop,Rendering,Persistence -Requires Windows,Desktop -Data $data
    }
    Register-Scenario "render.splitter.$($layout.Id)" Rendering `
        'Splitter grab offsets, capture loss, cancellation, and accepted drags' `
        Test-SplitterCapture -Tags Desktop,Rendering,External -Requires Windows,Desktop -Data $layout
}
foreach ($dark in 0, 1) {
    Register-Scenario "render.layouts.$dark" Rendering 'Layout choices preserve view windows, selection, and rendering' `
        Test-LayoutRoundTrip -Tags Desktop,Rendering -Requires Windows,Desktop -Data @{ Dark = $dark }
    Register-Scenario "render.toolbar.$dark" Rendering 'Icons and disabled buttons survive all supported toolbar scales' `
        Test-ToolbarScaling -Tags Desktop,Rendering -Requires Windows,Desktop -Data @{ Dark = $dark }
}
foreach ($font in 100, 200) {
    Register-Scenario "render.localization.$font" Rendering `
        'Localized formatted labels and controls remain usable at different text scales' `
        Test-ScaledLocalization -Tags Desktop,Rendering,Localization -Requires Windows,Desktop -Data @{ Font = $font; Dark = 1 }
}
Register-Scenario render.font-cache Rendering `
    'Runtime font changes invalidate and rebuild rendering caches consistently' `
    Test-FontCacheRoundTrip -Tags Desktop,Rendering -Requires Windows,Desktop
Register-Scenario render.list-paint Rendering `
    'List painting preserves column order, visibility, widths, and scrolling' `
    Test-ListPainting -Tags Desktop,Rendering -Requires Windows,Desktop
Register-Scenario render.resources Rendering 'Repeated renderer changes release obsolete GDI resources' `
    Test-GraphResourceCycles -Tags Desktop,Rendering,Extended -Requires Windows,Desktop
Register-Scenario render.empty-transitions Rendering 'Empty and zero-byte models render, populate, and clear without stale pixels' `
    Test-EmptyGraphTransitions -Tags Desktop,Rendering,External -Requires Windows,Desktop
