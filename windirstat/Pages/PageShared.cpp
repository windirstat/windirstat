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
#include "PageShared.h"
#include "MainFrame.h"

IMPLEMENT_DYNAMIC(COptionsPage, CMFCPropertyPage)

COptionsPage::COptionsPage(const UINT templateId) : CMFCPropertyPage(templateId) {}

COptionsPropertySheet* COptionsPage::GetSheet() const
{
    const auto sheet = DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
    ASSERT(sheet != nullptr);
    return sheet;
}

void COptionsPage::BindCheck(const int id, Setting<bool>& option, BOOL& value)
{
    BindOption(option, value, [id, &value](CDataExchange* pDX) { DDX_Check(pDX, id, value); });
}

void COptionsPage::BindCombo(const int id, Setting<int>& option, int& value)
{
    BindOption(option, value, [id, &value](CDataExchange* pDX) { DDX_CBIndex(pDX, id, value); });
}

void COptionsPage::BindRadio(const int id, Setting<int>& option, int& value)
{
    BindOption(option, value, [id, &value](CDataExchange* pDX) { DDX_Radio(pDX, id, value); });
}

void COptionsPage::BindText(const int id, Setting<int>& option, int& value)
{
    BindOption(option, value, [id, &value](CDataExchange* pDX) { DDX_Text(pDX, id, value); });
}

void COptionsPage::BindText(const int id, Setting<std::wstring>& option, CStringW& value)
{
    m_optionBindings.push_back({
        [&option, &value] { value = option.Obj().c_str(); },
        [id, &value](CDataExchange* pDX) { DDX_Text(pDX, id, value); },
        [&option, &value] { option = std::wstring(value.GetString()); },
    });
}

void COptionsPage::ApplyOptionBindings() const
{
    for (const auto& binding : m_optionBindings)
        binding.save();
}

void COptionsPage::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    for (const auto& binding : m_optionBindings)
    {
        if (binding.exchange)
            binding.exchange(pDX);
    }
}

BOOL COptionsPage::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();
    Localization::UpdateDialogs(*this);
    for (const auto& binding : m_optionBindings)
        binding.load();

    InitializePage();
    AdjustControls();
    m_initialized = true;
    return TRUE;
}

void COptionsPage::AdjustControls()
{
    DarkMode::AdjustControls(GetSafeHwnd());
}

void COptionsPage::SetModified(const BOOL changed)
{
    if (m_initialized || !changed)
        CMFCPropertyPage::SetModified(changed);
}

void COptionsPage::OnSettingChanged()
{
    SetModified();
}

void COptionsPage::OnSettingRangeChanged(UINT)
{
    SetModified();
}

void COptionsPage::OnSettingNotifyChanged(UINT, NMHDR*, LRESULT*)
{
    SetModified();
}

BEGIN_MESSAGE_MAP(COptionsPage, CMFCPropertyPage)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH COptionsPage::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}
