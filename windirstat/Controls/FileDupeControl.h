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
    bool m_showCloudWarningOnThisScan = COptions::ShowDupeDetectionCloudLinksWarning;

};
