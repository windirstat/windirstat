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
#include "ItemSearch.h"
#include "TreeListControl.h"

class CFileSearchControl final : public CTreeListControl
{
public:
    CFileSearchControl();
    ~CFileSearchControl() override { m_singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileSearchControl* Get() { return m_singleton; }
    static std::wregex ComputeSearchRegex(const std::wstring& searchTerm, bool searchCase, bool useRegex);
    void ProcessSearch(CItem* item);
    void RemoveItem(CItem* items);

protected:

    static CFileSearchControl* m_singleton;
    std::unordered_map<CItem*, CItemSearch*> m_itemTracker;

    void OnItemDoubleClick(int i) override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg BOOL OnDeleteAllItems(NMHDR* pNMHDR, LRESULT* pResult);
};
