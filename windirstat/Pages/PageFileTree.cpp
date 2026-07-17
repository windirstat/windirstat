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

#include "pch.h"
#include "PageFileTree.h"
#include "FileTreeControl.h"

IMPLEMENT_DYNAMIC(CPageFileTree, CMFCPropertyPage)

CPageFileTree::CPageFileTree() : CMFCPropertyPage(IDD) {}

void CPageFileTree::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_PACMANANIMATION, m_pacmanAnimation);
    DDX_Check(pDX, IDC_SHOWTIMESPENT, m_showTimeSpent);
    DDX_Check(pDX, IDC_TREECOL_FOLDERS, m_showColumnFolders);
    DDX_Check(pDX, IDC_TREECOL_SIZE_PHYSICAL, m_showColumnSizePhysical);
    DDX_Check(pDX, IDC_TREECOL_SIZE_LOGICAL, m_showColumnSizeLogical);
    DDX_Check(pDX, IDC_TREECOL_ITEMS, m_showColumnItems);
    DDX_Check(pDX, IDC_TREECOL_FILES, m_showColumnFiles);
    DDX_Check(pDX, IDC_TREECOL_ATTRIBUTES, m_showColumnAttributes);
    DDX_Check(pDX, IDC_TREECOL_LAST_CHANGE, m_showColumnLastChange);
    DDX_Check(pDX, IDC_TREECOL_OWNER, m_showColumnOwner);
    DDX_Check(pDX, IDC_TREECOL_PERCENTAGE, m_showColumnPercentage);
    for (const int i : std::views::iota(0, TREELISTCOLORCOUNT))
    {
        DDX_Control(pDX, IDC_COLORBUTTON0 + i, m_colorButton[i]);
        if (pDX->m_bSaveAndValidate)
        {
            m_fileTreeColor[i] = m_colorButton[i].GetColor();
        }
        else
        {
            m_colorButton[i].SetColor(m_fileTreeColor[i]);
        }
    }
    DDX_Control(pDX, IDC_SLIDER, m_slider);
}

BEGIN_MESSAGE_MAP(CPageFileTree, CMFCPropertyPage)
 ON_NOTIFY_RANGE(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON7, OnColorChanged)
    ON_WM_VSCROLL()
    ON_BN_CLICKED(IDC_PACMANANIMATION, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SHOWTIMESPENT, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_FOLDERS, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_ITEMS, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_FILES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_ATTRIBUTES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_LAST_CHANGE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_OWNER, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_PERCENTAGE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_SIZE_LOGICAL, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_TREECOL_SIZE_PHYSICAL, OnBnClickedSetModified)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPageFileTree::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CPageFileTree::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    m_pacmanAnimation = COptions::PacmanAnimation;
    m_showTimeSpent = COptions::ShowTimeSpent;
    const auto& visibility = COptions::FileTreeColumnVisibility.Obj();
    m_showColumnFolders = COptions::IsColumnVisible(visibility, COL_FOLDERS);
    m_showColumnItems = COptions::IsColumnVisible(visibility, COL_ITEMS);
    m_showColumnFiles = COptions::IsColumnVisible(visibility, COL_FILES);
    m_showColumnAttributes = COptions::IsColumnVisible(visibility, COL_ATTRIBUTES);
    m_showColumnLastChange = COptions::IsColumnVisible(visibility, COL_LAST_CHANGE);
    m_showColumnOwner = COptions::IsColumnVisible(visibility, COL_OWNER);
    m_showColumnPercentage = COptions::IsColumnVisible(visibility, COL_PERCENTAGE);
    m_showColumnSizePhysical = COptions::IsColumnVisible(visibility, COL_SIZE_PHYSICAL);
    m_showColumnSizeLogical = COptions::IsColumnVisible(visibility, COL_SIZE_LOGICAL);

    m_fileTreeColorCount = COptions::FileTreeColorCount;
    for (const int i : std::views::iota(0, TREELISTCOLORCOUNT))
    {
        m_fileTreeColor[i] = COptions::FileTreeColors[i];
    }

    m_slider.SetRange(1, TREELISTCOLORCOUNT);
    m_slider.SetPos(m_fileTreeColorCount);

    EnableButtons();
    UpdateData(FALSE);
    return TRUE;
}

void CPageFileTree::OnOK()
{
    UpdateData();

    COptions::PacmanAnimation = (FALSE != m_pacmanAnimation);
    COptions::ShowTimeSpent = (FALSE != m_showTimeSpent);
    const auto setColumnVisible = [](const int column, const BOOL visible)
    {
        if (auto* control = CFileTreeControl::Get())
        {
            control->SetColumnVisible(column, visible != FALSE);
        }
        else
        {
            COptions::SetColumnVisible(COptions::FileTreeColumnVisibility.Obj(), column, visible != FALSE);
        }
    };
    setColumnVisible(COL_FOLDERS, m_showColumnFolders);
    setColumnVisible(COL_ITEMS, m_showColumnItems);
    setColumnVisible(COL_FILES, m_showColumnFiles);
    setColumnVisible(COL_ATTRIBUTES, m_showColumnAttributes);
    setColumnVisible(COL_LAST_CHANGE, m_showColumnLastChange);
    setColumnVisible(COL_OWNER, m_showColumnOwner);
    setColumnVisible(COL_PERCENTAGE, m_showColumnPercentage);
    setColumnVisible(COL_SIZE_PHYSICAL, m_showColumnSizePhysical);
    setColumnVisible(COL_SIZE_LOGICAL, m_showColumnSizeLogical);
    COptions::FileTreeColorCount = m_fileTreeColorCount;
    for (const int i : std::views::iota(0, TREELISTCOLORCOUNT))
    {
        COptions::FileTreeColors[i] = m_fileTreeColor[i];
    }
    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
    CMFCPropertyPage::OnOK();
}

void CPageFileTree::OnBnClickedSetModified()
{
    SetModified();
}

void CPageFileTree::OnColorChanged(UINT, NMHDR*, LRESULT*)
{
    SetModified();
}

void CPageFileTree::EnableButtons()
{
    for (const int i : std::views::iota(0, m_fileTreeColorCount))
    {
        m_colorButton[i].EnableWindow(true);
    }
    for (const int i : std::views::iota(m_fileTreeColorCount, TREELISTCOLORCOUNT))
    {
        m_colorButton[i].EnableWindow(false);
    }
}

void CPageFileTree::OnVScroll(const UINT nSBCode, const UINT nPos, CScrollBar* pScrollBar)
{
    if (reinterpret_cast<CSliderCtrl*>(pScrollBar) == &m_slider)
    {
        const int pos = m_slider.GetPos();
        ASSERT(pos > 0);
        ASSERT(pos <= TREELISTCOLORCOUNT);

        m_fileTreeColorCount = pos;
        EnableButtons();
        SetModified();
    }
    CMFCPropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
}
