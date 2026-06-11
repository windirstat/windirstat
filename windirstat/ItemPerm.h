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

// Columns
enum ITEMPERMCOLUMNS : std::uint8_t
{
    COL_ITEMPERM_NAME,
    COL_ITEMPERM_ACCOUNT,
    COL_ITEMPERM_TYPE,
    COL_ITEMPERM_RIGHTS,
    COL_ITEMPERM_APPLIESTO,
    COL_ITEMPERM_INHERITANCE,
};

// Downward inheritance scope of an ACE, mapped from OI/CI/IO flags (folder semantics)
enum PERMSAPPLIES : std::uint8_t
{
    PERMSAPPLIES_THIS,            // This folder only
    PERMSAPPLIES_THIS_SUB_FILES,  // This folder, subfolders and files
    PERMSAPPLIES_THIS_SUB,        // This folder and subfolders
    PERMSAPPLIES_THIS_FILES,      // This folder and files
    PERMSAPPLIES_SUB_FILES,       // Subfolders and files only
    PERMSAPPLIES_SUB,             // Subfolders only
    PERMSAPPLIES_FILES,           // Files only
    PERMSAPPLIES_COUNT
};

// Summarized rights levels in display / matching priority order
enum PERMSLEVEL : std::uint8_t
{
    PERMSLEVEL_FULL,
    PERMSLEVEL_MODIFY,
    PERMSLEVEL_READ_EXECUTE,
    PERMSLEVEL_READ,
    PERMSLEVEL_WRITE,
    PERMSLEVEL_SPECIAL,
    PERMSLEVEL_COUNT
};

class CItemPerm final : public CTreeListItem
{
public:
    CItemPerm(const std::wstring& path, DWORD attributes, std::wstring account, ACCESS_MASK mask, bool deny, BYTE aceFlags, bool inheritanceDisabled);
    ~CItemPerm() override = default;

    // CTreeListItem required overrides
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* other, int subitem) const override;
    CTreeListItem* GetTreeListChild(int) const override { return nullptr; }
    int GetTreeListChildCount() const override { return 0; }
    CItem* GetLinkedItem() noexcept override { return m_item.get(); }
    HICON GetIcon() override;
    COLORREF GetItemTextColor() const override;

    std::wstring GetPath() const { return m_item->GetPath(); }
    const std::wstring& GetAccount() const { return m_account; }
    ACCESS_MASK GetAccessMask() const { return m_mask; }
    bool IsDeny() const { return m_deny; }
    bool IsInheritanceDisabled() const { return m_inheritanceDisabled; }
    std::wstring GetAppliesText() const;

    // Rights summarization and inheritance-scope helpers
    static PERMSLEVEL ComputeRightsLevel(ACCESS_MASK mask);
    static bool LevelSatisfied(ACCESS_MASK mask, PERMSLEVEL level);
    static std::wstring GetRightsLevelName(PERMSLEVEL level);
    static std::wstring GetAccessTypeName(bool deny);
    static PERMSAPPLIES ComputeApplies(BYTE aceFlags, bool isContainer);
    static std::wstring GetAppliesName(PERMSAPPLIES applies, bool isContainer);
    static std::wstring GetInheritedName(bool disabled);

    // Colorize rule helpers; bump the version to force a recompute and repaint
    static COLORREF GetRuleColor(const std::wstring& account, ACCESS_MASK mask);
    static int GetRuleVersion() { return m_ruleVersion.load(); }
    static void InvalidateRuleColors() { ++m_ruleVersion; }

private:
    static std::atomic<int> m_ruleVersion;

    std::unique_ptr<CItem> m_item;
    std::wstring m_account;
    ACCESS_MASK m_mask;
    bool m_isContainer;
    PERMSLEVEL m_level;
    PERMSAPPLIES m_applies;
    bool m_deny;
    bool m_inheritanceDisabled;
    mutable COLORREF m_color = CLR_NONE;
    mutable int m_colorVersion = -1;
};
