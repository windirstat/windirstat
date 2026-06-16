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
// CPageLayout. "Settings" property page for window layout configuration.
//
class CPageLayout final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageLayout)

    enum : std::uint8_t { IDD = IDD_PAGE_LAYOUT };

    CPageLayout();
    ~CPageLayout() override = default;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    void UpdateControlStates();
    void FillPanelCombo(CComboBox& combo, int selectedPanel);

    int  m_layoutMode = 0;
    int  m_treemapSide = 0; // DDX_Radio index: 0=right (IDC_LAYOUT_TREEMAP_RIGHT), 1=left
    CComboBox m_comboCol0;
    CComboBox m_comboCol1;
    CComboBox m_comboCol2;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnModeChanged();
    afx_msg void OnColChanged();
    afx_msg void OnBnClickedReset();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
