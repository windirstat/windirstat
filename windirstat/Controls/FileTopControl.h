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

#include "ItemTop.h"
#include "TreeListControl.h"

#include <set>
#include <unordered_map>
#include <shared_mutex>

class CFileTopControl final : public CTreeListControl
{
public:
    CFileTopControl();
    ~CFileTopControl() override { m_Singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileTopControl* Get() { return m_Singleton; }
    void ProcessTop(CItem* item);
    void RemoveItem(CItem* items);
    void SortItems() override;

protected:

    // Custom comparator to keep the list organized by size
    static constexpr auto CompareBySize = [](const CItem* lhs, const CItem* rhs)
    {
        return lhs->GetSizeLogical() > rhs->GetSizeLogical();
    };

    static CFileTopControl* m_Singleton;
    std::mutex m_SizeMutex;
    std::vector<CItem*> m_QueuedSet;
    std::multiset<CItem*, decltype(CompareBySize)> m_SizeMap;
    std::unordered_map<CItem*, CItemTop*> m_ItemTracker;

    void OnItemDoubleClick(int i) override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnDeleteAllItems(NMHDR* pNMHDR, LRESULT* pResult);
};
