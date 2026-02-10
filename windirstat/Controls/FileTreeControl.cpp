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

#include "pch.h"
#include "FileTreeView.h"

CFileTreeControl::CFileTreeControl() : CTreeListControl(COptions::FileTreeColumnOrder.Ptr(), COptions::FileTreeColumnWidths.Ptr(), LF_FILETREE, true)
{
    m_singleton = this;
}

bool CFileTreeControl::GetAscendingDefault(const int column)
{
    return column == COL_NAME || column == COL_LAST_CHANGE;
}

BEGIN_MESSAGE_MAP(CFileTreeControl, CTreeListControl)
    ON_WM_LBUTTONDOWN()
    ON_WM_SETCURSOR()
END_MESSAGE_MAP()

CFileTreeControl* CFileTreeControl::m_singleton = nullptr;

void CFileTreeControl::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    CTreeListControl::OnLButtonDown(nFlags, point);

    // Hit test
    LVHITTESTINFO hti{ .pt = point };
    const int i = HitTest(&hti);
    if (i == -1) return;

    // Check if item is a hardlink or hardlinks file reference
    const auto* item = reinterpret_cast<CItem*>(GetItem(i));
    if (item == nullptr || !item->IsTypeOrFlag(ITF_HARDLINK, IT_HLINKS_FILE)) return;

    // Validate if in physical size column
    if (!std::ranges::any_of(std::views::iota(0, m_columnCount), [&](const int col)
        {
            LVCOLUMN colInfo{ .mask = LVCF_SUBITEM };
            GetColumn(col, &colInfo);
            return colInfo.iSubItem == COL_SIZE_PHYSICAL && GetWholeSubitemRect(i, col).PtInRect(point);
        })) return;

    if (item->IsTypeOrFlag(ITF_HARDLINK))
    {
        // Navigate to the hardlink index item
        CItem* indexItem = item->FindHardlinksIndexItem();
        if (indexItem == nullptr) return;

        CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_SELECTIONACTION, indexItem);
        ExpandItem(indexItem);
    }
    else if (item->IsTypeOrFlag(IT_HLINKS_FILE))
    {
        // Navigate to the actual file in the tree
        CItem* linkedItem = const_cast<CItem*>(item)->GetLinkedItem();
        if (linkedItem == nullptr || linkedItem == item) return;

        CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_SELECTIONACTION, linkedItem);
    }
}

BOOL CFileTreeControl::OnSetCursor(CWnd* pWnd, const UINT nHitTest, const UINT message)
{
    auto defaultReturn = [&] { return CTreeListControl::OnSetCursor(pWnd, nHitTest, message); };
    if (nHitTest != HTCLIENT) return defaultReturn();

    CPoint point;
    GetCursorPos(&point);
    ScreenToClient(&point);

    // Hit test
    LVHITTESTINFO hti{ .pt = point };
    const int i = HitTest(&hti);
    if (i == -1) return defaultReturn();

    // Check if item is a hardlink or hardlinks file reference
    const auto* item = reinterpret_cast<CItem*>(GetItem(i));
    if (item == nullptr) return defaultReturn();
    
    // Check for ITF_HARDLINK or IT_HLINKS_FILE
    const bool isHardlink = item->IsTypeOrFlag(ITF_HARDLINK);
    const bool isHlinksFile = item->IsTypeOrFlag(IT_HLINKS_FILE);
    
    if (!isHardlink && !isHlinksFile) return defaultReturn();

    // Validate if in physical size column
    if (!std::ranges::any_of(std::views::iota(0, m_columnCount), [&](const int col)
    {
        LVCOLUMN colInfo{ .mask = LVCF_SUBITEM };
        GetColumn(col, &colInfo);
        return colInfo.iSubItem == COL_SIZE_PHYSICAL && GetWholeSubitemRect(i, col).PtInRect(point);
    })) return defaultReturn();

    SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
    return TRUE;
}
