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

CPageFileTree::CPageFileTree() : MessageTarget(IDD)
{
}

void CPageFileTree::InitializePage()
{
    SetChecked(IDC_PACMANANIMATION, COptions::PacmanAnimation);
    SetChecked(IDC_SHOWTIMESPENT, COptions::ShowTimeSpent);

    const auto& visibility = COptions::FileTreeColumnVisibility.Obj();
    for (const auto i : std::views::iota(size_t{0}, c_columns.size()))
    {
        SetChecked(c_columns[i].first, COptions::IsColumnVisible(visibility, c_columns[i].second));
    }

    m_fileTreeColorCount = COptions::FileTreeColorCount;
    for (const int i : std::views::iota(0, TREELISTCOLORCOUNT))
    {
        m_fileTreeColor[i] = COptions::FileTreeColors[i];
        m_colorButton[i].SubclassDlgItem(IDC_COLORBUTTON0 + i, this);
        m_colorButton[i].SetColor(m_fileTreeColor[i]);
    }

    m_slider.SubclassDlgItem(IDC_SLIDER, this);
    m_slider.SetRange(1, TREELISTCOLORCOUNT);
    m_slider.SetPos(m_fileTreeColorCount);

    EnableButtons();
}

void CPageFileTree::OnOK()
{
    COptions::PacmanAnimation = IsChecked(IDC_PACMANANIMATION);
    COptions::ShowTimeSpent = IsChecked(IDC_SHOWTIMESPENT);

    const auto setColumnVisible = [](const int column, const bool visible)
    {
        if (auto* control = CFileTreeControl::Get())
        {
            control->SetColumnVisible(column, visible);
        }
        else
        {
            COptions::SetColumnVisible(COptions::FileTreeColumnVisibility.Obj(), column, visible);
        }
    };
    for (const auto i : std::views::iota(size_t{0}, c_columns.size()))
        setColumnVisible(c_columns[i].second, IsChecked(c_columns[i].first));

    COptions::FileTreeColorCount = m_fileTreeColorCount;
    for (const int i : std::views::iota(0, TREELISTCOLORCOUNT))
    {
        m_fileTreeColor[i] = m_colorButton[i].GetColor();
        COptions::FileTreeColors[i] = m_fileTreeColor[i];
    }

    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
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

void CPageFileTree::OnVScroll(const UINT nSBCode, const UINT nPos, CWnd* scrollBar)
{
    if (scrollBar == &m_slider)
    {
        const int pos = m_slider.GetPos();
        assert(pos > 0);
        assert(pos <= TREELISTCOLORCOUNT);

        m_fileTreeColorCount = pos;
        EnableButtons();
        SetModified();
    }
    CSettingsPage::OnVScroll(nSBCode, nPos, scrollBar);
}
