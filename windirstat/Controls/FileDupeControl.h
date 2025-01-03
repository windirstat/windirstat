// FileDupeControl.h - Declaration of CFileDupeControl and CFileTreeView
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

#include "ItemDupe.h"
#include "TreeListControl.h"

#include <shared_mutex>
#include <queue>
#include <set>
#include <map>

class CFileDupeControl final : public CTreeListControl
{
public:
    CFileDupeControl();
    bool GetAscendingDefault(int column) override;
    static CFileDupeControl* Get() { return m_Singleton; }
    void SetRootItem(CTreeListItem* root) override;
    void ProcessDuplicate(CItem* item, BlockingQueue<CItem*>* queue);
    void RemoveItem(CItem* items);
    void SortItems() override;

    std::shared_mutex m_HashTrackerMutex;
    std::map<ULONGLONG, std::vector<CItem*>> m_SizeTracker;
    std::map<std::vector<BYTE>, std::vector<CItem*>> m_HashTracker;
    
    std::shared_mutex m_NodeTrackerMutex;
    std::map<std::vector<BYTE>, CItemDupe*> m_NodeTracker;
    std::map<CItemDupe*, std::set<CItem*>> m_ChildTracker;
    std::vector<std::pair<CItemDupe*, CItemDupe*>> m_PendingListAdds;

protected:

    static CFileDupeControl* m_Singleton;
    bool m_ShowCloudWarningOnThisScan = false;
    
    void OnItemDoubleClick(int i) override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};
