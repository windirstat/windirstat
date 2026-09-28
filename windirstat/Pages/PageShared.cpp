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

bool CSettingsPage::OnInitDialog()
{
    if (!CPropertyPage::OnInitDialog()) return false;
    Localization::UpdateDialogs(*this);

    InitializePage();
    AdjustControls();
    m_initialized = true;
    return true;
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

bool CSettingsPage::OnEraseBkgnd(CDC* pDC)
{
    const CRect rect = GetClientRect();
    pDC->FillSolidRect(rect, DarkMode::SystemColor(
        DarkMode::IsDarkModeActive() ? COLOR_WINDOW : COLOR_BTNFACE));
    return true;
}

std::optional<CPropertyPage::ValidationError> CSettingsPage::PrepareApply()
{
    m_pending.clear();
    m_effects.clear();
    return PrepareSettings();
}

void CSettingsPage::CommitApply()
{
    for (auto& commit : m_pending) commit();
    m_pending.clear();
}

void CSettingsPage::AfterApply()
{
    for (auto& effect : m_effects) effect();
    m_effects.clear();
}

std::optional<CPropertyPage::ValidationError> CSettingsPage::ReadInteger(
    const UINT control, int& value, const int minimum, const int maximum) const
{
    const std::wstring text = GetText(control);
    wchar_t* end = nullptr;
    const long long parsed = std::wcstoll(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != L'\0' || parsed < minimum || parsed > maximum)
        return ValidationError{ ::GetDlgItem(Handle(), control),
            std::format(L"{}\n[{}, {}]", TranslateError(ERROR_INVALID_DATA), minimum, maximum) };
    value = static_cast<int>(parsed);
    return {};
}

std::optional<CPropertyPage::ValidationError> CSettingsPage::ReadSelection(
    const UINT control, int& value, const int minimum, const int maximum) const
{
    value = GetComboSelection(control);
    if (value < minimum || value > maximum)
        return ValidationError{ ::GetDlgItem(Handle(), control),
            TranslateError(ERROR_INVALID_DATA) };
    return {};
}

std::optional<CPropertyPage::ValidationError> CSettingsPage::ValidateRegex(
    const UINT control, const std::wstring& pattern) const
{
    try { std::wregex{ pattern, std::regex::icase | std::regex::optimize }; }
    catch (const std::regex_error&)
    {
        return ValidationError{ ::GetDlgItem(Handle(), control),
            Localization::Lookup(IDS_PAGE_FILTERING_INVALID_FILTER) + L" " + pattern };
    }
    return {};
}
