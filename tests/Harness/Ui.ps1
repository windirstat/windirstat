function Get-ResourceId {
    param([string] $Name)
    Get-RequiredMapValue -Map $script:ResourceIds -Name $Name -Source $ResourceHeaderPath
}

function Start-TestApp {
    param(
        [string[]] $Roots = @(), [object] $Runner, [hashtable] $Settings = @{},
        [string[]] $Arguments = @(), [switch] $DuringScan, [string[]] $LanguageLines = @(),
        [int] $Width = 1100, [int] $Height = 760
    )
    if (-not $Runner) { $Runner = New-TestRunner $Settings $LanguageLines }
    $process = Start-TestProcess $Runner (@($Arguments) + $Roots)
    $app = [pscustomobject]@{
        Process = $process; Runner = $Runner; Window = [IntPtr]::Zero; Roots = $Roots
        ExpectExit = $false; TreeList = [IntPtr]::Zero; ExtensionList = [IntPtr]::Zero
    }
    $script:Context.Apps.Add($app)
    $app.Window = Wait-Until {
        @([TestDesktop]::Windows($process.Id) | Where-Object {
            [TestDesktop]::IsWindowVisible($_) -and [TestDesktop]::Class($_) -ne '#32770' -and
            [TestDesktop]::Text($_) -like '*WinDirStat*'
        } | Select-Object -First 1)
    } 'The process creates its main window'
    [void] [TestDesktop]::ShowWindow($app.Window, 9)
    $screen = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    [TestDesktop]::Position($app.Window, $screen.Left + 20, $screen.Top + 20,
        [Math]::Min($Width, $screen.Width - 40), [Math]::Min($Height, $screen.Height - 40))
    [TestDesktop]::KeepVisible($app.Window)
    [TestDesktop]::Activate($app.Window)
    if (-not $DuringScan -and ($Roots.Count -or $Arguments.Count)) { Wait-AppIdle $app }
    return $app
}

function Stop-TestApp {
    param([object] $App, [switch] $Force)
    $App.ExpectExit = $true
    if ($App.Process.HasExited) { return }
    if (-not $Force) {
        [void] [TestDesktop]::PostMessage($App.Window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
        $watch = [Diagnostics.Stopwatch]::StartNew()
        while (-not $App.Process.WaitForExit(100) -and $watch.Elapsed.TotalSeconds -lt 8) {}
        if ($App.Process.HasExited) {
            if ($App.Process.ExitCode -ne 0) { throw "Application shutdown failed: exit=$($App.Process.ExitCode)" }
            return
        }
    }
    $App.Process.Kill($true)
    [void] $App.Process.WaitForExit(5000)
    if (-not $Force) { throw 'Application did not shut down within 8 seconds.' }
}

function Find-MenuCommand {
    param([IntPtr] $Menu, [int] $Id)
    for ($index = 0; $index -lt [TestDesktop]::GetMenuItemCount($Menu); ++$index) {
        if ([TestDesktop]::GetMenuItemID($Menu, $index) -eq $Id) {
            return [pscustomobject]@{ Menu = $Menu; Position = $index }
        }
        $submenu = [TestDesktop]::GetSubMenu($Menu, $index)
        if ($submenu -eq [IntPtr]::Zero) { continue }
        $item = Find-MenuCommand $submenu $Id
        if ($item) { return $item }
    }
    return $null
}

function Get-AppCommandState {
    param([object] $App, [string] $Symbol)
    $id = Get-ResourceId $Symbol
    $item = Find-MenuCommand ([TestDesktop]::GetMenu($App.Window)) $id
    if (-not $item) {
        foreach ($toolbar in [TestDesktop]::Children($App.Window)) {
            if ([TestDesktop]::Class($toolbar) -ne 'ToolbarWindow32' -or
                [TestDesktop]::Message($toolbar, 0x0419, $id).ToInt32() -lt 0) { continue }
            return [pscustomobject]@{
                Enabled = [TestDesktop]::Message($toolbar, 0x0409, $id).ToInt32() -ne 0
                Checked = [TestDesktop]::Message($toolbar, 0x040A, $id).ToInt32() -ne 0
            }
        }
        throw "Command is missing from the application menu and toolbar: $Symbol"
    }
    [void] [TestDesktop]::Message($App.Window, 0x0117, $item.Menu.ToInt64(), 0)
    $state = [TestDesktop]::GetMenuState($item.Menu, $id, 0)
    return [pscustomobject]@{ Enabled = ($state -band 3) -eq 0; Checked = ($state -band 8) -ne 0 }
}

function Invoke-AppCommand {
    param([object] $App, [string] $Symbol, [switch] $Unchecked)
    $id = Get-ResourceId $Symbol
    $tabs = @('ID_VIEW_ALL_FILES', 'ID_VIEW_LARGEST_FILES', 'ID_VIEW_DUPLICATE_FILES', 'ID_VIEW_SEARCH_RESULTS')
    if (-not $Unchecked -and $Symbol -notin $tabs -and -not (Get-AppCommandState $App $Symbol).Enabled) {
        throw "Cannot invoke a disabled command: $Symbol"
    }
    $immediate = $Symbol -like 'ID_VIEW_*' -or $Symbol -eq 'ID_EDIT_COPY_CLIPBOARD'
    if ($immediate) { [void] [TestDesktop]::Message($App.Window, 0x0111, $id) }
    elseif (-not [TestDesktop]::PostMessage($App.Window, 0x0111, [IntPtr]$id, [IntPtr]::Zero)) {
        throw "Could not post command: $Symbol"
    }
    $tabName = switch ($Symbol) {
        ID_VIEW_ALL_FILES { 'All Files' }
        ID_VIEW_LARGEST_FILES { 'Largest Files' }
        ID_VIEW_DUPLICATE_FILES { 'Duplicate Files' }
        ID_VIEW_SEARCH_RESULTS { 'Search Results' }
    }
    if ($tabName) { Wait-AppTab $App $tabName }
}

function Wait-AppIdle {
    param([object] $App, [int] $Seconds = $TimeoutSeconds)
    Wait-Until {
        [TestDesktop]::IsWindowEnabled($App.Window) -and
        [TestDesktop]::Text($App.Window) -notmatch '(?:^| - )(Scanning|\d+([.,]\d+)?%)(?: |$)|\(Suspended\)'
    } 'Scan and modal operations finish' -Seconds $Seconds -StableMilliseconds 450 | Out-Null
}

function Wait-AppScanning {
    param([object] $App)
    Wait-Until {
        [TestDesktop]::Text($App.Window) -match '(?:^| - )(Scanning|\d+([.,]\d+)?%)(?: |$)' -and
        (Get-AppCommandState $App 'ID_SCAN_SUSPEND').Enabled
    } 'The scan reaches an observable active phase' | Out-Null
}

function Get-AppDialog {
    param([object] $App, [string] $Control)
    $id = if ($Control) { Get-ResourceId $Control } else { 0 }
    return @([TestDesktop]::Windows($App.Process.Id) | Where-Object {
        [TestDesktop]::Class($_) -eq '#32770' -and [TestDesktop]::IsWindowVisible($_) -and
        (-not $id -or [TestDesktop]::GetDlgItem($_, $id) -ne [IntPtr]::Zero)
    } | Select-Object -First 1)
}

function Open-AppDialog {
    param([object] $App, [string] $Command, [string] $Control)
    Invoke-AppCommand $App $Command
    return Wait-Until { Get-AppDialog $App $Control } "Open $Command dialog" -Seconds 20
}

function Close-AppDialog {
    param([IntPtr] $Dialog, [int] $Button = 2, [string] $ButtonName)
    $buttonWindow = if ($ButtonName) {
        @([TestDesktop]::Children($Dialog) | Where-Object {
            [TestDesktop]::Class($_) -eq 'Button' -and [TestDesktop]::IsWindowVisible($_) -and
            [TestDesktop]::ControlText($_).Replace('&', '').Trim() -eq $ButtonName
        } | Select-Object -First 1)
    } else { [TestDesktop]::GetDlgItem($Dialog, $Button) }
    if (-not $buttonWindow) { $buttonWindow = [IntPtr]::Zero }
    if ($buttonWindow -eq [IntPtr]::Zero) { throw "Dialog does not expose button $Button" }
    [void] [TestDesktop]::PostMessage($buttonWindow, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
    Wait-Until { -not [TestDesktop]::IsWindow($Dialog) } 'The dialog closes after the requested action' -Seconds 20 |
        Out-Null
}

function Wait-AppTab {
    param([object] $App, [string] $Name)
    Wait-Until {
        foreach ($tab in [TestDesktop]::Children($App.Window)) {
            if ([TestDesktop]::Class($tab) -ne 'SysTabControl32' -or
                -not @([TestDesktop]::Children($tab) |
                    Where-Object { [TestDesktop]::Class($_) -eq 'SysListView32' }).Count) {
                continue
            }
            $names = [NativeListViewHelper]::GetTabTexts($tab)
            $index = [TestDesktop]::Message($tab, 0x130B).ToInt32()
            if ($index -ge 0 -and $index -lt $names.Count -and $names[$index].Trim() -eq $Name) { return $true }
        }
    } "The '$Name' results tab becomes active" -StableMilliseconds 150 | Out-Null
}

function Get-AppList {
    param([object] $App, [switch] $AllFiles, [string] $TabName)
    if ($TabName) { Wait-AppTab $App $TabName }
    if ($AllFiles) {
        Invoke-AppCommand $App 'ID_VIEW_ALL_FILES'
        Wait-Until {
            $rootNames = @($App.Roots | ForEach-Object { [IO.Path]::GetFileName($_.TrimEnd('\')) })
            foreach ($list in [NativeListViewHelper]::GetVisibleListViews($App.Window)) {
                $texts = [NativeListViewHelper]::GetItemTexts($list)
                if (@($texts | Where-Object { $_ -in $rootNames -or $_ -in $App.Roots }).Count) { return $list }
            }
        } 'The All Files list exposes its scanned root' | ForEach-Object { $App.TreeList = [IntPtr]$_ }
        $extension = [NativeListViewHelper]::GetVisibleListViewsIncludingEmpty($App.Window) |
            Where-Object { $_ -ne $App.TreeList } | Select-Object -First 1
        $App.ExtensionList = if ($null -eq $extension) { [IntPtr]::Zero } else { [IntPtr]$extension }
        return $App.TreeList
    }
    return Wait-Until {
        $lists = @([NativeListViewHelper]::GetVisibleListViewsIncludingEmpty($App.Window) |
            Where-Object { $_ -ne $App.ExtensionList })
        if ($lists.Count -eq 1) { return $lists[0] }
    } 'Exactly one results list is active'
}

function Find-AppRow {
    param([IntPtr] $List, [string] $Name, [switch] $Optional)
    $texts = [NativeListViewHelper]::GetItemTexts($List)
    $matches = @(for ($index = 0; $index -lt $texts.Length; ++$index) {
        if ($texts[$index] -ieq $Name -or
            ($Name.Contains('\') -and $texts[$index] -ieq [IO.Path]::GetFileName($Name.TrimEnd('\')))) { $index }
    })
    if ($matches.Count -eq 1) { return [int]$matches[0] }
    if ($Optional -and -not $matches.Count) { return -1 }
    throw "Expected one row '$Name', found $($matches.Count). Rows: $($texts -join '; ')"
}

function Select-AppRows {
    param([object] $App, [IntPtr] $List, [string[]] $Names)
    $indices = @(foreach ($name in $Names) { Find-AppRow $List $name })
    [TestDesktop]::Activate($App.Window)
    if (-not [NativeListViewHelper]::FocusListView($List) -or
        -not [NativeListViewHelper]::SelectItems($List, [int[]]$indices)) {
        throw "Could not select the requested rows: $($Names -join '; ')"
    }
    Wait-Until {
        [NativeListViewHelper]::GetSelectedCount($List) -eq $Names.Count -and
        [TestDesktop]::Focus($App.Window) -eq $List
    } 'Selection and keyboard focus reach the requested list' -StableMilliseconds 150 | Out-Null
}

function Expand-AppDirectory {
    param([object] $App, [IntPtr] $List, [string] $Name, [string] $Child)
    if ($Child -and (Find-AppRow $List $Child -Optional) -ge 0) { return }
    Select-AppRows $App $List @($Name)
    [void] [NativeListViewHelper]::PostRight($List)
    Wait-Until { (Find-AppRow $List $Child -Optional) -ge 0 } "Expanding '$Name' reveals '$Child'" |
        Out-Null
}

function Expand-AppGroups {
    param([object] $App, [IntPtr] $List, [string[]] $Paths)
    $expanded = [System.Collections.Generic.HashSet[string]]::new()
    Wait-Until {
        $texts = [NativeListViewHelper]::GetItemTexts($List)
        if (@($Paths | Where-Object { $_ -notin $texts }).Count -eq 0) { return $true }
        foreach ($name in $texts) {
            if ($name -in $Paths -or -not $expanded.Add($name)) { continue }
            Select-AppRows $App $List @($name)
            [void] [NativeListViewHelper]::PostRight($List)
            return $false
        }
        return $false
    } 'Expanding duplicate groups exposes each requested file path' | Out-Null
}

function Invoke-AppKeys {
    param([object] $App, [string] $Keys, [IntPtr] $Window = $App.Window)
    [TestDesktop]::Activate($Window)
    Wait-Until { [TestDesktop]::GetForegroundWindow() -eq $Window } 'The target window owns foreground keyboard input' |
        Out-Null
    [System.Windows.Forms.SendKeys]::SendWait($Keys)
}

function Select-SettingsPage {
    param([IntPtr] $Dialog, [string] $Name)
    $tab = [TestDesktop]::GetDlgItem($Dialog, 0xCAFE)
    $names = [NativeListViewHelper]::GetTabTexts($tab)
    $index = [Array]::FindIndex($names, [Predicate[string]]{ param($Label); $Label.Trim() -eq $Name })
    if ($index -lt 0) { throw "Settings tab '$Name' is missing. Tabs: $($names -join '; ')" }
    $rect = [NativeListViewHelper]::GetTabRectangle($tab, $index)
    $point = (([TestDesktop]::ClientSize($tab)[1] / 2 -as [int]) -shl 16) -bor
        (($rect[0] + $rect[2]) / 2 -as [int])
    [void] [TestDesktop]::PostMessage($tab, 0x0201, [IntPtr]1, [IntPtr]$point)
    [void] [TestDesktop]::PostMessage($tab, 0x0202, [IntPtr]::Zero, [IntPtr]$point)
    Wait-Until { [TestDesktop]::Message($tab, 0x130B).ToInt32() -eq $index } "Settings tab '$Name' becomes active" |
        Out-Null
}

function Get-DialogControl {
    param([IntPtr] $Dialog, [string] $Symbol)
    $id = Get-ResourceId $Symbol
    return Wait-Until {
        $controls = @([TestDesktop]::Children($Dialog) | Where-Object {
            [TestDesktop]::GetDlgCtrlID($_) -eq $id -and [TestDesktop]::IsWindowVisible($_)
        })
        if ($controls.Count -gt 1) { throw "Expected one visible control $Symbol, found $($controls.Count)" }
        if ($controls.Count -eq 1) { return $controls[0] }
    } "Control $Symbol becomes visible"
}

function Set-DialogCheck {
    param([IntPtr] $Dialog, [string] $Symbol, [bool] $Checked)
    $control = Get-DialogControl $Dialog $Symbol
    if ([TestDesktop]::Message($control, 0x00F0).ToInt32() -eq [int]$Checked) { return }
    [void] [TestDesktop]::Message($control, 0x00F5)
}

function Save-AppReport {
    param([object] $App, [string] $Name = 'ui-export', [string] $Command = 'ID_SAVE_RESULTS', [string] $Format = 'csv')
    $path = Join-Path $script:Context.Root "$Name.$Format"
    if (Test-Path -LiteralPath $path) { throw "Report already exists: $path" }
    $dialog = Open-AppDialog $App $Command
    $edit = Wait-Until {
        @([TestDesktop]::Children($dialog) | Where-Object {
            [TestDesktop]::Class($_) -eq 'Edit' -and [TestDesktop]::IsWindowVisible($_) -and
            [TestDesktop]::GetDlgCtrlID($_) -in @(1001, 1152)
        } | Select-Object -First 1)
    } 'The file picker exposes its native filename field'
    [TestDesktop]::SetText($edit, $path)
    Wait-Until { [TestDesktop]::ControlText($edit) -eq $path } `
        'The file picker accepts the exact output path' | Out-Null
    Close-AppDialog $dialog 1
    Wait-AppIdle $App
    Wait-Until { Test-Path -LiteralPath $path -PathType Leaf } 'The UI export writes its report' | Out-Null
    return Read-ScanReport $path
}

function Get-GraphWindow {
    param([object] $App)
    return Wait-Until {
        @([TestDesktop]::Children($App.Window) | Where-Object {
            [TestDesktop]::IsWindowVisible($_) -and
            [TestDesktop]::Class($_) -in
                @('WinDirStatTreeMapClass', 'WinDirStatFlameGraphClass', 'WinDirStatSunburstClass')
        } | Select-Object -First 1)
    } 'A visualization pane is visible'
}

function Save-Frame {
    param([TestFrame] $Frame, [string] $Path)
    $bitmap = [Drawing.Bitmap]::new($Frame.Width, $Frame.Height, [Drawing.Imaging.PixelFormat]::Format32bppRgb)
    try {
        $data = $bitmap.LockBits([Drawing.Rectangle]::new(0, 0, $Frame.Width, $Frame.Height),
            [Drawing.Imaging.ImageLockMode]::WriteOnly, [Drawing.Imaging.PixelFormat]::Format32bppRgb)
        try { [Runtime.InteropServices.Marshal]::Copy($Frame.Pixels, 0, $data.Scan0, $Frame.Pixels.Length) }
        finally { $bitmap.UnlockBits($data) }
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }
}

function Get-StableFrame {
    param([IntPtr] $Window, [string] $Name = 'render', [switch] $AllowOcclusion)
    $state = @{ Previous = $null; Latest = $null }
    Wait-Until {
        if (-not $AllowOcclusion) { [TestDesktop]::Expose($Window) }
        $state.Latest = [TestDesktop]::Capture($Window)
        $stable = $state.Previous -and [TestDesktop]::Difference($state.Previous, $state.Latest) -eq 0
        $state.Previous = $state.Latest
        return $stable
    } "$Name pixels become stable" -StableMilliseconds 300 | Out-Null
    return $state.Latest
}

function Assert-FrameEqual {
    param([TestFrame] $Expected, [TestFrame] $Actual, [string] $Because, [string] $Name = 'repaint')
    $fraction = [TestDesktop]::Difference($Expected, $Actual)
    if ($fraction -gt 0.001) {
        Save-Frame $Expected (Join-Path $script:Context.Root "$Name-expected.png")
        Save-Frame $Actual (Join-Path $script:Context.Root "$Name-actual.png")
    }
    Assert-True ($fraction -le 0.001) "$Because (changed pixels=$($fraction.ToString('P3')))"
}

function Assert-GraphRendered {
    param([object] $App)
    $frame = Get-StableFrame (Get-GraphWindow $App)
    Assert-True ([TestDesktop]::ColorCount($frame) -gt 24) 'The graph contains painted data rather than a blank surface'
    return $frame
}
