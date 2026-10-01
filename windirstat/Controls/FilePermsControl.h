// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "TreeListControl.h"
#include "ItemPerm.h"

class CFilePermsControl final : public MessageTarget<CFilePermsControl, CTreeListControl>
{
public:
    CFilePermsControl();
    ~CFilePermsControl() override;

    static CFilePermsControl* Get() { return m_singleton; }

    bool StartScan();
    std::vector<const CItemPerm*> GetPermItems() const;

    // Headless parallel scan of the whole tree; used by the silent command-line export
    static std::vector<CItemPerm*> ScanTree(const CItem* docRoot);

protected:
    inline static CFilePermsControl* m_singleton = nullptr;

    // Collect the items to scan and flag those whose every ACE (incl. inherited) should be listed
    static std::vector<const CItem*> BuildScanList(const CItem* docRoot, std::unordered_set<const CItem*>& roots);
    // Scan items in parallel, reporting completion via onItemDone and honoring cancelled
    static std::vector<CItemPerm*> ScanItems(const std::vector<const CItem*>& items, const std::unordered_set<const CItem*>& roots,
        const std::function<bool()>& cancelled, const std::function<void()>& onItemDone);
    // Read one item's DACL and build a row per qualifying ACE; safe to call from worker threads
    static std::vector<CItemPerm*> ScanItem(const CItem* item, bool includeInherited, const std::optional<std::wregex>& excludeRegex);

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnDestroy();
};

inline std::span<const RouteEntry> CFilePermsControl::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnDestroy>(WM_DESTROY),
    };
    return entries;
}
