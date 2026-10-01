// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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

std::optional<CPropertyPage::ValidationError> CPageFileTree::PrepareSettings()
{
    const bool pacmanChanged = COptions::PacmanAnimation != IsChecked(IDC_PACMANANIMATION);
    Stage(COptions::PacmanAnimation, IsChecked(IDC_PACMANANIMATION));
    Stage(COptions::ShowTimeSpent, IsChecked(IDC_SHOWTIMESPENT));

    const auto previousVisibility = COptions::FileTreeColumnVisibility.Obj();
    auto visibility = previousVisibility;
    for (const auto [controlId, subItem] : c_columns)
        COptions::SetColumnVisible(visibility, subItem, IsChecked(controlId));
    Stage(COptions::FileTreeColumnVisibility, visibility);
    Stage(COptions::FileTreeColorCount, m_fileTreeColorCount);
    for (auto&& [button, color, optionColor] : std::views::zip(m_colorButton, m_fileTreeColor, COptions::FileTreeColors))
    {
        color = button.GetColor();
        Stage(optionColor, color);
    }

    StageEffect([previousVisibility, visibility = std::move(visibility), pacmanChanged]
    {
        if (auto* control = CFileTreeControl::Get())
        {
            for (const auto [controlId, subItem] : c_columns)
                if (COptions::IsColumnVisible(previousVisibility, subItem) !=
                    COptions::IsColumnVisible(visibility, subItem))
                    control->SetColumnVisible(subItem, COptions::IsColumnVisible(visibility, subItem), true);
            if (pacmanChanged) control->SortItems();
        }
        CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
    });
    return {};
}

void CPageFileTree::EnableButtons()
{
    for (const auto [i, button] : std::views::enumerate(m_colorButton))
    {
        button.EnableWindow(i < m_fileTreeColorCount);
    }
}

void CPageFileTree::OnVScroll(const UINT nSBCode, const UINT nPos, const WindowRef scrollBar)
{
    if (scrollBar.Handle() == m_slider.Handle())
    {
        m_fileTreeColorCount = m_slider.GetPos();
        EnableButtons();
        SetModified();
    }
    CSettingsPage::OnVScroll(nSBCode, nPos, scrollBar);
}
