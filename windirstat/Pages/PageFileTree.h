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
#include "PageShared.h"
#include "ColorButton.h"

//
// CPageFileTree. "Settings" property page "Folder List".
//
class CPageFileTree final : public COptionsPage
{
    DECLARE_DYNAMIC(CPageFileTree)

    enum : std::uint8_t { IDD = IDD_PAGE_TREELIST };

    CPageFileTree();
    ~CPageFileTree() override = default;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    void InitializePage() override;
    void OnOK() override;
    void EnableButtons();

    BOOL m_pacmanAnimation = FALSE;
    BOOL m_showTimeSpent = FALSE;
    inline static constexpr std::array<std::pair<UINT, int>, 9> c_columns = {{
        { IDC_TREECOL_FOLDERS, COL_FOLDERS },
        { IDC_TREECOL_ITEMS, COL_ITEMS },
        { IDC_TREECOL_FILES, COL_FILES },
        { IDC_TREECOL_ATTRIBUTES, COL_ATTRIBUTES },
        { IDC_TREECOL_LAST_CHANGE, COL_LAST_CHANGE },
        { IDC_TREECOL_OWNER, COL_OWNER },
        { IDC_TREECOL_PERCENTAGE, COL_PERCENTAGE },
        { IDC_TREECOL_SIZE_PHYSICAL, COL_SIZE_PHYSICAL },
        { IDC_TREECOL_SIZE_LOGICAL, COL_SIZE_LOGICAL },
    }};
    std::array<BOOL, c_columns.size()> m_showColumns{};

    int m_fileTreeColorCount = TREELISTCOLORCOUNT;
    COLORREF m_fileTreeColor[TREELISTCOLORCOUNT] = {};

    CColorButton m_colorButton[TREELISTCOLORCOUNT];
    CSliderCtrl m_slider;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};
