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

class COptionsPropertySheet;

//
// COptionsPage. Shared lifecycle and option bindings for settings pages.
//
class COptionsPage : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(COptionsPage)

protected:
    explicit COptionsPage(UINT templateId);

    COptionsPropertySheet* GetSheet() const;
    bool IsInitialized() const { return m_initialized; }
    void SetModified(BOOL changed = TRUE);
    void ApplyOptionBindings() const;

    void BindCheck(int id, Setting<bool>& option, BOOL& value);
    void BindCombo(int id, Setting<int>& option, int& value);
    void BindRadio(int id, Setting<int>& option, int& value);
    void BindText(int id, Setting<int>& option, int& value);
    void BindText(int id, Setting<std::wstring>& option, CStringW& value);

    template <typename T, typename Value>
    void BindOption(Setting<T>& option, Value& value, std::function<void(CDataExchange*)> exchange = {})
    {
        m_optionBindings.push_back({
            [&option, &value] { value = static_cast<Value>(option.Obj()); },
            std::move(exchange),
            [&option, &value] { option = static_cast<T>(value); },
        });
    }

    virtual void InitializePage() = 0;
    virtual void AdjustControls();

    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() final;

    afx_msg void OnSettingChanged();
    afx_msg void OnSettingRangeChanged(UINT id);
    afx_msg void OnSettingNotifyChanged(UINT id, NMHDR*, LRESULT*);

private:
    struct OptionBinding
    {
        std::function<void()> load;
        std::function<void(CDataExchange*)> exchange;
        std::function<void()> save;
    };

    std::vector<OptionBinding> m_optionBindings;
    bool m_initialized = false;

    DECLARE_MESSAGE_MAP()
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
