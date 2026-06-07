param(
    [string]$MsiPath,
    [string]$Arch,
    [string]$RelType,
    [string]$MajVer,
    [string]$MinVer,
    [string]$Patch,
    [string]$Build,
    [string]$EstimatedSize,
    [string]$ProductCode,
    [switch]$GenerateOnly
)


$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

if (-not $GenerateOnly) {
    if (-not $MsiPath -or -not $Arch -or -not $RelType -or -not $MajVer -or -not $MinVer -or -not $Patch -or -not $Build -or -not $EstimatedSize -or -not $ProductCode) {
        Write-Error "Missing required parameters for embedding transforms."
        exit 1
    }
}

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
# Generate WiX localization .wxl files dynamically
# ---------------------------------------------------------------------------
function Generate-WxlFiles {
    param(
        [string]$LangDir = "$PSScriptRoot\..\..\windirstat\res\langs",
        [string]$OutputDir = "$PSScriptRoot\temp_wxl"
    )

    if (-not (Test-Path $OutputDir)) {
        New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    }

    function Escape-XmlString ([string]$str) {
        if ([string]::IsNullOrEmpty($str)) { return "" }
        return $str.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;").Replace('"', "&quot;").Replace("'", "&apos;")
    }

    $languagesList = [System.Collections.Generic.List[PSCustomObject]]::new()

    $langFiles = Get-ChildItem -Path "$LangDir\lang_*.txt"
    foreach ($file in $langFiles) {
        $code = ($file.BaseName -replace '^lang_', '').ToLower()
        
        $lines = [System.IO.File]::ReadAllLines($file.FullName, [System.Text.UTF8Encoding]::new($false))
        
        $msiKeys = @{}
        foreach ($line in $lines) {
            if ($line -match '^MSI_([^=]+)=(.*)$') {
                $key = $Matches[1]
                $value = $Matches[2]
                $msiKeys[$key] = $value
            }
        }
        
        if (-not $msiKeys.ContainsKey('CULTURE') -or -not $msiKeys.ContainsKey('LCID') -or -not $msiKeys.ContainsKey('CODEPAGE')) {
            continue
        }
        
        $culture = $msiKeys['CULTURE']
        $lcid = [int]$msiKeys['LCID']
        $codepage = $msiKeys['CODEPAGE']
        
        $xmlLines = @(
            '<?xml version="1.0" encoding="UTF-8"?>'
            "<WixLocalization Culture=""$culture"" Codepage=""$codepage"" xmlns=""http://wixtoolset.org/schemas/v4/wxl"">"
        )
        
        $sortedKeys = $msiKeys.Keys | Sort-Object
        foreach ($key in $sortedKeys) {
            if ($key -eq 'CULTURE' -or $key -eq 'LCID' -or $key -eq 'CODEPAGE') {
                continue
            }
            $escapedValue = Escape-XmlString $msiKeys[$key]
            $xmlLines += "  <String Id=""$key"" Value=""$escapedValue""/>"
        }
        
        $xmlLines += "</WixLocalization>"
        $xmlContent = $xmlLines -join "`r`n"
        
        $wxlFileName = "WinDirStat_$code.wxl"
        $wxlPath = Join-Path $OutputDir $wxlFileName
        
        [System.IO.File]::WriteAllText($wxlPath, $xmlContent, [System.Text.UTF8Encoding]::new($false))
        
        $languagesList.Add([PSCustomObject]@{
            Code     = $code
            Culture  = $culture
            Lcid     = $lcid
            Codepage = $codepage
            WxlPath  = $wxlPath
        })
    }

    $languagesList
}

if ($GenerateOnly) {
    Generate-WxlFiles | Out-Null
    exit 0
}

# Dynamically generate .wxl files and load language metadata (except 'en')
$languages = Generate-WxlFiles | Where-Object { $_.Code -ne 'en' }


# Version/release args shared by every wix build invocation
$licenseRtf = if (Test-Path 'license-build.rtf') { 'license-build.rtf' } else { 'license.rtf' }
$verArgs = @(
    '-d', "RELTYPE=$RelType",
    '-d', "MAJVER=$MajVer",
    '-d', "MINVER=$MinVer",
    '-d', "PATCH=$Patch",
    '-d', "BUILD=$Build",
    '-d', "EstimatedSize=$EstimatedSize",
    '-d', "ProductCode=$ProductCode",
    '-d', "LicenseRtf=$licenseRtf"
)

# Resolve to absolute path so P/Invoke calls always get a full path
$MsiPath = (Resolve-Path $MsiPath).Path

$tmpDir = Join-Path $env:TEMP 'WinDirStat_transforms'
New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null

$embeddedLcids = [System.Collections.Generic.List[int]]::new()
$embeddedLcids.Add(1033)   # base English is always present

$failed = [System.Collections.Generic.List[string]]::new()

foreach ($lang in $languages) {
    $wxl     = $lang.WxlPath
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
