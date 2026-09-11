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
    for (const auto [controlId, subItem] : c_columns)
    {
        SetChecked(controlId, COptions::IsColumnVisible(visibility, subItem));
    }

    m_fileTreeColorCount = COptions::FileTreeColorCount;
    for (const auto [i, button] : std::views::enumerate(m_colorButton))
    {
        m_fileTreeColor[i] = COptions::FileTreeColors[i];
        button.SubclassDlgItem(static_cast<int>(IDC_COLORBUTTON0 + i), this);
        button.SetColor(m_fileTreeColor[i]);
    }

    m_slider.SubclassDlgItem(IDC_SLIDER, this);
    m_slider.SetRange(1, TREELISTCOLORCOUNT);
    m_slider.SetPos(m_fileTreeColorCount);

    EnableButtons();
}

void CPageFileTree::OnOK()
{
    const bool pacmanChanged = COptions::PacmanAnimation != IsChecked(IDC_PACMANANIMATION);
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
    for (const auto [controlId, subItem] : c_columns)
        setColumnVisible(subItem, IsChecked(controlId));

    COptions::FileTreeColorCount = m_fileTreeColorCount;
    for (auto&& [button, color, optionColor] : std::views::zip(m_colorButton, m_fileTreeColor, COptions::FileTreeColors))
    {
        color = button.GetColor();
        optionColor = color;
    }

    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
    if (const auto control = CFileTreeControl::Get(); pacmanChanged && control != nullptr) control->SortItems();
}

void CPageFileTree::EnableButtons()
{
    for (const auto [i, button] : std::views::enumerate(m_colorButton))
    {
        button.EnableWindow(i < m_fileTreeColorCount);
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
