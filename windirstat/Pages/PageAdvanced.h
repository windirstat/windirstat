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

#include "WinDirStat.h"

class COptionsPropertySheet;

//
// CPageAdvanced. "Settings" property page "General".
//
class CPageAdvanced final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageAdvanced)

    enum : std::uint8_t { IDD = IDD_PAGE_ADVANCED };

    CPageAdvanced();
    ~CPageAdvanced() override;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    BOOL m_ExcludeJunctions = TRUE;
    BOOL m_ExcludeVolumeMountPoints = TRUE;
    BOOL m_ExcludeSymbolicLinksDirectory = TRUE;
    BOOL m_SkipDupeDetectionCloudLinks = TRUE;
    BOOL m_SkipHiddenDirectory = FALSE;
    BOOL m_SkipProtectedDirectory = FALSE;
    BOOL m_ExcludeSymbolicLinksFile = TRUE;
    BOOL m_SkipHiddenFile = FALSE;
    BOOL m_SkipProtectedFile = FALSE;
    BOOL m_UseBackupRestore = FALSE;
    int m_ScanningThreads = 0;
    CStringW m_LargestFileCount;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnEnChangeLargestFileCount();
    afx_msg void OnSettingChanged();
    afx_msg void OnBnClickedResetPreferences();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
