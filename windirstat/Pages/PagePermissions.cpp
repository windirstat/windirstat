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
#include "PagePermissions.h"
#include "ItemPerm.h"

CPagePermissions::CPagePermissions() : MessageTarget(IDD)
{
}

void CPagePermissions::InitializePage()
{
    // Populate each level selection combo with "any" plus the summarized rights levels
    for (const auto [i, levelCombo, colorButton, accountOpt, levelOpt, colorOpt] :
        std::views::zip(std::views::iota(0, PERMSRULECOUNT), m_levelCombo, m_colorButton,
            COptions::PermsColorAccount, COptions::PermsColorLevel, COptions::PermsColor))
    {
        levelCombo.SubclassDlgItem(IDC_PERMS_LEVEL0 + i, this);
        colorButton.SubclassDlgItem(IDC_COLORBUTTON0 + i, this);

        // "Special" is excluded since it is not a meaningful colorization threshold
        levelCombo.AddString(Localization::Lookup(IDS_PERMS_ANY));
        for (const int level : std::views::iota(0, static_cast<int>(PERMSLEVEL_SPECIAL)))
        {
            levelCombo.AddString(CItemPerm::GetRightsLevelName(static_cast<PERMSLEVEL>(level)));
        }

        SetText(IDC_PERMS_ACCOUNT0 + i, accountOpt.Obj());
        SetComboSelection(IDC_PERMS_LEVEL0 + i, levelOpt);
        colorButton.SetColor(colorOpt);
    }

    SetText(IDC_PERMS_EXCLUDE, COptions::PermsExcludeRegex.Obj());
}

std::optional<CPropertyPage::ValidationError> CPagePermissions::PrepareSettings()
{
    for (const auto [i, accountOpt, levelOpt, colorOpt, colorButton] :
        std::views::zip(std::views::iota(0, PERMSRULECOUNT),
            COptions::PermsColorAccount, COptions::PermsColorLevel, COptions::PermsColor, m_colorButton))
    {
        const std::wstring account = GetText(IDC_PERMS_ACCOUNT0 + i);
        int level = 0;
        if (auto error = ValidateRegex(IDC_PERMS_ACCOUNT0 + i, account)) return error;
        if (auto error = ReadSelection(IDC_PERMS_LEVEL0 + i, level, levelOpt.Min(), levelOpt.Max())) return error;
        Stage(accountOpt, account);
        Stage(levelOpt, level);
        Stage(colorOpt, colorButton.GetColor());
    }
    const std::wstring exclude = GetText(IDC_PERMS_EXCLUDE);
    if (auto error = ValidateRegex(IDC_PERMS_EXCLUDE, exclude)) return error;
    Stage(COptions::PermsExcludeRegex, exclude);

    // Force colorization to be recomputed and repaint the list
    StageEffect([]
    {
        CItemPerm::InvalidateRuleColors();
        CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
    });
    return {};
}
