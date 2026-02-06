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

class COptionsPropertySheet;

//
// CPageAdvanced. "Settings" property page "Advanced".
//
class CPageAdvanced final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageAdvanced)

    enum : std::uint8_t { IDD = IDD_PAGE_ADVANCED };

    CPageAdvanced();
    ~CPageAdvanced() override = default;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    BOOL m_excludeJunctions = TRUE;
    BOOL m_excludeVolumeMountPoints = TRUE;
    BOOL m_excludeSymbolicLinksDirectory = TRUE;
    BOOL m_skipDupeDetectionCloudLinks = TRUE;
    BOOL m_skipHiddenDirectory = FALSE;
    BOOL m_skipProtectedDirectory = FALSE;
    BOOL m_excludeSymbolicLinksFile = TRUE;
    BOOL m_skipHiddenFile = FALSE;
    BOOL m_skipProtectedFile = FALSE;
    BOOL m_useBackupRestore = FALSE;
    BOOL m_processHardlinks = TRUE;
    int m_scanningThreads = 0;
    CStringW m_largestFileCount;
    CStringW m_folderHistoryCount;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnEnChangeLargestFileCount();
    afx_msg void OnEnChangeFolderHistoryCount();
    afx_msg void OnSettingChanged();
    afx_msg void OnBnClickedResetPreferences();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
