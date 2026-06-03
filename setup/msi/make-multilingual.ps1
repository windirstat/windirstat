# Embeds language transforms into a WinDirStat MSI, producing a single
# multilingual installer that displays the system locale language on launch.
#
# Called by build.cmd after the base (en-US) MSI has been built.
param(
    [Parameter(Mandatory)][string]$MsiPath,
    [Parameter(Mandatory)][string]$Arch,
    [Parameter(Mandatory)][string]$RelType,
    [Parameter(Mandatory)][string]$MajVer,
    [Parameter(Mandatory)][string]$MinVer,
    [Parameter(Mandatory)][string]$Patch,
    [Parameter(Mandatory)][string]$Build,
    [Parameter(Mandatory)][string]$EstimatedSize,
    [Parameter(Mandatory)][string]$ProductCode
)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

# ---------------------------------------------------------------------------
# IStorage COM interop - all logic in C# so PowerShell never touches the
# raw COM interface (avoids __ComObject late-binding issues).
# ---------------------------------------------------------------------------
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

[ComImport, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
 Guid("0000000b-0000-0000-C000-000000000046")]
public interface IStorage {
    void CreateStream(string n, uint m, uint r1, uint r2, out IStream s);
    void OpenStream(string n, IntPtr r1, uint m, uint r2, out IStream s);
    void CreateStorage(string n, uint m, uint r1, uint r2, out IStorage s);
    void OpenStorage(string n, IStorage p, uint m, IntPtr sn, uint r, out IStorage s);
    void CopyTo(uint c, IntPtr g, IntPtr sn, IStorage d);
    void MoveElementTo(string n, IStorage d, string nn, uint f);
    void Commit(uint f);
    void Revert();
    void EnumElements(uint r1, IntPtr r2, uint r3,
                      [MarshalAs(UnmanagedType.IUnknown)] out object e);
    void DestroyElement(string n);
    void RenameElement(string o, string n);
    void SetElementTimes(string n, IntPtr c, IntPtr a, IntPtr m);
    void SetClass(ref Guid c);
    void SetStateBits(uint b, uint m);
    void Stat(out System.Runtime.InteropServices.ComTypes.STATSTG s, uint f);
}

public static class MsiTransformHelper {
    // IStorage API
    [DllImport("ole32.dll", CharSet = CharSet.Unicode)]
    private static extern int StgOpenStorage(
        string name, IStorage priority, uint mode,
        IntPtr snb, uint reserved, out IStorage storage);

    const uint STGM_READ            = 0x00000000;
    const uint STGM_READWRITE       = 0x00000002;
    const uint STGM_SHARE_EXCLUSIVE = 0x00000010;
    const uint STGM_SHARE_DENY_WR   = 0x00000020;
    const uint STGM_TRANSACTED      = 0x00010000;
    const uint STGM_CREATE          = 0x00001000;

    // MSI Summary Information API
    [DllImport("msi.dll", CharSet = CharSet.Unicode)]
    private static extern uint MsiGetSummaryInformationW(
        uint hDatabase, string szDatabasePath, uint uiUpdateCount,
        out uint phSummaryInfo);

    [DllImport("msi.dll", CharSet = CharSet.Unicode)]
    private static extern uint MsiSummaryInfoGetPropertyW(
        uint hSi, uint uiProp, out uint uiDataType, out int iVal,
        IntPtr pftVal, System.Text.StringBuilder szVal, ref uint cchVal);

    [DllImport("msi.dll", CharSet = CharSet.Unicode)]
    private static extern uint MsiSummaryInfoSetPropertyW(
        uint hSi, uint uiProp, uint uiDataType, int iVal,
        IntPtr pftVal, string szVal);

    [DllImport("msi.dll")]
    private static extern uint MsiSummaryInfoPersist(uint hSi);

    [DllImport("msi.dll")]
    private static extern uint MsiCloseHandle(uint hAny);

    // Embed an MST file as a named substorage inside the MSI
    public static void EmbedTransform(string msiPath, string mstPath, int lcid) {
        IStorage msiStorage;
        int hr = StgOpenStorage(msiPath, null,
            STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED,
            IntPtr.Zero, 0, out msiStorage);
        if (hr != 0) throw Marshal.GetExceptionForHR(hr);

        IStorage mstStorage;
        hr = StgOpenStorage(mstPath, null,
            STGM_READ | STGM_SHARE_DENY_WR,
            IntPtr.Zero, 0, out mstStorage);
        if (hr != 0) {
            Marshal.ReleaseComObject(msiStorage);
            throw Marshal.GetExceptionForHR(hr);
        }

        try {
            IStorage sub;
            msiStorage.CreateStorage(lcid.ToString(),
                STGM_READWRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE,
                0, 0, out sub);
            mstStorage.CopyTo(0, IntPtr.Zero, IntPtr.Zero, sub);
            sub.Commit(0);
            Marshal.ReleaseComObject(sub);
        }
        finally {
            Marshal.ReleaseComObject(mstStorage);
            msiStorage.Commit(0);
            Marshal.ReleaseComObject(msiStorage);
        }
    }

    // Update the PID_TEMPLATE (7) Summary Information property to list all LCIDs
    public static void UpdateTemplate(string msiPath, int[] lcids) {
        uint hSi;
        uint err = MsiGetSummaryInformationW(0, msiPath, 1, out hSi);
        if (err != 0) throw new Exception("MsiGetSummaryInformation failed: " + err);
        try {
            uint dataType; int intVal;
            var buf = new System.Text.StringBuilder(512);
            uint bufLen = (uint)buf.Capacity;
            err = MsiSummaryInfoGetPropertyW(hSi, 7, out dataType, out intVal,
                                             IntPtr.Zero, buf, ref bufLen);
            if (err == 234) {
                buf = new System.Text.StringBuilder((int)bufLen + 1);
                bufLen = (uint)buf.Capacity;
                err = MsiSummaryInfoGetPropertyW(hSi, 7, out dataType, out intVal,
                                                 IntPtr.Zero, buf, ref bufLen);
            }
            if (err != 0) throw new Exception("MsiSummaryInfoGetProperty failed: " + err);

            string current = buf.ToString();
            string platform = current.IndexOf(';') >= 0 ? current.Split(';')[0] : current;
            string newTpl = platform + ";" +
                string.Join(",", Array.ConvertAll(lcids, l => l.ToString()));

            err = MsiSummaryInfoSetPropertyW(hSi, 7, 30 /*VT_LPSTR*/, 0,
                                             IntPtr.Zero, newTpl);
            if (err != 0) throw new Exception("MsiSummaryInfoSetProperty failed: " + err);

            err = MsiSummaryInfoPersist(hSi);
            if (err != 0) throw new Exception("MsiSummaryInfoPersist failed: " + err);
        }
        finally {
            MsiCloseHandle(hSi);
        }
    }
}
'@

# ---------------------------------------------------------------------------
# Embed one .mst as a named substorage inside the MSI
# ---------------------------------------------------------------------------
function Add-LanguageTransform {
    param([string]$MsiPath, [string]$MstPath, [int]$Lcid)
    [MsiTransformHelper]::EmbedTransform($MsiPath, $MstPath, $Lcid)
}

# ---------------------------------------------------------------------------
# Update the Template Summary Information to advertise all languages
# ---------------------------------------------------------------------------
function Update-TemplateLangs {
    param([string]$MsiPath, [int[]]$Lcids)
    [MsiTransformHelper]::UpdateTemplate($MsiPath, $Lcids)
}

# ---------------------------------------------------------------------------
# Language table  (project code -> WiX culture, MSI LCID)
# ---------------------------------------------------------------------------
$languages = @(
    [pscustomobject]@{ Code = 'cs';    Culture = 'cs-CZ'; Lcid = 1029 }
    [pscustomobject]@{ Code = 'da';    Culture = 'da-DK'; Lcid = 1030 }
    [pscustomobject]@{ Code = 'de';    Culture = 'de-DE'; Lcid = 1031 }
    [pscustomobject]@{ Code = 'es';    Culture = 'es-ES'; Lcid = 3082 }
    [pscustomobject]@{ Code = 'et';    Culture = 'et-EE'; Lcid = 1061 }
    [pscustomobject]@{ Code = 'fi';    Culture = 'fi-FI'; Lcid = 1035 }
    [pscustomobject]@{ Code = 'fr';    Culture = 'fr-FR'; Lcid = 1036 }
    [pscustomobject]@{ Code = 'hu';    Culture = 'hu-HU'; Lcid = 1038 }
    [pscustomobject]@{ Code = 'it';    Culture = 'it-IT'; Lcid = 1040 }
    [pscustomobject]@{ Code = 'ja';    Culture = 'ja-JP'; Lcid = 1041 }
    [pscustomobject]@{ Code = 'ko';    Culture = 'ko-KR'; Lcid = 1042 }
    [pscustomobject]@{ Code = 'nb';    Culture = 'nb-NO'; Lcid = 1044 }
    [pscustomobject]@{ Code = 'nl';    Culture = 'nl-NL'; Lcid = 1043 }
    [pscustomobject]@{ Code = 'pl';    Culture = 'pl-PL'; Lcid = 1045 }
    [pscustomobject]@{ Code = 'pt';    Culture = 'pt-PT'; Lcid = 2070 }
    [pscustomobject]@{ Code = 'ru';    Culture = 'ru-RU'; Lcid = 1049 }
    [pscustomobject]@{ Code = 'sl';    Culture = 'sl-SI'; Lcid = 1060 }
    [pscustomobject]@{ Code = 'sv';    Culture = 'sv-SE'; Lcid = 1053 }
    [pscustomobject]@{ Code = 'tr';    Culture = 'tr-TR'; Lcid = 1055 }
    [pscustomobject]@{ Code = 'uk';    Culture = 'uk-UA'; Lcid = 1058 }
    [pscustomobject]@{ Code = 'zh-hk'; Culture = 'zh-HK'; Lcid = 3076 }
    [pscustomobject]@{ Code = 'zh';    Culture = 'zh-CN'; Lcid = 2052 }
)

# Version/release args shared by every wix build invocation
$verArgs = @(
    '-d', "RELTYPE=$RelType",
    '-d', "MAJVER=$MajVer",
    '-d', "MINVER=$MinVer",
    '-d', "PATCH=$Patch",
    '-d', "BUILD=$Build",
    '-d', "EstimatedSize=$EstimatedSize",
    '-d', "ProductCode=$ProductCode"
)

# Resolve to absolute path so P/Invoke calls always get a full path
$MsiPath = (Resolve-Path $MsiPath).Path

$tmpDir = Join-Path $env:TEMP 'WinDirStat_transforms'
New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null

$embeddedLcids = [System.Collections.Generic.List[int]]::new()
$embeddedLcids.Add(1033)   # base English is always present

$failed = [System.Collections.Generic.List[string]]::new()

foreach ($lang in $languages) {
    $wxl     = "WinDirStat_$($lang.Code).wxl"
    $tmpMsi  = Join-Path $tmpDir "WinDirStat-$Arch-$($lang.Code).msi"
    $tmpMst  = Join-Path $tmpDir "WinDirStat-$Arch-$($lang.Code).mst"

    # Fall back to en-US so strings the WixUI extension doesn't localize for
    # this culture resolve to English instead of failing the build.
    Write-Host "  [$($lang.Culture)] building localized MSI..."
    & wix build -arch $Arch 'WinDirStat.wxs' `
        -loc $wxl -culture $lang.Culture -culture 'en-US' `
        -o $tmpMsi @verArgs `
        -ext WixToolset.UI.wixext `
        -ext WixToolset.Util.wixext
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "  wix build failed for $($lang.Culture) - skipping"
        $failed.Add($lang.Culture)
        continue
    }

    Write-Host "  [$($lang.Culture)] creating language transform..."
    & wix msi transform $MsiPath $tmpMsi -out $tmpMst -t language
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "  wix msi transform failed for $($lang.Culture) - skipping"
        $failed.Add($lang.Culture)
        Remove-Item $tmpMsi -ErrorAction SilentlyContinue
        continue
    }
    if (-not (Test-Path $tmpMst)) {
        Write-Warning "  Transform file not created for $($lang.Culture) - skipping"
        $failed.Add($lang.Culture)
        Remove-Item $tmpMsi -ErrorAction SilentlyContinue
        continue
    }

    Write-Host "  [$($lang.Culture)] embedding transform (LCID $($lang.Lcid))..."
    Add-LanguageTransform -MsiPath $MsiPath -MstPath $tmpMst -Lcid $lang.Lcid
    $embeddedLcids.Add($lang.Lcid)

    Remove-Item $tmpMsi, $tmpMst -ErrorAction SilentlyContinue
}

Write-Host "  Updating Template Summary Information..."
Update-TemplateLangs -MsiPath $MsiPath -Lcids $embeddedLcids.ToArray()

Remove-Item $tmpDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "  Done: $($embeddedLcids.Count) language(s) embedded in $MsiPath"

if ($failed.Count -gt 0) {
    Write-Error "  $($failed.Count) language(s) failed and were NOT embedded: $($failed -join ', ')"
    exit 1
}
