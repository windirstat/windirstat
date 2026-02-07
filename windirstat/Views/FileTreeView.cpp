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

IMPLEMENT_DYNCREATE(CFileTreeView, CControlView)

BEGIN_MESSAGE_MAP(CFileTreeView, CControlView)
    ON_WM_CREATE()
END_MESSAGE_MAP()

void CFileTreeView::CreateColumns(const bool all)
{
    if (all)
    {
        // Columns should be in enumeration order so initial sort will work
        InsertCol(IDS_COL_NAME, LVCFMT_LEFT, 250, COL_NAME);
        InsertCol(IDS_COL_SUBTREE_PERCENTAGE, LVCFMT_RIGHT, CItem::GetSubtreePercentageWidth() + 30, COL_SUBTREE_PERCENTAGE);
        InsertCol(IDS_COL_PERCENTAGE, LVCFMT_RIGHT, 90, COL_PERCENTAGE);
    }

    // reset sort and remove optional columns
    m_control.SetSorting(COL_PERCENTAGE, m_control.GetAscendingDefault(COL_PERCENTAGE));
    m_control.SortItems();
    while (m_control.DeleteColumn(COL_OPTIONAL_START)) {}

    // add optional columns based on settings
    if (COptions::ShowColumnSizePhysical) InsertCol(IDS_COL_SIZE_PHYSICAL, LVCFMT_RIGHT, 90, COL_SIZE_PHYSICAL);
    if (COptions::ShowColumnSizeLogical) InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 90, COL_SIZE_LOGICAL);
    if (COptions::ShowColumnItems) InsertCol(IDS_COL_ITEMS, LVCFMT_RIGHT, 90, COL_ITEMS);
    if (COptions::ShowColumnFiles) InsertCol(IDS_COL_FILES, LVCFMT_RIGHT, 90, COL_FILES);
    if (COptions::ShowColumnFolders) InsertCol(IDS_COL_FOLDERS, LVCFMT_RIGHT, 90, COL_FOLDERS);
    if (COptions::ShowColumnLastChange) InsertCol(IDS_COL_LAST_CHANGE, LVCFMT_LEFT, 120, COL_LAST_CHANGE);
    if (COptions::ShowColumnAttributes) InsertCol(IDS_COL_ATTRIBUTES, LVCFMT_LEFT, 90, COL_ATTRIBUTES);
    if (COptions::ShowColumnOwner) InsertCol(IDS_COL_OWNER, LVCFMT_LEFT, 200, COL_OWNER);

    m_control.OnColumnsInserted();
}

int CFileTreeView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CControlView::OnCreate(lpCreateStruct) == -1) return -1;

    constexpr RECT rect = { 0, 0, 0, 0 };
    m_control.CreateExtended(LVS_EX_HEADERDRAGDROP, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL);
    m_control.ShowGrid(COptions::ListGrid);
    m_control.ShowStripes(COptions::ListStripes);
    m_control.ShowFullRowSelection(COptions::ListFullRowSelection);

    CreateColumns(true);

    return 0;
}

void CFileTreeView::OnUpdate(CView* pSender, const LPARAM lHint, CObject* pHint)
{
    ASSERT(AfxGetThread() != nullptr);

    if (lHint == HINT_NEWROOT)
    {
        m_control.SetRootItem(CDirStatDoc::Get()->GetRootItem());
    }

    CControlView::OnUpdate(pSender, lHint, pHint);
}

IMPLEMENT_DYNCREATE(CFileWatcherView, CControlView)

BEGIN_MESSAGE_MAP(CFileWatcherView, CControlView)
    ON_WM_CREATE()
END_MESSAGE_MAP()

int CFileWatcherView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CControlView::OnCreate(lpCreateStruct) == -1) return -1;

    constexpr RECT rect = {0, 0, 0, 0};
    m_control.CreateExtended(LVS_EX_HEADERDRAGDROP, LVS_SINGLESEL | WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL);
    m_control.ShowGrid(COptions::ListGrid);
    m_control.ShowStripes(COptions::ListStripes);
    m_control.ShowFullRowSelection(COptions::ListFullRowSelection);

    // Columns should be in enumeration order so initial sort will work
    InsertCol(IDS_COL_NAME, LVCFMT_LEFT, 500, COL_ITEMWATCH_NAME);
    InsertCol(IDS_COL_TIME, LVCFMT_LEFT, 110, COL_ITEMWATCH_TIME);
    InsertCol(IDS_COL_OPERATION, LVCFMT_LEFT, 100, COL_ITEMWATCH_ACTION);
    InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 90, COL_ITEMWATCH_SIZE_LOGICAL);
    m_control.SetSorting(COL_ITEMWATCH_TIME, false);

    m_control.OnColumnsInserted();

    return 0;
}

IMPLEMENT_DYNCREATE(CFileTopView, CControlView)

BEGIN_MESSAGE_MAP(CFileTopView, CControlView)
    ON_WM_CREATE()
END_MESSAGE_MAP()

int CFileTopView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CControlView::OnCreate(lpCreateStruct) == -1) return -1;
    
    constexpr RECT rect = { 0, 0, 0, 0 };
    m_control.CreateExtended(LVS_EX_HEADERDRAGDROP, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL);
    m_control.ShowGrid(COptions::ListGrid);
    m_control.ShowStripes(COptions::ListStripes);
    m_control.ShowFullRowSelection(COptions::ListFullRowSelection);

    // Columns should be in enumeration order so initial sort will work
    InsertCol(IDS_COL_NAME, LVCFMT_LEFT, 500, COL_ITEMTOP_NAME);
    InsertCol(IDS_COL_SIZE_PHYSICAL, LVCFMT_RIGHT, 90, COL_ITEMTOP_SIZE_PHYSICAL);
    InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 90, COL_ITEMTOP_SIZE_LOGICAL);
    InsertCol(IDS_COL_LAST_CHANGE, LVCFMT_LEFT, 120, COL_ITEMTOP_LAST_CHANGE);
    m_control.SetSorting(COL_ITEMTOP_SIZE_PHYSICAL, false);

    m_control.OnColumnsInserted();

    return 0;
}

IMPLEMENT_DYNCREATE(CFileDupeView, CControlView)

BEGIN_MESSAGE_MAP(CFileDupeView, CControlView)
    ON_WM_CREATE()
END_MESSAGE_MAP()

int CFileDupeView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CControlView::OnCreate(lpCreateStruct) == -1) return -1;

    constexpr RECT rect = { 0, 0, 0, 0 };
    m_control.CreateExtended(LVS_EX_HEADERDRAGDROP, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL);
    m_control.ShowGrid(COptions::ListGrid);
    m_control.ShowStripes(COptions::ListStripes);
    m_control.ShowFullRowSelection(COptions::ListFullRowSelection);

    // Columns should be in enumeration order so initial sort will work
    const std::wstring hashName = Localization::Lookup(IDS_COL_HASH) + L" / " + Localization::Lookup(IDS_COL_NAME);
    m_control.InsertColumn(CHAR_MAX, hashName.c_str(), LVCFMT_LEFT, DpiRest(500), COL_ITEMDUP_NAME);
    InsertCol(IDS_COL_ITEMS, LVCFMT_RIGHT, 70, COL_ITEMDUP_ITEMS);
    InsertCol(IDS_COL_SIZE_PHYSICAL, LVCFMT_RIGHT, 80, COL_ITEMDUP_SIZE_PHYSICAL);
    InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 80, COL_ITEMDUP_SIZE_LOGICAL);
    InsertCol(IDS_COL_LAST_CHANGE, LVCFMT_LEFT, 120, COL_ITEMDUP_LAST_CHANGE);
    m_control.SetSorting(COL_ITEMDUP_SIZE_PHYSICAL, false);

    m_control.OnColumnsInserted();

    return 0;
}

IMPLEMENT_DYNCREATE(CFileSearchView, CControlView)

BEGIN_MESSAGE_MAP(CFileSearchView, CControlView)
    ON_WM_CREATE()
END_MESSAGE_MAP()

int CFileSearchView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CControlView::OnCreate(lpCreateStruct) == -1) return -1;

    constexpr RECT rect = { 0, 0, 0, 0 };
    m_control.CreateExtended(LVS_EX_HEADERDRAGDROP, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL);
    m_control.ShowGrid(COptions::ListGrid);
    m_control.ShowStripes(COptions::ListStripes);
    m_control.ShowFullRowSelection(COptions::ListFullRowSelection);

    // Columns should be in enumeration order so initial sort will work
    InsertCol(IDS_COL_NAME, LVCFMT_LEFT, 500, COL_ITEMSEARCH_NAME);
    InsertCol(IDS_COL_SIZE_PHYSICAL, LVCFMT_RIGHT, 90, COL_ITEMSEARCH_SIZE_PHYSICAL);
    InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 90, COL_ITEMSEARCH_SIZE_LOGICAL);
    InsertCol(IDS_COL_LAST_CHANGE, LVCFMT_LEFT, 120, COL_ITEMSEARCH_LAST_CHANGE);
    m_control.SetSorting(COL_ITEMSEARCH_SIZE_LOGICAL, false);

    m_control.OnColumnsInserted();

    return 0;
}


