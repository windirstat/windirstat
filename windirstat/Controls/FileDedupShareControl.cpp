// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ItemDedupShare.h"
#include "FileTreeView.h"
#include "DedupAccounting.h"

CFileDedupShareControl::CFileDedupShareControl() : CTreeListControl(COptions::DedupShareViewColumnOrder.Ptr(),
    COptions::DedupShareViewColumnWidths.Ptr(), COptions::DedupShareViewColumnVisibility.Ptr(), LF_DEDUPSHARELIST, false)
{
    m_singleton = this;
}

bool CFileDedupShareControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMDEDUPSHARE_NAME;
}

void CFileDedupShareControl::ProcessSharing(const std::vector<CItem*>& items)
{
    if (!CWinDirStatModel::Get()->IsScanSettled() || items.empty()) return;

    // The index is in memory, so the query is quick; keep the old results on failure
    CWaitCursor wait;
    const auto sharing = CDedupAccounting::Get().QuerySharing(items);
    if (!sharing.success)
    {
        ShowMessageBox(sharing.error);
        return;
    }

    // Tab must be visible before it can be activated
    CMainFrame::Get()->GetFileTabbedView()->SetDedupShareTabVisibility(true);
    Populate(items, sharing, {});
    CMainFrame::Get()->GetFileTabbedView()->SetActiveDedupShareView();
    CMainFrame::Get()->MoveFocus(LF_DEDUPSHARELIST);
}

void CFileDedupShareControl::Populate(const std::vector<CItem*>& items, const CDedupAccounting::SharingResult& sharing,
    const std::unordered_set<std::wstring>& selectPaths)
{
    // Remove previous results
    SetRootItem();
    m_queried = items;

    const std::wstring title = items.size() == 1 ? items.front()->GetPath() :
        Localization::Format(IDS_DEDUP_SHARING_ITEMS, FormatCount(items.size()));
    m_rootItem->SetSummary(title, sharing.queriedBytes, sharing.sharedBytes, sharing.outsideBytes);

    CollapseItem(0);
    std::vector<const CItemDedupShare*> selected;
    {
        const ScopedRedrawPause lock(this);
        for (const auto& row : sharing.rows)
        {
            auto* shareItem = new CItemDedupShare(row.item, row.sharedBytes, row.itemBytes);
            m_rootItem->AddShareItemChild(shareItem);
            if (!selectPaths.empty() && selectPaths.contains(row.item->GetPath())) selected.push_back(shareItem);
        }
        SortItems();
        ExpandItem(0, false);
    }
    for (const auto* item : selected) SelectItem(item, false, true);
}

void CFileDedupShareControl::RemoveVolumes(const std::unordered_set<CItem*>& volumes)
{
    if (m_rootItem == nullptr || m_queried.empty()) return;

    // Reverting a volume reaches into volumes mounted below it, so any listed
    // item inside one of the subtrees may be freed or change its values
    const auto inScope = [&](const CItem* item)
    {
        return std::ranges::any_of(volumes, [item](const CItem* volume) { return volume == item || volume->IsAncestorOf(item); });
    };
    const auto rows = m_rootItem->GetShareChildren();
    if (!std::ranges::any_of(m_queried, inScope) &&
        !std::ranges::any_of(rows, [&](CItemDedupShare* row) { return inScope(row->GetLinkedItem()); })) return;

    // Remember the query by path since the items may be replaced by the refresh
    m_pendingPaths.clear();
    m_pendingSelection.clear();
    for (const CItem* item : m_queried) m_pendingPaths.push_back(item->GetPath());
    for (auto* row : GetAllSelected<CItemDedupShare>(true))
    {
        if (const CItem* item = row->GetLinkedItem(); item != nullptr) m_pendingSelection.insert(item->GetPath());
    }

    // Collapsing first removes the visible rows in one pass
    const ScopedRedrawPause lock(this);
    CollapseItem(0);
    for (auto* row : rows) m_rootItem->RemoveShareItemChild(row);
    m_rootItem->SetSummary({}, 0, 0, 0);
    m_queried.clear();
}

void CFileDedupShareControl::RestoreItems()
{
    if (m_pendingPaths.empty() || m_rootItem == nullptr) return;

    std::vector<CItem*> items;
    for (const auto& path : m_pendingPaths)
    {
        if (CItem* item = CWinDirStatModel::Get()->GetRootItem()->FindItemByPath(path); item != nullptr)
            items.push_back(item);
    }

    if (items.empty())
    {
        // Everything queried is gone
        m_pendingPaths.clear();
        m_pendingSelection.clear();
        return;
    }

    // A stopped or failed refresh leaves the volume unaccounted; the query is
    // kept so a later refresh can still restore it
    const auto sharing = CDedupAccounting::Get().QuerySharing(items);
    if (!sharing.success) return;

    const auto selection = std::move(m_pendingSelection);
    m_pendingPaths.clear();
    m_pendingSelection.clear();
    Populate(items, sharing, selection);
}

void CFileDedupShareControl::AfterDeleteAllItems()
{
    m_queried.clear();
    m_pendingPaths.clear();
    m_pendingSelection.clear();

    // Delete and recreate root item
    delete m_rootItem;
    m_rootItem = new CItemDedupShare();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
