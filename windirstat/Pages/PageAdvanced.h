// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

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
    void OnOK() override;

public:
    static std::span<const RouteEntry> Routes();

protected:
    CComboBox m_priorityCombo;

    void OnEnChangeLargestFileCount();
    void OnEnChangeFolderHistoryCount();
    void OnBnClickedResetPreferences();
};

inline std::span<const RouteEntry> CPageAdvanced::Routes()
{
    using ThisClass = CPageAdvanced;
    static constexpr std::array entries
    {
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_BACKUP_RESTORE),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_EXCLUDE_HIDDEN_DIRECTORY),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_EXCLUDE_PROTECTED_DIRECTORY),
        Route::Control<&ThisClass::OnSettingChanged>(CBN_SELENDOK, IDC_COMBO_THREADS),
        Route::Control<&ThisClass::OnSettingChanged>(CBN_SELENDOK, IDC_HASH_ALGORITHM),
        Route::Control<&ThisClass::OnSettingChanged>(CBN_SELENDOK, IDC_PROCESS_PRIORITY),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_EXCLUDE_VOLUME_MOUNT_POINTS),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_EXCLUDE_JUNCTIONS),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_EXCLUDE_SYMLINKS_DIRECTORY),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_SKIP_CLOUD_LINKS),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_EXCLUDE_SYMLINKS_FILE),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_EXCLUDE_HIDDEN_FILE),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_EXCLUDE_PROTECTED_FILE),
        Route::Control<&ThisClass::OnSettingChanged>(BN_CLICKED, IDC_PROCESS_HARDLINKS),
        Route::Control<&CPageAdvanced::OnBnClickedResetPreferences>(BN_CLICKED, IDC_RESET_PREFERENCES),
        Route::Control<&CPageAdvanced::OnEnChangeLargestFileCount>(EN_CHANGE, IDC_LARGEST_FILE_COUNT),
        Route::Control<&CPageAdvanced::OnEnChangeFolderHistoryCount>(EN_CHANGE, IDC_FOLDER_HISTORY_COUNT),
    };
    return entries;
}
