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

IMPLEMENT_DYNAMIC(CPagePermissions, CMFCPropertyPage)

CPagePermissions::CPagePermissions() : CMFCPropertyPage(IDD) {}

void CPagePermissions::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    for (const int i : std::views::iota(0, PERMSRULECOUNT))
    {
        DDX_Text(pDX, IDC_PERMS_ACCOUNT0 + i, m_account[i]);
        DDX_Control(pDX, IDC_PERMS_LEVEL0 + i, m_levelCombo[i]);
        DDX_CBIndex(pDX, IDC_PERMS_LEVEL0 + i, m_level[i]);
        DDX_Control(pDX, IDC_COLORBUTTON0 + i, m_colorButton[i]);
        if (pDX->m_bSaveAndValidate) m_color[i] = m_colorButton[i].GetColor();
        else m_colorButton[i].SetColor(m_color[i]);
    }
    DDX_Text(pDX, IDC_PERMS_EXCLUDE, m_excludeRegex);
}

BEGIN_MESSAGE_MAP(CPagePermissions, CMFCPropertyPage)
    ON_NOTIFY_RANGE(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON4, OnColorChanged)
    ON_CONTROL_RANGE(EN_CHANGE, IDC_PERMS_ACCOUNT0, IDC_PERMS_ACCOUNT4, OnSettingChanged)
    ON_CONTROL_RANGE(CBN_SELCHANGE, IDC_PERMS_LEVEL0, IDC_PERMS_LEVEL4, OnSettingChanged)
    ON_EN_CHANGE(IDC_PERMS_EXCLUDE, OnExcludeChanged)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPagePermissions::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CPagePermissions::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    // Populate each level selection combo with "any" plus the summarized rights levels
    for (const int i : std::views::iota(0, PERMSRULECOUNT))
    {
        // "Special" is excluded since it is not a meaningful colorization threshold
        m_levelCombo[i].AddString(Localization::Lookup(IDS_PERMS_ANY).c_str());
        for (const int level : std::views::iota(0, static_cast<int>(PERMSLEVEL_SPECIAL)))
        {
            m_levelCombo[i].AddString(CItemPerm::GetRightsLevelName(static_cast<PERMSLEVEL>(level)).c_str());
        }

        m_account[i] = COptions::PermsColorAccount[i].Obj().c_str();
        m_level[i] = COptions::PermsColorLevel[i];
        m_color[i] = COptions::PermsColor[i];
    }

    m_excludeRegex = COptions::PermsExcludeRegex.Obj().c_str();

    UpdateData(FALSE);
    return TRUE;
}

void CPagePermissions::OnOK()
{
    UpdateData();
    for (const int i : std::views::iota(0, PERMSRULECOUNT))
    {
        COptions::PermsColorAccount[i] = std::wstring(m_account[i].GetString());
        COptions::PermsColorLevel[i] = m_level[i];
        COptions::PermsColor[i] = m_color[i];
    }

    COptions::PermsExcludeRegex = std::wstring(m_excludeRegex.GetString());

    // Force colorization to be recomputed and repaint the list
    CItemPerm::InvalidateRuleColors();
    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
    CMFCPropertyPage::OnOK();
}

void CPagePermissions::OnSettingChanged(UINT)
{
    SetModified();
}

void CPagePermissions::OnExcludeChanged()
{
    SetModified();
}

void CPagePermissions::OnColorChanged(UINT, NMHDR*, LRESULT*)
{
    SetModified();
}
