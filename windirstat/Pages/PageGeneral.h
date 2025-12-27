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
// CPageGeneral. "Settings" property page "General".
//
class CPageGeneral final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageGeneral)

    enum : std::uint8_t { IDD = IDD_PAGE_GENERAL };

    CPageGeneral();
    ~CPageGeneral() override;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    BOOL m_useWindowsLocale = FALSE;
    BOOL m_automaticallyElevateOnStartup = FALSE;
    BOOL m_automaticallyResizeColumns = FALSE;
    BOOL m_sizeSuffixesFormat = FALSE;
    BOOL m_portableMode = FALSE;
    BOOL m_listGrid = FALSE;
    BOOL m_listStripes = FALSE;
    BOOL m_listFullRowSelection = FALSE;

    CComboBox m_combo;
    int m_darkModeRadio = 0;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedSetModified();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
