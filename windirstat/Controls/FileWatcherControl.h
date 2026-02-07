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
#include "TreeListControl.h"

using ITEMWATCHCOLUMNS = enum : std::uint8_t
{
    COL_ITEMWATCH_NAME,
    COL_ITEMWATCH_TIME,
    COL_ITEMWATCH_ACTION,
    COL_ITEMWATCH_SIZE_LOGICAL,
};

class CWatcherItem final : public CTreeListItem
{
public:
    CWatcherItem(const std::wstring& path, const std::wstring& action, const FILETIME& timestamp, const ULONGLONG fileSize, const DWORD attributes)
        : m_action(action)
    {
        m_item = std::make_unique<CItem>(IT_FILE, path, timestamp, fileSize, fileSize, 0, attributes, 0, 0);
    }

    ~CWatcherItem() override = default;

    std::wstring GetText(const int subitem) const override
    {
        if (subitem == COL_ITEMWATCH_TIME) return FormatFileTime(m_item->GetLastChange(), true);
        if (subitem == COL_ITEMWATCH_NAME) return m_item->GetPath();
        if (subitem == COL_ITEMWATCH_ACTION) return m_action;
        if (subitem == COL_ITEMWATCH_SIZE_LOGICAL) return FormatSizeSuffixes(m_item->GetSizeLogical());
        return {};
    }

    // CTreeListItem required overrides
    int CompareSibling(const CTreeListItem* other, int subitem) const override
    {
        const auto* otherItem = reinterpret_cast<const CWatcherItem*>(other);
        if (subitem == COL_ITEMWATCH_ACTION) return signum(_wcsicmp(m_action.c_str(), otherItem->m_action.c_str()));
        return m_item->CompareSibling(otherItem->m_item.get(), s_columnMap.at(static_cast<uint8_t>(subitem)));
    }

    CTreeListItem* GetTreeListChild(int) const override { return nullptr; }
    int GetTreeListChildCount() const override { return 0; }

    CItem* GetLinkedItem() noexcept override { return m_item.get(); }
    HICON GetIcon() override;

private:

    std::unique_ptr<CItem> m_item;
    std::wstring m_action;
    static const std::unordered_map<uint8_t, uint8_t> s_columnMap;
};

class CFileWatcherControl final : public CTreeListControl
{
public:
    CFileWatcherControl();
    ~CFileWatcherControl() override;

    static CFileWatcherControl* Get() { return m_singleton; }

    void StartMonitoring();
    void StopMonitoring();
    bool IsMonitoring() const;

protected:
    static CFileWatcherControl* m_singleton;

    std::vector<std::jthread> m_watchThreads;
    void WatchDirectory(const std::wstring& path, const std::stop_token& stopToken);
    void AddChange(const std::wstring& path, DWORD action);

    std::vector<std::unique_ptr<CWatcherItem>> m_items;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnDestroy();
    afx_msg void OnSetFocus(CWnd* pOldWnd);
};
