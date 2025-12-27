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
    void ProcessDuplicate(CItem* item, BlockingQueue<CItem*>* queue);
    void RemoveItem(CItem* items);
    void SortItems() override;

    std::mutex m_hashTrackerMutex;
    std::map<ULONGLONG, std::vector<CItem*>> m_sizeTracker;
    std::map<std::vector<BYTE>, std::vector<CItem*>> m_hashTrackerSmall;
    std::map<std::vector<BYTE>, std::vector<CItem*>> m_hashTrackerLarge;
    
    std::mutex m_nodeTrackerMutex;
    std::map<std::vector<BYTE>, CItemDupe*> m_nodeTracker;
    std::map<CItemDupe*, std::set<CItem*>> m_childTracker;
    
    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to alignment specifier
    SingleConsumerQueue<std::pair<CItemDupe*, CItemDupe*>> m_pendingListAdds;
    #pragma warning(pop)

protected:

    static constexpr auto m_partialBufferSize = 4ull * 1024ull;
    static CFileDupeControl* m_singleton;
    bool m_showCloudWarningOnThisScan = false;
    
    void OnItemDoubleClick(int i) override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnDeleteAllItems(NMHDR* pNMHDR, LRESULT* pResult);
};
