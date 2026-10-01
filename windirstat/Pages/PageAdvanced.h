// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "PageShared.h"

//
// CPageAdvanced. "Settings" property page "Advanced".
//
class CPageAdvanced final : public MessageTarget<CPageAdvanced, CSettingsPage>
{
public:
    enum : std::uint8_t { IDD = IDD_PAGE_ADVANCED };

    CPageAdvanced();
    ~CPageAdvanced() override = default;

protected:
    void InitializePage() override;
    std::optional<ValidationError> PrepareSettings() override;

    static constexpr std::array CheckboxBindings
    {
        &COptions::ExcludeVolumeMountPoints,
        &COptions::ExcludeJunctions,
        &COptions::ExcludeSymbolicLinksDirectory,
        &COptions::SkipDupeDetectionCloudLinks,
        &COptions::ExcludeHiddenDirectory,
        &COptions::ExcludeProtectedDirectory,
        &COptions::ExcludeDropboxIgnored,
        &COptions::UseBackupRestore,
        &COptions::ExcludeSymbolicLinksFile,
        &COptions::ExcludeHiddenFile,
        &COptions::ExcludeProtectedFile,
        &COptions::ProcessHardlinks,
        &COptions::SampleLargeFiles,
    };

    static constexpr std::array NumberBindings
    {
        &COptions::LargeFileCount, &COptions::FolderHistoryCount, &COptions::ProcessPriority,
        &COptions::ScanningThreads, &COptions::FileHashAlgorithm,
    };

public:
    static std::span<const RouteEntry> Routes();

protected:
    CComboBox m_priorityCombo;

    void OnBnClickedResetPreferences();
};

inline std::span<const RouteEntry> CPageAdvanced::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_BACKUP_RESTORE),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_EXCLUDE_DROPBOX_IGNORED, IDC_EXCLUDE_VOLUME_MOUNT_POINTS),
        Route::Control<&OnSettingChanged>(CBN_SELENDOK, IDC_COMBO_THREADS),
        Route::Control<&OnSettingChanged>(CBN_SELENDOK, IDC_HASH_ALGORITHM),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_SAMPLE_LARGE_FILES),
        Route::Control<&OnSettingChanged>(CBN_SELENDOK, IDC_PROCESS_PRIORITY),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_SKIP_CLOUD_LINKS),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_PROCESS_HARDLINKS),
        Route::Control<&OnBnClickedResetPreferences>(BN_CLICKED, IDC_RESET_PREFERENCES),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_LARGEST_FILE_COUNT),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FOLDER_HISTORY_COUNT),
    };
    return entries;
}
