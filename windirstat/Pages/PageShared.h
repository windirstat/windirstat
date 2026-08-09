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

#pragma once

#include "pch.h"

class CSettingsSheet;

//
// CSettingsPage. Shared lifecycle and option bindings for settings pages.
//
class CSettingsPage : public MessageTarget<CSettingsPage, CPropertyPage>
{
protected:
    struct CheckboxSettingBinding
    {
        UINT controlId;
        Setting<bool>& setting;
    };

    explicit CSettingsPage(UINT templateId);

    CSettingsSheet* GetSheet() const;
    bool IsInitialized() const { return m_initialized; }
    void SetModified(bool changed = true);
    void LoadCheckboxSettings(std::span<const CheckboxSettingBinding> bindings);
    void SaveCheckboxSettings(std::span<const CheckboxSettingBinding> bindings);

    virtual void InitializePage() = 0;
    virtual void AdjustControls();

    bool OnInitDialog() final;

    void OnSettingChanged();
    void OnSettingRangeChanged(UINT id);
    void OnSettingNotifyChanged(UINT id, NMHDR*, LRESULT*);

private:
    bool m_initialized = false;

public:
    static std::span<const RouteEntry> Routes();

protected:
    bool OnEraseBkgnd(CDC* pDC);
    HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};

inline std::span<const RouteEntry> CSettingsPage::Routes()
{
    using ThisClass = CSettingsPage;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&ThisClass::OnCtlColor>(WM_CTLCOLOR),
    };
    return entries;
}
