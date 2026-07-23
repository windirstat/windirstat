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

IMPLEMENT_DYNAMIC(CPagePermissions, COptionsPage)

CPagePermissions::CPagePermissions() : COptionsPage(IDD)
{
    for (const int i : std::views::iota(0, PERMSRULECOUNT))
    {
        BindText(IDC_PERMS_ACCOUNT0 + i, COptions::PermsColorAccount[i], m_account[i]);
        BindCombo(IDC_PERMS_LEVEL0 + i, COptions::PermsColorLevel[i], m_level[i]);
        BindOption(COptions::PermsColor[i], m_color[i]);
    }
    BindText(IDC_PERMS_EXCLUDE, COptions::PermsExcludeRegex, m_excludeRegex);
}

void CPagePermissions::DoDataExchange(CDataExchange* pDX)
{
    COptionsPage::DoDataExchange(pDX);
    for (const int i : std::views::iota(0, PERMSRULECOUNT))
    {
        DDX_Control(pDX, IDC_PERMS_LEVEL0 + i, m_levelCombo[i]);
        DDX_Control(pDX, IDC_COLORBUTTON0 + i, m_colorButton[i]);
        if (pDX->m_bSaveAndValidate) m_color[i] = m_colorButton[i].GetColor();
        else m_colorButton[i].SetColor(m_color[i]);
    }
}

BEGIN_MESSAGE_MAP(CPagePermissions, COptionsPage)
    ON_NOTIFY_RANGE(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON4, OnSettingNotifyChanged)
    ON_CONTROL_RANGE(EN_CHANGE, IDC_PERMS_ACCOUNT0, IDC_PERMS_ACCOUNT4, OnSettingRangeChanged)
    ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_PERMS_LEVEL0, IDC_PERMS_LEVEL4, OnSettingRangeChanged)
    ON_EN_CHANGE(IDC_PERMS_EXCLUDE, OnSettingChanged)
END_MESSAGE_MAP()

void CPagePermissions::InitializePage()
{
    // Populate each level selection combo with "any" plus the summarized rights levels
    for (const int i : std::views::iota(0, PERMSRULECOUNT))
    {
        // "Special" is excluded since it is not a meaningful colorization threshold
        m_levelCombo[i].AddString(Localization::Lookup(IDS_PERMS_ANY).c_str());
        for (const int level : std::views::iota(0, static_cast<int>(PERMSLEVEL_SPECIAL)))
        {
            m_levelCombo[i].AddString(CItemPerm::GetRightsLevelName(static_cast<PERMSLEVEL>(level)).c_str());
        }
    }

    UpdateData(FALSE);
}

void CPagePermissions::OnOK()
{
    UpdateData();
    ApplyOptionBindings();

    // Force colorization to be recomputed and repaint the list
    CItemPerm::InvalidateRuleColors();
    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
    CMFCPropertyPage::OnOK();
}
