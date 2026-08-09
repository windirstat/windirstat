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

void CFileTreeView::InitializeColumns()
{
    auto& control = GetControl();

    // Columns should be in enumeration order so initial sort will work
    InsertCol(IDS_COL_NAME, LVCFMT_LEFT, 250, COL_NAME);
    InsertCol(IDS_COL_SIZE_PROPORTION, LVCFMT_RIGHT, CItem::GetSizeProportionWidth() + 30, COL_SIZE_PROPORTION);
    InsertCol(IDS_COL_PERCENTAGE, LVCFMT_RIGHT, 90, COL_PERCENTAGE);
    InsertCol(IDS_COL_SIZE_PHYSICAL, LVCFMT_RIGHT, 90, COL_SIZE_PHYSICAL);
    InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 90, COL_SIZE_LOGICAL);
    InsertCol(IDS_COL_ITEMS, LVCFMT_RIGHT, 90, COL_ITEMS);
    InsertCol(IDS_COL_FILES, LVCFMT_RIGHT, 90, COL_FILES);
    InsertCol(IDS_COL_FOLDERS, LVCFMT_RIGHT, 90, COL_FOLDERS);
    InsertCol(IDS_COL_LAST_CHANGE, LVCFMT_LEFT, 120, COL_LAST_CHANGE);
    InsertCol(IDS_COL_ATTRIBUTES, LVCFMT_LEFT, 90, COL_ATTRIBUTES);
    InsertCol(IDS_COL_OWNER, LVCFMT_LEFT, 200, COL_OWNER);
    control.SetSorting(COL_SIZE_PROPORTION, control.GetAscendingDefault(COL_SIZE_PROPORTION));
    control.OnColumnsInserted(
        { COL_NAME, COL_SIZE_PROPORTION },
        { COL_ITEMS, COL_FOLDERS, COL_ATTRIBUTES, COL_OWNER });
}

void CFileTreeView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    CControlView::OnUpdate(sender, change, CWinDirStatModel::Get()->GetRootItem());

    if (change == MODEL_CHANGE_SELECTION_ACTION)
    {
        GetControl().EmulateInteractiveSelection(item);
    }
}

void CFileWatcherView::InitializeColumns()
{
    auto& control = GetControl();

    // Columns should be in enumeration order so initial sort will work
    InsertCol(IDS_COL_NAME, LVCFMT_LEFT, 500, COL_ITEMWATCH_NAME);
    InsertCol(IDS_COL_TIME, LVCFMT_LEFT, 150, COL_ITEMWATCH_TIME);
    InsertCol(IDS_COL_OPERATION, LVCFMT_LEFT, 100, COL_ITEMWATCH_ACTION);
    InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 90, COL_ITEMWATCH_SIZE_LOGICAL);
    control.SetSorting(COL_ITEMWATCH_TIME, true);
    control.OnColumnsInserted();
}

void CFilePermsView::InitializeColumns()
{
    auto& control = GetControl();

    // Columns should be in enumeration order so initial sort will work
    InsertCol(IDS_COL_NAME, LVCFMT_LEFT, 400, COL_ITEMPERM_NAME);
    InsertCol(IDS_COL_ACCOUNT, LVCFMT_LEFT, 180, COL_ITEMPERM_ACCOUNT);
    InsertCol(IDS_COL_ACCESS, LVCFMT_LEFT, 70, COL_ITEMPERM_TYPE);
    InsertCol(IDS_COL_RIGHTS, LVCFMT_LEFT, 150, COL_ITEMPERM_RIGHTS);
    InsertCol(IDS_COL_APPLIES_TO, LVCFMT_LEFT, 200, COL_ITEMPERM_APPLIESTO);
    InsertCol(IDS_COL_INHERITANCE, LVCFMT_LEFT, 90, COL_ITEMPERM_INHERITANCE);
    control.SetSorting(COL_ITEMPERM_NAME, true);
    control.OnColumnsInserted();
}

void CFileTopView::InitializeColumns()
{
    auto& control = GetControl();

    // Columns should be in enumeration order so initial sort will work
    InsertCol(IDS_COL_NAME, LVCFMT_LEFT, 500, COL_ITEMTOP_NAME);
    InsertCol(IDS_COL_SIZE_PHYSICAL, LVCFMT_RIGHT, 90, COL_ITEMTOP_SIZE_PHYSICAL);
    InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 90, COL_ITEMTOP_SIZE_LOGICAL);
    InsertCol(IDS_COL_LAST_CHANGE, LVCFMT_LEFT, 120, COL_ITEMTOP_LAST_CHANGE);
    control.SetSorting(COL_ITEMTOP_SIZE_PHYSICAL, false);
    control.OnColumnsInserted();
}

void CFileDupeView::InitializeColumns()
{
    auto& control = GetControl();

    // Columns should be in enumeration order so initial sort will work
    const std::wstring hashName = Localization::Lookup(IDS_COL_HASH) + L" / " + Localization::Lookup(IDS_COL_NAME);
    control.InsertColumn(CHAR_MAX, hashName.c_str(), LVCFMT_LEFT, ScaleForDpi(500), COL_ITEMDUP_NAME);
    InsertCol(IDS_COL_ITEMS, LVCFMT_RIGHT, 70, COL_ITEMDUP_ITEMS);
    InsertCol(IDS_COL_SIZE_PHYSICAL, LVCFMT_RIGHT, 80, COL_ITEMDUP_SIZE_PHYSICAL);
    InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 80, COL_ITEMDUP_SIZE_LOGICAL);
    InsertCol(IDS_COL_LAST_CHANGE, LVCFMT_LEFT, 120, COL_ITEMDUP_LAST_CHANGE);
    control.SetSorting(COL_ITEMDUP_SIZE_PHYSICAL, false);
    control.OnColumnsInserted();
}

void CFileSearchView::InitializeColumns()
{
    auto& control = GetControl();

    // Columns should be in enumeration order so initial sort will work
    InsertCol(IDS_COL_NAME, LVCFMT_LEFT, 500, COL_ITEMSEARCH_NAME);
    InsertCol(IDS_COL_SIZE_PHYSICAL, LVCFMT_RIGHT, 90, COL_ITEMSEARCH_SIZE_PHYSICAL);
    InsertCol(IDS_COL_SIZE_LOGICAL, LVCFMT_RIGHT, 90, COL_ITEMSEARCH_SIZE_LOGICAL);
    InsertCol(IDS_COL_LAST_CHANGE, LVCFMT_LEFT, 120, COL_ITEMSEARCH_LAST_CHANGE);
    control.SetSorting(COL_ITEMSEARCH_SIZE_LOGICAL, false);
    control.OnColumnsInserted();
}
