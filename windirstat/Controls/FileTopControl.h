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
#include "ItemTop.h"
#include "TreeListControl.h"

class CFileTopControl final : public CTreeListControl
{
public:
    CFileTopControl();
    ~CFileTopControl() override { m_singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileTopControl* Get() { return m_singleton; }
    void ProcessTop(CItem* item);
    void RemoveItem(CItem* items);
    void SortItems() override;

protected:

    // Custom comparator to keep the list organized by size (largest first)
    static constexpr auto CompareBySize = [](const CItem* lhs, const CItem* rhs)
    {
        return lhs->GetSizeLogical() > rhs->GetSizeLogical();
    };

    static CFileTopControl* m_singleton;
    SingleConsumerQueue<CItem*> m_queuedSet;
    std::vector<CItem*> m_sizeMap;
    ULONGLONG m_topNMinSize = 0;
    bool m_needsResort = true;
    std::unordered_map<CItem*, CItemTop*> m_itemTracker;
    size_t m_previousTopN = 0;

    void OnItemDoubleClick(int i) override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnDeleteAllItems(NMHDR* pNMHDR, LRESULT* pResult);
};
