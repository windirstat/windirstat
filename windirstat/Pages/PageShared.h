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

//
// CSettingsPage. Shared lifecycle and option bindings for settings pages.
//
class CSettingsPage : public MessageTarget<CSettingsPage, CPropertyPage>
{
protected:
    template<std::ranges::range Bindings, typename... Owner>
    void LoadBinds(const Bindings& bindings, Owner&... owner)
    {
        for (const auto& target : bindings)
        {
            const auto& setting = ResolveBind(target, owner...);
            const auto& binding = setting.Bind();
            const auto& value = setting.Obj();
            using T = std::remove_cvref_t<decltype(value)>;
            static_assert(std::same_as<T, bool> || std::same_as<T, std::wstring> || std::same_as<T, int>);
            if constexpr (std::same_as<T, bool>) SetChecked(binding.controlId, value);
            else if constexpr (std::same_as<T, std::wstring>) SetText(binding.controlId, value);
            else if (binding.selectionOffset) SetComboSelection(binding.controlId, value - *binding.selectionOffset);
            else SetText(binding.controlId, std::to_wstring(value));
        }
    }

    template<typename Target, size_t Count, typename... Owner>
    auto ReadBinds(const std::array<Target, Count>& settings, Owner&... owner) const
    {
        auto drafts = std::apply([&](auto... setting)
        {
            return std::array{ SettingEdit{ ResolveBind(setting, owner...) }... };
        }, settings);
        using Result = std::expected<decltype(drafts), ValidationError>;
        for (auto& [setting, value] : drafts)
        {
            const auto& binding = setting.Bind();
            assert(binding.controlId != 0);
            using T = std::remove_cvref_t<decltype(value)>;
            static_assert(std::same_as<T, bool> || std::same_as<T, std::wstring> || std::same_as<T, int>);
            if constexpr (std::same_as<T, bool>) value = IsChecked(binding.controlId);
            else if constexpr (std::same_as<T, std::wstring>) value = GetText(binding.controlId);
            else
            {
                const int offset = binding.selectionOffset.value_or(0);
                auto error = binding.selectionOffset
                    ? ReadSelection(binding.controlId, value, setting.Min() - offset, setting.Max() - offset)
                    : ReadInteger(binding.controlId, value, setting.Min(), setting.Max());
                if (error) return Result{ std::unexpected(std::move(*error)) };
                value += offset;
            }
        }
        return Result{ std::move(drafts) };
    }

    template<std::ranges::range Bindings>
    void StageBinds(const Bindings& bindings)
    {
        for (const auto& [setting, value] : bindings) Stage(setting, value);
    }

    template<std::ranges::range Bindings, typename T>
    static bool BindsChanged(const Bindings& bindings, std::initializer_list<Setting<T>*> settings)
    {
        return std::ranges::any_of(bindings, [&](const auto& binding)
        {
            return std::ranges::find(settings, &binding.setting) != settings.end() && binding.Changed();
        });
    }

    explicit CSettingsPage(UINT templateId);

    bool IsInitialized() const { return m_initialized; }
    void SetModified(bool changed = true);
    std::optional<ValidationError> ReadInteger(UINT control, int& value, int minimum, int maximum) const;
    std::optional<ValidationError> ReadSelection(UINT control, int& value, int minimum, int maximum) const;
    std::optional<ValidationError> ValidateRegex(UINT control, const std::wstring& pattern) const;

    template<typename T>
    void Stage(Setting<T>& setting, T value)
    {
        Stage([&setting, value = std::move(value)] mutable { setting.Obj() = std::move(value); });
    }
    void Stage(std::function<void()> commit) { m_pending.push_back(std::move(commit)); }
    void StageEffect(std::function<void()> effect) { m_effects.push_back(std::move(effect)); }
    virtual std::optional<ValidationError> PrepareSettings() = 0;
    std::optional<ValidationError> PrepareApply() final;
    void CommitApply() final;
    void AfterApply() final;

    virtual void InitializePage() = 0;
    virtual void AdjustControls();

    bool OnInitDialog() final;

    void OnSettingChanged() { SetModified(); }

private:
    bool m_initialized = false;
    std::vector<std::function<void()>> m_pending;
    std::vector<std::function<void()>> m_effects;

public:
    static std::span<const RouteEntry> Routes();

protected:
    bool OnEraseBkgnd(CDC* pDC);
};

inline std::span<const RouteEntry> CSettingsPage::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
    };
    return entries;
}
