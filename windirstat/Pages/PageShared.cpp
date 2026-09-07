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

CSettingsPage::CSettingsPage(const UINT templateId) : MessageTarget(templateId) {}

CSettingsSheet* CSettingsPage::GetSheet() const
{
    const auto sheet = static_cast<CSettingsSheet*>(GetParent());
    assert(sheet != nullptr);
    return sheet;
}

bool CSettingsPage::OnInitDialog()
{
    if (!CPropertyPage::OnInitDialog()) return false;
    Localization::UpdateDialogs(*this);

    InitializePage();
    AdjustControls();
    m_initialized = true;
    return true;
}

void CSettingsPage::LoadCheckboxSettings(const std::span<const CheckboxSettingBinding> bindings)
{
    for (const auto& [controlId, setting] : bindings)
        SetChecked(controlId, setting.Obj());
}

void CSettingsPage::SaveCheckboxSettings(const std::span<const CheckboxSettingBinding> bindings)
{
    for (const auto& [controlId, setting] : bindings)
        setting = IsChecked(controlId);
}

void CSettingsPage::AdjustControls()
{
    DarkMode::AdjustControls(Handle());
}

void CSettingsPage::SetModified(const bool changed)
{
    if (m_initialized || !changed)
        CPropertyPage::SetModified(changed);
}

void CSettingsPage::OnSettingChanged()
{
    SetModified();
}

void CSettingsPage::OnSettingRangeChanged(UINT)
{
    SetModified();
}

void CSettingsPage::OnSettingNotifyChanged(UINT, NMHDR*, LRESULT*)
{
    SetModified();
}

bool CSettingsPage::OnEraseBkgnd(CDC* pDC)
{
    const CRect rect = ClientRect();
    pDC->FillSolidRect(&rect, DarkMode::SystemColor(
        DarkMode::IsDarkModeActive() ? COLOR_WINDOW : COLOR_BTNFACE));
    return true;
}

HBRUSH CSettingsPage::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}
