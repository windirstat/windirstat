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
    for (const int i : std::views::iota(0, PERMSRULECOUNT))
    {
        m_levelCombo[i].SubclassDlgItem(IDC_PERMS_LEVEL0 + i, this);
        m_colorButton[i].SubclassDlgItem(IDC_COLORBUTTON0 + i, this);

        // "Special" is excluded since it is not a meaningful colorization threshold
        m_levelCombo[i].AddString(Localization::Lookup(IDS_PERMS_ANY).c_str());
        for (const int level : std::views::iota(0, static_cast<int>(PERMSLEVEL_SPECIAL)))
        {
            m_levelCombo[i].AddString(CItemPerm::GetRightsLevelName(static_cast<PERMSLEVEL>(level)).c_str());
        }

        SetText(IDC_PERMS_ACCOUNT0 + i, COptions::PermsColorAccount[i].Obj());
        SetComboSelection(IDC_PERMS_LEVEL0 + i, COptions::PermsColorLevel[i]);
        m_colorButton[i].SetColor(COptions::PermsColor[i]);
    }

    SetText(IDC_PERMS_EXCLUDE, COptions::PermsExcludeRegex.Obj());
}

void CPagePermissions::OnOK()
{
    for (const int i : std::views::iota(0, PERMSRULECOUNT))
    {
        COptions::PermsColorAccount[i].Obj() = GetText(IDC_PERMS_ACCOUNT0 + i);
        COptions::PermsColorLevel[i] = ComboSelection(IDC_PERMS_LEVEL0 + i);
        COptions::PermsColor[i] = m_colorButton[i].GetColor();
    }
    COptions::PermsExcludeRegex.Obj() = GetText(IDC_PERMS_EXCLUDE);

    // Force colorization to be recomputed and repaint the list
    CItemPerm::InvalidateRuleColors();
    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
}
