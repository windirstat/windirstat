param(
    [Parameter(Mandatory = $true)][string]$DemoPath,
    [string]$LocalizedDemoRoot,
    [string]$OutputDir = (Join-Path $PSScriptRoot "screenshots"),
    [string]$ExeSource = (Join-Path $PSScriptRoot "..\..\publish\x64\WinDirStat.exe"),
    [string]$Python = "python",
    [string]$AppDir = (Join-Path ([IO.Path]::GetTempPath()) ("WinDirStatCapture-" + [guid]::NewGuid())),
    [ValidateRange(1280, 7680)][int]$Width = 2560,
    [ValidateRange(720, 4320)][int]$Height = 1440,
    [string[]]$Language = @(),
    [ValidateSet("treemap", "largest", "duplicates", "sunburst", "flame")]
    [string[]]$View = @("treemap", "largest", "duplicates", "sunburst", "flame"),
    [ValidateSet(0, 1)][int[]]$Theme = @(0, 1)
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$pngOptimizer = @'
import io, struct, sys, zlib
from pathlib import Path
from PIL import Image

path = Path(sys.argv[1])
original = path.read_bytes()
with Image.open(io.BytesIO(original)) as source:
    pixels = source.convert("RGBA").tobytes()
    if source.mode == "RGBA" and source.getchannel("A").getextrema() == (255, 255):
        source = source.convert("RGB")
    optimized = io.BytesIO()
    source.save(optimized, format="PNG", optimize=True, compress_level=9)

# Recompress the original scanlines as another lossless candidate for gradients.
chunks = []
offset = 8
while offset < len(original):
    length = struct.unpack_from(">I", original, offset)[0]
    chunks.append((original[offset + 4:offset + 8], original[offset:offset + length + 12]))
    offset += length + 12
scanlines = b"".join(chunk[8:-4] for kind, chunk in chunks if kind == b"IDAT")
compressed = zlib.compress(zlib.decompress(scanlines), level=9)
payload = b"IDAT" + compressed
replacement = struct.pack(">I", len(compressed)) + payload + struct.pack(">I", zlib.crc32(payload))
repacked = bytearray(original[:8])
for kind, chunk in chunks:
    repacked.extend(replacement if kind == b"IDAT" else chunk)
    if kind == b"IDAT":
        replacement = b""

result = min((original, optimized.getvalue(), bytes(repacked)), key=len)
with Image.open(io.BytesIO(result)) as image:
    if image.convert("RGBA").tobytes() != pixels:
        raise RuntimeError("PNG optimization changed screenshot pixels.")
if result != original:
    path.write_bytes(result)
'@

$signature = @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class NativeWindowTools
{
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool MoveWindow(IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter,
        int X, int Y, int cx, int cy, uint uFlags);

    [DllImport("user32.dll")]
    public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);

    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int index);

    [DllImport("user32.dll")]
    public static extern bool RedrawWindow(IntPtr hWnd, IntPtr rect, IntPtr region, uint flags);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    private static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam,
        uint flags, uint timeout, out IntPtr result);

    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(IntPtr parent, EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    private static extern int GetClassName(IntPtr hWnd, StringBuilder name, int count);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    private static extern IntPtr GetWindow(IntPtr hWnd, uint command);

    [DllImport("user32.dll")]
    private static extern IntPtr GetMenu(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern bool IsMenu(IntPtr menu);

    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr parameter);

    public static IntPtr FindMainWindow(int processId)
    {
        IntPtr match = IntPtr.Zero;
        EnumWindows((window, parameter) =>
        {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner != processId || GetWindow(window, 4) != IntPtr.Zero || !IsMenu(GetMenu(window)))
                return true;
            match = window;
            return false;
        }, IntPtr.Zero);
        return match;
    }

    public static long Send(IntPtr hWnd, uint message, int wParam, int lParam)
    {
        IntPtr result;
        if (SendMessageTimeout(hWnd, message, (IntPtr)wParam, (IntPtr)lParam, 2, 5000, out result) == IntPtr.Zero)
            throw new InvalidOperationException("WinDirStat did not respond to a capture command.");
        return result.ToInt64();
    }

    public static IntPtr FindChild(IntPtr parent, string className)
    {
        IntPtr match = IntPtr.Zero;
        long largestArea = -1;
        EnumChildWindows(parent, (window, parameter) =>
        {
            StringBuilder name = new StringBuilder(256);
            GetClassName(window, name, name.Capacity);
            RECT rect;
            if (name.ToString() != className || !IsWindowVisible(window) || !GetWindowRect(window, out rect))
                return true;
            long area = (long)(rect.Right - rect.Left) * (rect.Bottom - rect.Top);
            if (area > largestArea)
            {
                match = window;
                largestArea = area;
            }
            return true;
        }, IntPtr.Zero);
        return match;
    }

    public static void FitColumnHeaders(IntPtr parent)
    {
        EnumChildWindows(parent, (window, parameter) =>
        {
            StringBuilder name = new StringBuilder(256);
            GetClassName(window, name, name.Capacity);
            if (name.ToString() != "SysListView32" || !IsWindowVisible(window)) return true;
            IntPtr header = (IntPtr)Send(window, 0x101F, 0, 0);
            int count = (int)Send(header, 0x1200, 0, 0);
            for (int column = 0; column < count; column++)
            {
                int width = (int)Send(window, 0x101D, column, 0);
                if (width <= 0) continue;
                Send(window, 0x101E, column, -2);
                int fitted = (int)Send(window, 0x101D, column, 0);
                Send(window, 0x101E, column, Math.Max(width, fitted));
            }
            return true;
        }, IntPtr.Zero);
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
}
'@

if (-not ("NativeWindowTools" -as [type])) {
    Add-Type -TypeDefinition $signature
}

function New-LocalizedDemoTree {
    param([string]$Code, [hashtable]$Names)

    $localeRoot = [IO.Path]::GetFullPath((Join-Path $LocalizedDemoRoot $Code))
    $rootPrefix = $LocalizedDemoRoot.TrimEnd('\') + '\'
    if (-not $localeRoot.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Localized demo directory is outside the configured root."
    }
    $marker = Join-Path $localeRoot ".windirstat-demo-source"
    if (Test-Path -LiteralPath $localeRoot) {
        if (-not (Test-Path -LiteralPath $marker -PathType Leaf) -or
            [IO.File]::ReadAllText($marker) -ne $DemoPath -or
            ((Get-Item -LiteralPath $localeRoot).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to replace a directory not created for this demo: $localeRoot"
        }
        Remove-Item -LiteralPath $localeRoot -Recurse -Force
    }
    [IO.Directory]::CreateDirectory($localeRoot) | Out-Null
    [IO.File]::WriteAllText($marker, $DemoPath)
    $scanPath = Join-Path $localeRoot $Names.StoreDemoData
    [IO.Directory]::CreateDirectory($scanPath) | Out-Null
    foreach ($item in $demoItems) {
        $parts = [IO.Path]::GetRelativePath($DemoPath, $item.FullName).Split('\')
        $directoryCount = $parts.Count - [int](-not $item.PSIsContainer)
        for ($index = 0; $index -lt $directoryCount; $index++) {
            $parts[$index] = $Names[$parts[$index]]
        }
        $destination = Join-Path $scanPath ($parts -join '\')
        if ($item.PSIsContainer) {
            [IO.Directory]::CreateDirectory($destination) | Out-Null
        } else {
            # Hardlinks preserve sample contents and duplicate groups without copying the data for every locale.
            New-Item -ItemType HardLink -Path $destination -Target $item.FullName | Out-Null
        }
    }
    return $scanPath
}

function Invoke-StoreScreenshotCapture {
    param(
        [Parameter(Mandatory = $true)][string]$ScanPath,
        [Parameter(Mandatory = $true)][int]$LanguageId,
        [Parameter(Mandatory = $true)][int]$DarkMode,
        [Parameter(Mandatory = $true)][string]$Code,
        [Parameter(Mandatory = $true)][string[]]$Views
    )

    $sunburstLayout = $Views -contains "sunburst"
    $topology = if ($sunburstLayout) { 4 } else { 0 }
    $permutation = if ($sunburstLayout) { 3 } else { 0 }
    $mainSplit = if ($sunburstLayout) { "0.50" } else { "0.58" }
    $subSplit = if ($sunburstLayout) { "0.54" } else { "0.72" }
    $graphStyle = if ($sunburstLayout) { 3 } else { 0 }
    $columns = if ($sunburstLayout) { "1,1,0,0,1,0,0,0,0,0,0" } else { "1,1,1,0,1,0,1,0,0,0,0" }

    $ini = @(
        "[Options]", "LanguageId=$LanguageId", "DarkMode=$DarkMode",
        "ShowElevationPrompt=0", "AutoElevate=0", "UseFastScanEngine=0", "UseWindowsLocaleSetting=0",
        "ShowDupeDetectionCloudLinksWarning=0", "ShowFreeSpace=0", "ShowUnknown=0",
        "ShowStatusBar=1", "ShowToolBar=1", "ToolBarSizePercent=100", "FontSizePercent=100",
        "LayoutTopology=$topology", "LayoutPermutation=$permutation",
        "MainSplitterPos=$mainSplit", "SubSplitterPos=$subSplit", "LargeFileCount=50",
        "", "[FileTreeView]", "ColumnVisibility=$columns", "ShowTimeSpent=0",
        "", "[DupeView]", "ScanForDuplicates=1", "ColumnVisibility=1,1,0,1,1",
        "", "[TopView]", "ColumnVisibility=1,0,1,1",
        "", "[ExtView]", "ColumnVisibility=1,1,0,1,1,0",
        "", "[TreeMapView]", "ShowVisualization=1", "GraphPaneStyle=$graphStyle",
        "TreeMapShowExtensions=1", "TreeMapGrid=1", "TreeMapUseLogicalSize=1", ""
    )
    Set-Content -LiteralPath (Join-Path $AppDir "WinDirStat.ini") -Value $ini -Encoding Unicode

    $bgForm = $null
    $proc = $null
    try {
        $bgForm = New-Object System.Windows.Forms.Form
        $bgForm.BackColor = if ($DarkMode) { [Drawing.Color]::FromArgb(32, 32, 32) } else { [Drawing.Color]::White }
        $bgForm.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
        $bgForm.StartPosition = [System.Windows.Forms.FormStartPosition]::Manual
        $bgForm.Bounds = [Drawing.Rectangle]::new($captureX - 8, $captureY - 8, $Width + 16, $Height + 16)
        $bgForm.ShowInTaskbar = $false
        $bgForm.Show()
        [System.Windows.Forms.Application]::DoEvents()

        $start = @{
            FilePath = (Join-Path $AppDir "WinDirStat.exe")
            ArgumentList = ('"{0}"' -f $ScanPath)
            WorkingDirectory = $AppDir
            WindowStyle = "Hidden"
            PassThru = $true
        }
        $proc = Start-Process @start
        $hwnd = [IntPtr]::Zero
        $deadline = (Get-Date).AddSeconds(30)
        while ((Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 200
            $proc.Refresh()
            if ($proc.HasExited) { throw "WinDirStat exited before a screenshot could be captured." }
            $hwnd = [NativeWindowTools]::FindMainWindow($proc.Id)
            if ($hwnd -ne [IntPtr]::Zero) { break }
        }
        if ($hwnd -eq [IntPtr]::Zero) { throw "Could not find the WinDirStat main window." }

        $ready = $false
        $deadline = (Get-Date).AddSeconds(120)
        while ((Get-Date) -lt $deadline) {
            $proc.Refresh()
            if ($proc.HasExited) { throw "WinDirStat exited while scanning the demo data." }
            if (-not [NativeWindowTools]::IsWindowVisible($hwnd)) {
                [NativeWindowTools]::ShowWindow($hwnd, 9) | Out-Null
            }
            # Queued input lets the application refresh command states after its scan worker exits.
            [NativeWindowTools]::PostMessage($hwnd, 0, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
            $toolbar = [NativeWindowTools]::FindChild($hwnd, "ToolbarWindow32")
            # TB_GETSTATE / TBSTATE_ENABLED: Refresh All requires the scan worker to have finished.
            $state = if ($toolbar -ne [IntPtr]::Zero) {
                [NativeWindowTools]::Send($toolbar, 0x412, $commands.ID_REFRESH_ALL, 0)
            } else { -1 }
            if ($state -ge 0 -and ($state -band 4)) {
                $ready = $true
                break
            }
            Start-Sleep -Milliseconds 200
        }
        if (-not $ready) { throw "The demo scan did not finish within 120 seconds." }
        [NativeWindowTools]::ShowWindow($hwnd, 9) | Out-Null
        if (-not [NativeWindowTools]::MoveWindow($hwnd, $captureX, $captureY, $Width, $Height, $true)) {
            throw "Could not resize the WinDirStat window."
        }
        # HWND_TOPMOST = -1, SWP_NOMOVE | SWP_NOSIZE = 3
        [NativeWindowTools]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 3) | Out-Null
        [NativeWindowTools]::SetForegroundWindow($hwnd) | Out-Null

        Start-Sleep -Milliseconds 1000

        foreach ($viewName in $Views) {
            [NativeWindowTools]::Send($hwnd, 0x111, $commands.ID_VIEW_ALL_FILES, 0) | Out-Null
            $viewCommand = switch ($viewName) {
                "largest" { "ID_VIEW_LARGEST_FILES" }
                "duplicates" { "ID_VIEW_DUPLICATE_FILES" }
                "sunburst" { "ID_VIEW_SUNBURST" }
                "flame" { "ID_VIEW_FLAMEGRAPH" }
                default { "ID_VIEW_TREEMAP_ROWS" }
            }
            [NativeWindowTools]::Send($hwnd, 0x111, $commands[$viewCommand], 0) | Out-Null
            $tab = [NativeWindowTools]::FindChild($hwnd, "SysTabControl32")
            $tabIndex = switch ($viewName) { "largest" { 1 } "duplicates" { 2 } default { 0 } }
            if ($tab -eq [IntPtr]::Zero -or [NativeWindowTools]::Send($tab, 0x130B, 0, 0) -ne $tabIndex) {
                throw "WinDirStat did not select the $viewName tab."
            }
            $graphClass = switch ($viewName) {
                "sunburst" { "WinDirStatSunburstClass" }
                "flame" { "WinDirStatFlameGraphClass" }
                default { "WinDirStatTreeMapClass" }
            }
            if ([NativeWindowTools]::FindChild($hwnd, $graphClass) -eq [IntPtr]::Zero) {
                throw "WinDirStat did not display the $viewName graph."
            }
            $list = [NativeWindowTools]::FindChild($tab, "SysListView32")
            if ($list -eq [IntPtr]::Zero -or [NativeWindowTools]::Send($list, 0x1004, 0, 0) -lt 2) {
                throw "The $viewName view has no demo results to capture."
            }
            if ($viewName -eq "duplicates" -or $viewName -eq "treemap") {
                # Expand the first branch with the same navigation used by the application's keyboard UI.
                [NativeWindowTools]::Send($list, 0x100, 0x24, 0) | Out-Null
                [NativeWindowTools]::Send($list, 0x100, 0x27, 0) | Out-Null
                [NativeWindowTools]::Send($list, 0x100, 0x27, 0) | Out-Null
                [NativeWindowTools]::Send($list, 0x100, 0x24, 0) | Out-Null
            }
            [NativeWindowTools]::FitColumnHeaders($hwnd)
            [NativeWindowTools]::RedrawWindow($hwnd, [IntPtr]::Zero, [IntPtr]::Zero, 0x181) | Out-Null
            [NativeWindowTools]::PostMessage($hwnd, 0, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
            Start-Sleep -Milliseconds 900

            [NativeWindowTools+RECT]$rect = New-Object NativeWindowTools+RECT
            if (-not [NativeWindowTools]::GetWindowRect($hwnd, [ref]$rect) -or
                $rect.Right - $rect.Left -ne $Width -or $rect.Bottom - $rect.Top -ne $Height) {
                throw "WinDirStat did not retain the requested $Width x $Height window size."
            }
            $modeName = if ($DarkMode -eq 0) { "light" } else { "dark" }
            $suffix = if ($viewName -eq "treemap") { $modeName } else { $viewName + "_" + $modeName }
            $outPath = Join-Path $OutputDir ($Code + "_" + $suffix + ".png")
            $bmp = New-Object System.Drawing.Bitmap($Width, $Height)
            $gfx = [System.Drawing.Graphics]::FromImage($bmp)
            try {
                $gfx.CopyFromScreen($rect.Left, $rect.Top, 0, 0, [System.Drawing.Size]::new($Width, $Height))
                $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
                $gfx.Dispose()
                $bmp.Dispose()
            }
            & $Python -c $pngOptimizer $outPath
            if ($LASTEXITCODE -ne 0) { throw "Could not losslessly compress the screenshot: $outPath" }
            $script:done++
            Write-Host "[$script:done/$total] Captured $Code ($viewName, $modeName) -> $outPath"
        }
    }
    finally {
        if ($proc -and -not $proc.HasExited) {
            $proc.CloseMainWindow() | Out-Null
            if (-not $proc.WaitForExit(3000)) {
                $proc.Kill()
                $proc.WaitForExit()
            }
        }
        if ($bgForm) {
            $bgForm.Close()
            $bgForm.Dispose()
        }
    }
}

# Store language codes mapped to the LANGID values of available application resources
$languages = @(
    [pscustomobject]@{ Code = "en";    LangId = 9 },
    [pscustomobject]@{ Code = "en-us"; LangId = 9 },
    [pscustomobject]@{ Code = "ar";    LangId = 1 },
    [pscustomobject]@{ Code = "cs";    LangId = 5 },
    [pscustomobject]@{ Code = "da";    LangId = 6 },
    [pscustomobject]@{ Code = "de";    LangId = 7 },
    [pscustomobject]@{ Code = "es";    LangId = 10 },
    [pscustomobject]@{ Code = "et";    LangId = 37 },
    [pscustomobject]@{ Code = "fi";    LangId = 11 },
    [pscustomobject]@{ Code = "fr";    LangId = 12 },
    [pscustomobject]@{ Code = "hi";    LangId = 57 },
    [pscustomobject]@{ Code = "hu";    LangId = 14 },
    [pscustomobject]@{ Code = "it";    LangId = 16 },
    [pscustomobject]@{ Code = "ja";    LangId = 17 },
    [pscustomobject]@{ Code = "ko";    LangId = 18 },
    [pscustomobject]@{ Code = "nb";    LangId = 31764 },
    [pscustomobject]@{ Code = "nl";    LangId = 19 },
    [pscustomobject]@{ Code = "pl";    LangId = 21 },
    [pscustomobject]@{ Code = "pt";    LangId = 22 },
    [pscustomobject]@{ Code = "ru";    LangId = 25 },
    [pscustomobject]@{ Code = "sl";    LangId = 36 },
    [pscustomobject]@{ Code = "sv";    LangId = 29 },
    [pscustomobject]@{ Code = "tr";    LangId = 31 },
    [pscustomobject]@{ Code = "uk";    LangId = 34 },
    [pscustomobject]@{ Code = "zh-hk"; LangId = 3076 },
    [pscustomobject]@{ Code = "zh";    LangId = 30724 },
    [pscustomobject]@{ Code = "zh-cn"; LangId = 30724 }
)

$View = @("treemap", "largest", "duplicates", "sunburst", "flame" | Where-Object { $_ -in $View })
$Theme = @($Theme | Select-Object -Unique)
if ($Language.Count) {
    $unknown = @($Language | Where-Object { $_ -notin $languages.Code })
    if ($unknown.Count) { throw "Unsupported language codes: $($unknown -join ', ')" }
    $languages = @($languages | Where-Object { $_.Code -in $Language })
}
if (-not (Test-Path -LiteralPath $DemoPath -PathType Container) -or
    -not (Test-Path -LiteralPath $ExeSource -PathType Leaf)) {
    throw "Provide an existing demo directory and WinDirStat executable."
}
# Requires Python with Pillow: python -m pip install Pillow
& $Python -c "from PIL import Image"
if ($LASTEXITCODE -ne 0) { throw "Screenshot compression requires Python with Pillow. Use -Python to select it." }
$DemoPath = (Resolve-Path -LiteralPath $DemoPath).Path
$folderNames = Get-Content -LiteralPath (Join-Path $PSScriptRoot "demo-folder-names.json") -Raw -Encoding UTF8 |
    ConvertFrom-Json -AsHashtable
if (-not $LocalizedDemoRoot) { $LocalizedDemoRoot = Join-Path (Split-Path -Parent $DemoPath) "Localized" }
$LocalizedDemoRoot = [IO.Path]::GetFullPath($LocalizedDemoRoot).TrimEnd('\')
if ($LocalizedDemoRoot -eq $DemoPath -or
    $LocalizedDemoRoot.StartsWith($DemoPath.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw "Localized demo trees must be outside the original demo directory."
}
$demoItems = @(Get-ChildItem -LiteralPath $DemoPath -Recurse -Force | Sort-Object { $_.FullName.Length })
if ($demoItems | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) {
    throw "Demo data must contain regular directories and files."
}
$requiredNames = @("StoreDemoData") + @($demoItems | Where-Object PSIsContainer |
    Select-Object -ExpandProperty Name -Unique)
foreach ($lang in $languages) {
    $folderLanguage = switch ($lang.Code) { "en-us" { "en" } "zh-cn" { "zh" } default { $lang.Code } }
    $names = $folderNames[$folderLanguage]
    foreach ($name in $requiredNames) {
        $translated = $names[$name]
        if (-not $translated -or $translated.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0 -or
            $translated.TrimEnd(' ', '.') -ne $translated -or $translated -in @(".", "..")) {
            throw "Missing or invalid $($lang.Code) demo folder name: $name"
        }
    }
}
$resource = Get-Content -LiteralPath (Join-Path $PSScriptRoot "..\..\windirstat\resource.h") -Raw
$commands = @{}
foreach ($name in @("ID_REFRESH_ALL", "ID_VIEW_ALL_FILES", "ID_VIEW_LARGEST_FILES", "ID_VIEW_DUPLICATE_FILES",
    "ID_VIEW_TREEMAP_ROWS", "ID_VIEW_SUNBURST", "ID_VIEW_FLAMEGRAPH")) {
    $match = [regex]::Match($resource, "(?m)^#define\s+$name\s+(\d+)")
    if (-not $match.Success) { throw "Missing capture command: $name" }
    $commands[$name] = [int]$match.Groups[1].Value
}

$previousDpiContext = [NativeWindowTools]::SetThreadDpiAwarenessContext([IntPtr](-4))
try {
    $screenWidth = [NativeWindowTools]::GetSystemMetrics(0)
    $screenHeight = [NativeWindowTools]::GetSystemMetrics(1)
    if ($Width + 16 -gt $screenWidth -or $Height + 16 -gt $screenHeight) {
        throw "Capture size $Width x $Height exceeds the primary display. Use -Width and -Height to fit it."
    }
    $captureX = [int](($screenWidth - $Width) / 2)
    $captureY = [int](($screenHeight - $Height) / 2)
    New-Item -ItemType Directory -Force -Path $OutputDir, $AppDir | Out-Null
    $OutputDir = (Resolve-Path -LiteralPath $OutputDir).Path
    $AppDir = (Resolve-Path -LiteralPath $AppDir).Path
    Copy-Item -LiteralPath $ExeSource -Destination (Join-Path $AppDir "WinDirStat.exe") -Force
    $total = $languages.Count * $Theme.Count * $View.Count
    $script:done = 0
    Write-Host "Capturing $total screenshots at $Width x $Height physical pixels."
    foreach ($lang in $languages) {
        $folderLanguage = switch ($lang.Code) { "en-us" { "en" } "zh-cn" { "zh" } default { $lang.Code } }
        $scanPath = New-LocalizedDemoTree -Code $lang.Code -Names $folderNames[$folderLanguage]
        Write-Host "Localized demo for $($lang.Code): $scanPath"
        foreach ($darkMode in $Theme) {
            $standardViews = @($View | Where-Object { $_ -ne "sunburst" })
            if ($standardViews.Count) {
                Invoke-StoreScreenshotCapture -LanguageId $lang.LangId -DarkMode $darkMode `
                    -Code $lang.Code -Views $standardViews -ScanPath $scanPath
            }
            if ($View -contains "sunburst") {
                Invoke-StoreScreenshotCapture -LanguageId $lang.LangId -DarkMode $darkMode `
                    -Code $lang.Code -Views @("sunburst") -ScanPath $scanPath
            }
        }
    }
    Write-Host "Done. $script:done screenshots saved to: $OutputDir"
}
finally {
    [NativeWindowTools]::SetThreadDpiAwarenessContext($previousDpiContext) | Out-Null
}
