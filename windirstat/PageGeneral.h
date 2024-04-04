// PageGeneral.h - Declaration of CPageGeneral
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

class COptionsPropertySheet;

//
// CPageGeneral. "Settings" property page "General".
//
class CPageGeneral final : public CPropertyPage
{
    DECLARE_DYNAMIC(CPageGeneral)

    enum { IDD = IDD_PAGE_GENERAL };

    CPageGeneral();
    ~CPageGeneral() override;

protected:
    COptionsPropertySheet* GetSheet() const;

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    BOOL m_useWdsLocale = FALSE;
    BOOL m_humanFormat = FALSE;
    BOOL m_portableMode = FALSE;
    BOOL m_listGrid = FALSE;
    BOOL m_listStripes = FALSE;
    BOOL m_showDeletionWarning = FALSE;
    BOOL m_listFullRowSelection = FALSE;

    CComboBox m_combo;

    int m_originalLanguage = 0;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedSetModified();
    afx_msg void OnCbnSelendokCombo();
};
