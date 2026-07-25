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

IMPLEMENT_DYNAMIC(CPageFileTree, COptionsPage)

CPageFileTree::CPageFileTree() : COptionsPage(IDD)
{
    BindCheck(IDC_PACMANANIMATION, COptions::PacmanAnimation, m_pacmanAnimation);
    BindCheck(IDC_SHOWTIMESPENT, COptions::ShowTimeSpent, m_showTimeSpent);
}

void CPageFileTree::DoDataExchange(CDataExchange* pDX)
{
    COptionsPage::DoDataExchange(pDX);
    for (const auto i : std::views::iota(size_t{0}, c_columns.size()))
        DDX_Check(pDX, c_columns[i].first, m_showColumns[i]);

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

BEGIN_MESSAGE_MAP(CPageFileTree, COptionsPage)
    ON_NOTIFY_RANGE(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON7, OnSettingNotifyChanged)
    ON_WM_VSCROLL()
    ON_BN_CLICKED(IDC_PACMANANIMATION, OnSettingChanged)
    ON_BN_CLICKED(IDC_SHOWTIMESPENT, OnSettingChanged)
    ON_BN_CLICKED(IDC_TREECOL_FOLDERS, OnSettingChanged)
    ON_BN_CLICKED(IDC_TREECOL_ITEMS, OnSettingChanged)
    ON_BN_CLICKED(IDC_TREECOL_FILES, OnSettingChanged)
    ON_BN_CLICKED(IDC_TREECOL_ATTRIBUTES, OnSettingChanged)
    ON_BN_CLICKED(IDC_TREECOL_LAST_CHANGE, OnSettingChanged)
    ON_BN_CLICKED(IDC_TREECOL_OWNER, OnSettingChanged)
    ON_BN_CLICKED(IDC_TREECOL_PERCENTAGE, OnSettingChanged)
    ON_BN_CLICKED(IDC_TREECOL_SIZE_LOGICAL, OnSettingChanged)
    ON_BN_CLICKED(IDC_TREECOL_SIZE_PHYSICAL, OnSettingChanged)
END_MESSAGE_MAP()

void CPageFileTree::InitializePage()
{
    const auto& visibility = COptions::FileTreeColumnVisibility.Obj();
    for (const auto i : std::views::iota(size_t{0}, c_columns.size()))
        m_showColumns[i] = COptions::IsColumnVisible(visibility, c_columns[i].second);

    m_fileTreeColorCount = COptions::FileTreeColorCount;
    for (const int i : std::views::iota(0, TREELISTCOLORCOUNT))
        m_fileTreeColor[i] = COptions::FileTreeColors[i];

    m_slider.SetRange(1, TREELISTCOLORCOUNT);
    m_slider.SetPos(m_fileTreeColorCount);

    EnableButtons();
    UpdateData(FALSE);
}

void CPageFileTree::OnOK()
{
    UpdateData();

    ApplyOptionBindings();
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
    for (const auto i : std::views::iota(size_t{0}, c_columns.size()))
        setColumnVisible(c_columns[i].second, m_showColumns[i]);

    COptions::FileTreeColorCount = m_fileTreeColorCount;
    for (const int i : std::views::iota(0, TREELISTCOLORCOUNT))
        COptions::FileTreeColors[i] = m_fileTreeColor[i];

    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
    CMFCPropertyPage::OnOK();
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
    COptionsPage::OnVScroll(nSBCode, nPos, pScrollBar);
}
