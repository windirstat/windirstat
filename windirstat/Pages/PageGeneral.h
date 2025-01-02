// PageGeneral.h - Declaration of CPageGeneral
//
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
// CPageGeneral. "Settings" property page "General".
//
class CPageGeneral final : public CPropertyPageEx
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

    BOOL m_UseWindowsLocale = FALSE;
    BOOL m_AutomaticallyResizeColumns = FALSE;
    BOOL m_SizeSuffixesFormat = FALSE;
    BOOL m_PortableMode = FALSE;
    BOOL m_ListGrid = FALSE;
    BOOL m_ListStripes = FALSE;
    BOOL m_ShowDeletionWarning = FALSE;
    BOOL m_ListFullRowSelection = FALSE;

    CComboBox m_Combo;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedSetModified();
    afx_msg void OnCbnSelendokCombo();
};
