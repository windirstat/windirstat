// FileTopControl.h - Declaration of CFileTopControl and CFileTreeView
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

#include <unordered_set>
#include <map>
#include <vector>
#include <mutex>

class CFileTopControl final : public CTreeListControl
{
public:
    CFileTopControl();
    bool GetAscendingDefault(int column) override;
    static CFileTopControl* Get() { return m_Singleton; }
    void SetRootItem(CTreeListItem* root) override;
    void ProcessTop(CItem* item);
    void RemoveItem(CItem* items);
    void SortItems() override;

    template <class T = CTreeListItem> std::vector<T*> GetAllSelected()
    {
        std::vector<T*> array;
        for (POSITION pos = GetFirstSelectedItemPosition(); pos != nullptr;)
        {
            const int i = GetNextSelectedItem(pos);
            auto item = reinterpret_cast<T*>(reinterpret_cast<CItemTop*>(GetItem(i))->GetItem());
            if (item != nullptr) array.emplace_back(item);
        }
        return array;
    }

protected:

    static CFileTopControl* m_Singleton;
    std::shared_mutex m_SizeMutex;
    std::map<ULONGLONG, std::unordered_set<CItem*>> m_SizeMap;
    std::unordered_map<CItem*, CItemTop*> m_ItemTracker;

    void OnItemDoubleClick(int i) override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};
