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
#include "ColorButton.h"

//
// CPagePermissions. "Settings" property page "Permissions".
//
class CPagePermissions final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPagePermissions)

    enum : std::uint8_t { IDD = IDD_PAGE_PERMISSIONS };

    CPagePermissions();
    ~CPagePermissions() override = default;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    CString m_account[PERMSRULECOUNT];
    int m_level[PERMSRULECOUNT] = {};
    COLORREF m_color[PERMSRULECOUNT] = {};
    CComboBox m_levelCombo[PERMSRULECOUNT];
    CColorButton m_colorButton[PERMSRULECOUNT];
    CString m_excludeRegex;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnColorChanged(UINT id, NMHDR*, LRESULT*);
    afx_msg void OnSettingChanged(UINT id);
    afx_msg void OnExcludeChanged();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
