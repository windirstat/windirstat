param(
    [Parameter(Mandatory = $true)][string]$DemoPath,
    [string]$OutputDir = (Join-Path $PSScriptRoot "screenshots"),
    [string]$ExeSource = "V:\Code\windirstat\publish\x64\WinDirStat.exe",
    [string]$AppDir = "V:\Demo\WinDirStatCaptureApp"
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$signature = @"
using System;
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
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }
}
"@

if (-not ("NativeWindowTools" -as [type])) {
    Add-Type -TypeDefinition $signature
}

function Invoke-StoreScreenshotCapture {
    param(
        [Parameter(Mandatory = $true)][int]$LanguageId,
        [Parameter(Mandatory = $true)][ValidateSet(0, 1)][int]$DarkMode,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][string]$DemoPath,
        [Parameter(Mandatory = $true)][string]$ExeSource,
        [Parameter(Mandatory = $true)][string]$AppDir
    )

    New-Item -ItemType Directory -Force -Path $AppDir | Out-Null
    Copy-Item -LiteralPath $ExeSource -Destination (Join-Path $AppDir "WinDirStat.exe") -Force

    $ini = @"
[Options]
LanguageId=$LanguageId
DarkMode=$DarkMode
ShowElevationPrompt=0
AutoElevate=0
UseFastScanEngine=0
UseWindowsLocaleSetting=0
ScanForDuplicates=0
ShowFreeSpace=0
ShowUnknown=0
ShowTimeSpent=1
ShowStatusBar=1
ShowToolBar=1
LargeToolBar=1
TreeMapShowExtensions=1
TreeMapGrid=1
TreeMapUseLogicalSize=1

[FileTreeView]
ShowColumnSizeLogical=1
ShowColumnSizePhysical=1
ShowColumnFiles=1
ShowColumnFolders=1
ShowColumnItems=1
ShowColumnLastChange=1

"@
    Set-Content -LiteralPath (Join-Path $AppDir "WinDirStat.ini") -Value $ini -Encoding UTF8

    $exe = Join-Path $AppDir "WinDirStat.exe"

    $bgForm = $null
    try {
        $bgForm = New-Object System.Windows.Forms.Form
        $bgForm.BackColor = [System.Drawing.Color]::White
        $bgForm.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
        $bgForm.WindowState = [System.Windows.Forms.FormWindowState]::Maximized
        $bgForm.ShowInTaskbar = $false
        $bgForm.Show()
        [System.Windows.Forms.Application]::DoEvents()

        $proc = Start-Process -FilePath $exe -ArgumentList "`"$DemoPath`"" -WorkingDirectory $AppDir -PassThru

        try {
            $hwnd = [IntPtr]::Zero
            $deadline = (Get-Date).AddSeconds(25)
            while ((Get-Date) -lt $deadline) {
                Start-Sleep -Milliseconds 250
                $proc.Refresh()
                if ($proc.HasExited) {
                    throw "WinDirStat exited before a screenshot could be captured."
                }
                if ($proc.MainWindowHandle -ne [IntPtr]::Zero) {
                    $hwnd = $proc.MainWindowHandle
                    break
                }
            }
            if ($hwnd -eq [IntPtr]::Zero) {
                throw "Could not find the WinDirStat main window."
            }

            [NativeWindowTools]::ShowWindow($hwnd, 9) | Out-Null
            [NativeWindowTools]::MoveWindow($hwnd, 40, 40, 1366, 768, $true) | Out-Null
            # HWND_TOPMOST = -1, SWP_NOMOVE | SWP_NOSIZE = 3
            [NativeWindowTools]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 3) | Out-Null
            [NativeWindowTools]::SetForegroundWindow($hwnd) | Out-Null

            Start-Sleep -Seconds 4

            [NativeWindowTools+RECT]$rect = New-Object NativeWindowTools+RECT
            [NativeWindowTools]::GetWindowRect($hwnd, [ref]$rect) | Out-Null

            $width = $rect.Right - $rect.Left
            $height = $rect.Bottom - $rect.Top
            if ($width -le 0 -or $height -le 0) {
                throw "Invalid window bounds: $width x $height"
            }

            New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($OutputPath)) | Out-Null
            $bmp = New-Object System.Drawing.Bitmap($width, $height)
            $gfx = [System.Drawing.Graphics]::FromImage($bmp)
            try {
                $gfx.CopyFromScreen($rect.Left, $rect.Top, 0, 0, [System.Drawing.Size]::new($width, $height))
                $bmp.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
                $gfx.Dispose()
                $bmp.Dispose()
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
        }
    }
    finally {
        if ($bgForm) {
            $bgForm.Close()
            $bgForm.Dispose()
        }
    }
}

# Language codes and their Windows LANGID values (computed via LocaleNameToLCID with LOCALE_ALLOW_NEUTRAL_NAMES)
$languages = @(
    [pscustomobject]@{ Code = "en";    LangId = 9 },
    [pscustomobject]@{ Code = "en-us"; LangId = 9 },
    [pscustomobject]@{ Code = "cs";    LangId = 5 },
    [pscustomobject]@{ Code = "da";    LangId = 6 },
    [pscustomobject]@{ Code = "de";    LangId = 7 },
    [pscustomobject]@{ Code = "es";    LangId = 10 },
    [pscustomobject]@{ Code = "et";    LangId = 37 },
    [pscustomobject]@{ Code = "fi";    LangId = 11 },
    [pscustomobject]@{ Code = "fr";    LangId = 12 },
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
    [pscustomobject]@{ Code = "zh-hk";   LangId = 3076 },
    [pscustomobject]@{ Code = "zh-hans"; LangId = 4 },
    [pscustomobject]@{ Code = "zh-hant"; LangId = 31748 }
)

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$total = $languages.Count * 2
$done = 0

foreach ($lang in $languages) {
    foreach ($darkMode in @(0, 1)) {
        $modeName = if ($darkMode -eq 0) { "light" } else { "dark" }
        $outPath = Join-Path $OutputDir "$($lang.Code)_${modeName}.png"
        $done++
        Write-Host "[$done/$total] Capturing $($lang.Code) ($modeName) -> $outPath"
        Invoke-StoreScreenshotCapture `
            -LanguageId $lang.LangId `
            -DarkMode $darkMode `
            -OutputPath $outPath `
            -DemoPath $DemoPath `
            -ExeSource $ExeSource `
            -AppDir $AppDir
    }
}

Write-Host "Done. $total screenshots saved to: $OutputDir"
