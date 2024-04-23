// FileTreeControl.h - Declaration of CFileDupeControl and CFileTreeView
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

#include "ItemDupe.h"
#include "TreeListControl.h"

#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>

class CFileDupeControl final : public CTreeListControl
{
public:
    CFileDupeControl();
    bool GetAscendingDefault(int column) override;
    static CFileDupeControl* Get() { return m_Singleton; }
    void InsertItem(int i, CTreeListItem* item);
    void SetRootItem(CTreeListItem* root) override;
    void ProcessDuplicate(CItem* item);
    void RemoveItem(CItem* items);

    std::shared_mutex m_Mutex;
    std::unordered_map<ULONGLONG, std::unordered_set<CItem*>> m_SizeTracker;
    std::unordered_map<std::wstring, CItemDupe*> m_NodeTracker;
    std::unordered_map<std::wstring, std::unordered_set<CItem*>> m_HashTracker;

    template <class T = CTreeListItem> std::vector<T*> GetAllSelected()
    {
        std::vector<T*> array;
        for (POSITION pos = GetFirstSelectedItemPosition(); pos != nullptr;)
        {
            const int i = GetNextSelectedItem(pos);
            array.push_back(reinterpret_cast<T*>(
                reinterpret_cast<CItemDupe*>(GetItem(i))->GetItem()));
        }
        return array;
    }

protected:

    static CFileDupeControl* m_Singleton;
    
    void OnItemDoubleClick(int i) override;
    void PrepareDefaultMenu(CMenu* menu, const CItemDupe* item);

    DECLARE_MESSAGE_MAP()
    afx_msg void OnLvnItemchangingList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};
