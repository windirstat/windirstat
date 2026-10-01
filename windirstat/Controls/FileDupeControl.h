// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "ItemDupe.h"
#include "TreeListControl.h"

class CFileDupeControl final : public CTreeListControl
{
public:
    CFileDupeControl();
    ~CFileDupeControl() override { m_singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileDupeControl* Get() { return m_singleton; }
    CItemDupe* GetRootItem() const { return m_rootItem; }
    void ProcessDuplicate(CItem* item, BlockingQueue<CItem*>* queue);
    void RemoveItem(CItem* item);
    void SortItems() override;
    void AfterDeleteAllItems() override;

    using HashKey = std::pair<ULONGLONG, std::vector<BYTE>>;
    using HashTracker = std::map<HashKey, std::vector<CItem*>>;
    std::mutex m_trackerMutex;
    std::map<ULONGLONG, std::vector<CItem*>> m_sizeTracker;
    std::array<HashTracker, 3> m_hashTrackers;
    std::vector<std::pair<CItem*, size_t>> m_pendingHashes;

    std::mutex m_nodeTrackerMutex;
    std::map<HashKey, CItemDupe*> m_nodeTracker;
    std::map<CItemDupe*, std::set<CItem*>> m_childTracker;

    SingleConsumerQueue<std::pair<CItemDupe*, CItemDupe*>> m_pendingListAdds;

protected:

    constexpr static ULONGLONG HashThreshold(const ITEMTYPE hashLevel)
    {
        return
            hashLevel == ITHASH_SMALL ? 4ull * wds::Ki :
            hashLevel == ITHASH_MEDIUM ? wds::Mi : std::numeric_limits<ULONGLONG>::max();
    }

    inline static CFileDupeControl* m_singleton = nullptr;
    CItemDupe* m_rootItem = nullptr;
    bool m_sampleLargeFiles = COptions::SampleLargeFiles;
    std::atomic<bool> m_showCloudWarningOnThisScan{ COptions::ShowDupeDetectionCloudLinksWarning.Obj() };

};
