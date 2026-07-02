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
#include "ItemPerm.h"

std::atomic<int> CItemPerm::m_ruleVersion = 0;

// Cumulative rights required to satisfy each summarized level
static constexpr std::array<ACCESS_MASK, PERMSLEVEL_COUNT> levelMasks =
{
    FILE_ALL_ACCESS,
    FILE_GENERIC_READ | FILE_GENERIC_WRITE | FILE_GENERIC_EXECUTE | DELETE,
    FILE_GENERIC_READ | FILE_GENERIC_EXECUTE,
    FILE_GENERIC_READ,
    FILE_GENERIC_WRITE,
    0
};

namespace
{
std::wstring StripMnemonicMarkers(std::wstring text)
{
    // East Asian translations often append mnemonics like "(&Y)"; remove that block first.
    if (const size_t open = text.rfind(L"(&"); open != std::wstring::npos && text.size() == open + 4 && text.back() == L')')
    {
        text.erase(open);
    }

    std::wstring stripped;
    stripped.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == L'&')
        {
            if (i + 1 < text.size() && text[i + 1] == L'&')
            {
                stripped += L'&';
                ++i;
            }
            continue;
        }
        stripped += text[i];
    }
    return stripped;
}
}

CItemPerm::CItemPerm(const std::wstring& path, const DWORD attributes,
    std::wstring account, const ACCESS_MASK mask, const bool deny, const BYTE aceFlags, const bool inheritanceDisabled) : m_account(std::move(account)),
    m_mask(mask), m_isContainer((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0),
    m_level(ComputeRightsLevel(mask)), m_applies(ComputeApplies(aceFlags, m_isContainer)), m_deny(deny),
    m_inheritanceDisabled(inheritanceDisabled)
{
    m_item = std::make_unique<CItem>(m_isContainer ? IT_DIRECTORY : IT_FILE, path, FILETIME{}, 0, 0, 0, attributes, 0, 0);
}

std::wstring CItemPerm::GetAppliesText() const
{
    return GetAppliesName(m_applies, m_isContainer);
}

std::wstring CItemPerm::GetText(const int subitem) const
{
    if (subitem == COL_ITEMPERM_NAME) return m_item->GetPath();
    if (subitem == COL_ITEMPERM_ACCOUNT) return m_account;
    if (subitem == COL_ITEMPERM_TYPE) return GetAccessTypeName(m_deny);
    if (subitem == COL_ITEMPERM_RIGHTS) return m_level == PERMSLEVEL_SPECIAL ?
        std::format(L"{} (0x{:08X})", GetRightsLevelName(m_level), m_mask) : GetRightsLevelName(m_level);
    if (subitem == COL_ITEMPERM_APPLIESTO) return GetAppliesText();
    if (subitem == COL_ITEMPERM_INHERITANCE) return GetInheritedName(m_inheritanceDisabled);
    return {};
}

int CItemPerm::CompareSibling(const CTreeListItem* other, const int subitem) const
{
    const auto* otherItem = static_cast<const CItemPerm*>(other);
    switch (subitem)
    {
    case COL_ITEMPERM_NAME:    return m_item->ComparePath(otherItem->m_item.get());
    case COL_ITEMPERM_ACCOUNT: return signum(_wcsicmp(m_account.c_str(), otherItem->m_account.c_str()));
    case COL_ITEMPERM_TYPE:    return usignum(m_deny, otherItem->m_deny);
    case COL_ITEMPERM_RIGHTS:  return m_level != otherItem->m_level ?
        usignum(m_level, otherItem->m_level) : usignum(m_mask, otherItem->m_mask);
    case COL_ITEMPERM_APPLIESTO: return signum(_wcsicmp(GetAppliesText().c_str(), otherItem->GetAppliesText().c_str()));
    case COL_ITEMPERM_INHERITANCE: return usignum(m_inheritanceDisabled, otherItem->m_inheritanceDisabled);
    default:                   return 0;
    }
}

HICON CItemPerm::GetIcon()
{
    // No icon to return if not visible yet
    if (m_visualInfo == nullptr)
    {
        return nullptr;
    }

    // Return previously cached value
    if (m_visualInfo->icon != nullptr)
    {
        return m_visualInfo->icon;
    }

    // Fetch all icons
    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        m_visualInfo->control, m_item->GetPath(), m_item->GetAttributes(), &m_visualInfo->icon, nullptr));
    return nullptr;
}

COLORREF CItemPerm::GetItemTextColor() const
{
    // Recompute the rule color only when the configured rules have changed
    if (m_colorVersion != GetRuleVersion())
    {
        m_colorVersion = GetRuleVersion();
        m_color = GetRuleColor(m_account, m_mask);
    }
    return m_color != CLR_NONE ? m_color : CTreeListItem::GetItemTextColor();
}

PERMSLEVEL CItemPerm::ComputeRightsLevel(const ACCESS_MASK mask)
{
    for (const int level : std::views::iota(0, static_cast<int>(PERMSLEVEL_SPECIAL)))
    {
        if ((mask & levelMasks[level]) == levelMasks[level]) return static_cast<PERMSLEVEL>(level);
    }
    return PERMSLEVEL_SPECIAL;
}

bool CItemPerm::LevelSatisfied(const ACCESS_MASK mask, const PERMSLEVEL level)
{
    if (level == PERMSLEVEL_SPECIAL) return ComputeRightsLevel(mask) == PERMSLEVEL_SPECIAL;
    return (mask & levelMasks[level]) == levelMasks[level];
}

std::wstring CItemPerm::GetRightsLevelName(const PERMSLEVEL level)
{
    static auto names = SplitString(Localization::Lookup(IDS_PERMS_LEVELS), L',');
    return level < names.size() ? names[level] : std::wstring{};
}

std::wstring CItemPerm::GetAccessTypeName(const bool deny)
{
    static auto names = SplitString(Localization::Lookup(IDS_PERMS_TYPES), L',');
    const size_t index = deny ? 1 : 0;
    return index < names.size() ? names[index] : std::wstring{};
}

PERMSAPPLIES CItemPerm::ComputeApplies(const BYTE aceFlags, const bool isContainer)
{
    // Inheritance flags only have meaning on containers; files apply to themselves only
    if (!isContainer) return PERMSAPPLIES_THIS;
    const bool oi = (aceFlags & OBJECT_INHERIT_ACE) != 0;
    const bool ci = (aceFlags & CONTAINER_INHERIT_ACE) != 0;
    const bool io = (aceFlags & INHERIT_ONLY_ACE) != 0;

    // Map the OI/CI/IO combination to the standard "Applies to" classification
    if (oi && ci) return io ? PERMSAPPLIES_SUB_FILES : PERMSAPPLIES_THIS_SUB_FILES;
    if (ci)       return io ? PERMSAPPLIES_SUB : PERMSAPPLIES_THIS_SUB;
    if (oi)       return io ? PERMSAPPLIES_FILES : PERMSAPPLIES_THIS_FILES;
    return PERMSAPPLIES_THIS;
}

std::wstring CItemPerm::GetAppliesName(const PERMSAPPLIES applies, const bool isContainer)
{
    // Files have no inheritance scope so they use a dedicated single label
    if (!isContainer) return Localization::Lookup(IDS_PERMS_APPLIES_FILE);
    static auto names = SplitString(Localization::Lookup(IDS_PERMS_APPLIES), L'|');
    return applies < names.size() ? names[applies] : std::wstring{};
}

std::wstring CItemPerm::GetInheritedName(const bool disabled)
{
    // Reuse the shared yes/no strings, but strip button mnemonics before showing them in the grid/export.
    static const std::wstring yes = StripMnemonicMarkers(Localization::Lookup(IDS_GENERIC_YES));
    static const std::wstring no  = StripMnemonicMarkers(Localization::Lookup(IDS_GENERIC_NO));
    return disabled ? no : yes;
}

COLORREF CItemPerm::GetRuleColor(const std::wstring& account, const ACCESS_MASK mask)
{
    // Account patterns are case-insensitive regular expressions; compile them once per change
    struct Rule { std::wregex account; int level; COLORREF color; };
    static std::vector<Rule> rules;
    static int cachedVersion = -1;
    if (cachedVersion != m_ruleVersion.load())
    {
        cachedVersion = m_ruleVersion.load();
        rules.clear();
        for (const auto r : std::views::iota(0, PERMSRULECOUNT))
        {
            const std::wstring& pattern = COptions::PermsColorAccount[r].Obj();
            if (pattern.empty()) continue;
            try { rules.emplace_back(std::wregex(pattern, std::regex::icase | std::regex::optimize),
                COptions::PermsColorLevel[r], COptions::PermsColor[r]); }
            catch (const std::regex_error&) {}
        }
    }

    // Return the color of the first matching rule
    for (const auto& rule : rules)
    {
        if (!std::regex_search(account, rule.account)) continue;
        if (rule.level == 0 || LevelSatisfied(mask, static_cast<PERMSLEVEL>(rule.level - 1)))
            return rule.color;
    }
    return CLR_NONE;
}
