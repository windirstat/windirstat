// PageTreeList.h - Declaration of CPageTreelist
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

#include "colorbutton.h"
#include "afxcmn.h"

//
// CPageTreelist. "Settings" property page "Treelist".
//
class CPageTreelist final : public CPropertyPage
{
    DECLARE_DYNAMIC(CPageTreelist)

    enum { IDD = IDD_PAGE_TREELIST };

public:
    CPageTreelist();
    ~CPageTreelist() override = default;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;
    void EnableButtons();

    BOOL m_pacmanAnimation;
    BOOL m_showTimeSpent;
    BOOL m_showColumnSubdirs;
    BOOL m_showColumnItems;
    BOOL m_showColumnFiles;
    BOOL m_showColumnAttributes;
    BOOL m_showColumnLastChange;
    BOOL m_showColumnOwner;

    int m_treelistColorCount;
    COLORREF m_treelistColor[TREELISTCOLORCOUNT];

    CColorButton m_colorButton[TREELISTCOLORCOUNT];
    CSliderCtrl m_slider;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnColorChanged(UINT id, NMHDR*, LRESULT*);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnBnClickedSetModified();
};
