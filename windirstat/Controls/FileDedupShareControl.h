// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ItemDedupShare.h"
#include "TreeListControl.h"
#include "DedupAccounting.h"

// Lists the files sharing deduplicated chunks with a selection
class CFileDedupShareControl final : public CTreeListControl
{
public:
    CFileDedupShareControl();
    ~CFileDedupShareControl() override { m_singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileDedupShareControl* Get() { return m_singleton; }

    // Fills the tab with the files sharing data with the given items
    void ProcessSharing(const std::vector<CItem*>& items);

    // Refreshing a volume rebuilds its accounting, so the rows of the affected
    // volumes are dropped before the refresh and queried again afterwards
    void RemoveVolumes(const std::unordered_set<CItem*>& volumes);
    void RestoreItems();

    void AfterDeleteAllItems() override;

protected:

    void Populate(const std::vector<CItem*>& items, const CDedupAccounting::SharingResult& sharing,
        const std::unordered_set<std::wstring>& selectPaths);

    inline static CFileDedupShareControl* m_singleton = nullptr;
    CItemDedupShare* m_rootItem = nullptr;
    std::vector<CItem*> m_queried;            // items the rows were computed for
    std::vector<std::wstring> m_pendingPaths; // queried items to look up after a refresh
    std::unordered_set<std::wstring> m_pendingSelection;
};
