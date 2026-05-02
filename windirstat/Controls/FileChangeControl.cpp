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
#include "FileChangeControl.h"

CFileChangeControl::CFileChangeControl() : CTreeListControl(COptions::ChangeViewColumnOrder.Ptr(), COptions::ChangeViewColumnWidths.Ptr(), LF_CHANGELIST, false)
{
    m_singleton = this;
}

bool CFileChangeControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMCHANGE_NAME;
}

BEGIN_MESSAGE_MAP(CFileChangeControl, CTreeListControl)
END_MESSAGE_MAP()

CFileChangeControl* CFileChangeControl::m_singleton = nullptr;

void CFileChangeControl::SetChanges(const SnapshotGrowthResult& result)
{
    SetRootItem();
    m_hasChanges = !result.entries.empty();
    m_rootItem->SetPreviousSnapshotLabel(result.previousSnapshotLabel);
    m_rootItem->SetExpanded(false);
    m_rootItem->ReserveChangeItemChildren(result.entries.size());
    std::vector<CWdsListItem*> children;
    children.reserve(result.entries.size());

    SetRedraw(FALSE);
    for (const auto& entry : result.entries)
    {
        auto* child = new CItemChange(entry);
        m_rootItem->AddChangeItemChild(child);
        child->SetVisible(this, true);
        children.push_back(child);
    }

    m_rootItem->SetExpanded(true);
    if (!children.empty())
    {
        InsertListItem(1, children);
    }

    SetRedraw(TRUE);
    UpdateSortIndicator();
    Invalidate();
}

void CFileChangeControl::ClearChanges()
{
    SetRootItem();
    m_hasChanges = false;
}

void CFileChangeControl::AfterDeleteAllItems()
{
    delete m_rootItem;
    m_rootItem = new CItemChange();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
