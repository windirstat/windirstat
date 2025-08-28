// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

#include "WinDirStat.h"

class COptionsPropertySheet;

//
// CPageFiltering. "Settings" property page "Filtering".
//
class CPageFiltering final : public CPropertyPageEx
{
    DECLARE_DYNAMIC(CPageFiltering)

    enum : std::uint8_t { IDD = IDD_PAGE_FILTERING };

    CPageFiltering();
    ~CPageFiltering() override;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;
    void SetToolTips();

    int m_FilteringSizeMinimum = 0;
    int m_FilteringSizeUnits = 0;
    BOOL m_FilteringUseRegex = FALSE;
    CString m_FilteringExcludeDirs;
    CString m_FilteringExcludeFiles;
    CComboBox m_CtlFilteringSizeUnits;
    CEdit m_CtrlFilteringExcludeFiles;
    CEdit m_CtrlFilteringExcludeDirs;
    CToolTipCtrl m_ToolTip;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSettingChanged();
    BOOL PreTranslateMessage(MSG* pMsg) override;
};
