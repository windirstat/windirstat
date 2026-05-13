param(
    [ValidateSet('Release', 'Debug')]
    [string] $Configuration = 'Release',

    [ValidateSet('x64', 'Win32', 'ARM64')]
    [string] $Platform = 'x64',

    [string] $ExePath,

    [switch] $SkipBuild,

    [int] $TimeoutSeconds = 120,

    [switch] $KeepArtifacts,

    [switch] $ShowBuildOutput,

    [Alias('ShowPassedDetails')]
    [switch] $Details
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')
$repoRoot = $repoRoot.ProviderPath
$buildRoot = Join-Path $repoRoot 'build'
$workRoot = Join-Path $buildRoot 'nonvisual-settings-test'
$sourceRoot = Join-Path $workRoot 'source'
$runRoot = Join-Path $workRoot 'runner'
$scanRoot = Join-Path $workRoot 'scan-root'
$dupeRoot = Join-Path $workRoot 'dupe-root'
$platformShortName = switch ($Platform) {
    'Win32' { 'x86' }
    'x64' { 'x64' }
    'ARM64' { 'arm64' }
}
$targetName = "WinDirStat_$($platformShortName)_settingstest"
$recordSeparator = [string] [char] 0x1e
$symbolPass = [string] [char] 0x2713
$symbolFail = [string] [char] 0x2717
$symbolWarn = '!'

function Get-StatusColor {
    param([Parameter(Mandatory)] [string] $Status)

    switch ($Status) {
        'PASS' { 'Green' }
        'FAIL' { 'Red' }
        'WARN' { 'Yellow' }
        default { 'Gray' }
    }
}

function Write-ColoredLine {
    param(
        [Parameter(Mandatory)] [string] $Message,
        [ConsoleColor] $Color = [ConsoleColor]::Gray
    )

    Write-Host $Message -ForegroundColor $Color
}

function Write-LabelValue {
    param(
        [Parameter(Mandatory)] [string] $Label,
        [AllowNull()] [object] $Value,
        [ConsoleColor] $ValueColor = [ConsoleColor]::Gray
    )

    Write-Host -NoNewline "${Label}: " -ForegroundColor DarkCyan
    Write-Host $Value -ForegroundColor $ValueColor
}

function Write-SymbolCell {
    param(
        [Parameter(Mandatory)] [string] $Text,
        [Parameter(Mandatory)] [int] $Width,
        [ConsoleColor] $Color = [ConsoleColor]::Gray
    )

    Write-Host -NoNewline $Text.PadRight($Width) -ForegroundColor $Color
}

function Find-MSBuild {
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio',
        'C:\Program Files (x86)\Microsoft Visual Studio'
    )

    foreach ($root in $candidates) {
        if (!(Test-Path -LiteralPath $root)) { continue }
        $match = Get-ChildItem -LiteralPath $root -Recurse -Filter MSBuild.exe -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -like '*\MSBuild\Current\Bin\MSBuild.exe' } |
            Select-Object -First 1
        if ($match) { return $match.FullName }
    }

    throw 'MSBuild.exe was not found. Pass -SkipBuild -ExePath <path-to-settingstest-WinDirStat.exe> to test an existing settings-test build.'
}

function Clear-TestAttributes {
    param([Parameter(Mandatory)] [string] $Path)

    if (!(Test-Path -LiteralPath $Path)) { return }
    Get-ChildItem -LiteralPath $Path -Force -Recurse -ErrorAction SilentlyContinue |
        ForEach-Object {
            try {
                $_.Attributes = $_.Attributes -band (-bnot [System.IO.FileAttributes]::Hidden)
                $_.Attributes = $_.Attributes -band (-bnot [System.IO.FileAttributes]::System)
                $_.Attributes = $_.Attributes -band (-bnot [System.IO.FileAttributes]::ReadOnly)
            }
            catch {}
        }
}

function Remove-TestBuildArtifacts {
    Clear-TestAttributes -Path $workRoot
    if (Test-Path -LiteralPath $workRoot) {
        Remove-Item -LiteralPath $workRoot -Recurse -Force
    }
}

function Copy-SourceTreeForBuild {
    param(
        [Parameter(Mandatory)] [string] $Source,
        [Parameter(Mandatory)] [string] $Destination
    )

    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null

    $excludedRoots = @(
        (Join-Path $Source '.git'),
        (Join-Path $Source 'build'),
        (Join-Path $Source 'intermediate')
    ) | ForEach-Object { [System.IO.Path]::GetFullPath($_).TrimEnd('\') }

    Get-ChildItem -LiteralPath $Source -Force -Recurse -File |
        Where-Object {
            $full = [System.IO.Path]::GetFullPath($_.FullName)
            foreach ($excluded in $excludedRoots) {
                if ($full.Equals($excluded, [System.StringComparison]::OrdinalIgnoreCase) -or
                    $full.StartsWith("$excluded\", [System.StringComparison]::OrdinalIgnoreCase)) {
                    return $false
                }
            }
            return $true
        } |
        ForEach-Object {
            $relative = [System.IO.Path]::GetRelativePath($Source, $_.FullName)
            $target = Join-Path $Destination $relative
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $target -Force
        }
}

function Add-SettingsTestHarness {
    param([Parameter(Mandatory)] [string] $Source)

    $appPath = Join-Path $Source 'windirstat\WinDirStat.cpp'
    $text = [System.IO.File]::ReadAllText($appPath)

    $includeMarker = '#include "CsvLoader.h"'
    $includeReplacement = @'
#include "CsvLoader.h"
#ifdef WDS_SETTINGS_TEST
#include "FileSearchControl.h"
#include <iomanip>
#include <sstream>
#endif
'@
    if (!$text.Contains($includeMarker)) {
        throw "Could not locate include marker in $appPath"
    }
    $text = $text.Replace($includeMarker, $includeReplacement)

    $helper = @'
#ifdef WDS_SETTINGS_TEST
namespace WdsSettingsTest
{
    std::string ToUtf8(const std::wstring& value)
    {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};

        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            result.data(), size, nullptr, nullptr);
        return result;
    }

    std::string JsonString(const std::wstring& value)
    {
        const std::string utf8 = ToUtf8(value);
        std::ostringstream out;
        out << '"';
        for (const unsigned char ch : utf8)
        {
            switch (ch)
            {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20)
                {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(ch) << std::dec << std::setfill(' ');
                }
                else
                {
                    out << static_cast<char>(ch);
                }
                break;
            }
        }
        out << '"';
        return out.str();
    }

    std::string JsonString(const CString& value)
    {
        return JsonString(std::wstring(value.GetString()));
    }

    std::string JsonBool(const bool value)
    {
        return value ? "true" : "false";
    }

    void RawField(std::ostringstream& out, bool& first, const char* name, const std::string& raw)
    {
        if (!first) out << ',';
        out << "\n  \"" << name << "\": " << raw;
        first = false;
    }

    void BoolField(std::ostringstream& out, bool& first, const char* name, const bool value)
    {
        RawField(out, first, name, JsonBool(value));
    }

    void IntField(std::ostringstream& out, bool& first, const char* name, const long long value)
    {
        RawField(out, first, name, std::to_string(value));
    }

    void StringField(std::ostringstream& out, bool& first, const char* name, const std::wstring& value)
    {
        RawField(out, first, name, JsonString(value));
    }

    std::string IntArray(const std::vector<int>& values)
    {
        std::ostringstream out;
        out << '[';
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0) out << ',';
            out << values[i];
        }
        out << ']';
        return out.str();
    }

    std::string StringArray(const std::vector<std::wstring>& values)
    {
        std::ostringstream out;
        out << '[';
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0) out << ',';
            out << JsonString(values[i]);
        }
        out << ']';
        return out.str();
    }

    std::string LanguageArray()
    {
        std::vector<int> values;
        for (const auto language : Localization::GetLanguageList())
        {
            values.push_back(static_cast<int>(language));
        }
        return IntArray(values);
    }

    std::string ReparseFollowingJson()
    {
        std::ostringstream out;
        out << '{';
        bool first = true;
        BoolField(out, first, "None", CDirStatApp::Get()->IsFollowingAllowed(0));
        BoolField(out, first, "MountPoint", CDirStatApp::Get()->IsFollowingAllowed(IO_REPARSE_TAG_MOUNT_POINT));
        BoolField(out, first, "SymbolicLink", CDirStatApp::Get()->IsFollowingAllowed(IO_REPARSE_TAG_SYMLINK));
        BoolField(out, first, "Junction", CDirStatApp::Get()->IsFollowingAllowed(IO_REPARSE_TAG_JUNCTION_POINT));
        BoolField(out, first, "OtherReparsePoint", CDirStatApp::Get()->IsFollowingAllowed(0xA0001234));
        out << "\n  }";
        return out.str();
    }

    std::string SearchProbeJson()
    {
        std::ostringstream out;
        out << '{';
        bool first = true;

        const std::wregex searchRegex = CFileSearchControl::ComputeSearchRegex(
            COptions::SearchTerm.Obj(), COptions::SearchCase, COptions::SearchRegex);
        const auto matches = [&](const std::wstring& value)
        {
            try
            {
                return COptions::SearchWholePhrase ?
                    std::regex_match(value, searchRegex) :
                    std::regex_search(value, searchRegex);
            }
            catch (...)
            {
                return false;
            }
        };

        BoolField(out, first, "LowerLog", matches(L"alpha.log"));
        BoolField(out, first, "UpperLog", matches(L"Alpha.LOG"));
        BoolField(out, first, "NotesTxt", matches(L"notes.txt"));
        BoolField(out, first, "LiteralPattern", matches(L"literal.*"));
        BoolField(out, first, "TargetSubstring", matches(L"prefix-target-suffix"));
        out << "\n  }";
        return out.str();
    }

    std::string UserDefinedCleanupsJson()
    {
        std::ostringstream out;
        out << '[';
        for (size_t i = 0; i < COptions::UserDefinedCleanups.size(); ++i)
        {
            if (i > 0) out << ',';
            const auto& udc = COptions::UserDefinedCleanups[i];
            out << "\n    {";
            bool first = true;
            StringField(out, first, "Title", udc.Title.Obj());
            StringField(out, first, "CommandLine", udc.CommandLine.Obj());
            BoolField(out, first, "Enabled", udc.Enabled.Obj());
            BoolField(out, first, "VirginTitle", udc.VirginTitle.Obj());
            BoolField(out, first, "WorksForDrives", udc.WorksForDrives.Obj());
            BoolField(out, first, "WorksForDirectories", udc.WorksForDirectories.Obj());
            BoolField(out, first, "WorksForFiles", udc.WorksForFiles.Obj());
            BoolField(out, first, "WorksForUncPaths", udc.WorksForUncPaths.Obj());
            BoolField(out, first, "RecurseIntoSubdirectories", udc.RecurseIntoSubdirectories.Obj());
            BoolField(out, first, "AskForConfirmation", udc.AskForConfirmation.Obj());
            BoolField(out, first, "ShowConsoleWindow", udc.ShowConsoleWindow.Obj());
            BoolField(out, first, "WaitForCompletion", udc.WaitForCompletion.Obj());
            IntField(out, first, "RefreshPolicy", udc.RefreshPolicy.Obj());
            out << "\n    }";
        }
        out << "\n  ]";
        return out.str();
    }

    std::string BuildDumpJson()
    {
        std::ostringstream out;
        out << '{';
        bool first = true;

        BoolField(out, first, "AutomaticallyResizeColumns", COptions::AutomaticallyResizeColumns.Obj());
        BoolField(out, first, "AutoMapDrivesWhenElevated", COptions::AutoMapDrivesWhenElevated.Obj());
        BoolField(out, first, "ExcludeJunctions", COptions::ExcludeJunctions.Obj());
        BoolField(out, first, "ExcludeSymbolicLinksDirectory", COptions::ExcludeSymbolicLinksDirectory.Obj());
        BoolField(out, first, "ExcludeVolumeMountPoints", COptions::ExcludeVolumeMountPoints.Obj());
        BoolField(out, first, "ExcludeHiddenDirectory", COptions::ExcludeHiddenDirectory.Obj());
        BoolField(out, first, "ExcludeProtectedDirectory", COptions::ExcludeProtectedDirectory.Obj());
        BoolField(out, first, "ExcludeSymbolicLinksFile", COptions::ExcludeSymbolicLinksFile.Obj());
        BoolField(out, first, "ExcludeHiddenFile", COptions::ExcludeHiddenFile.Obj());
        BoolField(out, first, "ExcludeProtectedFile", COptions::ExcludeProtectedFile.Obj());
        BoolField(out, first, "FilteringUseRegex", COptions::FilteringUseRegex.Obj());
        BoolField(out, first, "FollowVolumeMountPoints", COptions::FollowVolumeMountPoints.Obj());
        BoolField(out, first, "UseSizeSuffixes", COptions::UseSizeSuffixes.Obj());
        BoolField(out, first, "ListFullRowSelection", COptions::ListFullRowSelection.Obj());
        BoolField(out, first, "ListGrid", COptions::ListGrid.Obj());
        BoolField(out, first, "ListStripes", COptions::ListStripes.Obj());
        BoolField(out, first, "PacmanAnimation", COptions::PacmanAnimation.Obj());
        BoolField(out, first, "ScanForDuplicates", COptions::ScanForDuplicates.Obj());
        BoolField(out, first, "SearchWholePhrase", COptions::SearchWholePhrase.Obj());
        BoolField(out, first, "SearchCase", COptions::SearchCase.Obj());
        BoolField(out, first, "SearchRegex", COptions::SearchRegex.Obj());
        IntField(out, first, "SearchMaxResults", COptions::SearchMaxResults.Obj());
        BoolField(out, first, "ShowColumnAttributes", COptions::ShowColumnAttributes.Obj());
        BoolField(out, first, "ShowColumnFiles", COptions::ShowColumnFiles.Obj());
        BoolField(out, first, "ShowColumnFolders", COptions::ShowColumnFolders.Obj());
        BoolField(out, first, "ShowColumnItems", COptions::ShowColumnItems.Obj());
        BoolField(out, first, "ShowColumnLastChange", COptions::ShowColumnLastChange.Obj());
        BoolField(out, first, "ShowColumnOwner", COptions::ShowColumnOwner.Obj());
        BoolField(out, first, "ShowColumnSizeLogical", COptions::ShowColumnSizeLogical.Obj());
        BoolField(out, first, "ShowColumnSizePhysical", COptions::ShowColumnSizePhysical.Obj());
        BoolField(out, first, "ShowDeleteWarning", COptions::ShowDeleteWarning.Obj());
        BoolField(out, first, "ShowElevationPrompt", COptions::ShowElevationPrompt.Obj());
        BoolField(out, first, "ShowMicrosoftProgress", COptions::ShowMicrosoftProgress.Obj());
        BoolField(out, first, "ShowFileTypes", COptions::ShowFileTypes.Obj());
        BoolField(out, first, "ShowFreeSpace", COptions::ShowFreeSpace.Obj());
        BoolField(out, first, "ShowStatusBar", COptions::ShowStatusBar.Obj());
        BoolField(out, first, "ShowTimeSpent", COptions::ShowTimeSpent.Obj());
        BoolField(out, first, "ShowToolBar", COptions::ShowToolBar.Obj());
        BoolField(out, first, "LargeToolBar", COptions::LargeToolBar.Obj());
        BoolField(out, first, "ShowTreeMap", COptions::ShowTreeMap.Obj());
        BoolField(out, first, "ShowUnknown", COptions::ShowUnknown.Obj());
        BoolField(out, first, "SkipDupeDetectionCloudLinks", COptions::SkipDupeDetectionCloudLinks.Obj());
        BoolField(out, first, "ShowDupeDetectionCloudLinksWarning", COptions::ShowDupeDetectionCloudLinksWarning.Obj());
        BoolField(out, first, "AutoElevate", COptions::AutoElevate.Obj());
        BoolField(out, first, "TreeMapGrid", COptions::TreeMapGrid.Obj());
        BoolField(out, first, "TreeMapShowExtensions", COptions::TreeMapShowExtensions.Obj());
        BoolField(out, first, "TreeMapUseLogical", COptions::TreeMapUseLogical.Obj());
        BoolField(out, first, "UseBackupRestore", COptions::UseBackupRestore.Obj());
        BoolField(out, first, "UseDrawTextCache", COptions::UseDrawTextCache.Obj());
        BoolField(out, first, "UseFastScanEngine", COptions::UseFastScanEngine.Obj());
        BoolField(out, first, "UseWindowsLocaleSetting", COptions::UseWindowsLocaleSetting.Obj());
        BoolField(out, first, "ProcessHardlinks", COptions::ProcessHardlinks.Obj());
        IntField(out, first, "ConfigPage", COptions::ConfigPage.Obj());
        IntField(out, first, "LanguageId", COptions::LanguageId.Obj());
        IntField(out, first, "FileHashAlgorithm", COptions::FileHashAlgorithm.Obj());
        IntField(out, first, "LargeFileCount", COptions::LargeFileCount.Obj());
        IntField(out, first, "MinimizeViewThreshold", COptions::MinimizeViewThreshold.Obj());
        IntField(out, first, "ScanningThreads", COptions::ScanningThreads.Obj());
        IntField(out, first, "SelectDrivesRadio", COptions::SelectDrivesRadio.Obj());
        IntField(out, first, "SizeProportionIndent", COptions::SizeProportionIndent.Obj());
        IntField(out, first, "FileTreeColorCount", COptions::FileTreeColorCount.Obj());
        IntField(out, first, "FilteringSizeMinimum", COptions::FilteringSizeMinimum.Obj());
        IntField(out, first, "FilteringSizeUnits", COptions::FilteringSizeUnits.Obj());
        IntField(out, first, "TreeMapAmbientLightPercent", COptions::TreeMapAmbientLightPercent.Obj());
        IntField(out, first, "TreeMapBrightness", COptions::TreeMapBrightness.Obj());
        IntField(out, first, "TreeMapHeightFactor", COptions::TreeMapHeightFactor.Obj());
        IntField(out, first, "TreeMapLightSourceX", COptions::TreeMapLightSourceX.Obj());
        IntField(out, first, "TreeMapLightSourceY", COptions::TreeMapLightSourceY.Obj());
        IntField(out, first, "TreeMapScaleFactor", COptions::TreeMapScaleFactor.Obj());
        IntField(out, first, "TreeMapStyle", COptions::TreeMapStyle.Obj());
        IntField(out, first, "DarkMode", COptions::DarkMode.Obj());
        IntField(out, first, "FolderHistoryCount", COptions::FolderHistoryCount.Obj());
        RawField(out, first, "DriveListColumnOrder", IntArray(COptions::DriveListColumnOrder.Obj()));
        RawField(out, first, "DriveListColumnWidths", IntArray(COptions::DriveListColumnWidths.Obj()));
        RawField(out, first, "DupeViewColumnOrder", IntArray(COptions::DupeViewColumnOrder.Obj()));
        RawField(out, first, "DupeViewColumnWidths", IntArray(COptions::DupeViewColumnWidths.Obj()));
        RawField(out, first, "FileTreeColumnOrder", IntArray(COptions::FileTreeColumnOrder.Obj()));
        RawField(out, first, "FileTreeColumnWidths", IntArray(COptions::FileTreeColumnWidths.Obj()));
        RawField(out, first, "ExtViewColumnOrder", IntArray(COptions::ExtViewColumnOrder.Obj()));
        RawField(out, first, "ExtViewColumnWidths", IntArray(COptions::ExtViewColumnWidths.Obj()));
        RawField(out, first, "SearchViewColumnOrder", IntArray(COptions::SearchViewColumnOrder.Obj()));
        RawField(out, first, "SearchViewColumnWidths", IntArray(COptions::SearchViewColumnWidths.Obj()));
        RawField(out, first, "TopViewColumnOrder", IntArray(COptions::TopViewColumnOrder.Obj()));
        RawField(out, first, "TopViewColumnWidths", IntArray(COptions::TopViewColumnWidths.Obj()));
        RawField(out, first, "WatcherColumnOrder", IntArray(COptions::WatcherColumnOrder.Obj()));
        RawField(out, first, "WatcherColumnWidths", IntArray(COptions::WatcherColumnWidths.Obj()));
        RawField(out, first, "SelectDrivesDrives", StringArray(COptions::SelectDrivesDrives.Obj()));
        RawField(out, first, "SelectDrivesFolder", StringArray(COptions::SelectDrivesFolder.Obj()));
        StringField(out, first, "FilteringExcludeDirs", COptions::FilteringExcludeDirs.Obj());
        StringField(out, first, "FilteringExcludeFiles", COptions::FilteringExcludeFiles.Obj());
        StringField(out, first, "FilteringIncludeDirs", COptions::FilteringIncludeDirs.Obj());
        StringField(out, first, "FilteringIncludeFiles", COptions::FilteringIncludeFiles.Obj());
        StringField(out, first, "SearchTerm", COptions::SearchTerm.Obj());
        RawField(out, first, "LanguageList", LanguageArray());
        IntField(out, first, "LocaleForFormatting", COptions::GetLocaleForFormatting());
        IntField(out, first, "UserDefaultLCID", GetUserDefaultLCID());
        RawField(out, first, "ReparseFollowing", ReparseFollowingJson());
        RawField(out, first, "SearchProbeMatches", SearchProbeJson());
        RawField(out, first, "UserDefinedCleanups", UserDefinedCleanupsJson());

        out << "\n}\n";
        return out.str();
    }

    void TryRun()
    {
        int argc = 0;
        SmartPointer<LPWSTR*> argv([](LPWSTR* value) { LocalFree(value); }, CommandLineToArgvW(GetCommandLineW(), &argc));
        if (!argv) return;

        std::wstring outputPath;
        for (int i = 1; i < argc; ++i)
        {
            const std::wstring arg = MakeLower(argv.Get()[i]);
            if ((arg == L"/wds-settings-dump" || arg == L"--wds-settings-dump") && i + 1 < argc)
            {
                outputPath = argv.Get()[i + 1];
                break;
            }
        }
        if (outputPath.empty()) return;

        std::ofstream out(outputPath, std::ios::binary);
        if (!out.is_open()) ExitProcess(1);
        out << BuildDumpJson();
        out.flush();
        ExitProcess(out.good() ? 0 : 1);
    }
}
#endif

'@

    $initMarker = 'BOOL CDirStatApp::InitInstance()'
    if (!$text.Contains($initMarker)) {
        throw "Could not locate InitInstance marker in $appPath"
    }
    $text = $text.Replace($initMarker, "$helper$initMarker")

    $loadMarker = '    COptions::LoadAppSettings();'
    $loadReplacement = @'
    COptions::LoadAppSettings();
#ifdef WDS_SETTINGS_TEST
    WdsSettingsTest::TryRun();
#endif
'@
    if (!$text.Contains($loadMarker)) {
        throw "Could not locate LoadAppSettings marker in $appPath"
    }
    $text = $text.Replace($loadMarker, $loadReplacement)

    [System.IO.File]::WriteAllText($appPath, $text, [System.Text.UTF8Encoding]::new($false))
}

function Build-SettingsTestExecutable {
    $msbuild = Find-MSBuild
    $solution = Join-Path $sourceRoot 'windirstat.sln'
    $buildArgs = @(
        $solution,
        '/m:1',
        '/v:minimal',
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/p:TargetName=$targetName",
        '/p:ExternalCompilerOptions=/DWDS_SETTINGS_TEST'
    )

    Write-ColoredLine "Building $targetName from isolated source copy..." Cyan
    $buildOutput = @(& $msbuild @buildArgs 2>&1)
    $buildExitCode = $LASTEXITCODE
    if ($ShowBuildOutput -or $buildExitCode -ne 0) {
        $buildOutput | ForEach-Object {
            $color = if ($buildExitCode -eq 0) { 'DarkGray' } else { 'Yellow' }
            Write-Host $_ -ForegroundColor $color
        }
    }
    if ($buildExitCode -ne 0) {
        throw "MSBuild failed with exit code $buildExitCode."
    }

    $builtExe = Join-Path $sourceRoot "build\$targetName.exe"
    if (!(Test-Path -LiteralPath $builtExe)) {
        throw "Build succeeded but did not create expected executable: $builtExe"
    }

    Write-ColoredLine "Build complete: $builtExe" Green
    return $builtExe
}

function ConvertTo-IniText {
    param([Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Sections)

    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($section in $Sections.GetEnumerator()) {
        [void] $lines.Add("[$($section.Key)]")
        foreach ($entry in $section.Value.GetEnumerator()) {
            [void] $lines.Add("$($entry.Key)=$($entry.Value)")
        }
        [void] $lines.Add('')
    }

    return $lines -join "`r`n"
}

function Write-PortableIni {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Sections
    )

    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    [System.IO.File]::WriteAllText($Path, (ConvertTo-IniText -Sections $Sections), [System.Text.Encoding]::Unicode)
}

function New-BaseIniSections {
    [ordered] @{
        Options = [ordered] @{
            LanguageId = 9
            UseFastScanEngine = 0
            UseBackupRestore = 0
            ShowElevationPrompt = 0
            AutoElevate = 0
            ShowFreeSpace = 0
            ShowUnknown = 0
            ProcessHardlinks = 0
        }
        DriveSelect = [ordered] @{}
        DupeView = [ordered] @{
            ScanForDuplicates = 0
        }
        SearchView = [ordered] @{}
    }
}

function Set-IniValue {
    param(
        [Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Sections,
        [Parameter(Mandatory)] [string] $Section,
        [Parameter(Mandatory)] [string] $Name,
        [AllowNull()] [object] $Value
    )

    if (!$Sections.Contains($Section)) {
        $Sections[$Section] = [ordered] @{}
    }
    $Sections[$Section][$Name] = $Value
}

function Invoke-ProcessWithTimeout {
    param(
        [Parameter(Mandatory)] [string] $FileName,
        [Parameter(Mandatory)] [string[]] $Arguments,
        [Parameter(Mandatory)] [string] $WorkingDirectory
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FileName
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in $Arguments) {
        [void] $startInfo.ArgumentList.Add($argument)
    }

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $process = [System.Diagnostics.Process]::Start($startInfo)
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        try { $process.Kill($true) } catch {}
        throw "$([System.IO.Path]::GetFileName($FileName)) did not finish within $TimeoutSeconds seconds."
    }
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $sw.Stop()
    $displayArgs = @($Arguments | ForEach-Object {
        if ($_ -match '\s') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }) -join ' '

    [pscustomobject] @{
        CommandLine = "`"$FileName`" $displayArgs"
        ExitCode = $process.ExitCode
        StdOut = $stdout
        StdErr = $stderr
        ElapsedSeconds = [math]::Round($sw.Elapsed.TotalSeconds, 3)
    }
}

function Invoke-SettingsDump {
    param(
        [Parameter(Mandatory)] [string] $Exe,
        [Parameter(Mandatory)] [System.Collections.Specialized.OrderedDictionary] $Sections,
        [Parameter(Mandatory)] [string] $Name
    )

    $safeName = $Name -replace '[^A-Za-z0-9_.-]', '_'
    $scenarioRoot = Join-Path $workRoot $safeName
    New-Item -ItemType Directory -Force -Path $scenarioRoot | Out-Null
    $scenarioIni = Join-Path $scenarioRoot 'WinDirStat.ini'
    $runnerIni = Join-Path $runRoot 'WinDirStat.ini'
    $jsonPath = Join-Path $scenarioRoot 'settings.json'

    Write-PortableIni -Path $scenarioIni -Sections $Sections
    Copy-Item -LiteralPath $scenarioIni -Destination $runnerIni -Force
    if (Test-Path -LiteralPath $jsonPath) {
        Remove-Item -LiteralPath $jsonPath -Force
    }

    $run = Invoke-ProcessWithTimeout -FileName $Exe -Arguments @('/wds-settings-dump', $jsonPath) -WorkingDirectory $runRoot
    if ($run.ExitCode -ne 0) {
        throw "Settings dump exited with code $($run.ExitCode). StdErr: $($run.StdErr)"
    }
    if (!(Test-Path -LiteralPath $jsonPath)) {
        throw "Settings dump did not create JSON output: $jsonPath"
    }

    [pscustomobject] @{
        Dump = Get-Content -LiteralPath $jsonPath -Raw -Encoding UTF8 | ConvertFrom-Json
        CommandLine = $run.CommandLine
        ExitCode = $run.ExitCode
        ElapsedSeconds = $run.ElapsedSeconds
        IniPath = $scenarioIni
        JsonPath = $jsonPath
    }
}

function Normalize-ComparePath {
    param([Parameter(Mandatory)] [string] $Path)
    return [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
}

function Test-PathUnder {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Root
    )

    $candidate = Normalize-ComparePath $Path
    $prefix = Normalize-ComparePath $Root
    return $candidate.Equals($prefix, [System.StringComparison]::OrdinalIgnoreCase) -or
        $candidate.StartsWith("$prefix\", [System.StringComparison]::OrdinalIgnoreCase)
}

function New-TestFile {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [int] $Size,
        [byte] $Seed = 31
    )

    $parent = Split-Path -Parent $Path
    New-Item -ItemType Directory -Force -Path $parent | Out-Null

    $bytes = [byte[]]::new($Size)
    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $bytes[$i] = [byte] ((($i + $Seed) % 251) + 1)
    }
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function Add-FileAttributes {
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [System.IO.FileAttributes] $Attributes
    )

    $item = Get-Item -LiteralPath $Path -Force
    $item.Attributes = $item.Attributes -bor $Attributes
}

function Read-CsvRows {
    param([Parameter(Mandatory)] [string] $Csv)

    if (!(Test-Path -LiteralPath $Csv)) {
        throw "CSV was not created: $Csv"
    }
    $rows = @(Import-Csv -LiteralPath $Csv -Encoding UTF8)
    if (!$rows) {
        throw "CSV did not contain any rows: $Csv"
    }
    return $rows
}

function Read-CsvPaths {
    param([Parameter(Mandatory)] [string] $Csv)

    $rows = Read-CsvRows -Csv $Csv
    if (!($rows[0].PSObject.Properties.Name -contains 'Name')) {
        throw "CSV did not contain the expected English 'Name' column. Headers: $($rows[0].PSObject.Properties.Name -join ', ')"
    }

    @($rows | ForEach-Object { Normalize-ComparePath $_.Name } | Sort-Object)
}

function Invoke-WinDirStatCsv {
    param(
        [Parameter(Mandatory)] [string] $Exe,
        [Parameter(Mandatory)] [string] $Csv,
        [Parameter(Mandatory)] [string] $Root,
        [switch] $Duplicates
    )

    if (Test-Path -LiteralPath $Csv) {
        Remove-Item -LiteralPath $Csv -Force
    }

    $flag = if ($Duplicates) { '/savedupesto' } else { '/saveto' }
    $run = Invoke-ProcessWithTimeout -FileName $Exe -Arguments @($flag, $Csv, $Root) -WorkingDirectory $runRoot
    if ($run.ExitCode -ne 0) {
        throw "WinDirStat exited with code $($run.ExitCode). StdErr: $($run.StdErr)"
    }
    if (!(Test-Path -LiteralPath $Csv)) {
        throw "WinDirStat exited successfully but did not create CSV: $Csv"
    }
    return $run
}

function New-CheckContext {
    [pscustomobject] @{
        Count = 0
        Failures = [System.Collections.Generic.List[string]]::new()
        Warnings = [System.Collections.Generic.List[string]]::new()
    }
}

function Add-Failure {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Message
    )

    [void] $Context.Failures.Add($Message)
}

function Add-Warning {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Message
    )

    [void] $Context.Warnings.Add($Message)
}

function Assert-Equal {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Name,
        [AllowNull()] [object] $Actual,
        [AllowNull()] [object] $Expected
    )

    $Context.Count++
    if ($Actual -ne $Expected) {
        Add-Failure -Context $Context -Message "$Name expected '$Expected' but was '$Actual'"
    }
}

function Assert-True {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Name,
        [bool] $Condition
    )

    $Context.Count++
    if (!$Condition) {
        Add-Failure -Context $Context -Message "$Name expected true but was false"
    }
}

function Assert-False {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Name,
        [bool] $Condition
    )

    $Context.Count++
    if ($Condition) {
        Add-Failure -Context $Context -Message "$Name expected false but was true"
    }
}

function Assert-ArrayEqual {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Name,
        [AllowNull()] [object[]] $Actual,
        [AllowNull()] [object[]] $Expected
    )

    $Context.Count++
    $actualArray = @($Actual)
    $expectedArray = @($Expected)
    if ($actualArray.Count -ne $expectedArray.Count) {
        Add-Failure -Context $Context -Message "$Name expected $($expectedArray.Count) item(s) but was $($actualArray.Count): $($actualArray -join ', ')"
        return
    }

    for ($i = 0; $i -lt $expectedArray.Count; $i++) {
        if ($actualArray[$i] -ne $expectedArray[$i]) {
            Add-Failure -Context $Context -Message "$Name[$i] expected '$($expectedArray[$i])' but was '$($actualArray[$i])'"
            return
        }
    }
}

function Assert-SetEqual {
    param(
        [Parameter(Mandatory)] [pscustomobject] $Context,
        [Parameter(Mandatory)] [string] $Name,
        [AllowNull()] [string[]] $Actual,
        [AllowNull()] [string[]] $Expected
    )

    $Context.Count++
    $actualSorted = @($Actual | Sort-Object)
    $expectedSorted = @($Expected | Sort-Object)
    $missing = @($expectedSorted | Where-Object { $_ -notin $actualSorted })
    $unexpected = @($actualSorted | Where-Object { $_ -notin $expectedSorted })
    if ($missing.Count -gt 0 -or $unexpected.Count -gt 0) {
        Add-Failure -Context $Context -Message "$Name differed. Missing: $($missing -join ', '); unexpected: $($unexpected -join ', ')"
    }
}

function Get-DefinedSettingNames {
    $optionsCpp = Join-Path $repoRoot 'windirstat\Options.cpp'
    $text = Get-Content -LiteralPath $optionsCpp -Raw
    $matches = [regex]::Matches($text, 'Setting<.+>\s+COptions::(?<name>[A-Za-z0-9_]+)\s*\(')
    @($matches | ForEach-Object { $_.Groups['name'].Value } | Sort-Object -Unique)
}

$filteringSettings = @(
    'FilteringUseRegex',
    'FilteringSizeMinimum',
    'FilteringSizeUnits',
    'FilteringExcludeDirs',
    'FilteringExcludeFiles',
    'FilteringIncludeDirs',
    'FilteringIncludeFiles'
)

$visualSettings = @(
    'AboutWindowRect',
    'ConfigPage',
    'DarkMode',
    'DriveListColumnOrder',
    'DriveListColumnWidths',
    'DriveSelectWindowRect',
    'DupeViewColumnOrder',
    'DupeViewColumnWidths',
    'ExtViewColumnOrder',
    'ExtViewColumnWidths',
    'FileTreeColor0',
    'FileTreeColor1',
    'FileTreeColor2',
    'FileTreeColor3',
    'FileTreeColor4',
    'FileTreeColor5',
    'FileTreeColor6',
    'FileTreeColor7',
    'FileTreeColorCount',
    'FileTreeColumnOrder',
    'FileTreeColumnWidths',
    'LargeToolBar',
    'ListFullRowSelection',
    'ListGrid',
    'ListStripes',
    'MainSplitterPos',
    'MainWindowPlacement',
    'MinimizeViewThreshold',
    'PacmanAnimation',
    'SearchViewColumnOrder',
    'SearchViewColumnWidths',
    'SearchWindowRect',
    'ShowFileTypes',
    'ShowStatusBar',
    'ShowTimeSpent',
    'ShowToolBar',
    'ShowTreeMap',
    'SizeProportionIndent',
    'SubSplitterPos',
    'TopViewColumnOrder',
    'TopViewColumnWidths',
    'TreeMapAmbientLightPercent',
    'TreeMapBrightness',
    'TreeMapGrid',
    'TreeMapGridColor',
    'TreeMapHeightFactor',
    'TreeMapHighlightColor',
    'TreeMapLightSourceX',
    'TreeMapLightSourceY',
    'TreeMapScaleFactor',
    'TreeMapShowExtensions',
    'TreeMapStyle',
    'TreeMapUseLogical',
    'WatcherColumnOrder',
    'WatcherColumnWidths'
)

$coveredNonVisualSettings = @(
    'AutomaticallyResizeColumns',
    'AutoElevate',
    'AutoMapDrivesWhenElevated',
    'ExcludeHiddenDirectory',
    'ExcludeHiddenFile',
    'ExcludeJunctions',
    'ExcludeProtectedDirectory',
    'ExcludeProtectedFile',
    'ExcludeSymbolicLinksDirectory',
    'ExcludeSymbolicLinksFile',
    'ExcludeVolumeMountPoints',
    'FileHashAlgorithm',
    'FolderHistoryCount',
    'FollowVolumeMountPoints',
    'LanguageId',
    'LargeFileCount',
    'ProcessHardlinks',
    'ScanForDuplicates',
    'ScanningThreads',
    'SearchCase',
    'SearchMaxResults',
    'SearchRegex',
    'SearchTerm',
    'SearchWholePhrase',
    'SelectDrivesDrives',
    'SelectDrivesFolder',
    'SelectDrivesRadio',
    'ShowColumnAttributes',
    'ShowColumnFiles',
    'ShowColumnFolders',
    'ShowColumnItems',
    'ShowColumnLastChange',
    'ShowColumnOwner',
    'ShowColumnSizeLogical',
    'ShowColumnSizePhysical',
    'ShowDeleteWarning',
    'ShowDupeDetectionCloudLinksWarning',
    'ShowElevationPrompt',
    'ShowFreeSpace',
    'ShowMicrosoftProgress',
    'ShowUnknown',
    'SkipDupeDetectionCloudLinks',
    'UseBackupRestore',
    'UseDrawTextCache',
    'UseFastScanEngine',
    'UseSizeSuffixes',
    'UseWindowsLocaleSetting'
)

function Invoke-Scenario {
    param(
        [Parameter(Mandatory)] [string] $Name,
        [Parameter(Mandatory)] [string] $Behavior,
        [Parameter(Mandatory)] [scriptblock] $Body
    )

    $context = New-CheckContext
    $commandLine = ''
    $elapsed = $null
    $errorText = $null

    try {
        $output = & $Body $context
        if ($output -and $output.PSObject.Properties.Name -contains 'CommandLine') {
            $commandLine = $output.CommandLine
        }
        if ($output -and $output.PSObject.Properties.Name -contains 'ElapsedSeconds') {
            $elapsed = $output.ElapsedSeconds
        }
    }
    catch {
        $errorText = $_.Exception.Message
        Add-Failure -Context $context -Message $errorText
    }

    $status = if ($context.Failures.Count -gt 0) { 'FAIL' } elseif ($context.Warnings.Count -gt 0) { 'WARN' } else { 'PASS' }
    [pscustomobject] @{
        Name = $Name
        Status = $status
        Behavior = $Behavior
        Checks = $context.Count
        Failures = @($context.Failures)
        Warnings = @($context.Warnings)
        CommandLine = $commandLine
        ElapsedSeconds = $elapsed
        Error = $errorText
    }
}

function Write-SuiteResultsTable {
    param([Parameter(Mandatory)] [pscustomobject[]] $Results)

    $scenarioWidth = [Math]::Max(
        8,
        @($Results | ForEach-Object { $_.Name.Length } | Measure-Object -Maximum).Maximum
    )
    $scenarioWidth = [Math]::Min($scenarioWidth, 66)
    $statusWidth = 6
    $checksWidth = 8
    $elapsedWidth = 8

    Write-ColoredLine 'Scenario results:' DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Status' $statusWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Scenario' $scenarioWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Checks' $checksWidth DarkCyan
    Write-Host -NoNewline '  '
    Write-SymbolCell 'Elapsed' $elapsedWidth DarkCyan
    Write-Host ''

    Write-Host -NoNewline '  '
    Write-ColoredLine ("".PadRight($statusWidth + $scenarioWidth + $checksWidth + $elapsedWidth + 6, '-')) DarkGray

    foreach ($result in $Results) {
        $statusColor = Get-StatusColor $result.Status
        $name = $result.Name
        if ($name.Length -gt $scenarioWidth) {
            $name = $name.Substring(0, [Math]::Max(0, $scenarioWidth - 3)) + '...'
        }
        $elapsedText = if ($null -eq $result.ElapsedSeconds) { '-' } else { "$($result.ElapsedSeconds)s" }

        Write-Host -NoNewline '  '
        Write-SymbolCell $result.Status $statusWidth $statusColor
        Write-Host -NoNewline '  '
        Write-SymbolCell $name $scenarioWidth Gray
        Write-Host -NoNewline '  '
        Write-SymbolCell ([string] $result.Checks) $checksWidth Gray
        Write-Host -NoNewline '  '
        Write-SymbolCell $elapsedText $elapsedWidth Gray
        Write-Host ''
    }
}

function Write-ScenarioSummary {
    param([Parameter(Mandatory)] [pscustomobject] $Result)

    $statusColor = Get-StatusColor $Result.Status
    $symbol = switch ($Result.Status) {
        'PASS' { $symbolPass }
        'WARN' { $symbolWarn }
        default { $symbolFail }
    }

    Write-Host ''
    Write-ColoredLine "=== $symbol $($Result.Name) ===" Cyan
    Write-LabelValue 'Result' $Result.Status $statusColor
    Write-LabelValue 'Expected behavior' $Result.Behavior
    Write-LabelValue 'Checks' $Result.Checks
    if ($Result.CommandLine) {
        Write-LabelValue 'Command' $Result.CommandLine
    }
    if ($null -ne $Result.ElapsedSeconds) {
        Write-LabelValue 'Elapsed seconds' $Result.ElapsedSeconds
    }
    if ($Result.Failures.Count -gt 0) {
        Write-ColoredLine 'Failures:' Red
        foreach ($failure in $Result.Failures) {
            Write-ColoredLine "  - $failure" Red
        }
    }
    if ($Result.Warnings.Count -gt 0) {
        Write-ColoredLine 'Warnings:' Yellow
        foreach ($warning in $Result.Warnings) {
            Write-ColoredLine "  - $warning" Yellow
        }
    }
}

function Prepare-ScanTrees {
    if (Test-Path -LiteralPath $scanRoot) {
        Clear-TestAttributes -Path $scanRoot
        Remove-Item -LiteralPath $scanRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $dupeRoot) {
        Remove-Item -LiteralPath $dupeRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $scanRoot, $dupeRoot | Out-Null

    New-TestFile -Path (Join-Path $scanRoot 'visible.txt') -Size 64
    New-TestFile -Path (Join-Path $scanRoot 'hidden-file.txt') -Size 64
    New-TestFile -Path (Join-Path $scanRoot 'protected-file.txt') -Size 64

    $hiddenDir = Join-Path $scanRoot 'hidden-dir'
    $protectedDir = Join-Path $scanRoot 'protected-dir'
    New-Item -ItemType Directory -Force -Path $hiddenDir, $protectedDir | Out-Null
    New-TestFile -Path (Join-Path $hiddenDir 'inside-hidden.txt') -Size 64
    New-TestFile -Path (Join-Path $protectedDir 'inside-protected.txt') -Size 64

    Add-FileAttributes -Path (Join-Path $scanRoot 'hidden-file.txt') -Attributes ([System.IO.FileAttributes]::Hidden)
    Add-FileAttributes -Path (Join-Path $scanRoot 'protected-file.txt') -Attributes ([System.IO.FileAttributes]::Hidden -bor [System.IO.FileAttributes]::System)
    Add-FileAttributes -Path $hiddenDir -Attributes ([System.IO.FileAttributes]::Hidden)
    Add-FileAttributes -Path $protectedDir -Attributes ([System.IO.FileAttributes]::Hidden -bor [System.IO.FileAttributes]::System)

    $dupeBytes = [byte[]]::new(2 * 1024 * 1024)
    for ($i = 0; $i -lt $dupeBytes.Length; $i++) {
        $dupeBytes[$i] = [byte] (($i % 251) + 1)
    }
    [System.IO.File]::WriteAllBytes((Join-Path $dupeRoot 'duplicate-a.bin'), $dupeBytes)
    [System.IO.File]::WriteAllBytes((Join-Path $dupeRoot 'duplicate-b.bin'), $dupeBytes)
    New-TestFile -Path (Join-Path $dupeRoot 'unique.bin') -Size (2 * 1024 * 1024) -Seed 83

    $linkInfo = [ordered] @{
        Created = $false
        FileLink = Join-Path $scanRoot 'file-link.bin'
        DirectoryLink = Join-Path $scanRoot 'dir-link'
        LinkedChild = Join-Path $scanRoot 'dir-link\target-child.txt'
    }

    $targetDir = Join-Path $scanRoot 'link-target'
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
    New-TestFile -Path (Join-Path $scanRoot 'file-link-target.bin') -Size 64 -Seed 99
    New-TestFile -Path (Join-Path $targetDir 'target-child.txt') -Size 64 -Seed 101
    try {
        New-Item -ItemType SymbolicLink -Path $linkInfo.FileLink -Target (Join-Path $scanRoot 'file-link-target.bin') -ErrorAction Stop | Out-Null
        New-Item -ItemType SymbolicLink -Path $linkInfo.DirectoryLink -Target $targetDir -ErrorAction Stop | Out-Null
        $linkInfo.Created = $true
    }
    catch {
        $linkInfo.Created = $false
        $linkInfo.Error = $_.Exception.Message
    }

    [pscustomobject] $linkInfo
}

$sourceExe = $ExePath
$suiteSucceeded = $false

try {
    if (Test-Path -LiteralPath $workRoot) {
        Remove-TestBuildArtifacts
    }
    New-Item -ItemType Directory -Force -Path $workRoot, $runRoot | Out-Null

    if (!$SkipBuild) {
        Write-ColoredLine "Copying isolated source tree to: $sourceRoot" Cyan
        Copy-SourceTreeForBuild -Source $repoRoot -Destination $sourceRoot
        Add-SettingsTestHarness -Source $sourceRoot
        $sourceExe = Build-SettingsTestExecutable
    }
    elseif ([string]::IsNullOrWhiteSpace($sourceExe)) {
        $sourceExe = Join-Path $buildRoot "$targetName.exe"
    }

    if (!(Test-Path -LiteralPath $sourceExe)) {
        throw "WinDirStat settings-test executable was not found: $sourceExe"
    }

    $testExe = Join-Path $runRoot 'WinDirStat.exe'
    Copy-Item -LiteralPath $sourceExe -Destination $testExe -Force

    $linkInfo = Prepare-ScanTrees
    if ($linkInfo.Created) {
        Write-ColoredLine 'Prepared symbolic link scan fixtures.' DarkGray
    }
    else {
        Write-ColoredLine "Symbolic link fixtures unavailable; symlink behavior will be reported as a warning. $($linkInfo.Error)" Yellow
    }

    $results = [System.Collections.Generic.List[pscustomobject]]::new()

    [void] $results.Add((Invoke-Scenario -Name 'Inventory_NonVisualSettingsClassified' -Behavior 'Every persisted setting in Options.cpp should either be covered here, covered by the filtering suite, or deliberately classified as visual/UI state.' -Body {
        param($ctx)

        $defined = @(Get-DefinedSettingNames)
        $classified = @($filteringSettings + $visualSettings + $coveredNonVisualSettings | Sort-Object -Unique)
        $unclassified = @($defined | Where-Object { $_ -notin $classified })
        $staleClassifications = @($classified | Where-Object { $_ -notin $defined })

        Assert-SetEqual -Context $ctx -Name 'Options.cpp unclassified settings' -Actual $unclassified -Expected @()
        foreach ($stale in $staleClassifications) {
            Add-Warning -Context $ctx -Message "Classification '$stale' no longer appears in Options.cpp."
        }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Defaults_LoadAndDerivedBehavior' -Behavior 'Default non-visual settings should load with expected values, default cleanup titles, and default reparse following restrictions.' -Body {
        param($ctx)

        $sections = [ordered] @{}
        $dump = Invoke-SettingsDump -Exe $testExe -Sections $sections -Name 'Defaults_LoadAndDerivedBehavior'
        $s = $dump.Dump

        Assert-Equal $ctx 'AutoMapDrivesWhenElevated' $s.AutoMapDrivesWhenElevated $true
        Assert-Equal $ctx 'ExcludeJunctions' $s.ExcludeJunctions $true
        Assert-Equal $ctx 'ExcludeSymbolicLinksDirectory' $s.ExcludeSymbolicLinksDirectory $true
        Assert-Equal $ctx 'ExcludeVolumeMountPoints' $s.ExcludeVolumeMountPoints $true
        Assert-Equal $ctx 'ExcludeHiddenDirectory' $s.ExcludeHiddenDirectory $false
        Assert-Equal $ctx 'ExcludeProtectedDirectory' $s.ExcludeProtectedDirectory $false
        Assert-Equal $ctx 'ExcludeSymbolicLinksFile' $s.ExcludeSymbolicLinksFile $true
        Assert-Equal $ctx 'ExcludeHiddenFile' $s.ExcludeHiddenFile $false
        Assert-Equal $ctx 'ExcludeProtectedFile' $s.ExcludeProtectedFile $false
        Assert-Equal $ctx 'FollowVolumeMountPoints' $s.FollowVolumeMountPoints $false
        Assert-Equal $ctx 'ScanForDuplicates' $s.ScanForDuplicates $false
        Assert-Equal $ctx 'SearchMaxResults' $s.SearchMaxResults 10000
        Assert-Equal $ctx 'ShowDeleteWarning' $s.ShowDeleteWarning $true
        Assert-Equal $ctx 'ShowElevationPrompt' $s.ShowElevationPrompt $true
        Assert-Equal $ctx 'ShowMicrosoftProgress' $s.ShowMicrosoftProgress $false
        Assert-Equal $ctx 'SkipDupeDetectionCloudLinks' $s.SkipDupeDetectionCloudLinks $true
        Assert-Equal $ctx 'ShowDupeDetectionCloudLinksWarning' $s.ShowDupeDetectionCloudLinksWarning $true
        Assert-Equal $ctx 'AutoElevate' $s.AutoElevate $false
        Assert-Equal $ctx 'UseBackupRestore' $s.UseBackupRestore $true
        Assert-Equal $ctx 'UseDrawTextCache' $s.UseDrawTextCache $true
        Assert-Equal $ctx 'UseFastScanEngine' $s.UseFastScanEngine $true
        Assert-Equal $ctx 'UseWindowsLocaleSetting' $s.UseWindowsLocaleSetting $true
        Assert-Equal $ctx 'ProcessHardlinks' $s.ProcessHardlinks $true
        Assert-Equal $ctx 'FileHashAlgorithm' $s.FileHashAlgorithm 4
        Assert-Equal $ctx 'LargeFileCount' $s.LargeFileCount 50
        Assert-Equal $ctx 'ScanningThreads' $s.ScanningThreads 4
        Assert-Equal $ctx 'SelectDrivesRadio' $s.SelectDrivesRadio 0
        Assert-Equal $ctx 'FolderHistoryCount' $s.FolderHistoryCount 10
        Assert-True $ctx 'LanguageId is available' ([int] $s.LanguageId -in @($s.LanguageList))
        Assert-Equal $ctx 'Reparse none follows' $s.ReparseFollowing.None $true
        Assert-Equal $ctx 'Reparse mount default blocked' $s.ReparseFollowing.MountPoint $false
        Assert-Equal $ctx 'Reparse symlink default blocked' $s.ReparseFollowing.SymbolicLink $false
        Assert-Equal $ctx 'Reparse junction default blocked' $s.ReparseFollowing.Junction $false
        Assert-Equal $ctx 'Reparse other default follows' $s.ReparseFollowing.OtherReparsePoint $true
        Assert-Equal $ctx 'UserDefinedCleanups count' @($s.UserDefinedCleanups).Count 10
        Assert-True $ctx 'Default cleanup 0 title populated' (![string]::IsNullOrWhiteSpace($s.UserDefinedCleanups[0].Title))
        Assert-Equal $ctx 'Default cleanup 0 enabled' $s.UserDefinedCleanups[0].Enabled $false
        Assert-Equal $ctx 'Default cleanup 0 refresh policy' $s.UserDefinedCleanups[0].RefreshPolicy 0

        $dump
    }))

    [void] $results.Add((Invoke-Scenario -Name 'ExplicitValues_LoadExactly' -Behavior 'Every non-visual setting with a stable direct representation should load from portable INI, including strings, vectors, and cleanup definitions.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        foreach ($entry in ([ordered] @{
            AutomaticallyResizeColumns = 0
            AutoMapDrivesWhenElevated = 0
            ExcludeJunctions = 0
            ExcludeSymbolicLinksDirectory = 0
            ExcludeVolumeMountPoints = 0
            ExcludeHiddenDirectory = 1
            ExcludeProtectedDirectory = 1
            ExcludeSymbolicLinksFile = 0
            ExcludeHiddenFile = 1
            ExcludeProtectedFile = 1
            FollowVolumeMountPoints = 1
            UseSizeSuffixes = 0
            ShowDeleteWarning = 0
            ShowElevationPrompt = 0
            ShowMicrosoftProgress = 1
            ShowFreeSpace = 1
            ShowUnknown = 1
            SkipDupeDetectionCloudLinks = 0
            ShowDupeDetectionCloudLinksWarning = 0
            AutoElevate = 1
            UseBackupRestore = 0
            UseDrawTextCache = 0
            UseFastScanEngine = 0
            UseWindowsLocaleSetting = 0
            ProcessHardlinks = 0
            FileHashAlgorithm = 2
            LargeFileCount = 123
            MinimizeViewThreshold = 42
            ScanningThreads = 7
        }).GetEnumerator()) {
            Set-IniValue $sections 'Options' $entry.Key $entry.Value
        }
        Set-IniValue $sections 'DupeView' 'ScanForDuplicates' 1
        Set-IniValue $sections 'DriveSelect' 'SelectDrivesRadio' 2
        Set-IniValue $sections 'DriveSelect' 'FolderHistoryCount' 3
        Set-IniValue $sections 'DriveSelect' 'SelectDrivesDrives' 'C:\|D:\'
        Set-IniValue $sections 'DriveSelect' 'SelectDrivesFolder' 'C:\Alpha|\\server\share\Beta'
        Set-IniValue $sections 'SearchView' 'SearchWholePhrase' 1
        Set-IniValue $sections 'SearchView' 'SearchRegex' 1
        Set-IniValue $sections 'SearchView' 'SearchCase' 1
        Set-IniValue $sections 'SearchView' 'SearchMaxResults' 321
        Set-IniValue $sections 'SearchView' 'SearchTerm' "alpha${recordSeparator}beta"
        $sections['Cleanups\UserDefinedCleanup00'] = [ordered] @{
            Title = 'Custom cleanup'
            CommandLine = "echo %p${recordSeparator}echo %sn"
            Enable = 1
            VirginTitle = 0
            WorksForDrives = 1
            WorksForDirectories = 1
            WorksForFiles = 1
            WorksForUncPaths = 1
            RecurseIntoSubdirectories = 1
            AskForConfirmation = 1
            ShowConsoleWindow = 1
            WaitForCompletion = 1
            RefreshPolicy = 2
        }
        $sections['Cleanups\UserDefinedCleanup01'] = [ordered] @{
            Title = ''
            VirginTitle = 1
        }

        $dump = Invoke-SettingsDump -Exe $testExe -Sections $sections -Name 'ExplicitValues_LoadExactly'
        $s = $dump.Dump

        Assert-Equal $ctx 'AutoMapDrivesWhenElevated' $s.AutoMapDrivesWhenElevated $false
        Assert-Equal $ctx 'ExcludeJunctions' $s.ExcludeJunctions $false
        Assert-Equal $ctx 'ExcludeSymbolicLinksDirectory' $s.ExcludeSymbolicLinksDirectory $false
        Assert-Equal $ctx 'ExcludeVolumeMountPoints' $s.ExcludeVolumeMountPoints $false
        Assert-Equal $ctx 'ExcludeHiddenDirectory' $s.ExcludeHiddenDirectory $true
        Assert-Equal $ctx 'ExcludeProtectedDirectory' $s.ExcludeProtectedDirectory $true
        Assert-Equal $ctx 'ExcludeSymbolicLinksFile' $s.ExcludeSymbolicLinksFile $false
        Assert-Equal $ctx 'ExcludeHiddenFile' $s.ExcludeHiddenFile $true
        Assert-Equal $ctx 'ExcludeProtectedFile' $s.ExcludeProtectedFile $true
        Assert-Equal $ctx 'FollowVolumeMountPoints' $s.FollowVolumeMountPoints $true
        Assert-Equal $ctx 'ScanForDuplicates' $s.ScanForDuplicates $true
        Assert-Equal $ctx 'ShowDeleteWarning' $s.ShowDeleteWarning $false
        Assert-Equal $ctx 'ShowElevationPrompt' $s.ShowElevationPrompt $false
        Assert-Equal $ctx 'ShowMicrosoftProgress' $s.ShowMicrosoftProgress $true
        Assert-Equal $ctx 'ShowFreeSpace' $s.ShowFreeSpace $true
        Assert-Equal $ctx 'ShowUnknown' $s.ShowUnknown $true
        Assert-Equal $ctx 'SkipDupeDetectionCloudLinks' $s.SkipDupeDetectionCloudLinks $false
        Assert-Equal $ctx 'ShowDupeDetectionCloudLinksWarning' $s.ShowDupeDetectionCloudLinksWarning $false
        Assert-Equal $ctx 'AutoElevate' $s.AutoElevate $true
        Assert-Equal $ctx 'UseBackupRestore' $s.UseBackupRestore $false
        Assert-Equal $ctx 'UseDrawTextCache' $s.UseDrawTextCache $false
        Assert-Equal $ctx 'UseFastScanEngine' $s.UseFastScanEngine $false
        Assert-Equal $ctx 'UseWindowsLocaleSetting' $s.UseWindowsLocaleSetting $false
        Assert-Equal $ctx 'ProcessHardlinks' $s.ProcessHardlinks $false
        Assert-Equal $ctx 'FileHashAlgorithm' $s.FileHashAlgorithm 2
        Assert-Equal $ctx 'LargeFileCount' $s.LargeFileCount 123
        Assert-Equal $ctx 'MinimizeViewThreshold' $s.MinimizeViewThreshold 42
        Assert-Equal $ctx 'ScanningThreads' $s.ScanningThreads 7
        Assert-Equal $ctx 'SelectDrivesRadio' $s.SelectDrivesRadio 2
        Assert-Equal $ctx 'FolderHistoryCount' $s.FolderHistoryCount 3
        Assert-ArrayEqual $ctx 'SelectDrivesDrives' @($s.SelectDrivesDrives) @('C:\', 'D:\')
        Assert-ArrayEqual $ctx 'SelectDrivesFolder' @($s.SelectDrivesFolder) @('C:\Alpha', '\\server\share\Beta')
        Assert-Equal $ctx 'SearchWholePhrase' $s.SearchWholePhrase $true
        Assert-Equal $ctx 'SearchRegex' $s.SearchRegex $true
        Assert-Equal $ctx 'SearchCase' $s.SearchCase $true
        Assert-Equal $ctx 'SearchMaxResults' $s.SearchMaxResults 321
        Assert-Equal $ctx 'SearchTerm record separator decoding' $s.SearchTerm "alpha`r`nbeta"
        Assert-Equal $ctx 'Cleanup 00 title' $s.UserDefinedCleanups[0].Title 'Custom cleanup'
        Assert-Equal $ctx 'Cleanup 00 command line' $s.UserDefinedCleanups[0].CommandLine "echo %p`r`necho %sn"
        Assert-Equal $ctx 'Cleanup 00 enabled' $s.UserDefinedCleanups[0].Enabled $true
        Assert-Equal $ctx 'Cleanup 00 works for drives' $s.UserDefinedCleanups[0].WorksForDrives $true
        Assert-Equal $ctx 'Cleanup 00 works for directories' $s.UserDefinedCleanups[0].WorksForDirectories $true
        Assert-Equal $ctx 'Cleanup 00 works for files' $s.UserDefinedCleanups[0].WorksForFiles $true
        Assert-Equal $ctx 'Cleanup 00 works for UNC paths' $s.UserDefinedCleanups[0].WorksForUncPaths $true
        Assert-Equal $ctx 'Cleanup 00 recurse' $s.UserDefinedCleanups[0].RecurseIntoSubdirectories $true
        Assert-Equal $ctx 'Cleanup 00 ask' $s.UserDefinedCleanups[0].AskForConfirmation $true
        Assert-Equal $ctx 'Cleanup 00 console' $s.UserDefinedCleanups[0].ShowConsoleWindow $true
        Assert-Equal $ctx 'Cleanup 00 wait' $s.UserDefinedCleanups[0].WaitForCompletion $true
        Assert-Equal $ctx 'Cleanup 00 refresh policy' $s.UserDefinedCleanups[0].RefreshPolicy 2
        Assert-True $ctx 'Cleanup 01 virgin title localized' (![string]::IsNullOrWhiteSpace($s.UserDefinedCleanups[1].Title))
        Assert-True $ctx 'Cleanup 01 title replaced' ($s.UserDefinedCleanups[1].Title -ne '')
        Assert-Equal $ctx 'Reparse mount allowed' $s.ReparseFollowing.MountPoint $true
        Assert-Equal $ctx 'Reparse symlink allowed' $s.ReparseFollowing.SymbolicLink $true
        Assert-Equal $ctx 'Reparse junction allowed' $s.ReparseFollowing.Junction $true

        $dump
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Bounds_ClampLowValues' -Behavior 'Out-of-range low numeric settings should clamp to their declared minimums instead of poisoning runtime state.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'FileHashAlgorithm' -99
        Set-IniValue $sections 'Options' 'LargeFileCount' -99
        Set-IniValue $sections 'Options' 'MinimizeViewThreshold' -99
        Set-IniValue $sections 'Options' 'ScanningThreads' -99
        Set-IniValue $sections 'Options' 'DarkMode' -99
        Set-IniValue $sections 'DriveSelect' 'SelectDrivesRadio' -99
        Set-IniValue $sections 'DriveSelect' 'FolderHistoryCount' -99
        Set-IniValue $sections 'SearchView' 'SearchMaxResults' -99

        $dump = Invoke-SettingsDump -Exe $testExe -Sections $sections -Name 'Bounds_ClampLowValues'
        $s = $dump.Dump

        Assert-Equal $ctx 'FileHashAlgorithm minimum' $s.FileHashAlgorithm 0
        Assert-Equal $ctx 'LargeFileCount minimum' $s.LargeFileCount 0
        Assert-Equal $ctx 'MinimizeViewThreshold minimum' $s.MinimizeViewThreshold 1
        Assert-Equal $ctx 'ScanningThreads minimum' $s.ScanningThreads 1
        Assert-Equal $ctx 'DarkMode minimum' $s.DarkMode 0
        Assert-Equal $ctx 'SelectDrivesRadio minimum' $s.SelectDrivesRadio 0
        Assert-Equal $ctx 'FolderHistoryCount minimum' $s.FolderHistoryCount 0
        Assert-Equal $ctx 'SearchMaxResults minimum' $s.SearchMaxResults 1

        $dump
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Bounds_ClampHighValues' -Behavior 'Out-of-range high numeric settings should clamp to their declared maximums.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'FileHashAlgorithm' 99
        Set-IniValue $sections 'Options' 'LargeFileCount' 999999
        Set-IniValue $sections 'Options' 'MinimizeViewThreshold' 999999
        Set-IniValue $sections 'Options' 'ScanningThreads' 999999
        Set-IniValue $sections 'Options' 'DarkMode' 99
        Set-IniValue $sections 'DriveSelect' 'SelectDrivesRadio' 99
        Set-IniValue $sections 'DriveSelect' 'FolderHistoryCount' 999999
        Set-IniValue $sections 'SearchView' 'SearchMaxResults' 9999999

        $dump = Invoke-SettingsDump -Exe $testExe -Sections $sections -Name 'Bounds_ClampHighValues'
        $s = $dump.Dump

        Assert-Equal $ctx 'FileHashAlgorithm maximum' $s.FileHashAlgorithm 4
        Assert-Equal $ctx 'LargeFileCount maximum' $s.LargeFileCount 10000
        Assert-Equal $ctx 'MinimizeViewThreshold maximum' $s.MinimizeViewThreshold 10000
        Assert-Equal $ctx 'ScanningThreads maximum' $s.ScanningThreads 16
        Assert-Equal $ctx 'DarkMode maximum' $s.DarkMode 2
        Assert-Equal $ctx 'SelectDrivesRadio maximum' $s.SelectDrivesRadio 2
        Assert-Equal $ctx 'FolderHistoryCount maximum' $s.FolderHistoryCount 100
        Assert-Equal $ctx 'SearchMaxResults maximum' $s.SearchMaxResults 1000000

        $dump
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Locale_UsesConfiguredLanguageWhenRequested' -Behavior 'Formatting locale should use the configured language when UseWindowsLocaleSetting is disabled, and the user default LCID when enabled.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'LanguageId' 7
        Set-IniValue $sections 'Options' 'UseWindowsLocaleSetting' 0
        $configured = Invoke-SettingsDump -Exe $testExe -Sections $sections -Name 'Locale_ConfiguredLanguage'
        Assert-Equal $ctx 'Configured locale LCID' $configured.Dump.LocaleForFormatting 7
        Assert-Equal $ctx 'Configured language id' $configured.Dump.LanguageId 7

        Set-IniValue $sections 'Options' 'UseWindowsLocaleSetting' 1
        $windows = Invoke-SettingsDump -Exe $testExe -Sections $sections -Name 'Locale_WindowsDefault'
        Assert-Equal $ctx 'Windows locale sentinel' $windows.Dump.LocaleForFormatting 1024

        [pscustomobject] @{
            CommandLine = $windows.CommandLine
            ElapsedSeconds = [math]::Round($configured.ElapsedSeconds + $windows.ElapsedSeconds, 3)
        }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'SearchSettings_DriveProbeMatching' -Behavior 'Search term, regex mode, case sensitivity, and whole-phrase settings should combine into the same probe matching behavior the app uses for searches.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'SearchView' 'SearchTerm' '*.LOG'
        Set-IniValue $sections 'SearchView' 'SearchRegex' 0
        Set-IniValue $sections 'SearchView' 'SearchCase' 0
        Set-IniValue $sections 'SearchView' 'SearchWholePhrase' 1
        $glob = Invoke-SettingsDump -Exe $testExe -Sections $sections -Name 'SearchSettings_GlobWholeCaseInsensitive'
        Assert-Equal $ctx 'Glob lower match' $glob.Dump.SearchProbeMatches.LowerLog $true
        Assert-Equal $ctx 'Glob upper match' $glob.Dump.SearchProbeMatches.UpperLog $true
        Assert-Equal $ctx 'Glob notes no match' $glob.Dump.SearchProbeMatches.NotesTxt $false

        Set-IniValue $sections 'SearchView' 'SearchTerm' '^Alpha\.LOG$'
        Set-IniValue $sections 'SearchView' 'SearchRegex' 1
        Set-IniValue $sections 'SearchView' 'SearchCase' 1
        Set-IniValue $sections 'SearchView' 'SearchWholePhrase' 1
        $regex = Invoke-SettingsDump -Exe $testExe -Sections $sections -Name 'SearchSettings_RegexWholeCaseSensitive'
        Assert-Equal $ctx 'Regex lower no match' $regex.Dump.SearchProbeMatches.LowerLog $false
        Assert-Equal $ctx 'Regex upper match' $regex.Dump.SearchProbeMatches.UpperLog $true

        Set-IniValue $sections 'SearchView' 'SearchTerm' 'target'
        Set-IniValue $sections 'SearchView' 'SearchRegex' 0
        Set-IniValue $sections 'SearchView' 'SearchCase' 0
        Set-IniValue $sections 'SearchView' 'SearchWholePhrase' 0
        $partial = Invoke-SettingsDump -Exe $testExe -Sections $sections -Name 'SearchSettings_Partial'
        Assert-Equal $ctx 'Partial search match' $partial.Dump.SearchProbeMatches.TargetSubstring $true

        [pscustomobject] @{
            CommandLine = $partial.CommandLine
            ElapsedSeconds = [math]::Round($glob.ElapsedSeconds + $regex.ElapsedSeconds + $partial.ElapsedSeconds, 3)
        }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Csv_AttributeExclusionSettings' -Behavior 'Hidden and protected file/directory exclusion settings should remove the correct paths from a real non-interactive scan.' -Body {
        param($ctx)

        $allExpected = @(
            $scanRoot,
            (Join-Path $scanRoot 'visible.txt'),
            (Join-Path $scanRoot 'hidden-file.txt'),
            (Join-Path $scanRoot 'protected-file.txt'),
            (Join-Path $scanRoot 'hidden-dir'),
            (Join-Path $scanRoot 'hidden-dir\inside-hidden.txt'),
            (Join-Path $scanRoot 'protected-dir'),
            (Join-Path $scanRoot 'protected-dir\inside-protected.txt'),
            (Join-Path $scanRoot 'link-target'),
            (Join-Path $scanRoot 'link-target\target-child.txt'),
            (Join-Path $scanRoot 'file-link-target.bin')
        ) | ForEach-Object { Normalize-ComparePath $_ }

        $sections = New-BaseIniSections
        $csv = Join-Path $workRoot 'attributes-all.csv'
        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $allRun = Invoke-WinDirStatCsv -Exe $testExe -Csv $csv -Root $scanRoot
        $allPaths = @(Read-CsvPaths -Csv $csv | Where-Object {
            Test-PathUnder -Path $_ -Root $scanRoot
        })
        foreach ($path in $allExpected) {
            Assert-True $ctx "All attributes present: $path" ($path -in $allPaths)
        }

        Set-IniValue $sections 'Options' 'ExcludeHiddenFile' 1
        Set-IniValue $sections 'Options' 'ExcludeProtectedFile' 1
        Set-IniValue $sections 'Options' 'ExcludeHiddenDirectory' 1
        Set-IniValue $sections 'Options' 'ExcludeProtectedDirectory' 1
        $csv = Join-Path $workRoot 'attributes-excluded.csv'
        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $excludedRun = Invoke-WinDirStatCsv -Exe $testExe -Csv $csv -Root $scanRoot
        $excludedPaths = @(Read-CsvPaths -Csv $csv)
        Assert-True $ctx 'Visible file remains' ((Normalize-ComparePath (Join-Path $scanRoot 'visible.txt')) -in $excludedPaths)
        Assert-False $ctx 'Hidden file omitted' ((Normalize-ComparePath (Join-Path $scanRoot 'hidden-file.txt')) -in $excludedPaths)
        Assert-False $ctx 'Protected file omitted' ((Normalize-ComparePath (Join-Path $scanRoot 'protected-file.txt')) -in $excludedPaths)
        Assert-False $ctx 'Hidden dir omitted' ((Normalize-ComparePath (Join-Path $scanRoot 'hidden-dir')) -in $excludedPaths)
        Assert-False $ctx 'Protected dir omitted' ((Normalize-ComparePath (Join-Path $scanRoot 'protected-dir')) -in $excludedPaths)

        [pscustomobject] @{
            CommandLine = $excludedRun.CommandLine
            ElapsedSeconds = [math]::Round($allRun.ElapsedSeconds + $excludedRun.ElapsedSeconds, 3)
        }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Csv_SymbolicLinkSettings' -Behavior 'Directory symlink following and file symlink exclusion should be enforced in a real scan when symbolic links are available.' -Body {
        param($ctx)

        if (!$linkInfo.Created) {
            Add-Warning -Context $ctx -Message "Symbolic link fixtures could not be created on this machine: $($linkInfo.Error)"
            return
        }

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 0
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile' 0
        Set-IniValue $sections 'Options' 'ExcludeJunctions' 1
        Set-IniValue $sections 'Options' 'ExcludeVolumeMountPoints' 1
        $csv = Join-Path $workRoot 'symlinks-follow.csv'
        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $followRun = Invoke-WinDirStatCsv -Exe $testExe -Csv $csv -Root $scanRoot
        $followPaths = @(Read-CsvPaths -Csv $csv)
        Assert-True $ctx 'File symlink included when allowed' ((Normalize-ComparePath $linkInfo.FileLink) -in $followPaths)
        Assert-True $ctx 'Directory symlink child included when following allowed' ((Normalize-ComparePath $linkInfo.LinkedChild) -in $followPaths)

        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksDirectory' 1
        Set-IniValue $sections 'Options' 'ExcludeSymbolicLinksFile' 1
        $csv = Join-Path $workRoot 'symlinks-excluded.csv'
        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $excludeRun = Invoke-WinDirStatCsv -Exe $testExe -Csv $csv -Root $scanRoot
        $excludePaths = @(Read-CsvPaths -Csv $csv)
        Assert-False $ctx 'File symlink omitted when excluded' ((Normalize-ComparePath $linkInfo.FileLink) -in $excludePaths)
        Assert-True $ctx 'Directory symlink node still visible' ((Normalize-ComparePath $linkInfo.DirectoryLink) -in $excludePaths)
        Assert-False $ctx 'Directory symlink child omitted when not following' ((Normalize-ComparePath $linkInfo.LinkedChild) -in $excludePaths)

        [pscustomobject] @{
            CommandLine = $excludeRun.CommandLine
            ElapsedSeconds = [math]::Round($followRun.ElapsedSeconds + $excludeRun.ElapsedSeconds, 3)
        }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Csv_OwnerColumnSetting' -Behavior 'ShowColumnOwner should add the Owner column to the non-interactive CSV export, and disabling it should remove that column.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'FileTreeView' 'ShowColumnOwner' 0
        $csv = Join-Path $workRoot 'owner-off.csv'
        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $offRun = Invoke-WinDirStatCsv -Exe $testExe -Csv $csv -Root $scanRoot
        $offHeaders = @((Read-CsvRows -Csv $csv)[0].PSObject.Properties.Name)
        Assert-False $ctx 'Owner header absent when disabled' ('Owner' -in $offHeaders)

        Set-IniValue $sections 'FileTreeView' 'ShowColumnOwner' 1
        $csv = Join-Path $workRoot 'owner-on.csv'
        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $onRun = Invoke-WinDirStatCsv -Exe $testExe -Csv $csv -Root $scanRoot
        $onHeaders = @((Read-CsvRows -Csv $csv)[0].PSObject.Properties.Name)
        Assert-True $ctx 'Owner header present when enabled' ('Owner' -in $onHeaders)

        [pscustomobject] @{
            CommandLine = $onRun.CommandLine
            ElapsedSeconds = [math]::Round($offRun.ElapsedSeconds + $onRun.ElapsedSeconds, 3)
        }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Duplicates_FileHashAlgorithm' -Behavior 'Each duplicate hash algorithm setting should produce hashes with the expected width in non-interactive duplicate CSV output.' -Body {
        param($ctx)

        $expectations = @(
            @{ Algorithm = 0; Name = 'MD5' }
            @{ Algorithm = 1; Name = 'SHA1' }
            @{ Algorithm = 2; Name = 'SHA256' }
            @{ Algorithm = 3; Name = 'SHA384' }
            @{ Algorithm = 4; Name = 'SHA512' }
        )
        $elapsed = 0.0
        $lastCommand = ''
        $referenceFile = Join-Path $dupeRoot 'duplicate-a.bin'

        foreach ($expectation in $expectations) {
            $expectedPrefix = (Get-FileHash -LiteralPath $referenceFile -Algorithm $expectation.Name).Hash.Substring(0, 32).ToLowerInvariant()
            $sections = New-BaseIniSections
            Set-IniValue $sections 'Options' 'FileHashAlgorithm' $expectation.Algorithm
            Set-IniValue $sections 'Options' 'UseFastScanEngine' 0
            Set-IniValue $sections 'DupeView' 'ScanForDuplicates' 1
            $csv = Join-Path $workRoot "dupes-$($expectation.Name).csv"
            Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
            $run = Invoke-WinDirStatCsv -Exe $testExe -Csv $csv -Root $dupeRoot -Duplicates
            $elapsed += $run.ElapsedSeconds
            $lastCommand = $run.CommandLine
            $rows = @(Read-CsvRows -Csv $csv)
            Assert-Equal $ctx "$($expectation.Name) duplicate row count" $rows.Count 2
            foreach ($row in $rows) {
                Assert-Equal $ctx "$($expectation.Name) hash prefix length for $($row.Name)" $row.'Hash Prefix'.Length 32
                Assert-Equal $ctx "$($expectation.Name) hash prefix for $($row.Name)" $row.'Hash Prefix'.ToLowerInvariant() $expectedPrefix
            }
        }

        [pscustomobject] @{
            CommandLine = $lastCommand
            ElapsedSeconds = [math]::Round($elapsed, 3)
        }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Json_Results_ValidJsonAndStructure' -Behavior 'Saving scan results to a .json path should produce valid JSON: an array of objects with a required set of properties, hex-formatted WinDirStat Attributes and Index fields, and ISO-8601 Last Change timestamps.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        $jsonPath = Join-Path $workRoot 'json-results-structure.json'
        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $run = Invoke-WinDirStatCsv -Exe $testExe -Csv $jsonPath -Root $scanRoot

        $rawText = Get-Content -LiteralPath $jsonPath -Raw -Encoding UTF8
        $items = @($rawText | ConvertFrom-Json)

        Assert-True $ctx 'JSON array is non-empty' ($items.Count -gt 0)

        $requiredProps = @('Name', 'Files', 'Folders', 'Logical Size', 'Physical Size', 'Attributes', 'Last Change', 'WinDirStat Attributes', 'Index')
        foreach ($prop in $requiredProps) {
            Assert-True $ctx "First item has '$prop' property" ($null -ne $items[0].PSObject.Properties[$prop])
        }

        $scanRootNorm = Normalize-ComparePath $scanRoot
        $jsonNames = @($items | ForEach-Object { Normalize-ComparePath $_.Name })
        Assert-True $ctx 'Scan root appears in results' ($scanRootNorm -in $jsonNames)

        $badWdsAttr = @($items | Where-Object { $_.'WinDirStat Attributes' -notmatch '^0x[0-9A-Fa-f]{8}$' })
        Assert-True $ctx 'All WinDirStat Attributes are 0x-prefixed hex' ($badWdsAttr.Count -eq 0)

        $badIndex = @($items | Where-Object { $_.Index -notmatch '^0x[0-9A-Fa-f]{16}$' })
        Assert-True $ctx 'All Index values are 0x-prefixed hex' ($badIndex.Count -eq 0)

        $badTimestamp = @($items | Where-Object {
            $v = $_.'Last Change'
            if ($v -is [datetime]) { return $false }
            $v -notmatch '^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)?$'
        })
        Assert-True $ctx 'All Last Change values are ISO-8601 UTC timestamps or empty' ($badTimestamp.Count -eq 0)

        $badFiles = @($items | Where-Object { $_.'Files' -is [string] })
        Assert-True $ctx 'All Files values are numeric' ($badFiles.Count -eq 0)

        $badFolders = @($items | Where-Object { $_.'Folders' -is [string] })
        Assert-True $ctx 'All Folders values are numeric' ($badFolders.Count -eq 0)

        [pscustomobject] @{ CommandLine = $run.CommandLine; ElapsedSeconds = $run.ElapsedSeconds }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Json_Results_ContentMatchesCsv' -Behavior 'Saving the same scan to .json and .csv should yield the same set of file-system paths in both outputs.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        $csvPath = Join-Path $workRoot 'json-vs-csv.csv'
        $jsonPath = Join-Path $workRoot 'json-vs-csv.json'

        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $csvRun = Invoke-WinDirStatCsv -Exe $testExe -Csv $csvPath -Root $scanRoot

        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $jsonRun = Invoke-WinDirStatCsv -Exe $testExe -Csv $jsonPath -Root $scanRoot

        $csvPaths = @(Read-CsvPaths -Csv $csvPath | Where-Object { Test-PathUnder -Path $_ -Root $scanRoot })

        $jsonItems = @(Get-Content -LiteralPath $jsonPath -Raw -Encoding UTF8 | ConvertFrom-Json)
        $jsonPaths = @($jsonItems | ForEach-Object {
            try { $n = Normalize-ComparePath $_.Name; if (Test-PathUnder -Path $n -Root $scanRoot) { $n } } catch {}
        } | Where-Object { $_ })

        Assert-SetEqual $ctx 'JSON and CSV report the same paths' -Actual $jsonPaths -Expected $csvPaths

        [pscustomobject] @{
            CommandLine = $jsonRun.CommandLine
            ElapsedSeconds = [math]::Round($csvRun.ElapsedSeconds + $jsonRun.ElapsedSeconds, 3)
        }
    }))

    [void] $results.Add((Invoke-Scenario -Name 'Json_Duplicates_ValidJsonAndStructure' -Behavior 'Saving duplicate results to a .json path should produce valid JSON: an array where the two paired duplicates share a Hash Prefix and all entries carry the expected typed fields.' -Body {
        param($ctx)

        $sections = New-BaseIniSections
        Set-IniValue $sections 'Options' 'UseFastScanEngine' 0
        Set-IniValue $sections 'DupeView' 'ScanForDuplicates' 1
        $jsonPath = Join-Path $workRoot 'json-dupes-structure.json'
        Write-PortableIni -Path (Join-Path $runRoot 'WinDirStat.ini') -Sections $sections
        $run = Invoke-WinDirStatCsv -Exe $testExe -Csv $jsonPath -Root $dupeRoot -Duplicates

        $rawText = Get-Content -LiteralPath $jsonPath -Raw -Encoding UTF8
        $items = @($rawText | ConvertFrom-Json)

        Assert-Equal $ctx 'Duplicate JSON entry count' $items.Count 2

        $requiredDupeProps = @('Hash Prefix', 'Name', 'Logical Size', 'Physical Size', 'Last Change', 'Attributes')
        if ($items.Count -gt 0) {
            foreach ($prop in $requiredDupeProps) {
                Assert-True $ctx "Dupe entry has '$prop' property" ($null -ne $items[0].PSObject.Properties[$prop])
            }
        }

        if ($items.Count -eq 2) {
            Assert-Equal $ctx 'Duplicates share the same Hash Prefix' $items[0].'Hash Prefix' $items[1].'Hash Prefix'
            Assert-True $ctx 'Hash Prefix is non-empty' (![string]::IsNullOrEmpty($items[0].'Hash Prefix'))
        }

        $badTimestamp = @($items | Where-Object {
            $v = $_.'Last Change'
            if ($v -is [datetime]) { return $false }
            $v -notmatch '^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)?$'
        })
        Assert-True $ctx 'All dupe Last Change values are ISO-8601 UTC timestamps or empty' ($badTimestamp.Count -eq 0)

        $badSizes = @($items | Where-Object { $_.'Logical Size' -is [string] })
        Assert-True $ctx 'All dupe Logical Size values are numeric' ($badSizes.Count -eq 0)

        [pscustomobject] @{ CommandLine = $run.CommandLine; ElapsedSeconds = $run.ElapsedSeconds }
    }))

    $failed = @($results | Where-Object { $_.Status -eq 'FAIL' })
    $warned = @($results | Where-Object { $_.Status -eq 'WARN' })
    Write-Host ''
    Write-ColoredLine '=== Suite Summary ===' Cyan
    Write-LabelValue 'Scenarios run' $results.Count
    Write-LabelValue 'Passed' ($results.Count - $failed.Count - $warned.Count) Green
    Write-LabelValue 'Warned' $warned.Count $(if ($warned.Count -eq 0) { 'Green' } else { 'Yellow' })
    Write-LabelValue 'Failed' $failed.Count $(if ($failed.Count -eq 0) { 'Green' } else { 'Red' })
    Write-SuiteResultsTable -Results @($results)

    if ($Details) {
        Write-ColoredLine 'Scenario details:' Cyan
        foreach ($result in $results) {
            Write-ScenarioSummary -Result $result
        }
    }
    elseif ($failed.Count -gt 0 -or $warned.Count -gt 0) {
        if ($failed.Count -gt 0) {
            Write-ColoredLine 'Failed scenario details:' Red
            foreach ($result in $failed) {
                Write-ScenarioSummary -Result $result
            }
        }
        if ($warned.Count -gt 0) {
            Write-ColoredLine 'Warning scenario details:' Yellow
            foreach ($result in $warned) {
                Write-ScenarioSummary -Result $result
            }
        }
    }

    if ($failed.Count -gt 0) {
        throw "Non-visual settings suite failed in $($failed.Count) scenario(s): $(@($failed | ForEach-Object { $_.Name }) -join ', ')"
    }

    Write-ColoredLine 'Non-visual settings suite passed.' Green
    $suiteSucceeded = $true
}
finally {
    if (!$KeepArtifacts) {
        Remove-TestBuildArtifacts
    }
    elseif (Test-Path -LiteralPath $workRoot) {
        Write-ColoredLine "Kept test artifacts in: $workRoot" Yellow
    }

    if (!$suiteSucceeded) {
        Write-ColoredLine 'Non-visual settings suite failed.' Red
    }
}
