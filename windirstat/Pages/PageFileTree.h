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
#include "ColorButton.h"

//
// CPageFileTree. "Settings" property page "Folder List".
//
class CPageFileTree final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageFileTree)

    enum : std::uint8_t { IDD = IDD_PAGE_TREELIST };

    CPageFileTree();
    ~CPageFileTree() override = default;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;
    void EnableButtons();

    BOOL m_PacmanAnimation = FALSE;
    BOOL m_ShowTimeSpent = FALSE;
    BOOL m_ShowColumnFolders = FALSE;
    BOOL m_ShowColumnItems = FALSE;
    BOOL m_ShowColumnFiles = FALSE;
    BOOL m_ShowColumnAttributes = FALSE;
    BOOL m_ShowColumnLastChange = FALSE;
    BOOL m_ShowColumnOwner = FALSE;
    BOOL m_ShowColumnSizePhysical = FALSE;
    BOOL m_ShowColumnSizeLogical = FALSE;

    int m_FileTreeColorCount = TREELISTCOLORCOUNT;
    COLORREF m_FileTreeColor[TREELISTCOLORCOUNT] = {};

    CColorButton m_ColorButton[TREELISTCOLORCOUNT];
    CSliderCtrl m_Slider;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnColorChanged(UINT id, NMHDR*, LRESULT*);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnBnClickedSetModified();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
