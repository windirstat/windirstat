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
// CPageFiltering. "Settings" property page "Filtering".
//
class CPageFiltering final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageFiltering)

    enum : std::uint8_t { IDD = IDD_PAGE_FILTERING };

    CPageFiltering();
    ~CPageFiltering() override = default;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;
    void SetToolTips();

    int m_filteringSizeMinimum = 0;
    int m_filteringSizeUnits = 0;
    BOOL m_filteringUseRegex = FALSE;
    CString m_filteringExcludeDirs;
    CString m_filteringExcludeFiles;
    CComboBox m_ctlFilteringSizeUnits;
    CEdit m_ctrlFilteringExcludeFiles;
    CEdit m_ctrlFilteringExcludeDirs;
    CToolTipCtrl m_toolTip;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSettingChanged();
    BOOL PreTranslateMessage(MSG* pMsg) override;
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
