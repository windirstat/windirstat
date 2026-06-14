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
    [int]$Jobs = 0,
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
$languages = @(Generate-WxlFiles | Where-Object { $_.Code -ne 'en' })

if ($Jobs -le 0) {
    $jobsFromEnv = 0
    if ([int]::TryParse($env:WDS_MSI_LANG_JOBS, [ref]$jobsFromEnv) -and $jobsFromEnv -gt 0) {
        $Jobs = $jobsFromEnv
    }
    else {
        $Jobs = [Math]::Min([Math]::Max([Environment]::ProcessorCount - 1, 1), 6)
    }
}
$Jobs = [Math]::Max(1, $Jobs)


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

$languageJobScript = {
    param(
        [string]$ScriptRoot,
        [string]$Arch,
        [string]$MsiPath,
        [string]$RelType,
        [string]$MajVer,
        [string]$MinVer,
        [string]$Patch,
        [string]$Build,
        [string]$EstimatedSize,
        [string]$ProductCode,
        [string]$LicenseRtf,
        [string]$TempRoot,
        [string]$Code,
        [string]$Culture,
        [int]$Lcid,
        [string]$WxlPath
    )

    $ErrorActionPreference = 'Stop'
    Set-Location $ScriptRoot

    function Read-LogText {
        param([string]$Path)
        if (Test-Path $Path) {
            return [System.IO.File]::ReadAllText($Path)
        }
        return ''
    }

    $workDir = Join-Path $TempRoot "$Arch-$Code"
    $tmpMsi = Join-Path $workDir "WinDirStat-$Arch-$Code.msi"
    $tmpMst = Join-Path $workDir "WinDirStat-$Arch-$Code.mst"
    $buildOut = Join-Path $workDir 'wix-build.out.log'
    $buildErr = Join-Path $workDir 'wix-build.err.log'
    $transformOut = Join-Path $workDir 'wix-transform.out.log'
    $transformErr = Join-Path $workDir 'wix-transform.err.log'
    $logParts = [System.Collections.Generic.List[string]]::new()

    try {
        New-Item -ItemType Directory -Force -Path $workDir | Out-Null

        $localVerArgs = @(
            '-d', "RELTYPE=$RelType",
            '-d', "MAJVER=$MajVer",
            '-d', "MINVER=$MinVer",
            '-d', "PATCH=$Patch",
            '-d', "BUILD=$Build",
            '-d', "EstimatedSize=$EstimatedSize",
            '-d', "ProductCode=$ProductCode",
            '-d', "LicenseRtf=$LicenseRtf"
        )

        # Fall back to en-US so strings the WixUI extension doesn't localize for
        # this culture resolve to English instead of failing the build.
        $buildArgs = @(
            'build',
            '-arch', $Arch,
            'WinDirStat.wxs',
            '-loc', $WxlPath,
            '-culture', $Culture,
            '-culture', 'en-US',
            '-o', $tmpMsi,
            '-intermediatefolder', (Join-Path $workDir 'build'),
            '-pdbtype', 'none'
        ) + $localVerArgs + @(
            '-ext', 'WixToolset.UI.wixext',
            '-ext', 'WixToolset.Util.wixext'
        )

        & wix @buildArgs > $buildOut 2> $buildErr
        $buildExitCode = $LASTEXITCODE
        foreach ($logFile in @($buildOut, $buildErr)) {
            $logText = Read-LogText $logFile
            if (-not [string]::IsNullOrWhiteSpace($logText)) {
                $logParts.Add($logText.Trim())
            }
        }
        if ($buildExitCode -ne 0) {
            return [PSCustomObject]@{
                Code    = $Code
                Culture = $Culture
                Lcid    = $Lcid
                Success = $false
                Stage   = 'wix build'
                MstPath = $null
                Log     = ($logParts -join [Environment]::NewLine)
            }
        }

        $transformArgs = @(
            'msi',
            'transform',
            $MsiPath,
            $tmpMsi,
            '-out', $tmpMst,
            '-t', 'language',
            '-intermediateFolder', (Join-Path $workDir 'transform')
        )

        & wix @transformArgs > $transformOut 2> $transformErr
        $transformExitCode = $LASTEXITCODE
        foreach ($logFile in @($transformOut, $transformErr)) {
            $logText = Read-LogText $logFile
            if (-not [string]::IsNullOrWhiteSpace($logText)) {
                $logParts.Add($logText.Trim())
            }
        }
        if ($transformExitCode -ne 0) {
            Remove-Item $tmpMsi -ErrorAction SilentlyContinue
            return [PSCustomObject]@{
                Code    = $Code
                Culture = $Culture
                Lcid    = $Lcid
                Success = $false
                Stage   = 'wix msi transform'
                MstPath = $null
                Log     = ($logParts -join [Environment]::NewLine)
            }
        }
        if (-not (Test-Path $tmpMst)) {
            Remove-Item $tmpMsi -ErrorAction SilentlyContinue
            return [PSCustomObject]@{
                Code    = $Code
                Culture = $Culture
                Lcid    = $Lcid
                Success = $false
                Stage   = 'transform output'
                MstPath = $null
                Log     = 'Transform file was not created.'
            }
        }

        Remove-Item $tmpMsi -ErrorAction SilentlyContinue

        return [PSCustomObject]@{
            Code    = $Code
            Culture = $Culture
            Lcid    = $Lcid
            Success = $true
            Stage   = 'complete'
            MstPath = $tmpMst
            Log     = ($logParts -join [Environment]::NewLine)
        }
    }
    catch {
        return [PSCustomObject]@{
            Code    = $Code
            Culture = $Culture
            Lcid    = $Lcid
            Success = $false
            Stage   = 'job'
            MstPath = $null
            Log     = $_.Exception.Message
        }
    }
}

Write-Host "  Building language transforms with up to $Jobs parallel job(s)..."

$pending = [System.Collections.Queue]::new()
foreach ($lang in $languages) {
    $pending.Enqueue($lang)
}

$running = @()
$results = [System.Collections.Generic.List[object]]::new()

while ($pending.Count -gt 0 -or $running.Count -gt 0) {
    while ($pending.Count -gt 0 -and $running.Count -lt $Jobs) {
        $lang = $pending.Dequeue()
        Write-Host "  [$($lang.Culture)] starting localized MSI/transform job..."
        $job = Start-Job -ScriptBlock $languageJobScript -ArgumentList @(
            $PSScriptRoot,
            $Arch,
            $MsiPath,
            $RelType,
            $MajVer,
            $MinVer,
            $Patch,
            $Build,
            $EstimatedSize,
            $ProductCode,
            $licenseRtf,
            $tmpDir,
            $lang.Code,
            $lang.Culture,
            $lang.Lcid,
            $lang.WxlPath
        )
        $running += [PSCustomObject]@{
            Job  = $job
            Lang = $lang
        }
    }

    if ($running.Count -eq 0) {
        continue
    }

    Wait-Job -Job @($running | ForEach-Object { $_.Job }) -Any | Out-Null

    $finished = @($running | Where-Object { $_.Job.State -ne 'Running' })
    foreach ($entry in $finished) {
        $job = $entry.Job
        $lang = $entry.Lang

        try {
            $jobOutput = @(Receive-Job -Job $job -ErrorAction Stop)
            $result = $jobOutput | Select-Object -Last 1
            if ($null -eq $result) {
                throw 'Job returned no result.'
            }
        }
        catch {
            $result = [PSCustomObject]@{
                Code    = $lang.Code
                Culture = $lang.Culture
                Lcid    = $lang.Lcid
                Success = $false
                Stage   = 'job'
                MstPath = $null
                Log     = $_.Exception.Message
            }
        }
        finally {
            Remove-Job -Job $job -Force -ErrorAction SilentlyContinue
        }

        $results.Add($result)
        if ($result.Success) {
            Write-Host "  [$($result.Culture)] language transform ready."
        }
        else {
            Write-Warning "  $($result.Stage) failed for $($result.Culture) - skipping"
            if (-not [string]::IsNullOrWhiteSpace($result.Log)) {
                Write-Warning $result.Log
            }
            $failed.Add($result.Culture)
        }
    }

    $finishedIds = @($finished | ForEach-Object { $_.Job.Id })
    $running = @($running | Where-Object { $finishedIds -notcontains $_.Job.Id })
}

foreach ($lang in $languages) {
    $result = $results | Where-Object { $_.Code -eq $lang.Code } | Select-Object -First 1
    if ($null -eq $result -or -not $result.Success) {
        continue
    }

    Write-Host "  [$($lang.Culture)] embedding transform (LCID $($lang.Lcid))..."
    Add-LanguageTransform -MsiPath $MsiPath -MstPath $result.MstPath -Lcid $lang.Lcid
    $embeddedLcids.Add($lang.Lcid)

    Remove-Item $result.MstPath -ErrorAction SilentlyContinue
}

Write-Host "  Updating Template Summary Information..."
Update-TemplateLangs -MsiPath $MsiPath -Lcids $embeddedLcids.ToArray()

Remove-Item $tmpDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "  Done: $($embeddedLcids.Count) language(s) embedded in $MsiPath"

if ($failed.Count -gt 0) {
    Write-Error "  $($failed.Count) language(s) failed and were NOT embedded: $($failed -join ', ')"
    exit 1
}
