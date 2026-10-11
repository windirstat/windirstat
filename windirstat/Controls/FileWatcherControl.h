// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "TreeListControl.h"

enum ITEMWATCHCOLUMNS : std::uint8_t
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
        : m_path(path), m_action(action), m_timestamp(timestamp), m_fileSize(fileSize), m_attributes(attributes) {}

    ~CWatcherItem() override = default;

    std::wstring GetText(const int subitem) const override
    {
        if (subitem == COL_ITEMWATCH_TIME) return FormatFileTime(m_timestamp, true);
        if (subitem == COL_ITEMWATCH_NAME) return m_path;
        if (subitem == COL_ITEMWATCH_ACTION) return m_action;
        if (subitem == COL_ITEMWATCH_SIZE_LOGICAL) return FormatSizeSuffixes(m_fileSize);
        return {};
    }

    // CTreeListItem required overrides
    int CompareSibling(const CTreeListItem* other, const int subitem) const override
    {
        const auto* otherItem = static_cast<const CWatcherItem*>(other);
        if (subitem == COL_ITEMWATCH_ACTION) return signum(_wcsicmp(m_action.c_str(), otherItem->m_action.c_str()));

        switch (subitem)
        {
        case COL_ITEMWATCH_TIME:          return CompareFileTime(&m_timestamp, &otherItem->m_timestamp);
        case COL_ITEMWATCH_NAME:          return signum(_wcsicmp(m_path.c_str(), otherItem->m_path.c_str()));
        case COL_ITEMWATCH_SIZE_LOGICAL:  return usignum(m_fileSize, otherItem->m_fileSize);
        default:                          return 0;
        }
    }

    CTreeListItem* GetTreeListChild(int) const override { return nullptr; }
    int GetTreeListChildCount() const override { return 0; }

    CItem* GetLinkedItem() override;
    HICON GetIcon() override;
    const std::wstring& GetPath() const { return m_path; }

private:

    std::unique_ptr<CItem> m_item;
    std::wstring m_path;
    std::wstring m_action;
    FILETIME m_timestamp;
    ULONGLONG m_fileSize;
    DWORD m_attributes;
};

class CFileWatcherControl final : public MessageTarget<CFileWatcherControl, CTreeListControl>
{
public:
    CFileWatcherControl();
    ~CFileWatcherControl() override;

    static CFileWatcherControl* Get() { return m_singleton; }

    void StartMonitoring();
    void StopMonitoring();
    bool IsMonitoring() const { return !m_watchThreads.empty(); }
    void ClearResults();
    bool HasResults() const { return !m_history.empty() || !m_pendingItems.empty(); }
    bool SetQuickFilter(const std::wstring& pattern);

protected:
    inline static CFileWatcherControl* m_singleton = nullptr;

    std::vector<std::jthread> m_watchThreads;
    SingleConsumerQueue<CWatcherItem*> m_pendingItems;
    std::atomic<bool> m_changePending = false;
    std::vector<std::unique_ptr<CWatcherItem>> m_history;
    std::optional<std::wregex> m_quickFilter;

    static constexpr DWORD WM_WATCHER_CHANGE = WM_APP + 2;

    void WatchDirectory(const std::wstring& path, const std::stop_token& stopToken);
    void AddChange(const std::wstring& path, DWORD action);
    void ClearPendingItems();
    void PostWatcherChange();

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnDestroy();
    LRESULT OnWatcherChange(WPARAM wParam, LPARAM lParam);
};

inline std::span<const RouteEntry> CFileWatcherControl::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnDestroy>(WM_DESTROY),
        Route::Window<&OnWatcherChange>(WM_WATCHER_CHANGE),
    };
    return entries;
}
